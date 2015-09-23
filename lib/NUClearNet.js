'use strict';

var net = require('net');
var util = require('util');
var dgram = require('dgram');
var EventEmitter = require('events').EventEmitter;
var murmurhash3 = require('murmurhash-native').murmurHash128x64;
var packet = require('./Packet')

var NUClearNet = function(name, multicastGroup, multicastPort, passive) {
    // Array of network targets
    this.targets = [];
    this.nameTargets = {};
    this.udpTargets = {};
    this.tcpTargets = {};

    this.callbackMap = {};

    this.name = name;
    this.multicastGroup = multicastGroup;
    this.multicastPort = multicastPort;
    this.passive = passive === undefined ? true : passive;

    this.tcpPort = 0;
    this.udpPort = 0;

    // Setup our tcp connection
    this.tcpSocket = net.createServer(this.tcpConnection.bind(this));
    this.tcpSocket.listen(0, '0.0.0.0');
    this.tcpSocket.on('listening', function() {
        this.tcpPort = this.tcpSocket.address().port;
    }.bind(this));

    // Setup our udp connection
    this.udpSocket = dgram.createSocket('udp4');

    this.udpSocket.on('listening', function () {
        this.udpSocket.setBroadcast(true);
        this.udpPort = this.udpSocket.address().port;
    }.bind(this));

    this.udpSocket.on('message', this.udpHandler.bind(this));

    this.udpSocket.bind();

    // Setup our multicast socket
    this.multicastSocket = dgram.createSocket({type: 'udp4', reuseAddr: true});

    this.multicastSocket.on('listening', function () {
        this.multicastSocket.setBroadcast(true);
        this.multicastSocket.setMulticastTTL(128);
        this.multicastSocket.addMembership(this.multicastGroup);
    }.bind(this));

    this.multicastSocket.on('message', this.udpHandler.bind(this));

    this.multicastSocket.bind(this.multicastPort);

    // Setup our announce
    setInterval(this.announce.bind(this), 1000);

    // Setup our request hashing converter
    this.on('newListener', function (event) {
        // Get our hash
        var hash = murmurhash3(event, 'ascii', 0x4e55436c).toString('hex');

        this.callbackMap[hash] = event;
    }, this);

    this.on('removeListener', function (event) {

        // If we are no longer listening to this type
        if(this.listenerCount(event) === 0) {
            // Get our hash
            var hash = murmurhash3(event, 'ascii', 0x4e55436c).toString('hex');

            delete this.callbackMap[hash];
        }
    });
};

// Inherit from event emitter
util.inherits(NUClearNet, EventEmitter);

NUClearNet.prototype.announce = function() {
    // Construct an announce packet
    var message = packet.packAnnounce(this.name, this.tcpPort, this.udpPort);

    // Send it from our udp socket to the multicast address
    this.udpSocket.send(message, 0, message.length, this.multicastPort, this.multicastGroup);
};

NUClearNet.prototype.tcpHandler = function(addr, data) {

    // Calculate our key
    var key = addr.address + ":" + addr.port;

    // Get our target
    var target = this.tcpTargets[key];

    // Append our new data
    target.tcpBuffer = Buffer.concat([target.tcpBuffer, data]);

    // Check if we have enough data
    var packetSize = target.tcpBuffer.readUInt32LE(5) + 9;
    if (packetSize <= target.tcpBuffer.length) {

        // Slice out our packet
        var slice = target.tcpBuffer.slice(0, packetSize);
        target.tcpBuffer = target.tcpBuffer.slice(packetSize);

        var obj = packet.unpack(slice);

        if (obj.type === packet.ANNOUNCE) {
            var udpKey = addr.address + ":" + obj.udpPort;
            // Look for this in our existing connections
            if (this.udpTargets[udpKey] === undefined) {
                // This is a new connection record the details
                target.udpPort = obj.udpPort;
                target.tcpPort = obj.tcpPort;
                target.name    = obj.name;
                target.address = addr.address;

                // Link it up properly
                this.udpTargets[udpKey] = target;

                // Lazy instansiate our list
                if (this.nameTargets[target.name] === undefined) {
                    this.nameTargets[target.name] = [];
                }
                this.nameTargets[target.name].push(target);

                this.emit('nuclear_join', target.name, target.address, target.tcpPort, target.udpPort);
            }
            else {
                // We already have one
                // we need to close or something
                // for now this will never happen in passive mode
            }
        }
        else if (obj.type === packet.DATA) {

            // Get our event name
            var eventName = this.callbackMap[obj.hash];

            // Emit if we need to
            if(eventName !== undefined) {
                this.emit(eventName, {
                    name: target.name,
                    address: target.address,
                    tcpPort: target.tcpPort,
                    udpPort: target.udpPort
                }, obj.data);
            }
        }
    }
};

