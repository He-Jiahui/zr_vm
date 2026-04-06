const test = require('node:test');
const assert = require('node:assert/strict');

const {
    CLOSE_ACTION_RESTART,
    ERROR_ACTION_CONTINUE,
    LANGUAGE_CLIENT_STATE_RUNNING,
    LANGUAGE_CLIENT_STATE_STOPPED,
    createTransportAwareLanguageClientLifecycle,
    isBenignLanguageClientStopError,
    isTransportDestroyedError,
    stopLanguageClientSafely,
} = require('../out/languageClientLifecycle.js');

class FakeClient {
    constructor() {
        this.state = LANGUAGE_CLIENT_STATE_RUNNING;
        this.stopCalls = 0;
        this.listeners = [];
        this.defaultErrorHandler = {
            error: async () => ({ action: 99 }),
            closed: async () => ({ action: CLOSE_ACTION_RESTART }),
        };
    }

    createDefaultErrorHandler() {
        return this.defaultErrorHandler;
    }

    onDidChangeState(listener) {
        this.listeners.push(listener);
        return {
            dispose: () => {
                this.listeners = this.listeners.filter((entry) => entry !== listener);
            },
        };
    }

    async stop() {
        this.stopCalls += 1;
        this.transition(LANGUAGE_CLIENT_STATE_STOPPED);
    }

    transition(nextState) {
        const previous = this.state;
        this.state = nextState;
        for (const listener of [...this.listeners]) {
            listener({ oldState: previous, newState: nextState });
        }
    }
}

test('detects destroyed transport errors from code and message text', () => {
    assert.equal(isTransportDestroyedError({ code: 'ERR_STREAM_DESTROYED' }), true);
    assert.equal(isTransportDestroyedError(new Error('Cannot call write after a stream was destroyed')), true);
    assert.equal(isTransportDestroyedError(new Error('different failure')), false);
    assert.equal(isBenignLanguageClientStopError(new Error("Client is not running and can't be stopped. It's current state is: stopped")), true);
});

test('marks transport as broken and continues on destroyed transport errors', async () => {
    const lifecycle = createTransportAwareLanguageClientLifecycle();
    const client = new FakeClient();

    lifecycle.attachClient(client);

    const result = await lifecycle.errorHandler.error(
        Object.assign(new Error('Cannot call write after a stream was destroyed'), { code: 'ERR_STREAM_DESTROYED' }),
        undefined,
        undefined,
    );

    assert.equal(result.action, ERROR_ACTION_CONTINUE);
    assert.equal(lifecycle.isTransportBroken(), true);
});

test('delegates close handling and clears transport-broken state after running again', async () => {
    const lifecycle = createTransportAwareLanguageClientLifecycle();
    const client = new FakeClient();

    lifecycle.attachClient(client);
    await lifecycle.errorHandler.closed();
    assert.equal(lifecycle.isTransportBroken(), true);

    client.transition(LANGUAGE_CLIENT_STATE_STOPPED);
    client.transition(LANGUAGE_CLIENT_STATE_RUNNING);

    assert.equal(lifecycle.isTransportBroken(), false);
});

test('skips graceful stop when transport is already broken', async () => {
    const lifecycle = createTransportAwareLanguageClientLifecycle();
    const client = new FakeClient();

    lifecycle.attachClient(client);
    await lifecycle.errorHandler.error(
        Object.assign(new Error('Cannot call write after a stream was destroyed'), { code: 'ERR_STREAM_DESTROYED' }),
        undefined,
        undefined,
    );

    await stopLanguageClientSafely(client, lifecycle, 10);

    assert.equal(client.stopCalls, 0);
});

test('uses graceful stop while transport is healthy', async () => {
    const lifecycle = createTransportAwareLanguageClientLifecycle();
    const client = new FakeClient();

    lifecycle.attachClient(client);
    await stopLanguageClientSafely(client, lifecycle, 10);

    assert.equal(client.stopCalls, 1);
});
