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
  /** The name of this node on the NUClearNetwork */
  name: string;

  /** The announce address for this network. Defaults to `239.226.152.162`. */
  address?: string;

  /** The announce port for this network. Defaults to `7447`. */
  port?: number;

  /** The MTU of the network. Used for splitting packets optimally. */
  mtu?: number;
}

/**
 * Data provided for sending information on the network
 */
export interface NUClearNetSend {
  /**
   * The type of the message to send: either a string that will be hashed,
   * or a Buffer with the 64 bit hash.
   */
  type: string | Buffer;

  /** The data to send */
  payload: Buffer;

  /**
   * The target to send the packet to. If `undefined`, the packet will be sent
   * to everyone on the network.
   */
  target?: string;

  /**
   * If `true`, the packet should be sent using a reliable protocol
   * which handles retransmissions.
   */
  reliable?: boolean;
}

/**
 * Information about a peer on the NUClear network
 */
export interface NUClearNetPeer {
  /** The name the peer has on the network. See `NUClearNetOptions.name` */
  name: string;

  /** The IP address of the peer: either a dotted decimal IPv4 string, or an IPv6 string. */
  address: string;

  /**
   * The port the peer has connected on. This will be an ephemeral port.
   * The combination of `address` + `port` will be unique for a peer.
   */
  port: number;
}

/**
 * A data packet that received on the NUClear network
 */
export interface NUClearNetPacket {
  /** The peer the packet was sent from */
  peer: NUClearNetPeer;

  /** The hash code of the packet's type */
  hash: Buffer;

  /** The data that was sent from the peer */
  payload: Buffer;

  /**
   * Will be set to `true`, if the peer sent the packet with reliable transmission
   * (see `NUClearNetSend.reliable`).
   */
  reliable: boolean;
}

/**
 * A packet that is received using `on('message.type')` will always have a known string type:
 * the same string that was provided in the on statement.
 */
export interface NUClearNetTypedPacket extends NUClearNetPacket {
  /** The type that was provided in the on statement for this packet */
  type: string;
}

/**
 * When using `on('nuclear_packet')` the type as a string may or may not be known depending on
 * whether another user has requested the same type. This interface represents that type of packet.
 */
export interface NUClearNetMaybeTypedPacket extends NUClearNetPacket {
  /**
   * The type that was provided in an on statement, or `undefined` if nobody has executed
   * an on statement for this type.
   */
  type: string | undefined;
}

/**
 * Represents a NUClearNet network client.
 *
 * Usage notes:
 *   - Before it will provide data, the instance must be connected first via `.connect()`.
 *   - Join and leave callbacks should be set before calling `.connect()`. If not, join
 *     events from already connected peers will not be received.
 *   - After calling `.destroy()`, the instance should not be used again.
 */
export declare class NUClearNet {
  /** Stores the `connect()` options. Is an empty object until `connect()` is called. */
  options: Partial<NUClearNetOptions>;

  /** Create a new NUClearNet instance. */
  public constructor();

  /** Emitted when a peer joins or leaves the network. */
  public on(event: 'nuclear_join' | 'nuclear_leave', callback: (peer: NUClearNetPeer) => void): this;

  /** Emitted when NUClearNet receives any packet */
  public on(event: 'nuclear_packet', callback: (packet: NUClearNetMaybeTypedPacket) => void): this;

  /** Emitted when connection to the network is lost */
  public on(event: 'disconnect', callback: () => void): this;

  /** Emitted when the given packet is received */
  public on(event: string, callback: (packet: NUClearNetTypedPacket) => void): this;

  /**
   * Hash the provided string using the NUClearNet hashing method.
   * These hashes will be identical to those used by NUClear
   */
  public hash(data: string): Buffer;

  /**
   * Stop listening for the given type.
   * Note that after removing the last listener for a type, the type will revert to
   * being an undefined type for `NUClearNetMaybeTypedPacket`.
   */
  public removeListener(event: string, listener: Function): this;

  /**
   * Connect this instance to the NUclear network.
   * Subsequent calls to this function will disconnect from the previous network and
   * reconnect to a new one.
   */
  public connect(options: NUClearNetOptions): void;

  /**
   * Disconnect this instance from the NUClear network.
   * Does not remove event listeners, therefore reconnecting will resume events.
   */
  public disconnect(): void;

  /**
   * Disconnect and destroy this NUClearNetwork instance, clearing all event listeners.
   * Attempting to use this instance after calling `destroy()` will throw an error.
   */
  public destroy(): void;

  /**
   * Send the given packet over the NUClear network.
   * Will throw if the network is not connected.
   */
  public send(options: NUClearNetSend): void;
}
