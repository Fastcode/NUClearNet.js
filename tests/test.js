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

var nu = new NUClearNet({
    name: 'nettest0',
    group: '239.226.152.162',
    port: 7447
});

nu.on('nuclear_join', function (name) {
    console.log('Join', name);
});

nu.on('nuclear_leave', function (name) {
    console.log('Leave', name);
});

var stringName = 'std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >';

nu.on(stringName, function(source, data) {
    console.log('Message from', source.name);
    var string = data.toString();

    if(string.length < 100) {
        console.log(string);
    }
    else {
        console.log(string[0]);
    }

});

setTimeout(function() {
//     console.log('All Unreliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (ALL UNRELIABLE)'));
//     console.log('All Reliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (ALL RELIABLE)'), undefined, true);
//     console.log('nettest1 Unreliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (nettest1 UNRELIABLE)'), 'nettest1');
//     console.log('nettest1 Reliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (nettest1 RELIABLE)'), 'nettest1', true);
//     console.log('nettest1 Big');
    nu.send(stringName, new Buffer(65535), 'nettest1');
}, 2000)
