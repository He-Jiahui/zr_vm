const test = require('node:test');
const assert = require('node:assert/strict');
const { setImmediate: nextTurn } = require('node:timers/promises');
const { deferred, loadExtensionHost } = require('./helpers/extensionHost');

async function within(promise) {
    let timer;
    try {
        return await Promise.race([promise, new Promise((_, reject) => {
            timer = setTimeout(() => reject(new Error('Lifecycle did not settle after cancellation')), 500);
        })]);
    } finally {
        clearTimeout(timer);
    }
}

function assertAttemptReleased(host) {
    for (const resource of host.resources.filter((item) => ['watcher', 'channel', 'diagnostics'].includes(item.kind))) {
        assert.ok(resource.disposals > 0, `${resource.kind} was not released`);
        if (resource.kind !== 'diagnostics') { assert.equal(resource.disposals, 1, resource.kind); }
    }
    for (const client of host.clients) { assert.equal(client.listeners.length, 0); }
    for (const worker of host.workers) { assert.equal(worker.terminations, 1); }
    for (const count of host.urls.values()) { assert.equal(count, 1); }
    assert.equal(host.requestClient(), undefined);
}

for (const kind of ['native', 'web']) {
    test(`${kind} retirement also releases a pending SDK automatic restart`, async () => {
        const host = loadExtensionHost(kind);
        await host.activate(host.extensionContext);
        const client = host.clients[0];
        const restart = deferred();
        client.state = 1;
        client.plan = restart.promise;
        const restarted = client.start();
        const settled = restarted.catch(() => {});
        await nextTurn(); // Acquire native transports before retirement.
        await within(host.deactivate());
        restart.resolve();
        await settled;
        await nextTurn();
        assert.equal(client.state, 1, 'late SDK recovery must be stopped');
        assert.ok(client.disposeCalls >= 2);
        assertAttemptReleased(host);
    });

    test(`${kind} a hanging SDK automatic restart has its own startup deadline`, async () => {
        const host = loadExtensionHost(kind, { startupTimeoutMs: 20 });
        await host.activate(host.extensionContext);
        const client = host.clients[0];
        const restart = deferred();
        client.state = 1;
        client.plan = restart.promise;
        await assert.rejects(within(client.start()), /Timed out while starting/);
        assertAttemptReleased(host);
        restart.resolve();
        await nextTurn();
        await host.deactivate();
    });

    test(`${kind} activation reports failure and a subsequent restart succeeds`, async () => {
        const host = loadExtensionHost(kind, { starts: [new Error('startup failed')] });
        await assert.doesNotReject(host.activate(host.extensionContext));
        assert.equal(host.errors.length, 1);
        assertAttemptReleased(host);
        await host.commands.get('zr.restartLanguageServer')();
        assert.equal(host.clients.length, 2);
        assert.equal(host.requestClient(), host.clients[1]);
        await host.deactivate();
        assertAttemptReleased(host);
    });

    test(`${kind} concurrent restarts remain serialized after a rejection`, async () => {
        const host = loadExtensionHost(kind, { starts: [undefined, new Error('restart failed'), undefined] });
        await host.activate(host.extensionContext);
        const restart = host.commands.get('zr.restartLanguageServer');
        await Promise.allSettled([restart(), restart()]);
        assert.equal(host.clients.length, 3);
        assert.equal(host.requestClient(), host.clients[2]);
        await host.deactivate();
        assertAttemptReleased(host);
    });

    test(`${kind} deactivation retires pending startup and its late completion`, async () => {
        const startup = deferred();
        const host = loadExtensionHost(kind, { starts: [startup.promise] });
        const activation = host.activate(host.extensionContext);
        while (host.clients.length === 0) { await nextTurn(); }
        await within(host.deactivate());
        await within(activation);
        assertAttemptReleased(host);
        startup.resolve();
        await nextTurn();
        await nextTurn();
        assertAttemptReleased(host);
        assert.ok(host.clients[0].disposeCalls >= 2, 'late running client must also be disposed');
        assert.equal((await host.clients[0].options.errorHandler.closed()).action, 1);
    });

    test(`${kind} stop errors still release every owned resource`, async () => {
        const host = loadExtensionHost(kind, { stopError: new Error('shutdown failed') });
        await host.activate(host.extensionContext);
        await assert.rejects(host.deactivate(), /shutdown failed/);
        assertAttemptReleased(host);
    });

    test(`${kind} a timed out startup cannot replace the client from a later retry`, async () => {
        const startup = deferred();
        const host = loadExtensionHost(kind, { starts: [startup.promise], startupTimeoutMs: 20 });
        await within(host.activate(host.extensionContext));
        assert.match(host.errors[0], /Timed out/);
        assertAttemptReleased(host);
        await host.commands.get('zr.restartLanguageServer')();
        const replacement = host.clients[1];
        assert.equal(host.requestClient(), replacement);
        startup.resolve();
        await nextTurn();
        await nextTurn();
        assert.equal(host.requestClient(), replacement);
        assert.ok(host.clients[0].disposeCalls >= 2);
        await host.deactivate();
        assertAttemptReleased(host);
    });

    test(`${kind} a hanging stop is bounded and still releases owned resources`, async () => {
        const stop = deferred();
        const host = loadExtensionHost(kind, { stopPending: stop.promise });
        await host.activate(host.extensionContext);
        await assert.rejects(within(host.deactivate()), /Timed out while disposing/);
        assertAttemptReleased(host);
        stop.resolve();
    });
}

test('web Worker construction failure revokes its Blob URL and permits another attempt', async () => {
    const options = { workerError: new Error('worker failed') };
    const host = loadExtensionHost('web', options);
    await assert.doesNotReject(host.activate(host.extensionContext));
    assert.equal(host.errors.length, 1);
    assertAttemptReleased(host);
    options.workerError = undefined;
    await host.commands.get('zr.restartLanguageServer')();
    assert.equal(host.requestClient(), host.clients[0]);
    await host.deactivate();
    assertAttemptReleased(host);
});

test('web deactivation during worker fetch prevents late worker creation', async () => {
    const pendingFetch = deferred();
    const host = loadExtensionHost('web', { fetch: pendingFetch.promise });
    const activation = host.activate(host.extensionContext);
    await nextTurn();
    await within(host.deactivate());
    pendingFetch.resolve();
    await within(activation);
    await nextTurn();
    assert.equal(host.workers.length, 0);
    assertAttemptReleased(host);
});

for (const retirement of ['deactivation', 'timeout']) {
test(`native transport acquired after ${retirement} is released without waiting for initialization`, async () => {
    const transport = deferred();
    const initialization = deferred();
    const host = loadExtensionHost('native', {
        transportPending: transport.promise,
        starts: [initialization.promise],
        startupTimeoutMs: retirement === 'timeout' ? 20 : 30000,
    });
    const activation = host.activate(host.extensionContext);
    while (host.clients.length === 0) { await nextTurn(); }
    if (retirement === 'deactivation') { await within(host.deactivate()); }
    await within(activation);
    transport.resolve();
    await nextTurn();
    await nextTurn();
    for (const kind of ['native-process', 'reader', 'writer']) {
        const acquired = host.resources.filter((item) => item.kind === kind);
        assert.equal(acquired.length, 1, `${kind} acquisition should finish after retirement`);
        assert.equal(acquired[0].disposals, 1, `${kind} must be released before initialization settles`);
    }
    assertAttemptReleased(host);
    await within(host.deactivate());
});
}
