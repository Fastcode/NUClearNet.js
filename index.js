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

var NetworkBinding = require('bindings')('nuclearnet');
var util = require('util');
var EventEmitter = require('events').EventEmitter;

var NUClearNet = function() {
    // Create a new network object
    this._net = new NetworkBinding();
    this._callbackMap = {};
    this._active = false;
    this._waiting = 0;

    // We have started listening to a new type
    this.on('newListener', function (event) {
        if(event !== 'nuclear_join'
         && event !== 'nuclear_leave'
         && event !== 'newListener'
         && event !== 'removeListener'
         && this.listenerCount(event) === 0) {
            var hash = this._net.hash(event);
            this._callbackMap[hash] = event;
        }
    }.bind(this));

    // We are no longer listening to this type
    this.on('removeListener', function (event) {
        // If we are no longer listening to this type
        if(event !== 'nuclear_join'
         && event !== 'nuclear_leave'
         && event !== 'newListener'
         && event !== 'removeListener'
         && this.listenerCount(event) === 0) {
            // Get our hash and delete it
            var hash = this._net.hash(event);
            delete this._callbackMap[hash];
        }
    }.bind(this));

    // Bind our callback functions
    this._net.on('packet', this._onPacket.bind(this));
    this._net.on('join', this._onJoin.bind(this));
    this._net.on('leave', this._onLeave.bind(this));
    this._net.on('wait', this._onWait.bind(this));
};

// Inherit from event emitter
util.inherits(NUClearNet, EventEmitter);

NUClearNet.prototype._onPacket = function(name, address, port, reliable, hash, payload) {

    var eventName = this._callbackMap[hash];

    if (eventName !== undefined) {
        this.emit(eventName, {
            'peer': {
                'name': name,
                'address': address,
                'port': port
            },
            'payload': payload,
            'reliable': reliable
        }, payload)
    }
};

NUClearNet.prototype._onJoin = function(name, address, port) {
    this.emit('nuclear_join', {
        'name': name,
        'address': address,
        'port': port
    });
};

NUClearNet.prototype._onLeave = function(name, address, port) {
    this.emit('nuclear_leave', {
        'name': name,
        'address': address,
        'port': port
    });
};

NUClearNet.prototype._onWait = function(duration) {
    ++this._waiting;
    setTimeout(function() {
        --this._waiting;

        // Only process if we're active
        if (this._active) {
            this._net.process();
        }

        // Sometimes due to weird timing artifacts we run out of these
        // Restart in 100ms!
        if (this._active && this._waiting === 0) {
            this._onWait(100);
        }
    }.bind(this), duration);
}


NUClearNet.prototype.connect = function (options) {
    // Default some of the options
    var name = options.name;
    var group = options.group === undefined ? '239.226.152.162' : options.group;
    var port = options.port === undefined ? 7447 : options.port;
    var mtu = options.mtu === undefined ? 1500 : options.mtu;

    // Connect to the network
    this._active = true;
    this._net.reset(name, group, port, mtu);

    // Run our first "process" to kick things off
    this._net.process();
};

NUClearNet.prototype.disconnect = function() {
    this._active = false;
    this._net.shutdown();
};

NUClearNet.prototype.send = function (options) {
    if (!this._active) {
        throw new Error("The network is not currently connected");
    }
    else {
        this._net.send(options.type, options.payload, options.target, options.reliable);
    }
};

exports.NUClearNet = NUClearNet;
