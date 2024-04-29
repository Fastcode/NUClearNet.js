const { test } = require('uvu');
const assert = require('uvu/assert');

const { NUClearNet } = require('..');

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

function asyncTest(testFn, { timeout = 1000 } = {}) {
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

test('NUClearNet emits join events', async () => {
  // Test set up:
  //   - Create N network instances and connect all of them
  //   - Each time one peer joins another, check that they've all joined each other
  //   - If so, end successfully
  //   - Otherwise end with failure when the timeout expires
  await asyncTest(
    (done) => {
      const peers = createPeers(5, (self, peers) => {
        return {
          ...self, // name and net instance
          joinedBy: Object.fromEntries(peers.map((peer) => [peer.name, peer.name === self.name])), // { [peerName]: peerJoined }
        };
      });

      function cleanUp() {
        peers.forEach((peer) => peer.net.destroy());
      }

      function checkComplete() {
        const isComplete = peers.every((peer) => {
          // Check that every other peer joined this peer
          return Object.values(peer.joinedBy).every((otherPeerJoined) => otherPeerJoined);
        });

        if (isComplete) {
          cleanUp();
          done();
        }
      }

      // Set up the join event listeners
      for (const peer of peers) {
        peer.net.on('nuclear_join', (otherPeer) => {
          peer.joinedBy[otherPeer.name] = true;
          checkComplete();
        });
      }

      // Connect the peers
      peers.forEach((peer) => peer.net.connect({ name: peer.name }));

      return cleanUp;
    },
    { timeout: 1000 },
  );
});

test('NUClearNet emits leave events', async () => {
  // Test set up:
  //   - Create two network instances (A and B) and connect them
  //   - Wait for B to join A, then disconnect B to trigger the `nuclear_leave` event on A
  //   - End successfully if B triggered the leave event after joining and disconnecting from A
  //   - Automatically end with failure if the above didn't happen before the timeout
  await asyncTest(
    (done) => {
      console.log('NUClearNet emits leave events')
      const [peerA, peerB] = createPeers(2);
      console.log('NUClearNet emits leave events', peerA, peerB)
      function cleanUp() {
        console.log('Executing cleanUp');
        [peerA, peerB].forEach((peer) => {
          console.log(`Destroying network for ${peer.name}`);
          peer.net.destroy();
        });
      }

      console.log('Before peerA.net.on(nuclear_leave)');
      peerA.net.on('nuclear_leave', (peer) => {
        console.log('nuclear_leave event triggered', peer.name);
        // End the test when B disconnects from A
        if (peer.name === peerB.net.options.name) {
          cleanUp();
          done();
        }
      });

      console.log('Before peerA.net.on(nuclear_join)');
      peerA.net.on('nuclear_join', (peer) => {
        console.log('nuclear_join event triggered', peer.name);
        // Disconnect B after it joins, to trigger the leave event on A
        if (peer.name === peerB.net.options.name) {
          peerB.net.disconnect();
        }
      });

      // Connect the peers
      console.log('Connecting peers');
      [peerA, peerB].forEach((peer) => {
        console.log(`Connecting peer ${peer.name}`);
        peer.net.connect({ name: peer.name });
      });

      return cleanUp;
    },
    { timeout: 2000 },
  );
});

test('NUClearNet can send and receive reliable targeted messages', async () => {
  // Test set up:
  //   - Create one sender and N-1 receiver network instances and connect them
  //   - Wait for receivers to join the sender, and send each receiver a unique payload
  //   - End successfully when all receivers get their respective payloads from the sender
  //   - End with failure if a receiver gets a payload that is not their own (this verifies targeting works)
  //   - Automatically end with failure if all of the above doesn't happen before the timeout
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self) => {
        return {
          ...self, // name and net instance
          gotExpectedMessage: false,
        };
      });

      function cleanUp() {
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      function checkComplete() {
        // Complete the test if every receiver got their expected message
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      const receiverNames = receivers.map((peer) => peer.name);

      // Set up the sender to send to receivers when they join
      sender.net.on('nuclear_join', (peer) => {
        // Send message to the peer if it's one of our receivers
        if (receiverNames.includes(peer.name)) {
          sender.net.send({
            target: peer.name,
            reliable: true,
            type: 'message-from-a',
            payload: Buffer.from('oh hai ' + peer.name),
          });
        }
      });

      // Set up a listener on each of the receivers
      for (const receiver of receivers) {
        const expectedPayload = Buffer.from('oh hai ' + receiver.name);

        receiver.net.on('message-from-a', (packet) => {
          // Ignore packets not from our sender
          if (packet.peer.name !== sender.name) {
            return;
          }

          // Ensure that the packet is what we expected to be sent to this receiver
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

      // Connect the peers
      [sender, ...receivers].forEach((peer) => peer.net.connect({ name: peer.name }));

      return cleanUp;
    },
    { timeout: 1000 },
  );
});

test('NUClearNet can send and receive unreliable targeted messages', async () => {
  // Test set up:
  //   - Create one sender and N-1 receiver network instances and connect them
  //   - Wait for each receiver to join the sender, then start an interval to unreliably send the receiver a unique payload.
  //     The interval ensures multiple messages will be sent, to compensate for any that are dropped due to the unreliable send.
  //   - End successfully when all receivers get their respective payloads from the sender
  //   - End with failure if a receiver gets a payload that is not their own (this verifies targeting works)
  //   - Automatically end with failure if all of the above doesn't happen before the timeout
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self) => {
        return {
          ...self, // name and net instance
          gotExpectedMessage: false,
        };
      });

      const sendIntervals = {};

      function cleanUp() {
        Object.values(sendIntervals).forEach((interval) => clearInterval(interval));
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      function checkComplete() {
        // Complete the test if every receiver got their expected message
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      const receiverNames = receivers.map((peer) => peer.name);

      // Set up the sender to send when receivers join
      sender.net.on('nuclear_join', (peer) => {
        // Start sending unreliable messages to the peer if it's one of our receivers
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

      // Set up a listener on each of the receivers
      for (const receiver of receivers) {
        const expectedPayload = Buffer.from('oh hai ' + receiver.name);

        receiver.net.on('message-from-a', (packet) => {
          // Ignore packets not from our sender
          if (packet.peer.name !== sender.name) {
            return;
          }

          // Ensure that the packet is what we expected to be sent to this receiver
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

      // Connect the peers
      [sender, ...receivers].forEach((peer) => peer.net.connect({ name: peer.name }));

      return cleanUp;
    },
    { timeout: 1000 },
  );
});

test('NUClearNet can send and receive reliable untargeted messages', async () => {
  // Test set up:
  //   - Create one sender and N-1 receiver network instances and connect them
  //   - Wait for both all receivers to join the sender, then send the payload with `reliable` set, untargeted
  //   - End successfully when all receivers get the payload from the sender
  //   - End with failure if any receiver gets a payload from the sender that is not the expected payload
  //   - Automatically end with failure if all of the above doesn't happen before the timeout
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self, peers) => {
        return {
          ...self, // name and net instance
          joinedBy: Object.fromEntries(peers.map((peer) => [peer.name, peer.name === self.name])), // { [peerName]: peerJoined }
          gotExpectedMessage: false,
        };
      });

      function cleanUp() {
        [sender, ...receivers].forEach((peer) => peer.net.destroy());
      }

      const payload = Buffer.from('oh hai guys!');

      function checkConnected() {
        // Send the untargeted message when every receiver has joined the sender
        if (receivers.every((r) => sender.joinedBy[r.name])) {
          sender.net.send({
            reliable: true,
            type: 'message-from-a',
            payload,
          });
        }
      }

      function checkComplete() {
        // Complete the test if every receiver got their expected message
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      // Keep track of which receivers that are joining the sender
      sender.net.on('nuclear_join', (peer) => {
        const receiver = receivers.find((receiver) => receiver.name === peer.name);
        if (receiver) {
          sender.joinedBy[receiver.name] = true;
          checkConnected();
        }
      });

      // Set up a listener on each of the receivers
      for (const receiver of receivers) {
        receiver.net.on('message-from-a', (packet) => {
          // Ignore packets not from our sender
          if (packet.peer.name !== sender.name) {
            return;
          }

          // Ensure that the packet is what we expected from the sender
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

      // Connect the peers
      [sender, ...receivers].forEach((peer) => peer.net.connect({ name: peer.name }));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

test('NUClearNet can send and receive unreliable untargeted messages', async () => {
  // Test set up:
  //   - Create one sender and N-1 receiver network instances and connect them
  //   - Wait for all receivers to join the sender, then start an interval to unreliably send the same payload, without a target.
  //     The interval ensures multiple messages will be sent, to compensate for any that are dropped due to the unreliable send.
  //   - End successfully when every receiver gets the payload from the sender
  //   - End with failure if any receiver gets a payload from the sender that was not the expected payload
  //   - Automatically end with failure if all of the above doesn't happen before the timeout
  await asyncTest(
    (done, fail) => {
      const [sender, ...receivers] = createPeers(4, (self, peers) => {
        return {
          ...self, // name and net instance
          joinedBy: Object.fromEntries(peers.map((peer) => [peer.name, peer.name === self.name])), // { [peerName]: peerJoined }
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
        // Start sending unreliable messages after every receiver is connected
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
        // Complete the test if every receiver got the expected message
        if (receivers.every((receiver) => receiver.gotExpectedMessage)) {
          cleanUp();
          done();
        }
      }

      // Keep track of which receivers that are joining the sender
      sender.net.on('nuclear_join', (peer) => {
        const receiver = receivers.find((receiver) => receiver.name === peer.name);
        if (receiver) {
          sender.joinedBy[receiver.name] = true;
          checkConnected();
        }
      });

      // Set up a listener on each of the receivers
      for (const receiver of receivers) {
        receiver.net.on('message-from-a', (packet) => {
          // Ignore packets not from our sender
          if (packet.peer.name !== sender.name) {
            return;
          }

          // Ensure that the packet is what we expected from the sender
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

      // Connect the peers
      [sender, ...receivers].forEach((peer) => peer.net.connect({ name: peer.name }));

      return cleanUp;
    },
    { timeout: 5000 },
  );
});

test.run();
