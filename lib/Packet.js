'use strict';

var murmurhash3 = require('murmurhash-native').murmurHash128x64;

module.exports = {
    ANNOUNCE: 1,
    DATA: 2,
    unpack: function(buff) {

        var out = {};

        buff.readUInt8(0);
        buff.readUInt8(1);
        buff.readUInt8(2);

        // Read the data
        out.version = buff.readUInt8(3);
        out.type    = buff.readUInt8(4);
        out.length  = buff.readUInt32LE(5);

        if (out.type === module.exports.ANNOUNCE) {
            out.tcpPort = buff.readUInt16LE(9);
            out.udpPort = buff.readUInt16LE(11);
            out.name = buff.toString('utf8', 13, buff.length - 1);
        }
        else if (out.type === module.exports.DATA) {
            out.packetId    = buff.readUInt16LE(9);
            out.packetNo    = buff.readUInt16LE(11);
            out.packetCount = buff.readUInt16LE(13);
            out.multicast   = buff.readUInt8(15) === 0;
            out.hash        = buff.toString('hex', 16, 32);
            out.data        = buff.slice(32);
        }

        return out;
    },
    packAnnounce: function(name, tcpPort, udpPort) {

        var message = new Buffer(14 + name.length);

        // Write the header
        message.writeUInt8(0xE2, 0);
        message.writeUInt8(0x98, 1);
        message.writeUInt8(0xA2, 2);

        // Write the version
        message.writeUInt8(0x01, 3);

        // Write the type
        message.writeUInt8(0x01, 4);

        // Write the length
        message.writeUInt32LE(name.length + 5, 5);

        // Write the tcpPort
        message.writeUInt16LE(tcpPort, 9);
        // Write the udpPort
        message.writeUInt16LE(udpPort, 11);
        // Write the name
        message.write(this.name, 13);

        // Write the trailing null
        message.writeUInt8(0x00, message.length - 1);

        return message;
    }
};
