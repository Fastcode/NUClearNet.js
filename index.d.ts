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

/**
 * NUClearNet options for connecting to the network
 */
export interface NUClearNetOptions {
    /// The name of this node on the NUClearNetwork
    name: string
    /// The multicast group this network is on, supports ipv4 and ipv6
    group?: string
    /// The port the mulitcast group runs on
    port?: number
    /// The MTU of the network. Used for splitting packets optimally
    mtu?: number
}

/**
 * Data provided for sending information on the network
 */
export interface NUClearNetSend {
    /// The type of the message to send, either as a string that will be hashed or as a 64 bit hash directly
    type: string|Buffer
    /// The data to send
    payload: Buffer
    /// The target to send the packet to, undefined means send to everyone on the network
    target?: string
    /// If the packet should be sent using a more reliable protocol which handles retransmissions
    reliable?: boolean
}

/**
 * Information for a peer on the NUClear network
 */
export interface NUClearNetPeer {
    /// The name the peer has on the network. See NUClearNetOptions.name
    name: string
    /// The IP address of the peer either as dotted decimal for IPv4 or an IPv6 string
    address: string
    /// The port the peer has connected on.
    /// This will be an ephemeral port and the combination of address+port will be unique for a peer
    port: number
}

/**
 * A data packet that is received by the NUClear network
 */
export interface NUClearNetPacket {
    /// The peer the packet was sent from
    peer: NUClearNetPeer
    /// The hashcode of the packets type
    hash: Buffer
    /// The bytes that were sent from the peer
    payload: Buffer
    /// If the peer sent this as a reliable transmission (see NUClearNetSend.reliable)
    reliable: boolean
}

/**
 * A packet that will be received using on('message.type') will always have a known string type
 * It will be the same string as provided in the on statement
 */
export interface NUClearNetTypedPacket extends NUClearNetPacket {
    /// The type that was provided in the on statement for this packet
    type: string
}

/**
 * When using on('nuclear_packet') the type as a string may or may not be known depending on if another
 * user has requested this type.
 */
export interface NUClearNetMaybeTypedPacket extends NUClearNetPacket {
    /// The type that was provided in an on statement, or undefined if nobody has executed an on statement for this type
    type: string|undefined
}

/**
 * Create a new NUClearNet instance
 * Must be connected before it will provide data however you should set your join and leave callbacks before calling
 * connect.
 * If connect is called first you will not receive the join events from already connected peers.
 */
export declare class NUClearNet {

    public constructor()

    /// Will fire when a peer joins or leaves the network.
    public on(event: 'nuclear_join'|'nuclear_leave', callback: (peer: NUClearNetPeer) => void): this

    /// Will fire when NUClearNet receives any packet
    public on(event: 'nuclear_packet', callback: (packet: NUClearNetMaybeTypedPacket) => void): this

    /// Will fire when the requested packet is received
    public on(event: string, callback: (packet: NUClearNetTypedPacket) => void): this

    /// Hash the provided string using the hashing method of NUClearNet.
    /// These hashes will be identical to those used by NUClearNet
    public hash(data: string): Buffer

    /// Stop listening for a type
    /// Note that if this is the last listener for a type, the type will revert to being an undefined type
    /// for NUClearNetMaybeTypedPacket
    public removeListener(event: string, listener: Function): this

    /// Connect to the NUClearNetwork
    /// If this function is called multiple times
    /// it will disconnect from the previous network and reconnect to a new one
    public connect(options: NUClearNetOptions): void

    /// Disconnect from the NUClearNetwork
    /// Does not disconnect listeners and reconnecting will resume events
    public disconnect(): void

    /// Send the packet over the NUClearNetwork
    /// This function will throw an error if the network is not connected
    public send(options: NUClearNetSend): void
}