NUClearNet.prototype.tcpClose = function(addr) {

    // Get our key
    var key = addr.address + ":" + addr.port;

    // Cleanup
    var target = this.tcpTargets[key];

    var udpKey = target.address + ":" + target.udpPort;
    delete this.udpTargets[udpKey];
    delete this.tcpTargets[key];

    // Remove our name target
    for (var i = 0; i < this.nameTargets[target.name].length; i++) {
        var t = this.nameTargets[target.name][i];

        if (t === target) {
            // Remove this element from the list
            this.nameTargets[target.name].splice(i, 1);
        }
    }

    for (var i = 0; i < this.targets.length; i++) {
        if (target === this.targets[i]) {
            // Remove this element
            this.targets.splice(i, 1);
        }
    }

    this.emit('nuclear_leave', target.name, target.address, target.tcpPort, target.udpPort);
};

NUClearNet.prototype.udpHandler = function (msg, remote) {

    // Get our packet type
    var obj = packet.unpack(msg);

    if (obj.type === packet.ANNOUNCE) {
        // Check it's not announcing us
        if(!(obj.name === this.name && obj.tcpPort === this.tcpPort && obj.udpPort === this.udpPort)) {
            // TODO connect and stuff
        }
    }
    else if (obj.type === packet.DATA) {

        // Get target
        var target = this.udpTargets[remote.address + ':' + remote.port];

        // If we know who this is
        if(target !== undefined) {

            // If this is a single packet of data we can deserialise now
            if (obj.packetNo === 0 && obj.packetCount === 1) {

                // Get our event name
                var eventName = this.callbackMap[obj.hash];

                // Emit if we need to
                if(eventName !== undefined) {
                    this.emit(eventName, {
                        name: target.name,
                        address: target.address,
                        tcpPort: target.tcpPort,
                        udpPort: target.udpPort
                    }, obj.data);
                }
            }
            // Otherwise we need to add it to our list
            else {

                var bufferKey = target.address + ':' + target.udpPort + ':' + obj.packetId;

                // Lazy initialise the buffer for this id
                if (target.udpBuffer[bufferKey] === undefined) {
                    target.udpBuffer[bufferKey] = { time: Date.now(), packets: [] };
                }

                // Put our item in the buffer
                var buffer = target.udpBuffer[bufferKey];
                buffer.time = Date.now(),
                buffer.packets.push(obj);

                // If we are finished reassemble and send
                if (buffer.packets.length === obj.packetCount) {
                    // sort packets by id
                    buffer.packets.sort(function (a, b) {
                        return a.packetNo - b.packetNo;
                    });
                    var data = [];
                    for (var i = 0; i < buffer.packets.length; i++) {
                        data.push(buffer.packets[i].data);
                    };

                    data = Buffer.concat(data);

                    // Get our event name
                    var eventName = this.callbackMap[obj.hash];

                    // Emit if we need to
                    if(eventName !== undefined) {
                        this.emit(eventName, {
                            name: target.name,
                            address: target.address,
                            tcpPort: target.tcpPort,
                            udpPort: target.udpPort
                        }, data);
                    }

                    // Delete this item
                    delete target.udpBuffer[bufferKey];
                }

                // If we have too many floating packet sets, delete the oldest one
                if (target.udpBuffer.length >= 5) {
                    console.log("TODO DELETE ME!!!");
                    // Pick the oldest one and delete it
                }
            }
        }
    }
};

NUClearNet.prototype.tcpConnection = function (socket) {

    // Calculate our key
    var addr = socket.address();
    var key = addr.address + ":" + addr.port;

    // Push this socket onto the list and add it's lookup map
    this.targets.push({ tcpSocket: socket, tcpBuffer: new Buffer(0), udpBuffer: {} });
    this.tcpTargets[key] = this.targets[this.targets.length - 1];

    socket.on('data', this.tcpHandler.bind(this, addr));
    socket.on('end', this.tcpClose.bind(this, addr));
};

module.exports = NUClearNet;
