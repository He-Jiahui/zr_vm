const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');
const { setImmediate: nextTurn } = require('node:timers/promises');

function deferred() {
    let resolve;
    const promise = new Promise(yes => { resolve = yes; });
    return { promise, resolve };
}

function structureHost(symbols, project) {
    const providers = new Map();
    const disposable = () => ({ dispose() {} });
    const uri = { toString: () => 'file:///workspace/main.zr', fsPath: '/workspace/main.zr' };
    let projectCalls = 0;
    const vscode = {
        EventEmitter: class { event = () => disposable(); fire() {} dispose() {} },
        ThemeIcon: class {},
        TreeItemCollapsibleState: { None: 0, Collapsed: 1, Expanded: 2 },
        Range: class { constructor(sl, sc, el, ec) { this.start = { line: sl, character: sc }; this.end = { line: el, character: ec }; } },
        window: {
            activeTextEditor: { document: { uri, languageId: 'zr', getText: () => 'module main;\nvar value: int = 1;' } },
            createTreeView: (id, options) => { providers.set(id, options.treeDataProvider); return disposable(); },
            onDidChangeActiveTextEditor: disposable,
        },
        commands: { registerCommand: disposable, executeCommand: () => symbols.promise },
        workspace: new Proxy({}, { get: (_target, key) => key === 'asRelativePath' ? () => 'main.zr' : disposable }),
    };
    const source = path.join(__dirname, '../src/structure.ts');
    const exports = {};
    vm.runInNewContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    }).outputText, {
        exports, setTimeout, clearTimeout, console,
        require: name => {
            if (name === 'vscode') { return vscode; }
            if (name === './languageClientRequests') { return { onDidChangeLanguageClient: disposable, sendLanguageServerRequest: async () => [] }; }
            if (name === './workspaceProjects') { return {
                activeWorkspaceFolder: () => undefined, onDidChangeSelectedProject: disposable,
                resolveSelectedWorkspaceProject: () => { projectCalls++; return project.promise; },
            }; }
            assert.equal(name, './structure/builtinModules');
            return require('../out/structure/builtinModules');
        },
    }, { filename: source });
    return { controller: exports.registerZrStructureViews({ subscriptions: [] }), providers, projectCalls: () => projectCalls };
}

test('file and builtin trees publish while project discovery and symbols are pending', async () => {
    const symbols = deferred(); const project = deferred();
    const host = structureHost(symbols, project);
    try {
        await nextTurn();
        const files = await host.providers.get('zrFiles').getChildren();
        const builtin = await host.providers.get('zrBuiltinModules').getChildren();
        assert.equal(files[0]?.label, 'main', 'The current file must appear before semantic symbols complete');
        assert.ok(builtin.length > 0, 'Static builtin modules must not wait for project discovery');
    } finally {
        symbols.resolve([]); project.resolve(undefined);
        await host.controller.refresh(); host.controller.dispose();
    }
});

test('project tree can publish before document symbols complete', async () => {
    const symbols = deferred(); const project = deferred();
    const host = structureHost(symbols, project);
    try {
        project.resolve(undefined);
        await nextTurn();
        const roots = await host.providers.get('zrImports').getChildren();
        assert.ok(roots.some(node => node.nodeType === 'action'), 'Project actions must not wait for the symbol provider');
    } finally {
        symbols.resolve([]); project.resolve(undefined);
        await host.controller.refresh(); host.controller.dispose();
    }
});

test('a burst of refresh requests coalesces while a previous project scan is pending', async () => {
    const symbols = deferred(); const project = deferred();
    symbols.resolve([]);
    const host = structureHost(symbols, project);
    await nextTurn();
    const refreshes = Array.from({ length: 12 }, () => host.controller.refresh());
    project.resolve(undefined);
    await Promise.all(refreshes);
    assert.ok(host.projectCalls() <= 2, 'A refresh burst must not enqueue a full project request per event');
    host.controller.dispose();
});
