const { test } = require('uvu');
const assert = require('uvu/assert');

const { NUClearNet } = require('..');

function uniqueId() {
  return String(Math.random() * 100000000).slice(0, 8);
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

test('NUClearNet: throws if used after destroy()', () => {
  const net = new NUClearNet();
  net.destroy();

  assert.throws(() => {
    net.hash('nuclearnet');
  }, /This network instance has been destroyed/);

  assert.throws(() => {
    net.on('some_event', () => {});
  }, /This network instance has been destroyed/);

  assert.throws(() => {
    net.addListener('some_event', () => {});
  }, /This network instance has been destroyed/);

  assert.throws(() => {
    net.connect('some_event', () => {});
  }, /This network instance has been destroyed/);

  assert.throws(() => {
    net.disconnect('some_event', () => {});
  }, /This network instance has been destroyed/);
});

test('NUClearNet.hash()', () => {
  const net = new NUClearNet();

  const expected = Buffer.from('71226911ef5289e4', 'hex');
  const hash = net.hash('nuclearnet');

  assert.equal(hash, expected);

  net.destroy();
});

test('NUClearNet.send() throws if not connected', () => {
  const net = new NUClearNet();

  assert.throws(() => {
    net.send('nuclearnet');
  }, /The network is not currently connected/);

  net.destroy();
});

test('NUClearNet emits join events', async () => {
  await asyncTest(
    (done) => {
      const netA = new NUClearNet();
      const netB = new NUClearNet();

      function cleanUp() {
        netA.destroy();
        netB.destroy();
      }

      let aJoinedB = false;
      let bJoinedA = false;

      function checkComplete() {
        if (aJoinedB && bJoinedA) {
          cleanUp();
          done();
        }
      }

      netA.on('nuclear_join', (peer) => {
        if (peer.name === netB.options.name) {
          bJoinedA = true;
          checkComplete();
        }
      });

      netB.on('nuclear_join', (peer) => {
        if (peer.name === netA.options.name) {
          aJoinedB = true;
          checkComplete();
        }
      });

      netA.connect({
        name: 'netA_' + uniqueId(),
      });

      netB.connect({
        name: 'netB_' + uniqueId(),
      });

      return cleanUp;
    },
    { timeout: 1000 }
  );
});

test('NUClearNet emits leave events', async () => {
  await asyncTest(
    (done) => {
      const netA = new NUClearNet();
      const netB = new NUClearNet();

      function cleanUp() {
        netA.destroy();
        netB.destroy();
      }

      netA.on('nuclear_leave', (peer) => {
        if (peer.name === netB.options.name) {
          cleanUp();
          done();
        }
      });

      netA.on('nuclear_join', (peer) => {
        // Disconnect B after it joins, to trigger the leave
        if (peer.name === netB.options.name) {
          netB.disconnect();
        }
      });

      netA.connect({
        name: 'netA_' + uniqueId(),
      });

      netB.connect({
        name: 'netB_' + uniqueId(),
      });

      return cleanUp;
    },
    { timeout: 1000 }
  );
});

test('NUClearNet can send and receive reliable targeted messages', async () => {
  await asyncTest(
    (done, fail) => {
      const netA = new NUClearNet();
      const netB = new NUClearNet();
      const netC = new NUClearNet();

      function cleanUp() {
        netA.destroy();
        netB.destroy();
        netC.destroy();
      }

      const payloadToB = Buffer.from('oh hai b!');
      const payloadToC = Buffer.from('oh hai c!');

      netA.on('nuclear_join', (peer) => {
        // Send message to B after it joins
        if (peer.name === netB.options.name) {
          netA.send({
            target: netB.options.name,
            reliable: true,
            type: 'message-from-a',
            payload: payloadToB,
          });
        }

        // Send message to C after it joins
        if (peer.name === netC.options.name) {
          netA.send({
            target: netC.options.name,
            reliable: true,
            type: 'message-from-a',
            payload: payloadToC,
          });
        }
      });

      let bGotMessage = false;
      let cGotMessage = false;

      function checkComplete() {
        if (bGotMessage && cGotMessage) {
          cleanUp();
          done();
        }
      }

      netB.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payloadToB) === 0) {
            bGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netB received unexpected data' +
                '\n  expected: ' +
                payloadToB.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8')
            );
          }
        }
      });

      netC.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payloadToC) === 0) {
            cGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netC received unexpected data' +
                '\n  expected: ' +
                payloadToC.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf8')
            );
          }
        }
      });

      netA.connect({
        name: 'netA_' + uniqueId(),
      });

      netB.connect({
        name: 'netB_' + uniqueId(),
      });

      netC.connect({
        name: 'netC_' + uniqueId(),
      });

      return cleanUp;
    },
    { timeout: 1000 }
  );
});

