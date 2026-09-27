const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

/** 在隔离 VS Code 宿主中运行真实项目发现代码，并记录磁盘读取以检验编辑器优先级。 */
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

/** 生成满足项目发现最小契约的 .zrp 文本，便于区分磁盘与未保存版本。 */
function manifest(name) {
    return JSON.stringify({ name, source: 'src', binary: 'bin', entry: 'main' });
}

/** 未打开清单应从工作区文件系统读取，且不调用 openTextDocument。 */
test('project discovery reads unopened UTF-8 manifests without creating editor documents', async () => {
    const host = projectHost();
    const project = await host.api.createWorkspaceProject(host.uri);
    assert.equal(project.label, '磁盘项目');
    assert.equal(project.sourceRootPath, '/workspace/src');
    assert.equal(host.diskReads(), 1);
});

/** 编辑器的未保存文本应决定当前项目标签和源码根，不能被磁盘旧版本覆盖。 */
test('project discovery preserves unsaved manifest text instead of reading its disk version', async () => {
    const host = projectHost(manifest('unsaved'));
    const project = await host.api.createWorkspaceProject(host.uri);
    assert.equal(project.label, 'unsaved');
    assert.equal(host.diskReads(), 0);
});

/** 未保存文本无效时不回退磁盘旧版本，避免调试与 LSP 选到非用户所见的项目。 */
test('an invalid unsaved manifest is not replaced with a valid stale disk manifest', async () => {
    const host = projectHost('{');
    assert.equal(await host.api.createWorkspaceProject(host.uri), undefined);
    assert.equal(host.diskReads(), 0);
});
