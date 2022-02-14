# NUClearNet.js

Node.js module for interacting with the [NUClear](https://github.com/Fastcode/NUClear) network.

## Installation

The package contains a native module, so you'll need a working C++ compiler on your system to install and build it.

```
npm install nuclearnet.js --save
```

## Usage

The following example show a typical usage pattern of creating a network instance, listening for join, leave, and packet events, and sending data.

```js
const { NUClearNet } = require('nuclearnet.js');

// Create a network instance
const net = new NUClearNet();

// Set a join listener to be notified when peers join the network.
// This should be done before calling `connect()` for the first time, to get join events
// from peers that are already on the network.
net.on('nuclear_join', function (peer) {
  console.log(`peer ${peer.name} has joined`);
});

// Set a leave listener to be notified when peers leave the network
net.on('nuclear_leave', function (peer) {
  console.log(`peer ${peer.name} has left`);
});

// Listen for all incoming packets
net.on('nuclear_packet', function (packet) {
  const packetType = packet.type !== undefined ? packet.type : 'unknown type';
  console.log(`got a packet (${packetType}) of length ${packet.payload.length} from peer ${packet.peer.name}`);
});

// Listen for packets of a specific type
net.on('packet_type_a', function (packet) {
  console.log(`got a packet (packet_type_a) of length ${packet.payload.length} from peer ${packet.peer.name}`);

  // Send a response to the peer
  net.send({
    target: packet.peer.name,
    reliable: true,
    type: 'packet_type_b',
    payload: Buffer.from('hello there!'),
  });
});

// Connect to the network using the peer name "My Name"
net.connect({ name: 'My Name' });
```

## API

See [`index.d.ts`](./index.d.ts) for types and API details.
