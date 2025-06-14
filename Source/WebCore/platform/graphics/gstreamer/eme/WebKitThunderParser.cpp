/* GStreamer Thunder Parser
 *
 * Copyright (C) 2025 Comcast Inc.
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitThunderParser.h"

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)

#include "CDMProxyThunder.h"
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringView.h>

GST_DEBUG_CATEGORY(webkitMediaThunderParserDebugCategory);
#define GST_CAT_DEFAULT webkitMediaThunderParserDebugCategory

typedef struct _WebKitMediaThunderParserPrivate {
    GRefPtr<GstElement> decryptor;
    GRefPtr<GstElement> parser;
} WebKitMediaThunderParserPrivate;

typedef struct _WebKitMediaThunderParser {
    GstBin parent;
    WebKitMediaThunderParserPrivate* priv;
} WebKitMediaThunderParser;

typedef struct _WebKitMediaThunderParserClass {
    GstBinClass parentClass;
} WebKitMediaThunderParserClass;

using namespace WebCore;

WEBKIT_DEFINE_TYPE(WebKitMediaThunderParser, webkit_media_thunder_parser, GST_TYPE_BIN)

static GstStaticPadTemplate thunderParseSrcTemplate = GST_STATIC_PAD_TEMPLATE("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS(
        "video/webm; "
        "audio/webm; "
        "video/mp4; "
        "audio/mp4; "
        "audio/mpeg; "
        "audio/x-flac; "
        "audio/x-eac3; "
        "audio/x-ac3; "
        "video/x-h264; "
        "video/x-h265; "
        "video/x-vp9; video/x-vp8; "
        "video/x-av1; "
        "audio/x-opus; audio/x-vorbis"));

static GRefPtr<GstCaps> createThunderParseSinkPadTemplateCaps()
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_new_empty());

    auto& supportedKeySystems = CDMFactoryThunder::singleton().supportedKeySystems();

    if (supportedKeySystems.isEmpty()) {
        GST_WARNING("no supported key systems in Thunder, we won't be able to decrypt anything with the decryptor");
        return caps;
    }

    for (const auto& mediaType : GStreamerEMEUtilities::s_cencEncryptionMediaTypes) {
        gst_caps_append_structure(caps.get(), gst_structure_new_empty(mediaType.characters()));
        gst_caps_append_structure(caps.get(), gst_structure_new("application/x-cenc", "original-media-type", G_TYPE_STRING, mediaType.characters(), nullptr));
    }
    for (const auto& keySystem : supportedKeySystems) {
        for (const auto& mediaType : GStreamerEMEUtilities::s_cencEncryptionMediaTypes) {
            gst_caps_append_structure(caps.get(), gst_structure_new("application/x-cenc", "original-media-type", G_TYPE_STRING,
                mediaType.characters(), "protection-system", G_TYPE_STRING, GStreamerEMEUtilities::keySystemToUuid(keySystem), nullptr));
        }
    }

    if (supportedKeySystems.contains(GStreamerEMEUtilities::s_WidevineKeySystem) || supportedKeySystems.contains(GStreamerEMEUtilities::s_ClearKeyKeySystem)) {
        for (const auto& mediaType : GStreamerEMEUtilities::s_webmEncryptionMediaTypes) {
            gst_caps_append_structure(caps.get(), gst_structure_new_empty(mediaType.characters()));
            gst_caps_append_structure(caps.get(), gst_structure_new("application/x-webm-enc", "original-media-type", G_TYPE_STRING, mediaType.characters(), nullptr));
        }
    }

    return caps;
}

static void webkitMediaThunderParserConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_media_thunder_parser_parent_class)->constructed(object);

    auto self = WEBKIT_MEDIA_THUNDER_PARSER(object);

    self->priv->decryptor = gst_element_factory_make("webkitthunder", nullptr);
    self->priv->parser = makeGStreamerElement("parsebin"_s, "inner-parser"_s);

    gst_bin_add_many(GST_BIN_CAST(self), self->priv->decryptor.get(), self->priv->parser.get(), nullptr);
    gst_element_link(self->priv->decryptor.get(), self->priv->parser.get());

    g_signal_connect(self->priv->parser.get(), "autoplug-factories", G_CALLBACK(+[](GstElement*, GstPad*, GstCaps* caps, gpointer) -> GValueArray* {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN;
        GValueArray* result;

        auto factories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODABLE, GST_RANK_MARGINAL);
        auto list = gst_element_factory_list_filter(factories, caps, GST_PAD_SINK, gst_caps_is_fixed(caps));
        result = g_value_array_new(g_list_length(list));
        for (GList* tmp = list; tmp; tmp = tmp->next) {
            auto factory = GST_ELEMENT_FACTORY_CAST(tmp->data);
            auto name = StringView::fromLatin1(gst_plugin_feature_get_name(GST_PLUGIN_FEATURE_CAST(factory)));
            if (name == "webkitthunderparser"_s)
                continue;

            GValue value = G_VALUE_INIT;
            g_value_init(&value, G_TYPE_OBJECT);
            g_value_set_object(&value, factory);
            g_value_array_append(result, &value);
            g_value_unset(&value);
        }
        gst_plugin_feature_list_free(list);
        gst_plugin_feature_list_free(factories);
        return result;
        ALLOW_DEPRECATED_DECLARATIONS_END;
    }), self);

    g_signal_connect(self->priv->parser.get(), "pad-added", G_CALLBACK(+[](GstElement*, GstPad* pad, gpointer userData) {
        static unsigned counter = 0;
        auto name = makeString("src_"_s, counter);
        counter++;
        gst_element_add_pad(GST_ELEMENT_CAST(userData), gst_ghost_pad_new(name.ascii().data(), pad));
    }), self);

    auto decryptorSinkPad = adoptGRef(gst_element_get_static_pad(self->priv->decryptor.get(), "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(self), gst_ghost_pad_new("sink", decryptorSinkPad.get()));
    gst_bin_sync_children_states(GST_BIN_CAST(self));
}

static void webkit_media_thunder_parser_class_init(WebKitMediaThunderParserClass* klass)
{
    GST_DEBUG_CATEGORY_INIT(webkitMediaThunderParserDebugCategory, "webkitthunderparser", 0, "Thunder parser");

    auto objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitMediaThunderParserConstructed;

    auto elementClass = GST_ELEMENT_CLASS(klass);
    auto padTemplateCaps = createThunderParseSinkPadTemplateCaps();
    gst_element_class_add_pad_template(elementClass, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, padTemplateCaps.get()));
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&thunderParseSrcTemplate));

    gst_element_class_set_static_metadata(elementClass, "Parse potentially encrypted content", "Codec/Parser/Audio/Video",
        "Parse potentially encrypted content", "Philippe Normand <philn@igalia.com>");
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)
