/*
 * Copyright (C) 2013-2016 Trent Houliston <trent@houliston.me>, Jake Woods <jake.f.woods@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/// <reference types="node" />

export interface NUClearNetOptions {
    name: string
    group?: string
    port?: number
    mtu?: number
}

export interface NUClearNetSend {
    type: string|Buffer
    payload: Buffer
    target?: string
    reliable?: boolean
}

export interface NUClearNetPeer {
    name: string
    address: string
    port: number
}

export interface NUClearNetPacket {
    peer: NUClearNetPeer
    payload: Buffer
    reliable: boolean
}

export declare class NUClearNet {

    public constructor()

    public on(event: 'nuclear_join'|'nuclear_leave', callback: (peer: NUClearNetPeer) => void): this

    public on(event: string, callback: (packet: NUClearNetPacket) => void): this

    public removeListener(event: string, listener: Function): this

    public connect(options: NUClearNetOptions): void

    public disconnect(): void

    public send(options: NUClearNetSend): void
}
