const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

// 将协议、提示位置和进程退出契约的首个失配转成脚本失败。
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// 最终收尾清除独占的临时项目；兼容较旧 Node 的删除接口。
function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (fs.existsSync(targetPath)) {
        fs.rmdirSync(targetPath, { recursive: true });
    }
}

// 此脚本只检查成功响应，复用共享客户端分配请求 ID 与处理 JSON-RPC 错误的规则。
async function request(client, method, params, timeoutMs = 10000) {
    return client.requestWithId(method, params, timeoutMs).promise;
}

// 真实项目文件与编辑器 didOpen 并用，验证 inlayHint 从规范声明事实提取推断变量类型，
// 且显式类型声明不生成重复提示；最后完成 LSP 握手并回收临时目录。
// TODO: 当前 CTest 注册中未发现本脚本入口；确认该回归是否应进入常规测试门禁。
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath,
        'Usage: node stdio_inlay_canonical_declaration_smoke.js <serverPath>');

    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-inlay-declaration-'));
    const sourcePath = path.join(rootPath, 'src');
    const documentPath = path.join(sourcePath, 'inlay_declaration.zr');
    const documentUri = pathToFileURL(documentPath).toString();
    const text = [
        'fn run(): void {',
        '    var inferred = 1;',
        '    var explicit: int = 2;',
        '}',
        '',
    ].join('\n');
    // BUG: 临时目录已创建，但客户端启动位于 try/finally 之外；服务端路径无效时，
    // 子进程未监听的异步 error 会终止 Node，使下面的 removePathSync 无法清理该目录。
    // 应把启动与临时项目纳入同一清理边界。
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;

    try {
        // 同一文件同时在磁盘和内存中可见，确保项目路径与客户端 URI 共同参与查询。
        fs.mkdirSync(sourcePath, { recursive: true });
        fs.writeFileSync(documentPath, text);

        const initialize = await request(client, 'initialize', {
            processId: process.pid,
            rootUri: pathToFileURL(rootPath).toString(),
            capabilities: {},
        });
        assert(initialize && initialize.capabilities &&
            initialize.capabilities.inlayHintProvider,
        'inlayHintProvider must be enabled');

        client.notify('initialized', {});
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: documentUri,
                languageId: 'zr',
                version: 1,
                text,
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics', 10000);

        // 诊断通知给 didOpen 建立处理边界；随后核对唯一提示属于推断声明的确切位置。
        const hints = await request(client, 'textDocument/inlayHint', {
            textDocument: { uri: documentUri },
            range: {
                start: { line: 0, character: 0 },
                end: { line: 4, character: 0 },
            },
        });
        assert(Array.isArray(hints) && hints.length === 1,
            `expected one canonical inferred-local hint, got ${JSON.stringify(hints)}`);
        assert(typeof hints[0].label === 'string' && hints[0].label.startsWith(': int'),
            `expected canonical int label, got ${JSON.stringify(hints[0])}`);
        assert(hints[0].position && hints[0].position.line === 1 &&
            hints[0].position.character === 16,
        `expected inferred declaration hint at 1:16, got ${JSON.stringify(hints[0].position)}`);

        // 正常 shutdown/exit 必须得到零退出码；异常路径由 finally 强制终止客户端。
        const shutdown = await request(client, 'shutdown', undefined);
        assert(shutdown === null, 'shutdown must return null');
        client.notify('exit', undefined);
        const exitCode = await client.waitForExit(10000);
        assert(exitCode === 0,
            `server exited with ${exitCode}. stderr=${client.stderr()}`);
        assert(client.stderr().trim() === '',
            `language server stderr must stay empty. stderr=${client.stderr()}`);
        cleanExit = true;
    } finally {
        if (!cleanExit && !client.closed) {
            await client.terminate();
        }
        removePathSync(rootPath);
    }
}

// Promise 入口将任何异步断言失败交给 Node 非零退出码，供命令行或测试编排者识别。
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
