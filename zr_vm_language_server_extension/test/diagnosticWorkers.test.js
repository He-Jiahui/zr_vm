const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

const deferred = () => {
    let resolve, reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
};
const tick = () => new Promise(resolve => setImmediate(resolve));
const uri = value => ({ toString: () => value });
const token = () => {
    const listeners = new Set();
    return {
        isCancellationRequested: false,
        onCancellationRequested(fn) { listeners.add(fn); return { dispose: () => listeners.delete(fn) }; },
        cancel() { this.isCancellationRequested = true; for (const fn of [...listeners]) fn(); },
    };
};

function harness() {
    const events = new Map();
    const documents = [{ uri: uri('file:///a.zr'), languageId: 'zr', version: 1, getText: () => 'module a;' }];
    class CancellationError extends Error {}
    const vscode = { CancellationError, workspace: { textDocuments: documents, workspaceFolders: [],
        ...Object.fromEntries(['onDidOpenTextDocument', 'onDidChangeTextDocument', 'onDidCloseTextDocument'].map(name =>
            [name, fn => { events.set(name, fn); return { dispose: () => events.delete(name) }; }])),
    } };
    const file = path.join(__dirname, '../src/diagnosticWorkers.ts');
    const javascript = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    }).outputText;
    const exports = {};
    vm.runInNewContext(javascript, { exports, console, setTimeout, clearTimeout,
        require: name => name === 'vscode' ? vscode : name === './diagnosticWorkerProcess' ? {} : require(name),
    }, { filename: file });
    const workers = [], calls = [], logs = [];
    const factory = () => {
        const worker = { busy: false, notifications: [], disposals: 0,
            request(method, params) { const gate = deferred(); calls.push({ worker, method, params, gate }); return gate.promise; },
            notify(method, params) { this.notifications.push({ method, params }); },
            async dispose() { this.disposals++; },
        };
        workers.push(worker); return worker;
    };
    const pool = exports.createDiagnosticWorkers('server', { appendLine: line => logs.push(line) }, () => undefined,
        { workerFactory: factory });
    const middleware = pool.createMiddleware(() => ({
        protocol2CodeConverter: { asDiagnostics: async items => items.map(item => ({ ...item, converted: true })), asUri: uri },
    }));
    return { pool, middleware, workers, calls, events, documents, CancellationError, logs };
}

test('document middleware preserves result IDs, converts diagnostics and uses the SDK signature', async () => {
    const h = harness();
    try {
        const pending = h.middleware.provideDiagnostics(h.documents[0], 'previous', token(), () => assert.fail('must stay off main LSP'));
        await tick();
        assert.equal(h.calls[0].params.previousResultId, 'previous');
        assert(h.workers[0].notifications.some(n => n.method === 'textDocument/didOpen'), 'unsaved buffers must be synchronized');
        h.calls[0].gate.resolve({ kind: 'full', resultId: 'next', items: [{ message: 'error' }] });
        const report = await pending;
        assert.equal(report.kind, 'full');
        assert.equal(report.resultId, 'next');
        assert.equal(report.items[0].converted, true);
    } finally { await h.pool.dispose(); }
});

test('two workers process scripts concurrently and drain queued work', async () => {
    const h = harness();
    const active = [];
    try {
        for (let index = 0; index < 3; index++) active.push(h.middleware.provideDiagnostics(uri(`file:///${index}.zr`), undefined, token(), () => assert.fail()));
        await tick();
        assert.equal(h.workers.length, 2);
        assert.equal(h.calls.length, 2, 'two different scripts must really be running');
        h.calls[0].gate.resolve({ kind: 'full', items: [] });
        await tick();
        assert.equal(h.calls.length, 3);
        h.calls[1].gate.resolve({ kind: 'full', items: [] });
        h.calls[2].gate.resolve({ kind: 'full', items: [] });
        await Promise.all(active);
        assert.equal(h.workers.length, 2);
    } finally { await h.pool.dispose(); }
});

test('cancellation retires the busy process and never publishes its late result', async () => {
    const h = harness(), cancel = token();
    try {
        const pending = h.middleware.provideDiagnostics(h.documents[0], undefined, cancel, () => assert.fail());
        const rejected = assert.rejects(pending, h.CancellationError);
        await tick();
        cancel.cancel();
        await rejected;
        await tick();
        assert.equal(h.workers[0].disposals, 1);
        h.calls[0].gate.resolve({ kind: 'full', items: [{ message: 'stale' }] });
        await tick();
    } finally { await h.pool.dispose(); }
});

