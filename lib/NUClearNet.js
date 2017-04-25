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

var NUClearNet = function(name, group='239.226.152.162', port=7447, mtu=1500) {
    // Create a new network object
    this.net = new NetworkBinding();
    this._callbackMap = {};
    this._name = name;

    // We have started listening to a new type
    this.on('newListener', function (event) {
        if(event !== 'nuclear_join'
         && event !== 'nuclear_leave'
         && event !== 'newListener'
         && event !== 'removeListener'
         && this.listenerCount(event) === 0) {
            var hash = this.net.hash(event);
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
            var hash = this.net.hash(event);
            delete this._callbackMap[hash];
        }
    }.bind(this));

    // Bind our callback functions
    this.net.on('packet', function(name, address, port, hash, payload) {

        var eventName = this._callbackMap[hash];

        if (eventName !== undefined) {
            this.emit(eventName, {
                'name': name,
                'address': address,
                'port': port,

            }, payload);
        }
    }.bind(this));

    this.net.on('join', function(name, address, port) {
        this.emit('nuclear_join', {
            'name': name,
            'address': address,
            'port': port
        });
    });

    this.net.on('leave', function(name, address, port) {
        this.emit('nuclear_leave', {
            'name': name,
            'address': address,
            'port': port
        });
    });

    this.net.on('wait', function(duration) {
        setTimeout(function() {
            this.net.process();
        }.bind(this), duration);
    }.bind(this));

    // Connect to the network
    this.reset(name, group, port, mtu);

    // Run our first "process" to kick things off
    this.net.process();
};

// Inherit from event emitter
util.inherits(NUClearNet, EventEmitter);

NUClearNet.prototype.reset = function (name, group='239.226.152.162', port=7447, mtu=1500) {
    this.net.reset(name, group, port, mtu)
};

NUClearNet.prototype.send = function (typeName, data, target, reliable) {
    this.net.send(typeName, data, target, reliable);
};

module.exports = NUClearNet;
