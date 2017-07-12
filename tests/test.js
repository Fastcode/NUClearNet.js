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

var NUClearNet = require('../index.js').NUClearNet;

var SegfaultHandler = require('segfault-handler');
SegfaultHandler.registerHandler("crash.log");

var name = 'nuclearnet_js_test'

if (process.argv.length > 2) {
    name = process.argv[2]
}
var nu = new NUClearNet();

nu.on('nuclear_join', function (peer) {
    console.log('Join', peer);

    console.log('Send', peer.name, 'Unreliable');
    nu.send({
        type: stringName,
        payload: new Buffer('Hello World from Javascript! (targeted UNRELIABLE)'),
        target: peer.name,
    });

    console.log('Send', peer.name, 'Reliable');
    nu.send({
        type: stringName,
        payload: new Buffer('Hello World from Javascript! (targeted RELIABLE)'),
        target: peer.name,
        reliable: true,
    });
});

nu.on('nuclear_leave', function (peer) {
    console.log('Leave', peer);
});

var stringName = 'std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >';

nu.on(stringName, function(packet) {
    console.log(packet.reliable ? 'Reliable' : 'Unreliable', 'message from', packet.peer.name);
    var string = packet.payload.toString();

    if(string.length < 100) {
        console.log(string);
    }
    else {
        console.log(string[0]);
    }
});

nu.connect({
    name: name,
});

process.on('SIGINT', () => {
    console.log('Disconnecting');
    nu.disconnect();
});

console.log('Sending All Unreliable');
nu.send({
    type: stringName,
    payload: new Buffer('Hello World from Javascript! (ALL UNRELIABLE)'),
});

console.log('Sending All Reliable');
nu.send({
    type: stringName,
    payload: new Buffer('Hello World from Javascript! (ALL RELIABLE)'),
    reliable: true,
});