test('workspace middleware converts reports and sends them to the SDK callback', async () => {
    const h = harness(), chunks = [];
    try {
        const pending = h.middleware.provideWorkspaceDiagnostics([{ uri: uri('file:///a.zr'), value: 'previous' }], token(), chunk => chunks.push(chunk), () => assert.fail());
        await tick();
        h.calls[0].gate.resolve({ items: [{ uri: 'file:///a.zr', version: 1, kind: 'full', resultId: 'previous', items: [] }] });
        await pending;
        assert.equal(chunks[0].items[0].kind, 'unChanged');
        assert.equal(chunks[0].items[0].uri.toString(), 'file:///a.zr');
    } finally { await h.pool.dispose(); }
});

test('editing a dependency cancels stale diagnostics and disposal rejects queued work', async () => {
    const h = harness();
    const pending = [0, 1, 2].map(i => h.middleware.provideDiagnostics(uri(`file:///${i}.zr`), undefined, token(), () => assert.fail()));
    const results = Promise.allSettled(pending);
    await tick();
    h.events.get('onDidChangeTextDocument')({ document: h.documents[0], contentChanges: [{ text: 'changed' }] });
    await h.pool.dispose();
    assert((await results).every(result => result.status === 'rejected' && result.reason instanceof h.CancellationError));
    assert(h.workers.every(worker => worker.disposals === 1));
    assert.equal(h.events.size, 0);
});

test('a failed worker stays off the interactive process and the next pull creates a replacement', async () => {
    const h = harness();
    try {
        const first = h.middleware.provideDiagnostics(h.documents[0], undefined, token(), () => assert.fail('no main-process fallback'));
        await tick();
        h.calls[0].gate.reject(new Error('worker exited'));
        assert.equal((await first).kind, 'full');
        await tick();
        assert.equal(h.workers[0].disposals, 1);
        const second = h.middleware.provideDiagnostics(h.documents[0], undefined, token(), () => assert.fail());
        await tick();
        assert.equal(h.workers.length, 2);
        h.calls[1].gate.resolve({ kind: 'full', resultId: 'recovered', items: [] });
        assert.equal((await second).resultId, 'recovered');
    } finally { await h.pool.dispose(); }
});

test('workspace cancellation is a normal empty poll and publishes no stale chunk', async () => {
    const h = harness(), cancel = token();
    try {
        const pending = h.middleware.provideWorkspaceDiagnostics([], cancel, () => assert.fail('stale workspace report'), () => assert.fail());
        const observed = pending.then(value => ({ value }), error => ({ error }));
        await tick();
        cancel.cancel();
        const result = await observed;
        assert.equal(result.error, undefined, 'SDK 8.1 stops polling after repeated workspace failures');
        assert.equal(result.value.items.length, 0);
    } finally { await h.pool.dispose(); }
});

test('workspace polling reuses a completed revision and rereads after watched disk edits', async () => {
    const h = harness(), collected = [];
    const poll = ids => h.middleware.provideWorkspaceDiagnostics(ids, token(), chunk => collected.push(chunk), () => assert.fail());
    try {
        const first = poll([]);
        await tick();
        h.calls[0].gate.resolve({ items: [{ uri: 'file:///a.zr', version: 1, kind: 'full', resultId: 'workspace', items: [] }] });
        await first;
        await poll([{ uri: uri('file:///a.zr'), value: 'document-worker-id' }]);
        assert.equal(h.calls.length, 1, 'an idle workspace must not reenter native lint to compare worker-specific IDs');
        assert.equal(collected[1].items[0].kind, 'full', 'a different previous ID still needs the cached full report');
        await poll([{ uri: uri('file:///a.zr'), value: 'workspace' }]);
        assert.equal(collected[2].items[0].kind, 'unChanged');
        h.pool.notify('workspace/didChangeWatchedFiles', { changes: [{ uri: 'file:///b.zr', type: 2 }] });
        const changed = poll([{ uri: uri('file:///a.zr'), value: 'workspace' }]);
        await tick();
        assert.equal(h.calls.length, 2, 'disk change must refresh the workspace report');
        assert.equal(h.calls[1].params.previousResultIds.length, 0, 'cold worker refresh must capture complete reports');
        h.calls[1].gate.resolve({ items: [] });
        await changed;
    } finally { await h.pool.dispose(); }
});
