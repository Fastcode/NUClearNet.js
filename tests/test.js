const { test } = require('uvu');
const assert = require('uvu/assert');

const { NUClearNet } = require('..');

// GitHub Actions macOS runners do not support IPv4 multicast loopback used by these tests.
// Loopback unicast with SO_REUSEPORT load-balances deliveries on macOS, so skip there too.
const multicastTestsSupported = !(process.platform === 'darwin' && process.env.CI);
const loopbackTestsSupported = process.platform !== 'darwin';

const networkModes = [
  { label: 'multicast', address: '239.226.152.162', supported: multicastTestsSupported },
  { label: 'loopback', address: '127.0.0.1', supported: loopbackTestsSupported },
];

function netTestForModes(name, fn, options) {
  for (const mode of networkModes) {
    const runner = mode.supported ? test : test.skip;
    runner(`${name} [${mode.label}]`, () => fn(mode, options));
  }
}

function randomId() {
  return String(Math.random() * 100000000).slice(0, 7);
}

function createPeers(count, setupState = (peer) => peer) {
  const peers = [];

  for (let i = 0; i < count; i++) {
    peers.push({
      name: `net-${i}-${randomId()}`,
      net: new NUClearNet(),
    });
  }

  return peers.map((peer) => {
    return setupState(peer, peers);
  });
}

function connectPeer(peer, mode, extra = {}) {
  peer.net.connect({
    name: peer.name,
    address: mode.address,
    ...extra,
  });
}

function asyncTest(testFn, { timeout = 3000 } = {}) {
  return new Promise((resolve, reject) => {
    let cleanUp;
    let timeoutId;

    if (timeout > 0) {
      timeoutId = setTimeout(() => {
        timeoutId = null;
        cleanUp && cleanUp();
        reject(new Error(`async test timed out after ${timeout}ms`));
      }, timeout);
    }

    function done() {
      if (timeoutId) {
        clearTimeout(timeoutId);
        timeoutId = null;
      }

      resolve();
    }

    function fail(reason) {
      if (timeoutId) {
        clearTimeout(timeoutId);
        timeoutId = null;
      }

      reject(new Error(reason));
    }

    cleanUp = testFn(done, fail);
  });
}

test('NUClearNet instance throws if used after destroy()', () => {
  const net = new NUClearNet();
  net.destroy();

  assert.throws(
    () => {
      net.on('some-event', () => {});
    },
    /This network instance has been destroyed/,
    'NUClearNet.on() throws if called after instance is destroyed',
  );

  assert.throws(
    () => {
      net.addListener('some-event', () => {});
    },
    /This network instance has been destroyed/,
    'NUClearNet.addListener() throws if called after instance is destroyed',
  );

  assert.throws(
    () => {
      net.hash('nuclearnet');
    },
    /This network instance has been destroyed/,
    'NUClearNet.hash() throws if called after instance is destroyed',
  );

  assert.throws(
    () => {
      net.connect({});
    },
    /This network instance has been destroyed/,
    'NUClearNet.connect() throws if called after instance is destroyed',
  );

  assert.throws(
    () => {
      net.disconnect();
    },
    /This network instance has been destroyed/,
    'NUClearNet.disconnect() throws if called after instance is destroyed',
  );

  assert.throws(
    () => {
      net.send({});
    },
    /This network instance has been destroyed/,
    'NUClearNet.send() throws if called after instance is destroyed',
  );
});

test('NUClearNet.hash()', () => {
  const net = new NUClearNet();

  const expected = Buffer.from('71226911ef5289e4', 'hex');
  const hash = net.hash('nuclearnet');

  assert.equal(hash, expected, 'Hash of "nuclearnet" matches known hash');

  net.destroy();
});

test('NUClearNet.send() throws if used before connect()', () => {
  const net = new NUClearNet();

  assert.throws(() => {
    net.send({});
  }, /The network is not currently connected/);

  net.destroy();
});

