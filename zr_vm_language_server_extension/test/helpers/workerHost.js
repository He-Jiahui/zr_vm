const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

/**
 * 在隔离 VM 中装载真实 worker 注册代码，注入可控 WASM 响应并暴露 LSP handlers。
 * 此宿主验证协议路由与快照时序，不运行 Emscripten 或真实浏览器消息传输。
 */
function loadWorker(bridgeResponses = {}) {
    const handlers = new Map();
    const requests = new Map();
    const bridgeCalls = [];
    const diagnostics = [];
    // 分开记录 typed handler 与原始 method 注册，以检验能力宣告和实际路由一致。
    const connection = {
        onRequest: (method, handler) => requests.set(method, handler),
        onNotification: () => {},
        listen() {},
        sendDiagnostics: (value) => diagnostics.push(value),
        console: { warn() {} },
    };
    for (const event of [
        'onInitialize', 'onInitialized', 'onShutdown',
        'onDidOpenTextDocument', 'onDidChangeTextDocument',
        'onDidCloseTextDocument', 'onDidSaveTextDocument',
        'onCompletion', 'onHover', 'onDefinition', 'onReferences',
        'onDocumentSymbol', 'onWorkspaceSymbol', 'onDocumentHighlight',
        'onPrepareRename', 'onRenameRequest',
    ]) {
        connection[event] = (handler) => handlers.set(event, handler);
    }
    /** 响应函数可延迟或失败，用于观察文档队列和错误封装的传播。 */
    class TestBridge {
        async initialize(baseUrl) { bridgeCalls.push(['initialize', baseUrl]); }
    }
    // 默认让文档增删与诊断可用；场景只需覆盖自己关心的导出。
    for (const [method, data] of Object.entries({
        updateDocument: {}, closeDocument: {},
        getDiagnosticReport: { resultId: 'initial', items: [] },
        ...bridgeResponses,
    })) {
        TestBridge.prototype[method] = async (...args) => {
            bridgeCalls.push([method, ...args]);
            const value = typeof data === 'function' ? await data(...args) : data;
            if (value && typeof value === 'object' && Object.hasOwn(value, 'success')) { return value; }
            return { success: true, data: value };
        };
    }
    /** 保留 JSON-RPC 错误码与附加数据，供断言比对真实客户端收到的错误。 */
    class TestResponseError extends Error {
        constructor(code, message, data) { super(message); this.code = code; this.data = data; }
    }
    const protocol = {
        BrowserMessageReader: class {}, BrowserMessageWriter: class {},
        createConnection: () => connection,
        TextDocumentSyncKind: { Incremental: 2 },
        ResponseError: TestResponseError,
        ErrorCodes: { InternalError: -32603, InvalidParams: -32602 },
        LSPErrorCodes: { ContentModified: -32801 },
    };
    const modules = new Map();
    /** 递归转译同目录 TS 依赖，替换协议、浏览器全局对象及 WASM 边界。 */
    function load(file) {
        if (modules.has(file)) { return modules.get(file); }
        const exports = {};
        modules.set(file, exports);
        const javascript = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
            compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
            fileName: file,
        }).outputText;
        vm.runInNewContext(javascript, {
            exports,
            require: (name) => {
                if (name === 'vscode-languageserver/browser' || name === 'vscode-jsonrpc') { return protocol; }
                if (name === './wasm-bridge') { return { ZrWasmBridge: TestBridge }; }
                assert.ok(name.startsWith('./'), name);
                return load(path.resolve(path.dirname(file), `${name}.ts`));
            },
            self: { addEventListener() {} },
            console: { ...console, error() {}, warn() {} },
        }, { filename: file });
        return exports;
    }
    load(path.join(__dirname, '..', '..', 'src', 'browser', 'worker', 'server-worker.ts'));
    return { handlers, requests, bridgeCalls, diagnostics };
}

module.exports = { loadWorker };
