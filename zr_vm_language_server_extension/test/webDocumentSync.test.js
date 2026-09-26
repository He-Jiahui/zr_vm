const test = require('node:test');
const assert = require('node:assert/strict');
const { setImmediate: nextTurn } = require('node:timers/promises');
const { loadWorker } = require('./helpers/workerHost');
const { deferred } = require('./helpers/extensionHost');

const uri = 'file:///workspace/main.zr';
const full = (text) => [{ text }];
const edit = (start, end, text, rangeLength) => ({
    range: { start, end }, text, ...(rangeLength === undefined ? {} : { rangeLength }),
});
const pos = (line, character) => ({ line, character });
const open = (host, text = 'abc', version = 1) => host.handlers.get('onDidOpenTextDocument')({ textDocument: { uri, text, version } });
const change = (host, version, contentChanges) => host.handlers.get('onDidChangeTextDocument')({ textDocument: { uri, version }, contentChanges });
const hover = (host) => host.handlers.get('onHover')({ textDocument: { uri }, position: pos(0, 0) });
const close = (host) => host.handlers.get('onDidCloseTextDocument')({ textDocument: { uri } });
const modified = (promise) => assert.rejects(promise, (error) => error.code === -32801);
const updates = (host) => host.bridgeCalls.filter(([method]) => method === 'updateDocument');

test('Web rejects invalid edits atomically and only a single full replacement recovers', async () => {
    const host = loadWorker({ getHover: { contents: 'ok' } });
    await open(host);
    await change(host, 2, [edit(pos(0, 0), pos(0, 1), 'x'), edit(pos(3, 0), pos(3, 0), 'y')]);
    assert.equal(updates(host).length, 1);
    await modified(hover(host));
    await change(host, 3, [edit(pos(0, 0), pos(0, 1), 'z')]);
    await change(host, 4, [...full('new'), ...full('last')]);
    await change(host, 5, [{ text: 'new', rangeLength: 3 }]);
    assert.equal(updates(host).length, 1);
    await change(host, 6, full('recovered'));
    assert.equal(updates(host).at(-1)[2], 'recovered');
    assert.deepEqual(await hover(host), { contents: 'ok' });
});

test('Web rejects duplicate, decreasing and malformed versions without changing the snapshot', async () => {
    for (const version of [1, 0, -1, 1.5, undefined, null, '2', Number.NaN, 2 ** 32]) {
        const host = loadWorker({ getHover: null });
        await open(host);
        await change(host, version, full('bad'));
        assert.equal(updates(host).length, 1, String(version));
        await modified(hover(host));
        await change(host, 2, full('ok'));
        assert.equal(updates(host).at(-1)[2], 'ok');
    }
});

test('Web validates UTF-16 boundaries, CRLF line endings and rangeLength', async () => {
    for (const invalid of [
        edit(pos(0, 2), pos(0, 3), ''), // inside the emoji surrogate pair
        edit(pos(0, 4), pos(0, 4), ''), // within CRLF, beyond line content
        edit(pos(4, 0), pos(4, 0), ''),
        edit(pos(1, 1), pos(0, 0), ''),
        edit(pos(-1, 0), pos(0, 0), ''),
        edit(pos(0, 0.5), pos(0, 1), ''),
        edit(pos(0, 1), pos(0, 3), '', 1),
        edit(pos(0, 0), pos(0, 1), '', -1),
    ]) {
        const host = loadWorker({ getHover: null });
        await open(host, 'a😀\r\nb\rc\n');
        await change(host, 2, [invalid]);
        assert.equal(updates(host).length, 1);
        await modified(hover(host));
    }
    const host = loadWorker();
    await open(host, 'a😀\r\nb\rc\n');
    await change(host, 2, [
        edit(pos(0, 1), pos(0, 3), 'Z', 2),
        edit(pos(0, 2), pos(1, 1), 'Y', 3),
        edit(pos(2, 0), pos(2, 0), 'end', 0),
    ]);
    assert.equal(updates(host).at(-1)[2], 'aZY\rc\nend');
});

test('Web update failures preserve the last committed text and save never changes it', async () => {
    let fail = false;
    const host = loadWorker({
        updateDocument: () => fail ? { success: false, code: -32603, error: 'failed' } : {},
        getHover: null,
    });
    await open(host);
    fail = true;
    await change(host, 2, full('failed replacement'));
    await modified(hover(host));
    await host.handlers.get('onDidSaveTextDocument')({ textDocument: { uri }, text: 'disk' });
    assert.equal(updates(host).length, 2);
    fail = false;
    await change(host, 2, full('ab'));
    await change(host, 3, [edit(pos(0, 1), pos(0, 2), 'c', 1)]);
    assert.equal(updates(host).at(-1)[2], 'ac');
});