netTestForModes('NUClearNet emits join events', async (mode) => {
  await asyncTest(
    (done) => {
      const peers = createPeers(5, (self, peers) => {
        return {
          ...self,
          joinedBy: Object.fromEntries(peers.map((peer) => [peer.name, peer.name === self.name])),
        };
      });

      function cleanUp() {
        peers.forEach((peer) => peer.net.destroy());
      }

      function checkComplete() {
        const isComplete = peers.every((peer) => {
          return Object.values(peer.joinedBy).every((otherPeerJoined) => otherPeerJoined);
        });

        if (isComplete) {
          cleanUp();
          done();
        }
      }

      for (const peer of peers) {
        peer.net.on('nuclear_join', (otherPeer) => {
          peer.joinedBy[otherPeer.name] = true;
          checkComplete();
        });
      }

      peers.forEach((peer) => connectPeer(peer, mode));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet emits leave events', async (mode) => {
  await asyncTest(
    (done) => {
      const [peerA, peerB] = createPeers(2);

      function cleanUp() {
        [peerA, peerB].forEach((peer) => peer.net.destroy());
      }

      peerA.net.on('nuclear_leave', (peer) => {
        if (peer.name === peerB.name) {
          cleanUp();
          done();
        }
      });

      peerA.net.on('nuclear_join', (peer) => {
        if (peer.name === peerB.name) {
          peerB.net.disconnect();
        }
      });

      [peerA, peerB].forEach((peer) => connectPeer(peer, mode));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet can send and receive reliable targeted messages', async (mode) => {
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self) => {
        return {
          ...self,
          gotExpectedMessage: false,
        };
      });

      function cleanUp() {
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      function checkComplete() {
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      const receiverNames = receivers.map((peer) => peer.name);

      sender.net.on('nuclear_join', (peer) => {
        if (receiverNames.includes(peer.name)) {
          sender.net.send({
            target: peer.name,
            reliable: true,
            type: 'message-from-a',
            payload: Buffer.from('oh hai ' + peer.name),
          });
        }
      });

      for (const receiver of receivers) {
        const expectedPayload = Buffer.from('oh hai ' + receiver.name);

        receiver.net.on('message-from-a', (packet) => {
          if (packet.peer.name !== sender.name) {
            return;
          }

          if (packet.payload.compare(expectedPayload) === 0) {
            receiver.gotExpectedMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              `receiver ${receiver.name} got unexpected data from sender` +
                '\n  expected: ' +
                expectedPayload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8'),
            );
          }
        });
      }

      [sender, ...receivers].forEach((peer) => connectPeer(peer, mode));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet can send and receive unreliable targeted messages', async (mode) => {
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self) => {
        return {
          ...self,
          gotExpectedMessage: false,
        };
      });

      const sendIntervals = {};

      function cleanUp() {
        Object.values(sendIntervals).forEach((interval) => clearInterval(interval));
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      function checkComplete() {
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      const receiverNames = receivers.map((peer) => peer.name);

      sender.net.on('nuclear_join', (peer) => {
        if (receiverNames.includes(peer.name)) {
          const sendInterval = sendIntervals[peer.name];
          sendInterval && clearInterval(sendInterval);

          sendIntervals[peer.name] = setInterval(() => {
            sender.net.send({
              target: peer.name,
              reliable: false,
              type: 'message-from-a',
              payload: Buffer.from('oh hai ' + peer.name),
            });
          }, 50);
        }
      });

      for (const receiver of receivers) {
        const expectedPayload = Buffer.from('oh hai ' + receiver.name);

        receiver.net.on('message-from-a', (packet) => {
          if (packet.peer.name !== sender.name) {
            return;
          }

          if (packet.payload.compare(expectedPayload) === 0) {
            receiver.gotExpectedMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              `receiver ${receiver.name} got unexpected data from sender` +
                '\n  expected: ' +
                expectedPayload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8'),
            );
          }
        });
      }

      [sender, ...receivers].forEach((peer) => connectPeer(peer, mode));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet can send and receive reliable untargeted messages', async (mode) => {
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self, peers) => {
        return {
          ...self,
          joinedBy: Object.fromEntries(peers.map((peer) => [peer.name, peer.name === self.name])),
          gotExpectedMessage: false,
        };
      });

      function cleanUp() {
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      const payload = Buffer.from('oh hai guys!');

      function checkConnected() {
        if (receivers.every((r) => sender.joinedBy[r.name])) {
          sender.net.send({
            reliable: true,
            type: 'message-from-a',
            payload,
          });
        }
      }

      function checkComplete() {
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      sender.net.on('nuclear_join', (peer) => {
        const receiver = receivers.find((receiver) => receiver.name === peer.name);
        if (receiver) {
          sender.joinedBy[receiver.name] = true;
          checkConnected();
        }
      });

      for (const receiver of receivers) {
        receiver.net.on('message-from-a', (packet) => {
          if (packet.peer.name !== sender.name) {
            return;
          }

          if (packet.payload.compare(payload) === 0) {
            receiver.gotExpectedMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              `receiver ${receiver.name} got unexpected data from sender` +
                '\n  expected: ' +
                payload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8'),
            );
          }
        });
      }

      [sender, ...receivers].forEach((peer) => connectPeer(peer, mode));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet can send and receive unreliable untargeted messages', async (mode) => {
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self, peers) => {
        return {
          ...self,
          joinedBy: Object.fromEntries(peers.map((peer) => [peer.name, peer.name === self.name])),
          gotExpectedMessage: false,
        };
      });

      let sendInterval;

      function cleanUp() {
        sendInterval && clearInterval(sendInterval);
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      const payload = Buffer.from('oh hai guys!');

      function checkConnected() {
        if (receivers.every((r) => sender.joinedBy[r.name])) {
          sendInterval && clearInterval(sendInterval);
          sendInterval = setInterval(() => {
            sender.net.send({
              reliable: false,
              type: 'message-from-a',
              payload,
            });
          }, 50);
        }
      }

      function checkComplete() {
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      sender.net.on('nuclear_join', (peer) => {
        const receiver = receivers.find((receiver) => receiver.name === peer.name);
        if (receiver) {
          sender.joinedBy[receiver.name] = true;
          checkConnected();
        }
      });

      for (const receiver of receivers) {
        receiver.net.on('message-from-a', (packet) => {
          if (packet.peer.name !== sender.name) {
            return;
          }

          if (packet.payload.compare(payload) === 0) {
            receiver.gotExpectedMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              `receiver ${receiver.name} got unexpected data from sender` +
                '\n  expected: ' +
                payload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8'),
            );
          }
        });
      }

      [sender, ...receivers].forEach((peer) => connectPeer(peer, mode));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet only receives subscribed message types', async (mode) => {
  await asyncTest(
    (done, fail) => {
      const [peerA, peerB] = createPeers(2);

      const payloadA = Buffer.from('payload-for-a');
      const payloadB = Buffer.from('payload-for-b');

      function cleanUp() {
        [peerA, peerB].forEach((peer) => peer.net.destroy());
      }

      let aReady = false;
      let bReady = false;

      function trySend() {
        if (!aReady || !bReady) {
          return;
        }

        peerA.net.send({
          type: 'type-for-b-only',
          payload: payloadB,
          reliable: false,
        });

        peerB.net.send({
          type: 'type-for-a-only',
          payload: payloadA,
          reliable: false,
        });
      }

      peerA.net.on('nuclear_join', (peer) => {
        if (peer.name === peerB.name) {
          aReady = true;
          trySend();
        }
      });

      peerB.net.on('nuclear_join', (peer) => {
        if (peer.name === peerA.name) {
          bReady = true;
          trySend();
        }
      });

      peerA.net.on('type-for-a-only', (packet) => {
        if (packet.peer.name !== peerB.name) {
          return;
        }
        if (packet.payload.compare(payloadA) !== 0) {
          cleanUp();
          fail('peer A got unexpected payload on type-for-a-only');
          return;
        }
        peerA.gotExpected = true;
        if (peerA.gotExpected && peerB.gotExpected) {
          cleanUp();
          done();
        }
      });

      peerB.net.on('type-for-b-only', (packet) => {
        if (packet.peer.name !== peerA.name) {
          return;
        }
        if (packet.payload.compare(payloadB) !== 0) {
          cleanUp();
          fail('peer B got unexpected payload on type-for-b-only');
          return;
        }
        peerB.gotExpected = true;
        if (peerA.gotExpected && peerB.gotExpected) {
          cleanUp();
          done();
        }
      });

      peerA.gotExpected = false;
      peerB.gotExpected = false;

      connectPeer(peerA, mode);
      connectPeer(peerB, mode);

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

netTestForModes('NUClearNet reconnect restores join and messaging', async (mode) => {
  await asyncTest(
    (done, fail) => {
      const [peerA, peerB] = createPeers(2);
      const payload = Buffer.from('reconnect-payload');
      let phase = 'initial';

      function cleanUp() {
        [peerA, peerB].forEach((peer) => peer.net.destroy());
      }

      peerB.net.on('reconnect-test', (packet) => {
        if (packet.peer.name !== peerA.name) {
          return;
        }
        if (packet.payload.compare(payload) === 0) {
          cleanUp();
          done();
        } else {
          cleanUp();
          fail('peer B got unexpected payload after reconnect');
        }
      });

      peerA.net.on('nuclear_join', (peer) => {
        if (peer.name !== peerB.name) {
          return;
        }

        if (phase === 'initial') {
          phase = 'disconnected';
          peerA.net.disconnect();
          peerB.net.disconnect();

          setTimeout(() => {
            phase = 'reconnected';
            connectPeer(peerA, mode);
            connectPeer(peerB, mode);
          }, 200);
        } else if (phase === 'reconnected') {
          peerA.net.send({
            target: peerB.name,
            reliable: true,
            type: 'reconnect-test',
            payload,
          });
        }
      });

      connectPeer(peerA, mode);
      connectPeer(peerB, mode);

      return cleanUp;
    },
    { timeout: 8000 },
  );
});

test.run();
