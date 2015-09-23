'use strict';

var murmurhash3 = require('murmurhash-native').murmurHash128x64;

module.exports = {
    type: {
        ANNOUNCE: 1,
        DATA: 2
    },
    MAX_UDP_PAYLOAD_LENGTH: 1500 /*MTU*/ - 20 /*IP header*/ - 8 /*UDP header*/ - 32 /*Data packet header*/,
    _packetIDSource: 0,
    unpack: function(buff) {

        var out = {};

        buff.readUInt8(0);
        buff.readUInt8(1);
        buff.readUInt8(2);

        // Read the data
        out.version = buff.readUInt8(3);
        out.type    = buff.readUInt8(4);
        out.length  = buff.readUInt32LE(5);

        if (out.type === this.type.ANNOUNCE) {
            out.tcpPort = buff.readUInt16LE(9);
            out.udpPort = buff.readUInt16LE(11);
            out.name = buff.toString('utf8', 13, buff.length - 1);
        }
        else if (out.type === this.type.DATA) {
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
        message.writeUInt8(this.type.ANNOUNCE, 4);

        // Write the length
        message.writeUInt32LE(name.length + 5, 5);

        // Write the tcpPort
        message.writeUInt16LE(tcpPort, 9);
        // Write the udpPort
        message.writeUInt16LE(udpPort, 11);
        // Write the name
        message.write(name, 13);

        // Write the trailing null
        message.writeUInt8(0x00, message.length - 1);

        return message;
    },
    packData: function(typeName, data, reliable, multicast) {
        // Get an ID for this message
        var messageId = this._packetIDSource = (this._packetIDSource + 1) % (Math.pow(2, 16) - 1);
        var messages = [];

        // Calculate the hash for this type
        var hash = murmurhash3(typeName, 'ascii', 0x4e55436c);

        // Work out the size we are splitting packets into
        var split = reliable ? (Math.pow(2, 32) - 1) : this.MAX_UDP_PAYLOAD_LENGTH;

        // Make our packets
        for (var i = 0; i < data.length; i += split) {
            // Work out how much data to slice off
            var payloadLength = (i + split) < data.length ? split : data.length % split;

            // Build our data header
            var header = new Buffer(16);

            // Write the header
            header.writeUInt8(0xE2, 0);
            header.writeUInt8(0x98, 1);
            header.writeUInt8(0xA2, 2);

            // Write the version
            header.writeUInt8(0x01, 3);

            // Write the type
            header.writeUInt8(this.type.DATA, 4);

            // Write the length
            header.writeUInt32LE(23 + data.length, 5);

            // Write the data packet sequence info
            header.writeUInt16LE(messageId, 9);
            header.writeUInt16LE(Math.floor(i / split), 11);
            header.writeUInt16LE(Math.floor(data.length / split) + 1, 13);
            header.writeUInt8(multicast, 15);

            messages.push(Buffer.concat([header, hash, data.slice(i, i + payloadLength)]));
        }

        return messages;
    }
};
