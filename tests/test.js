var NUClearNet = require('../lib/NUClearNet.js');

var nu = new NUClearNet('nettest0', '238.158.129.230', 7447);

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
    console.log('All Unreliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (ALL UNRELIABLE)'));
    console.log('All Reliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (ALL RELIABLE)'), undefined, true);
    console.log('nettest1 Unreliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (nettest1 UNRELIABLE)'), 'nettest1');
    console.log('nettest1 Reliable');
    nu.send(stringName, new Buffer('Hello World from Javascript! (nettest1 RELIABLE)'), 'nettest1', true);
    console.log('nettest1 Big');
    nu.send(stringName, new Buffer(65535), 'nettest1');
}, 2000)