'use strict';

var net = require('net');
var util = require('util');
var dgram = require('dgram');
var EventEmitter = require('events').EventEmitter;
var murmurhash3 = require('murmurhash-native').murmurHash128x64;
var packet = require('./Packet');

var NUClearNet = function(name, multicastGroup, multicastPort, passive) {
    // Array of network targets
    this._targets = [];
    this._nameTargets = {};
    this._udpTargets = {};
    this._tcpTargets = {};

    this._callbackMap = {};

    this._name = name;
    this._multicastGroup = multicastGroup;
    this._multicastPort = multicastPort;
    this._passive = passive === undefined ? true : passive;

    this._tcpPort = 0;
    this._udpPort = 0;

    // Setup our tcp connection
    this._tcpSocket = net.createServer(this._tcpConnection.bind(this));
    this._tcpSocket.listen(0, '0.0.0.0');
    this._tcpSocket.on('listening', function() {
        this._tcpPort = this._tcpSocket.address().port;
    }.bind(this));

    // Setup our udp connection
    this._udpSocket = dgram.createSocket('udp4');

    this._udpSocket.on('listening', function () {
        this._udpSocket.setBroadcast(true);
        this._udpPort = this._udpSocket.address().port;
    }.bind(this));

    this._udpSocket.on('message', this._udpHandler.bind(this));

    this._udpSocket.bind();

    // Setup our multicast socket
    this._multicastSocket = dgram.createSocket({type: 'udp4', reuseAddr: true});

    this._multicastSocket.on('listening', function () {
        this._multicastSocket.setBroadcast(true);
        this._multicastSocket.setMulticastTTL(128);
        this._multicastSocket.addMembership(this._multicastGroup);
    }.bind(this));

    this._multicastSocket.on('message', this._udpHandler.bind(this));

    this._multicastSocket.bind(this._multicastPort);

    // Setup our announce
    setInterval(this._announce.bind(this), 1000);

    // Setup our request hashing converter
    this.on('newListener', function (event) {
        // Get our hash
        var hash = murmurhash3(event, 'ascii', 0x4e55436c).toString('hex');

        this._callbackMap[hash] = event;
    }, this);

    this.on('removeListener', function (event) {

        // If we are no longer listening to this type
        if(this._listenerCount(event) === 0) {
            // Get our hash
            var hash = murmurhash3(event, 'ascii', 0x4e55436c).toString('hex');

            delete this._callbackMap[hash];
        }
    });
};

// Inherit from event emitter
util.inherits(NUClearNet, EventEmitter);

NUClearNet.prototype._announce = function() {
    // Construct an announce packet
    var message = packet.packAnnounce(this._name, this._tcpPort, this._udpPort);

    // Send it from our udp socket to the multicast address
    this._udpSocket.send(message, 0, message.length, this._multicastPort, this._multicastGroup);
};

