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

'use strict';

const { NetworkBinding } = require('bindings')('nuclearnet');
const { EventEmitter } = require('events');

class NUClearNet extends EventEmitter {
  constructor() {
    super();

    // Create a new network object
    this._net = new NetworkBinding();
    this._callbackMap = {};
    this._active = false;
    this._waiting = 0;
    this._destroyed = false;

    // Stores the connect() options
    this.options = {};

    // We have started listening to a new type
    this.on('newListener', (event) => {
      this.assertNotDestroyed();

      if (
        event !== 'nuclear_join' &&
        event !== 'nuclear_leave' &&
        event !== 'nuclear_packet' &&
        event !== 'newListener' &&
        event !== 'removeListener' &&
        event !== 'disconnect' &&
        this.listenerCount(event) === 0
      ) {
        const hash = this._net.hash(event);
        this._callbackMap[hash] = event;
      }
    });

    // We are no longer listening to this type
    this.on('removeListener', (event) => {
      // If we are no longer listening to this type
      if (
        event !== 'nuclear_join' &&
        event !== 'nuclear_leave' &&
        event !== 'nuclear_packet' &&
        event !== 'newListener' &&
        event !== 'removeListener' &&
        event !== 'disconnect' &&
        this.listenerCount(event) === 0
      ) {
        // Get our hash and delete it
        const hash = this._net.hash(event);
        delete this._callbackMap[hash];
      }
    });

    // Pass our javascript callbacks to the C++ side
    this._net.onPacket(this._onPacket.bind(this));
    this._net.onJoin(this._onJoin.bind(this));
    this._net.onLeave(this._onLeave.bind(this));
    this._net.onWait(this._onWait.bind(this));
  }

  _onPacket(name, address, port, reliable, hash, payload) {
    const eventName = this._callbackMap[hash];

    // Construct our packet
    const packet = {
      peer: {
        name: name,
        address: address,
        port: port,
      },
      payload: payload,
      type: eventName,
      hash: hash,
      reliable: reliable,
    };

    // Emit via nuclear_packet for people listening to everything
    this.emit('nuclear_packet', packet);

    // If someone was listening to this send it to them specifically too
    if (eventName !== undefined) {
      this.emit(eventName, packet);
    }
  }

  _onJoin(name, address, port) {
    this.emit('nuclear_join', {
      name: name,
      address: address,
      port: port,
    });
  }

  _onLeave(name, address, port) {
    this.emit('nuclear_leave', {
      name: name,
      address: address,
      port: port,
    });
  }

  _onWait(duration) {
    ++this._waiting;

    setTimeout(() => {
      --this._waiting;

      // Only process if we're active
      if (this._active) {
        try {
          this._net.process();
        } catch {
          // An error occurred during processing, disconnect.
          // This needs to check again if this is still active, as multiple
          // `_onWait` calls run concurrently, and only the first one to fail
          // should disconnect.
          if (this._active) {
            this.disconnect();
          }
        }
      }

      // Sometimes due to weird timing artifacts we run out of these
      // Restart in 100ms!
      if (this._active && this._waiting === 0) {
        this._onWait(100);
      }
    }, duration);
  }

  hash(data) {
    this.assertNotDestroyed();
    return this._net.hash(data);
  }

  connect(options) {
    this.assertNotDestroyed();

    // Store the options
    this.options = options;

    // Default some of the options
    const name = options.name;
    const address = options.address === undefined ? '239.226.152.162' : options.address;
    const port = options.port === undefined ? 7447 : options.port;
    const mtu = options.mtu === undefined ? 1500 : options.mtu;

    // Connect to the network
    this._net.reset(name, address, port, mtu);

    // Run our first "process" to kick things off
    this._net.process();

    // If the first process is successful we are active
    this._active = true;
  }

  disconnect() {
    this.assertNotDestroyed();

    this._active = false;
    this._net.shutdown();

    this.emit('disconnect');
  }

  send(options) {
    this.assertNotDestroyed();

    if (!this._active) {
      throw new Error('The network is not currently connected');
    } else {
      this._net.send(
        options.type,
        options.payload,
        options.target,
        options.reliable !== undefined ? options.reliable : false
      );
    }
  }

  destroy() {
    if (this._active) {
      this.disconnect();
    }

    this.removeAllListeners();

    for (const prop in this._callbackMap) {
      delete this._callbackMap[prop];
    }

    this._net.destroy();

    this._destroyed = true;
  }

  assertNotDestroyed() {
    if (this._destroyed) {
      throw new Error('This network instance has been destroyed');
    }
  }

  on(...args) {
    this.assertNotDestroyed();
    return super.on(...args);
  }

  addListener(...args) {
    this.assertNotDestroyed();
    return super.addListener(...args);
  }
}

exports.NUClearNet = NUClearNet;