test('Web serializes changes behind pending updates and commits only successful results', async () => {
    const pending = deferred();
    const host = loadWorker({ updateDocument: (_uri, _text, version) => version === 2 ? pending.promise : {} });
    await open(host);
    const second = change(host, 2, full('abcd'));
    await nextTurn();
    const third = change(host, 3, [edit(pos(0, 3), pos(0, 4), 'e')]);
    await nextTurn();
    assert.equal(updates(host).length, 2);
    pending.resolve({});
    await Promise.all([second, third]);
    assert.equal(updates(host).at(-1)[2], 'abce');
});

test('Web drops late diagnostics after edits, desynchronization and close/reopen at the same version', async () => {
    for (const action of ['change', 'invalid', 'reopen']) {
        const delayed = deferred();
        let reports = 0;
        const host = loadWorker({ getDiagnosticReport: () => ++reports === 1 ? delayed.promise : { resultId: 'new', items: [] } });
        const opening = open(host);
        await nextTurn();
        if (action === 'change') { await change(host, 2, full('new')); }
        if (action === 'invalid') { await change(host, 1, full('invalid')); }
        if (action === 'reopen') { await close(host); await open(host, 'reopened', 1); }
        delayed.resolve({ resultId: 'old', items: [{ message: 'stale' }] });
        await opening;
        assert.equal(host.diagnostics.some((entry) => entry.diagnostics.some((item) => item.message === 'stale')), false, action);
        if (action === 'reopen') { assert.ok(host.diagnostics.some((entry) => entry.version === undefined && entry.diagnostics.length === 0)); }
    }
});

test('Web queries wait for synchronization and reject a result from an obsolete revision', async () => {
    const pending = deferred();
    const host = loadWorker({ getHover: () => pending.promise });
    await open(host);
    const query = hover(host);
    await nextTurn();
    await change(host, 2, full('new'));
    pending.resolve({ contents: 'stale' });
    await modified(query);
    await close(host);
    await modified(hover(host));
});

test('Web rejects changes to unopened documents and duplicate opens do not overwrite snapshots', async () => {
    const host = loadWorker({ getHover: null });
    await change(host, 1, full('unopened'));
    assert.equal(updates(host).length, 0);
    await modified(hover(host));
    await open(host);
    await open(host, 'duplicate', 2);
    assert.equal(updates(host).length, 1);
});

test('Web validates complete notification payloads and Unicode before reaching WASM', async () => {
    for (const changes of [[], null, {}, [null], [{ text: null }], [{ text: 'x', range: null }],
        [{ text: '\ud800' }], [{ text: '\udc00' }],
        [edit(pos(0, 0), pos(0, 1), 'x', null)]]) {
        const host = loadWorker({ getHover: null });
        await open(host);
        await change(host, 2, changes);
        assert.equal(updates(host).length, 1);
        await modified(hover(host));
    }
    for (const [text, version] of [[null, 1], ['x', -1], ['x', undefined], ['\ud800', 1]]) {
        const host = loadWorker();
        await host.handlers.get('onDidOpenTextDocument')({ textDocument: { uri, text, version } });
        assert.equal(updates(host).length, 0);
    }
});

test('Web document queries wait for the preceding backend update', async () => {
    const pending = deferred();
    const host = loadWorker({ updateDocument: (_uri, _text, version) => version === 2 ? pending.promise : {}, getHover: null });
    await open(host);
    const changed = change(host, 2, full('new'));
    const query = hover(host);
    await nextTurn();
    assert.equal(host.bridgeCalls.some(([name]) => name === 'getHover'), false);
    pending.resolve({});
    await changed;
    assert.equal(await query, null);
});

test('Web can recover a failed initial update with a newer full replacement', async () => {
    let failing = true;
    const host = loadWorker({ updateDocument: () => {
        if (failing) { throw new Error('WASM unavailable'); }
        return {};
    }, getHover: null });
    await open(host);
    await modified(hover(host));
    failing = false;
    await change(host, 2, full('recovered'));
    assert.equal(updates(host).at(-1)[2], 'recovered');
    assert.equal(await hover(host), null);
});

test('Web close and reopen serialize backend mutation while clearing diagnostics immediately', async () => {
    const pending = deferred();
    const order = [];
    const host = loadWorker({
        updateDocument: async (_uri, text) => { if (text === 'pending') { await pending.promise; } order.push(text); return {}; },
        closeDocument: () => { order.push('close'); return {}; },
    });
    await open(host, 'first');
    const changing = change(host, 2, full('pending'));
    await nextTurn();
    const closing = close(host);
    const reopening = open(host, 'reopened', 1);
    assert.equal(host.diagnostics.at(-1).version, undefined);
    assert.equal(host.diagnostics.at(-1).diagnostics.length, 0);
    pending.resolve();
    await Promise.all([changing, closing, reopening]);
    assert.deepEqual(order, ['first', 'pending', 'close', 'reopened']);
    assert.equal(host.diagnostics.at(-1).version, 1);
});
