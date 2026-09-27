const { spawnSync } = require('child_process');

// 独立 stdio 诊断探针：接口 const 字段约束在实现类的两种违规位置产生关联到接口声明的 canonical 诊断。
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

/** 将每个实现类违规点限定到具体字段标识符，避免诊断退化到整个类型声明。 */
function assertRange(range, startLine, startCharacter, endLine, endCharacter,
    label) {
    assert(range &&
        range.start.line === startLine &&
        range.start.character === startCharacter &&
        range.end.line === endLine &&
        range.end.character === endCharacter,
    `${label}: ${JSON.stringify(range)}`);
}

/** 共享两个违规位置的 canonical 契约：等级、建议、接口声明关联和无自动修复。 */
function assertCanonicalDiagnostic(diagnostic, expectedRange, suggestion) {
    assert(diagnostic.severity === 1 &&
        diagnostic.message ===
            "Interface const field 'version' must remain const in implementing class\n" +
            'Cause: The interface requires this field to preserve const access in every implementation.\n' +
            `Suggestion: ${suggestion}`,
    `Expected canonical severity and message: ${JSON.stringify(diagnostic)}`);
    assertRange(diagnostic.range, ...expectedRange, 'Unexpected primary range');
    assert(Array.isArray(diagnostic.relatedInformation) &&
        diagnostic.relatedInformation.length === 1 &&
        diagnostic.relatedInformation[0].location &&
        diagnostic.relatedInformation[0].location.uri === documentUri,
    `Expected one related interface declaration: ${JSON.stringify(diagnostic)}`);
    assertRange(diagnostic.relatedInformation[0].location.range,
        1, 14, 1, 21, 'Unexpected related interface field range');
    assert(diagnostic.relatedInformation[0].message ===
        'Const field is required by this interface declaration',
    'Expected canonical related-information message');
    assert(diagnostic.data && diagnostic.data.descriptorId === 2014 &&
        diagnostic.data.noFixReason === 'requires_user_decision',
    `Expected descriptor 2014 and no-fix disposition: ${JSON.stringify(diagnostic)}`);
    assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
        'Interface const-field diagnostics must not publish a machine fix');
    assert(diagnostic.codeDescription &&
        diagnostic.codeDescription.href ===
            'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected registered diagnostic help URI');
}

const serverPath = process.argv[2];
const serverArgs = process.argv.slice(3);
const documentUri = 'file:///zr-interface-const-field-query-smoke.zr';
// 接口声明与实现类的两个违规用法一起入文档，供主范围及 relatedInformation 对照。
const documentText = [
    'interface Versioned {',
    '    pub const version: int;',
    '}',
    'class MutableVersion: Versioned {',
    '    pub var version: int;',
    '}',
    'class MissingVersion: Versioned {',
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
assert(publication, 'Expected interface const-field diagnostic publication');

// 按 canonical code 核对诊断目录的投影、范围与修复 disposition；不能用泛型错误替代。
const canonical = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'const_interface_mismatch');
assert(canonical.length === 2,
    `Expected exactly two canonical diagnostics: ${JSON.stringify(publication.params.diagnostics)}`);

// TODO: 这里按发布顺序对应两类违规；若服务端诊断顺序不是契约，需按主范围匹配以避免顺序变化造成假失败。
assertCanonicalDiagnostic(
    canonical[0], [4, 12, 4, 19], 'Mark the implementing field const.');
assertCanonicalDiagnostic(
    canonical[1], [6, 6, 6, 20],
    'Declare a const field with the required name and type.');
