const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

function synchronizationHost(resolveSelectedProjectUri) {
    const exports = {};
    const source = path.join(__dirname, '../src/selectedProjectSync.ts');
    vm.runInNewContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    }).outputText, {
        exports,
        require: name => name === './workspaceProjects'
            ? { activeWorkspaceFolder: () => undefined, resolveSelectedProjectUri }
            : require('../out/languageClientRequests'),
    }, { filename: source });
    return exports.sendZrSelectedProjectToLanguageServer;
}

test('an active session notifies the discovered project URI', async () => {
    const synchronize = synchronizationHost(async () => ({ toString: () => 'file:///workspace/current.zrp' }));
    const notifications = [];
    await synchronize({}, { sendNotification: async (...args) => notifications.push(args) }, new AbortController().signal);
    assert.equal(notifications.length, 1);
    assert.equal(notifications[0][0], 'zr/selectedProject');
    assert.equal(notifications[0][1].uri, 'file:///workspace/current.zrp');
});

test('retiring a session during project discovery suppresses its late notification', async () => {
    let resolve;
    const discovery = new Promise(yes => { resolve = yes; });
    const synchronize = synchronizationHost(() => discovery);
    const abort = new AbortController();
    const notifications = [];
    const pending = synchronize({}, { sendNotification: async (...args) => notifications.push(args) }, abort.signal);
    abort.abort();
    resolve({ toString: () => 'file:///workspace/old.zrp' });
    await pending;
    assert.equal(notifications.length, 0);
});

test('an already retired session does not start project discovery', async () => {
    let discoveries = 0;
    const synchronize = synchronizationHost(async () => { discoveries++; });
    const abort = new AbortController();
    abort.abort();
    await synchronize({}, { sendNotification: async () => assert.fail('Retired client was notified') }, abort.signal);
    assert.equal(discoveries, 0);
});
