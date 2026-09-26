const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

test('Web structure keeps current file and builtins and explains unavailable project indexing', async () => {
    const commands = new Map();
    const disposable = () => ({ dispose() {} });
    const uri = { toString: () => 'file:///main.zr', fsPath: '/main.zr' };
    const vscode = {
        EventEmitter: class { event = () => disposable(); fire() {} dispose() {} },
        ThemeIcon: class {},
        TreeItemCollapsibleState: { None: 0, Collapsed: 1, Expanded: 2 },
        Range: class { constructor(sl, sc, el, ec) { this.start = { line: sl, character: sc }; this.end = { line: el, character: ec }; } },
        window: {
            activeTextEditor: { document: { uri, languageId: 'zr', getText: () => 'module main;\nvar seed: int = 1;' } },
            createTreeView: disposable,
            onDidChangeActiveTextEditor: disposable,
        },
        commands: {
            registerCommand: (name, callback) => { commands.set(name, callback); return disposable(); },
            executeCommand: async () => [],
        },
        workspace: new Proxy({}, { get: (_target, key) => key === 'asRelativePath' ? () => 'main.zr' : disposable }),
    };
    const source = path.join(__dirname, '..', 'src', 'structure.ts');
    const exports = {};
    vm.runInNewContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    }).outputText, {
        exports, setTimeout, clearTimeout,
        require: (name) => {
            if (name === 'vscode') { return vscode; }
            if (name === './languageClientRequests') {
                return { onDidChangeLanguageClient: disposable, sendLanguageServerRequest: () => assert.fail('Web must not request project modules') };
            }
            if (name === './workspaceProjects') {
                return { onDidChangeSelectedProject: disposable, resolveSelectedWorkspaceProject: () => assert.fail('Web must not scan projects') };
            }
            assert.equal(name, './structure/builtinModules');
            return require('../out/structure/builtinModules');
        },
    }, { filename: source });
    const controller = exports.registerZrStructureViews({ subscriptions: [] }, { projectIndexAvailable: false });
    const result = await commands.get('zr.__inspectStructureViews')();
    assert.equal(result.files[0].nodeType, 'file');
    assert.equal(result.files[0].label, 'main');
    assert.ok(result.builtin.length > 0);
    assert.equal(result.project.length, 1);
    assert.equal(result.project[0].nodeType, 'info');
    assert.match(result.project[0].label, /Project indexing is unavailable in VS Code Web/);
    assert.equal(result.project[0].commandId, undefined);
    controller.dispose();
});
