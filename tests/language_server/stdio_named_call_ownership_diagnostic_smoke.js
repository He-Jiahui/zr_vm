const { spawnSync } = require('child_process');

// 独立 stdio 诊断探针：命名调用参数传递中的 Shared→Unique 错误应归于调用位置，且不生成机器修复。
// TODO: 当前 tests/CMakeLists.txt 与 tests/language_server/CMakeLists.txt 均未直接注册本脚本；
// 核查是否有其他正式验收入口，再决定把此回归接入哪一层自动测试。

/** 将协议和诊断契约失败传给 Node 进程状态，供手工或未来自动入口判定。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** 按 UTF-8 实际字节数生成 LSP 帧，供一次性 stdio 输入中的各请求共用。 */
function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

/** 从真实服务端 stdout 寻找诊断通知与请求响应。
 * BUG: Content-Length 计 UTF-8 字节，output 却已解码为 JS 字符串；响应含非 ASCII 文本且后续有帧时，
 *      以字节数推进字符串偏移会把下一帧前缀并入 JSON。双帧含中文消息的独立 Node 复现触发 SyntaxError。
 */
function parseMessages(output) {
    const messages = [];
    let offset = 0;

    while (offset < output.length) {
        const headerEnd = output.indexOf('\r\n\r\n', offset);
        if (headerEnd < 0) {
            break;
        }
        const header = output.slice(offset, headerEnd);
        const lengthMatch = /Content-Length: (\d+)/i.exec(header);
        assert(lengthMatch, `Malformed LSP header: ${header}`);
        const bodyStart = headerEnd + 4;
        const bodyEnd = bodyStart + Number(lengthMatch[1]);
        assert(bodyEnd <= output.length, 'Truncated LSP response body');
        messages.push(JSON.parse(output.slice(bodyStart, bodyEnd)));
        offset = bodyEnd;
    }
    return messages;
}

const serverPath = process.argv[2];
const serverArgs = process.argv.slice(3);
const documentUri = 'file:///zr-named-call-ownership-query-smoke.zr';
// 调用实参的所有权修饰符与形参冲突，诊断必须落在调用位置。
const documentText = [
    'resource class Resource {}',
    'fn consume(resource: Unique<Resource>) {}',
    'fn run(resource: Shared<Resource>) {',
    '    consume(resource);',
    '}',
    '',
].join('\n');

assert(serverPath, 'Expected stdio server executable path');

// initialize → didOpen → documentSymbol 请求 → shutdown/exit 构成有序协议探针；文档仅存在于编辑器覆盖层。
const payload = Buffer.concat([
    createMessage({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: { capabilities: {} },
    }),
    createMessage({ jsonrpc: '2.0', method: 'initialized', params: {} }),
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
        params: { textDocument: { uri: documentUri } },
    }),
    createMessage({ jsonrpc: '2.0', id: 3, method: 'shutdown', params: {} }),
    createMessage({ jsonrpc: '2.0', method: 'exit', params: {} }),
]);

// 向真实 stdio 服务端送入整条请求链，失败或超时必须阻止诊断断言继续。
const result = spawnSync(serverPath, serverArgs, {
    input: payload,
    encoding: 'utf8',
    timeout: 10000,
    windowsHide: true,
});
assert(result.status === 0,
    `Expected stdio server exit zero, got status=${result.status} signal=${result.signal}`);

// 仅接收本次打开文档的版本化 push 诊断，避免把请求响应当成诊断结果。
const publication = parseMessages(result.stdout).find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === documentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics));
assert(publication, 'Expected named-call ownership diagnostic publication');

// 按 canonical code 核对诊断目录的投影、范围与修复 disposition；不能用泛型错误替代。
const diagnostics = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'ownership_mismatch');
assert(diagnostics.length === 1,
    `Expected exactly one canonical ownership mismatch: ${JSON.stringify(publication.params.diagnostics)}`);

const diagnostic = diagnostics[0];
assert(diagnostic.range &&
    diagnostic.range.start.line === 3 &&
    diagnostic.range.start.character === 12 &&
    diagnostic.range.end.line === 3 &&
    diagnostic.range.end.character === 20,
`Expected the argument token range: ${JSON.stringify(diagnostic)}`);
assert(diagnostic.severity === 1 &&
    diagnostic.message ===
        'Ownership qualifier mismatch\n' +
        'Cause: Actual value has type Shared<Resource>, but the target requires Unique<Resource>.\n' +
        'Suggestion: Provide a Unique<Resource> value, use an ownership builtin, or change the target annotation to match.',
`Expected canonical ownership mismatch payload: ${JSON.stringify(diagnostic)}`);
assert(diagnostic.data && diagnostic.data.descriptorId === 2008 &&
    diagnostic.data.noFixReason === 'requires_user_decision',
`Expected descriptor 2008 and no-fix disposition: ${JSON.stringify(diagnostic)}`);
assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
    'Ownership mismatch diagnostics must not publish a machine fix');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
'Expected registered diagnostic help URI');