NUClearNet.prototype._tcpHandler = function(addr, data) {

    // Calculate our key
    var key = addr.address + ":" + addr.port;

    // Get our target
    var target = this._tcpTargets[key];

    // Append our new data
    target.tcpBuffer = Buffer.concat([target.tcpBuffer, data]);

    // Check if we have enough data
    var packetSize = target.tcpBuffer.readUInt32LE(5) + 9;
    if (packetSize <= target.tcpBuffer.length) {

        // Slice out our packet
        var slice = target.tcpBuffer.slice(0, packetSize);
        target.tcpBuffer = target.tcpBuffer.slice(packetSize);

        var obj = packet.unpack(slice);

        if (obj.type === packet.type.ANNOUNCE) {
            var udpKey = addr.address + ":" + obj.udpPort;
            // Look for this in our existing connections
            if (this._udpTargets[udpKey] === undefined) {
                // This is a new connection record the details
                target.udpPort = obj.udpPort;
                target.tcpPort = obj.tcpPort;
                target.name    = obj.name;
                target.address = addr.address;

                // Link it up properly
                this._udpTargets[udpKey] = target;

                // Lazy instansiate our list
                if (this._nameTargets[target.name] === undefined) {
                    this._nameTargets[target.name] = [];
                }
                this._nameTargets[target.name].push(target);

                this.emit('nuclear_join', target.name, target.address, target.tcpPort, target.udpPort);
            }
            else {
                // We already have one
                // we need to close or something
                // for now this will never happen in passive mode
            }
        }
        else if (obj.type === packet.type.DATA) {

            // Get our event name
            var eventName = this._callbackMap[obj.hash];

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

NUClearNet.prototype._tcpClose = function(addr) {

    // Get our key
    var key = addr.address + ":" + addr.port;

    // Cleanup
    var target = this._tcpTargets[key];

    var udpKey = target.address + ":" + target.udpPort;
    delete this._udpTargets[udpKey];
    delete this._tcpTargets[key];

    // Remove our name target
    var nameIndex = this._nameTargets[target.name].indexOf(target);
    if (nameIndex >= 0) this._nameTargets[target.name].splice(nameIndex, 1);

    // Remove our actual target
    var index = this._targets.indexOf(target);
    if (index >= 0) this._targets.splice(index, 1);

    this.emit('nuclear_leave', target.name, target.address, target.tcpPort, target.udpPort);
};

NUClearNet.prototype._udpHandler = function (msg, remote) {

    // Get our packet type
    var obj = packet.unpack(msg);

    if (obj.type === packet.type.ANNOUNCE) {
        // Check it's not announcing us
        if(!(obj.name === this._name && obj.tcpPort === this._tcpPort && obj.udpPort === this._udpPort)) {
            // TODO connect and stuff
        }
    }
    else if (obj.type === packet.type.DATA) {

        // Get target
        var target = this._udpTargets[remote.address + ':' + remote.port];

        // Get our event name
        var eventName = this._callbackMap[obj.hash];

        // If we know who this is and somebody wants this message type
        if(target !== undefined && eventName !== undefined) {

            // If this is a single packet of data we can deserialise now
            if (obj.packetNo === 0 && obj.packetCount === 1) {

                // Emit to the masses
                this.emit(eventName, {
                    name: target.name,
                    address: target.address,
                    tcpPort: target.tcpPort,
                    udpPort: target.udpPort
                }, obj.data);
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
                buffer.time = Date.now();
                buffer.packets.push(obj);

                // If we are finished reassemble and send
                if (buffer.packets.length === obj.packetCount) {

                    // Get all our data components
                    var data = [];

                    // sort packets by id
                    buffer.packets.sort(function (a, b) {
                        return a.packetNo - b.packetNo;
                    });
                    // Get all the parts
                    buffer.packets.forEach(function (p) {
                        data.push(p.data);
                    });
                    // Put them together
                    data = Buffer.concat(data);

                    // Emit to the masses
                    this.emit(eventName, {
                        name: target.name,
                        address: target.address,
                        tcpPort: target.tcpPort,
                        udpPort: target.udpPort
                    }, data);

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

NUClearNet.prototype._tcpConnection = function (socket) {

    // Calculate our key
    var addr = socket.address();
    var key = addr.address + ":" + addr.port;

    // Push this socket onto the list and add it's lookup map
    this._targets.push({ tcpSocket: socket, tcpBuffer: new Buffer(0), udpBuffer: {} });
    this._tcpTargets[key] = this._targets[this._targets.length - 1];

    socket.on('data', this._tcpHandler.bind(this, addr));
    socket.on('end', this._tcpClose.bind(this, addr));
};

NUClearNet.prototype.send = function (typeName, data, target, reliable) {

    reliable = reliable === undefined ? false : reliable;

    // Get our packets to send
    var messages = packet.packData(typeName, data, reliable, target === undefined);

    if (reliable) {
        // Reliable (TCP) send

        this._targets.forEach(function (t) {
            // Send if we are sending to all, or to this node
            if (target === undefined || t.name === target) {
                t.tcpSocket.write(messages[0]);
            }
        }, this);
    }
    else {
        // Unreliable (UDP) send

        if (target === undefined) {
            // Send via multicast
            messages.forEach(function (m) {
                this._udpSocket.send(m, 0, m.length, this._multicastPort, this._multicastGroup);
            }, this);
        }
        else {
            // Send to each via unicast
            this._targets.forEach(function (t) {
                // If we are sending to this target
                if (t.name === target) {

                    // Send all the messages
                    messages.forEach(function (m) {
                        this._udpSocket.send(m, 0, m.length, t.udpPort, t.address);
                    }, this);
                }
            }, this);
        }
    }
};

module.exports = NUClearNet;
