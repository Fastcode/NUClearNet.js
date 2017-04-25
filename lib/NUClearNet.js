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

    // Bind our callback functions
    this.net.on('packet', function(name, address, port, hash, payload) {
        console.log('packet', hash, name, address, port);
        // TODO lookup who is interested in this hash and send it to them
    });

    this.net.on('join', function(name, address, port) {
        console.log('join', name, address, port);
        // TODO event emit a network join
    });

    this.net.on('leave', function(name, address, port) {
        console.log('leave', name, address, port);
        // TODO event emit a network leave
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
