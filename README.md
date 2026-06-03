# NUClearNet.js

[![Node.js CI](https://github.com/Fastcode/NUClearNet.js/actions/workflows/node.js.yml/badge.svg?branch=main)](https://github.com/Fastcode/NUClearNet.js/actions/workflows/node.js.yml)

Node.js module for interacting with the [NUClear](https://github.com/Fastcode/NUClear) network.

## NUClearNet 2 (v2.0.0+)

Version 2 uses the redesigned **NUClearNet** library from [NUClear PR #190](https://github.com/Fastcode/NUClear/pull/190) (wire protocol **0x03**). It is **not** compatible with 1.x clients or NUClear builds that still use the old `NUClearNetwork` stack (protocol 0x02). Upgrade Node clients and NUClear robots together.

The vendored NUClear tree is updated via `git subtree` from the `houliston/nuclearnet-v2` branch (currently [NUClear@ce389fbf](https://github.com/Fastcode/NUClear/commit/ce389fbfea56ce97527474865dc343e0c087bd70)).

Peer join events may arrive slightly later than in 1.x because connection requires both multicast announce and a unicast CONNECT handshake.

## Installation

The package contains a native module, so you'll need a working C++ compiler on your system to install and build it.

```
npm install nuclearnet.js --save
```

## Usage

The following example shows a typical usage pattern of creating a network instance, listening for join, leave, and packet events, and sending data.

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

## Debugging

Logging is off by default. Enable tiered logs with `connect({ debug: ... })`, the constructor default, or the `NUCLEARNET_DEBUG` environment variable (`connect` wins when both are set).

| Level | JavaScript | Native (stderr) |
| ----- | ---------- | ----------------- |
| `info` | connect, join, leave, subscriptions | reset, shutdown, peer timeouts |
| `debug` | send, packets, listener subscribe/unsubscribe | handshake, announce/connect, send routing |
| `trace` | process wait scheduling | `process()` ticks, socket reads |

```js
const net = new NUClearNet({ debug: 'info' });
net.connect({ name: 'node-1', debug: 'debug' }); // overrides constructor for this session
```

```bash
NUCLEARNET_DEBUG=info node your-app.js
```

## API

See [`index.d.ts`](./index.d.ts) for types and API details.