test('NUClearNet can send and receive unreliable targeted messages', async () => {
  await asyncTest(
    (done, fail) => {
      const netA = new NUClearNet();
      const netB = new NUClearNet();
      const netC = new NUClearNet();

      let bSendInterval;
      let cSendInterval;

      function cleanUp() {
        if (bSendInterval) {
          clearInterval(bSendInterval);
          bSendInterval = undefined;
        }

        if (cSendInterval) {
          clearInterval(cSendInterval);
          cSendInterval = undefined;
        }

        netA.destroy();
        netB.destroy();
        netC.destroy();
      }

      const payloadToB = Buffer.from('oh hai b!');
      const payloadToC = Buffer.from('oh hai c!');

      netA.on('nuclear_join', (peer) => {
        // Start sending unreliable messages to B after it joins
        if (peer.name === netB.options.name) {
          bSendInterval && clearInterval(bSendInterval);
          bSendInterval = setInterval(() => {
            netA.send({
              target: netB.options.name,
              reliable: false,
              type: 'message-from-a',
              payload: payloadToB,
            });
          }, 50);
        }

        // Start sending unreliable messages to C after it joins
        if (peer.name === netC.options.name) {
          cSendInterval && clearInterval(cSendInterval);
          cSendInterval = setInterval(() => {
            netA.send({
              target: netC.options.name,
              reliable: false,
              type: 'message-from-a',
              payload: payloadToC,
            });
          }, 50);
        }
      });

      let bGotMessage = false;
      let cGotMessage = false;

      function checkComplete() {
        if (bGotMessage && cGotMessage) {
          cleanUp();
          done();
        }
      }

      netB.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payloadToB) === 0) {
            bGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netB received unexpected data' +
                '\n  expected: ' +
                payloadToB.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8')
            );
          }
        }
      });

      netC.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payloadToC) === 0) {
            cGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netC received unexpected data' +
                '\n  expected: ' +
                payloadToC.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf8')
            );
          }
        }
      });

      netA.connect({
        name: 'netA_' + uniqueId(),
      });

      netB.connect({
        name: 'netB_' + uniqueId(),
      });

      netC.connect({
        name: 'netC_' + uniqueId(),
      });

      return cleanUp;
    },
    { timeout: 1000 }
  );
});

test('NUClearNet can send and receive reliable untargeted messages', async () => {
  await asyncTest(
    (done, fail) => {
      const netA = new NUClearNet();
      const netB = new NUClearNet();
      const netC = new NUClearNet();

      function cleanUp() {
        netA.destroy();
        netB.destroy();
        netC.destroy();
      }

      const payload = Buffer.from('oh hai guys!');

      let bConnected = false;
      let cConnected = false;

      function checkConnected() {
        if (bConnected && cConnected) {
          netA.send({
            reliable: true,
            type: 'message-from-a',
            payload,
          });
        }
      }

      netA.on('nuclear_join', (peer) => {
        if (peer.name === netB.options.name) {
          bConnected = true;
          checkConnected();
        }

        if (peer.name === netC.options.name) {
          cConnected = true;
          checkConnected();
        }
      });

      let bGotMessage = false;
      let cGotMessage = false;

      function checkComplete() {
        if (bGotMessage && cGotMessage) {
          cleanUp();
          done();
        }
      }

      netB.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payload) === 0) {
            bGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netB received unexpected data' +
                '\n  expected: ' +
                payload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8')
            );
          }
        }
      });

      netC.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payload) === 0) {
            cGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netC received unexpected data' +
                '\n  expected: ' +
                payload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf8')
            );
          }
        }
      });

      netA.connect({
        name: 'netA_' + uniqueId(),
      });

      netB.connect({
        name: 'netB_' + uniqueId(),
      });

      netC.connect({
        name: 'netC_' + uniqueId(),
      });

      return cleanUp;
    },
    { timeout: 5000 }
  );
});

test('NUClearNet can send and receive unreliable untargeted messages', async () => {
  await asyncTest(
    (done, fail) => {
      const netA = new NUClearNet();
      const netB = new NUClearNet();
      const netC = new NUClearNet();

      let sendInterval;

      function cleanUp() {
        if (sendInterval) {
          clearInterval(sendInterval);
          sendInterval = undefined;
        }

        netA.destroy();
        netB.destroy();
        netC.destroy();
      }

      const payload = Buffer.from('oh hai guys!');

      let bConnected = false;
      let cConnected = false;

      function checkConnected() {
        // Start sending unreliable messages after both peers (B and C) are connected
        if (bConnected && cConnected) {
          sendInterval && clearInterval(sendInterval);
          sendInterval = setInterval(() => {
            netA.send({
              reliable: false,
              type: 'message-from-a',
              payload,
            });
          }, 50);
        }
      }

      netA.on('nuclear_join', (peer) => {
        if (peer.name === netB.options.name) {
          bConnected = true;
          checkConnected();
        }

        if (peer.name === netC.options.name) {
          cConnected = true;
          checkConnected();
        }
      });

      let bGotMessage = false;
      let cGotMessage = false;

      function checkComplete() {
        if (bGotMessage && cGotMessage) {
          cleanUp();
          done();
        }
      }

      netB.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payload) === 0) {
            bGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netB received unexpected data' +
                '\n  expected: ' +
                payload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf-8')
            );
          }
        }
      });

      netC.on('message-from-a', (packet) => {
        if (packet.peer.name === netA.options.name) {
          if (packet.payload.compare(payload) === 0) {
            cGotMessage = true;
            checkComplete();
          } else {
            cleanUp();
            fail(
              'netC received unexpected data' +
                '\n  expected: ' +
                payload.toString('utf-8') +
                '\n    actual: ' +
                packet.payload.toString('utf8')
            );
          }
        }
      });

      netA.connect({
        name: 'netA_' + uniqueId(),
      });

      netB.connect({
        name: 'netB_' + uniqueId(),
      });

      netC.connect({
        name: 'netC_' + uniqueId(),
      });

      return cleanUp;
    },
    { timeout: 5000 }
  );
});

test.run();
