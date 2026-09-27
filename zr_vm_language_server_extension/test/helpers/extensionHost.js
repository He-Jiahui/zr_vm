const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

// 供宿主重启与 Web 文档同步测试共享的手动异步闸门；测试显式决定获取或初始化何时结算。
function deferred() {
    let resolve;
    let reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
}

/**
 * 在隔离 VM 中运行实际桌面/Web 入口，并替换其外部依赖以记录资源与请求客户端。
 * options 控制 SDK 启动、停止、传输、Worker 和 fetch 的失败时序；每次调用生成独立宿主。
 */
function loadExtensionHost(host, options = {}) {
    const commands = new Map();
    const clients = [];
    const resources = [];
    const workers = [];
    const urls = new Map();
    const errors = [];
    let requestClient;
    // 每个租约记录释放次数，供测试区分遗漏释放和错误的重复释放。
    const resource = (kind) => {
        const value = { kind, disposals: 0, dispose() { this.disposals++; }, appendLine() {} };
        resources.push(value);
        return value;
    };
    // 入口只依赖这些最小 VS Code 接口；其余能力由注入模块隔离。
    const uri = (value) => ({ toString: () => value });
    const configuration = { get: (key, fallback) => key === 'mode' ? host : fallback };
    const vscode = {
        env: { uiKind: host === 'web' ? 2 : 1 },
        UIKind: { Web: 2, Desktop: 1 },
        Uri: { joinPath: (base, ...parts) => uri(`${base}/${parts.join('/')}`) },
        commands: {
            registerCommand: (name, handler) => { commands.set(name, handler); return resource('command'); }, // 测试从 Map 调用真实入口注册的重启命令。
        },
        workspace: {
            getConfiguration: () => configuration,
            createFileSystemWatcher: () => resource('watcher'),
            onDidChangeConfiguration: () => resource('configuration-listener'),
        },
        window: {
            createOutputChannel: () => resource('channel'),
            showErrorMessage: async (message) => { errors.push(message); }, // 捕获启动故障提示供宿主回归验收。
            showWarningMessage: async () => {},
            showInformationMessage: async () => {},
        },
    };
    /**
     * 提供 SDK 公开启动、停止和状态回调边界；Web/Node 的 clientOptions 分别取 args[2]/args[3]。
     * TODO: 此替身没有真实 SDK 的连接/特性内部清理时序；故障注入场景仍需真实编辑器冒烟验证。
     */
    class FakeClient {
        constructor(...args) {
            this.options = args[host === 'web' ? 2 : 3];
            this.plan = (options.starts ?? [])[clients.length];
            this.state = 1;
            this.disposeCalls = 0;
            this.listeners = [];
            clients.push(this);
        }
        // 宿主通过公开 start 覆盖包住此异步操作，故同一实例的手动恢复也经过 session 观察。
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
        // 故意允许进程和 reader/writer 在退役后才取得，检验 native 宿主的获取点清理。
        async createMessageTransports() {
            if (options.transportPending) { await options.transportPending; }
            this.process = resource('native-process');
            return { reader: resource('reader'), writer: resource('writer') };
        }
        // 对齐 SDK 在 Starting/StartFailed 时的公开拒绝语义，并允许故障或永久挂起。
        async stop() {
            if (this.state !== 2) {
                throw new Error("Client is not running and can't be stopped. It's current state is: startFailed");
            }
            if (options.stopError) { throw options.stopError; }
            if (options.stopPending) { await options.stopPending; }
            this.state = 1;
        }
        // 即使 stop 拒绝也运行进程终止 finalizer，供会话清理路径断言。
        async dispose() {
            this.disposeCalls++;
            // The installed SDK also cannot clean a Starting/StartFailed client here.
            try { await this.stop(); } finally {
                // Node's stop finalizer can terminate only a process that already exists.
                this.process?.dispose();
                this.process = undefined;
            }
        }
        // 测试只需可区分的默认动作；此替身不验证真实 SDK 的重启计数与预算。
        createDefaultErrorHandler() {
            return { error: () => ({ action: 1 }), closed: () => ({ action: 2 }) };
        }
        // 返回单次订阅租约，让退役后监听器数量可检查。
        onDidChangeState(listener) {
            this.listeners.push(listener);
            return { dispose: () => { this.listeners = this.listeners.filter((value) => value !== listener); } };
        }
    }
    /** 用构造失败和 terminate 次数验证 Web Worker 及 Blob URL 的归属。 */
    class FakeWorker {
        constructor() {
            if (options.workerError) { throw options.workerError; }
            this.terminations = 0;
            workers.push(this);
        }
        addEventListener() {}
        terminate() { this.terminations++; }
    }
    // 非会话视图由扩展上下文持有，这里只提供入口激活所需接口。
    const controller = () => ({ ...resource('view'), refresh: async () => {}, createMiddleware: () => ({}) });
    const modules = { // 注入非生命周期依赖，让两入口继续走真实会话链并暴露资源归属。
        vscode,
        'vscode-languageclient/node': { LanguageClient: FakeClient, Trace: { Off: 0 } },
        'vscode-languageclient/browser': { LanguageClient: FakeClient, Trace: { Off: 0 } },
        './debug/configProvider': { registerDesktopDebugSupport: () => [] },
        './debug/webSupport': { registerWebDebugSupportUnavailable: () => [] },
        './nativeAssets': { LANGUAGE_SERVER_CONFIG_SECTION: 'zr.languageServer', resolveNativeLanguageServerPath: () => 'server' }, // 固定可用路径以进入 native 启动链。
        './projectActions': { registerDesktopProjectActions: () => [] },
        './projectActionsWeb': { registerWebProjectActionsUnavailable: () => [] },
        './organizeImports': { registerOrganizeImportsCommand: () => resource('command') },
        './referenceCodeLens': { registerReferenceCodeLensCommand: () => resource('command') },
        './richHover': { registerRichHoverSupport: controller },
        './structure': { registerZrStructureViews: controller },
        './virtualDocuments': { registerVirtualDocumentSupport: () => resource('virtual-documents') },
        './zrpSupport': { createDocumentSelector: () => [], registerZrpJsonSupport: () => resource('zrp-support') },
        './selectedProjectSync': { sendZrSelectedProjectToLanguageServer: async () => {} }, // 本批隔离项目通知，只验收会话重启。
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
    // 缩短测试预算，同时保留正式 session/controller 实现和实际入口调用链。
    const sessions = require('../../out/languageServerSession');
    modules['./languageServerSession'] = {
        ...sessions,
        LanguageServerController: class extends sessions.LanguageServerController {
            constructor() { super(options.startupTimeoutMs ?? 30000, options.stopTimeoutMs ?? 20); }
        },
    };
    // 将入口源码单独转译并放进隔离 VM；其相对依赖仍来自已编译的 out 目录。
    const entryPath = path.join(__dirname, '..', '..', 'src', host === 'web' ? 'browser.ts' : 'extension.ts');
    const javascript = ts.transpileModule(fs.readFileSync(entryPath, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
        fileName: entryPath,
    }).outputText;
    const exports = {};
    // VM 注入可控 Worker/URL/fetch 和备用计时器；session 期限仍由 out 模块的宿主计时器运行。
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
