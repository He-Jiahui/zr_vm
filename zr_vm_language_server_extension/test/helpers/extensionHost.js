const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

function deferred() {
    let resolve;
    let reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
}

function loadExtensionHost(host, options = {}) {
    const commands = new Map();
    const clients = [];
    const resources = [];
    const workers = [];
    const urls = new Map();
    const errors = [];
    let requestClient;
    const resource = (kind) => {
        const value = { kind, disposals: 0, dispose() { this.disposals++; }, appendLine() {} };
        resources.push(value);
        return value;
    };
    const uri = (value) => ({ toString: () => value });
    const configuration = { get: (key, fallback) => key === 'mode' ? host : fallback };
    const vscode = {
        env: { uiKind: host === 'web' ? 2 : 1 },
        UIKind: { Web: 2, Desktop: 1 },
        Uri: { joinPath: (base, ...parts) => uri(`${base}/${parts.join('/')}`) },
        commands: {
            registerCommand: (name, handler) => { commands.set(name, handler); return resource('command'); },
        },
        workspace: {
            getConfiguration: () => configuration,
            createFileSystemWatcher: () => resource('watcher'),
            onDidChangeConfiguration: () => resource('configuration-listener'),
        },
        window: {
            createOutputChannel: () => resource('channel'),
            showErrorMessage: async (message) => { errors.push(message); },
            showWarningMessage: async () => {},
            showInformationMessage: async () => {},
        },
    };
    class FakeClient {
        constructor(...args) {
            this.options = args[host === 'web' ? 2 : 3];
            this.plan = (options.starts ?? [])[clients.length];
            this.state = 1;
            this.disposeCalls = 0;
            this.listeners = [];
            clients.push(this);
        }
        async start() {
            this.diagnostics ??= resource('diagnostics');
            try {
                if (host === 'native') { await this.createMessageTransports('utf8'); }
                if (this.plan instanceof Error) { throw this.plan; }
                if (this.plan) { await this.plan; }
                this.state = 2;
                for (const listener of [...this.listeners]) { listener({ oldState: 1, newState: 2 }); }
            } catch (error) {
                this.state = 1; // 8.1 exposes StartFailed as Stopped.
                throw error;
            }
        }
        async setTrace() {}
        async sendNotification() {}
        async createMessageTransports() {
            if (options.transportPending) { await options.transportPending; }
            this.process = resource('native-process');
            return { reader: resource('reader'), writer: resource('writer') };
        }
        async stop() {
            if (this.state !== 2) {
                throw new Error("Client is not running and can't be stopped. It's current state is: startFailed");
            }
            if (options.stopError) { throw options.stopError; }
            if (options.stopPending) { await options.stopPending; }
            this.state = 1;
        }
        async dispose() {
            this.disposeCalls++;
            // The installed SDK also cannot clean a Starting/StartFailed client here.
            try { await this.stop(); } finally {
                // Node's stop finalizer can terminate only a process that already exists.
                this.process?.dispose();
                this.process = undefined;
            }
        }
        createDefaultErrorHandler() {
            return { error: () => ({ action: 1 }), closed: () => ({ action: 2 }) };
        }
        onDidChangeState(listener) {
            this.listeners.push(listener);
            return { dispose: () => { this.listeners = this.listeners.filter((value) => value !== listener); } };
        }
    }
    class FakeWorker {
        constructor() {
            if (options.workerError) { throw options.workerError; }
            this.terminations = 0;
            workers.push(this);
        }
        addEventListener() {}
        terminate() { this.terminations++; }
    }
    const controller = () => ({ ...resource('view'), refresh: async () => {}, createMiddleware: () => ({}) });
    const modules = {
        vscode,
        'vscode-languageclient/node': { LanguageClient: FakeClient, Trace: { Off: 0 } },
        'vscode-languageclient/browser': { LanguageClient: FakeClient, Trace: { Off: 0 } },
        './debug/configProvider': { registerDesktopDebugSupport: () => [] },
        './debug/webSupport': { registerWebDebugSupportUnavailable: () => [] },
        './nativeAssets': { LANGUAGE_SERVER_CONFIG_SECTION: 'zr.languageServer', resolveNativeLanguageServerPath: () => 'server' },
        './projectActions': { registerDesktopProjectActions: () => [] },
        './projectActionsWeb': { registerWebProjectActionsUnavailable: () => [] },
        './organizeImports': { registerOrganizeImportsCommand: () => resource('command') },
        './referenceCodeLens': { registerReferenceCodeLensCommand: () => resource('command') },
        './richHover': { registerRichHoverSupport: controller },
        './structure': { registerZrStructureViews: controller },
        './virtualDocuments': { registerVirtualDocumentSupport: () => resource('virtual-documents') },
        './zrpSupport': { createDocumentSelector: () => [], registerZrpJsonSupport: () => resource('zrp-support') },
        './selectedProjectSync': { sendZrSelectedProjectToLanguageServer: async () => {} },
        './workspaceProjects': {
            activeWorkspaceFolder: () => undefined,
            resolveSelectedProjectUri: async () => undefined,
            onDidChangeSelectedProject: () => resource('project-listener'),
        },
        './languageClientRequests': {
            sendLanguageServerRequest: async () => undefined,
            setLanguageClientRequestClient: (client) => { requestClient = client; },
        },
    };
    const sessions = require('../../out/languageServerSession');
    modules['./languageServerSession'] = {
        ...sessions,
        LanguageServerController: class extends sessions.LanguageServerController {
            constructor() { super(options.startupTimeoutMs ?? 30000, options.stopTimeoutMs ?? 20); }
        },
    };
    const entryPath = path.join(__dirname, '..', '..', 'src', host === 'web' ? 'browser.ts' : 'extension.ts');
    const javascript = ts.transpileModule(fs.readFileSync(entryPath, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
        fileName: entryPath,
    }).outputText;
    const exports = {};
    const context = {
        exports,
        require: (name) => {
            if (Object.prototype.hasOwnProperty.call(modules, name)) { return modules[name]; }
            return name.startsWith('./') ? require(path.join(__dirname, '..', '..', 'out', name)) : require(name);
        },
        console: { ...console, warn() {}, error() {} },
        setTimeout: (callback, delay) => setTimeout(callback, Math.min(delay, 30)),
        clearTimeout,
        AbortController,
        Blob,
        URL: {
            createObjectURL: () => { const value = `blob:${urls.size}`; urls.set(value, 0); return value; },
            revokeObjectURL: (value) => { urls.set(value, urls.get(value) + 1); },
        },
        Worker: FakeWorker,
        fetch: async () => {
            if (options.fetch) { await options.fetch; }
            return { ok: true, text: async () => 'worker source' };
        },
    };
    vm.runInNewContext(javascript, context, { filename: entryPath });
    return {
        ...exports,
        extensionContext: { subscriptions: [], extensionUri: uri('file:///extension') },
        commands, clients, resources, workers, urls, errors,
        requestClient: () => requestClient,
    };
}

module.exports = { deferred, loadExtensionHost };
