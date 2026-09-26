const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

function projectHost(openText) {
    const uri = { path: '/workspace/demo.zrp', fsPath: '/workspace/demo.zrp', toString: () => 'file:///workspace/demo.zrp' };
    const workspaceFolder = { uri: { path: '/workspace', fsPath: '/workspace' } };
    let diskReads = 0;
    const vscode = {
        EventEmitter: class { event() {} },
        workspace: {
            getWorkspaceFolder: () => workspaceFolder,
            textDocuments: openText === undefined ? [] : [{ uri, getText: () => openText }],
            openTextDocument: () => assert.fail('Project discovery must not open a document or activate its language extensions'),
            fs: { readFile: async () => { diskReads++; return Buffer.from(manifest('磁盘项目')); } },
        },
    };
    const source = path.join(__dirname, '../src/workspaceProjects.ts');
    const exports = {};
    vm.runInNewContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    }).outputText, {
        exports, TextDecoder,
        require: name => name === 'vscode' ? vscode : require('../out/projectSupport'),
    }, { filename: source });
    return { uri, api: exports, diskReads: () => diskReads };
}

function manifest(name) {
    return JSON.stringify({ name, source: 'src', binary: 'bin', entry: 'main' });
}

test('project discovery reads unopened UTF-8 manifests without creating editor documents', async () => {
    const host = projectHost();
    const project = await host.api.createWorkspaceProject(host.uri);
    assert.equal(project.label, '磁盘项目');
    assert.equal(project.sourceRootPath, '/workspace/src');
    assert.equal(host.diskReads(), 1);
});

test('project discovery preserves unsaved manifest text instead of reading its disk version', async () => {
    const host = projectHost(manifest('unsaved'));
    const project = await host.api.createWorkspaceProject(host.uri);
    assert.equal(project.label, 'unsaved');
    assert.equal(host.diskReads(), 0);
});

test('an invalid unsaved manifest is not replaced with a valid stale disk manifest', async () => {
    const host = projectHost('{');
    assert.equal(await host.api.createWorkspaceProject(host.uri), undefined);
    assert.equal(host.diskReads(), 0);
});
