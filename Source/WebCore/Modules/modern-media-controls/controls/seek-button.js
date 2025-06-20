/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

class SeekButton extends Button
{

    constructor(options)
    {
        super(options);

        this.element.addEventListener("mousedown", this);
    }

    // Protected

    handleEvent(event)
    {
        if (event.type === "mousedown" && event.currentTarget === this.element)
            this._didStartPressing();
        else if (event.type === "mouseup")
            this._didStopPressing();
        else
            super.handleEvent(event);
    }

    // Private

    _didStartPressing()
    {
        const mediaControls = this.parentOfType(MediaControls);
        if (!mediaControls)
            return;

        this._mouseupTarget = mediaControls.element;
        this._mouseupTarget.addEventListener("mouseup", this, true);
        this._notifyDelegateOfPressingState(true);
    }

    _didStopPressing()
    {
        this._mouseupTarget.removeEventListener("mouseup", this, true);
        this._notifyDelegateOfPressingState(false);
    }

    _notifyDelegateOfPressingState(isPressed)
    {
        if (this._enabled && this.uiDelegate && typeof this.uiDelegate.buttonPressedStateDidChange === "function")
            this.uiDelegate.buttonPressedStateDidChange(this, isPressed);
    }

}
