const { spawnSync } = require('child_process');

// 让最小协议回归的首个失配以非零 Node 退出码反馈给 CTest。
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// 按 UTF-8 正文字节数封装 LSP 帧，使 smoke 经过真实 stdio 解码入口。
function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

// CTest 注入刚构建的服务端；此脚本自行完成单会话的启动与关闭。
const serverPath = process.argv[2];

assert(serverPath, 'Expected stdio server executable path');

// 两个相互引用的顶层变量用于检验 didOpen 后文档能进入诊断与符号查询路径。
const documentUri = 'file:///zr-minimal-open-smoke.zr';
const documentText = [
    'var value = 1;',
    'var other = value + 1;',
    '',
].join('\n');

// 将 initialize、didOpen、documentSymbol 与 shutdown/exit 串在一次输入中，
// 覆盖通知后的诊断发布、后续请求处理以及正常退出。
const payload = Buffer.concat([
    createMessage({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: { capabilities: {} },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: documentUri,
                languageId: 'zr',
                version: 1,
                text: documentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 2,
        method: 'textDocument/documentSymbol',
        params: {
            textDocument: { uri: documentUri },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 3,
        method: 'shutdown',
        params: {},
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'exit',
        params: {},
    }),
]);

// 同步运行使退出码和全部 stdout 可一并检查；超时限制损坏协议时的挂起时间。
const result = spawnSync(serverPath, [], {
    input: payload,
    encoding: 'utf8',
    timeout: 10000,
    windowsHide: true,
});

// BUG: 这里只检索 ID 子串；若 documentSymbol 返回 id=2 的错误信封，smoke 仍会通过，
// 从而漏掉符号查询回归。应按 Content-Length 解析响应并检查 id=2 的 result。
assert(result.status === 0, `Expected stdio server to exit cleanly, got status=${result.status} signal=${result.signal}`);
assert(typeof result.stdout === 'string' && result.stdout.includes('"method":"textDocument/publishDiagnostics"'),
    'Expected didOpen to publish diagnostics so live editor feedback stays in sync');
assert(typeof result.stdout === 'string' && result.stdout.includes('"id":2'),
    'Expected documentSymbol response after didOpen');
assert(result.stdout.includes('"id":3'),
    'Expected shutdown response after didOpen');
