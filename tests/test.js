var NUClearNet = require('../lib/NUClearNet.js');

var nu = new NUClearNet('nettest0', '238.158.129.230', 7447);

nu.on('nuclear_join', function (name) {
    console.log('Join', name);
});

nu.on('nuclear_leave', function (name) {
    console.log('Leave', name);
});

nu.on('std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >', function(source, data) {

    var string = data.toString();

    if(string.length < 100) {
        console.log(string);
    }
    else {
        console.log(string[0]);
    }

});
