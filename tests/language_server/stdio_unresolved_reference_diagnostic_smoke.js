const { spawnSync } = require('child_process');

// 只有真正缺失的具名成员应发布 member_not_found；计算索引另有类型诊断。

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

/** Content-Length 以字节为单位；只在截出完整消息后解码。 */
function parseMessages(output) {
    const messages = [];
    let offset = 0;

    while (offset < output.length) {
        const headerEnd = output.indexOf('\r\n\r\n', offset);
        if (headerEnd < 0) {
            break;
        }
        const header = output.subarray(offset, headerEnd).toString('ascii');
        const lengthMatch = /Content-Length: (\d+)/i.exec(header);
        assert(lengthMatch, `Malformed LSP header: ${header}`);
        const bodyStart = headerEnd + 4;
        const bodyEnd = bodyStart + Number(lengthMatch[1]);
        assert(bodyEnd <= output.length, 'Truncated LSP response body');
        messages.push(JSON.parse(output.subarray(bodyStart, bodyEnd).toString('utf8')));
        offset = bodyEnd;
    }
    return messages;
}

/** 约束未解析成员诊断只覆盖成员名称，保护编辑器高亮的精确性。 */
function assertRange(range, startLine, startCharacter, endLine, endCharacter) {
    assert(range &&
        range.start.line === startLine &&
        range.start.character === startCharacter &&
        range.end.line === endLine &&
        range.end.character === endCharacter,
    `Unexpected diagnostic range: ${JSON.stringify(range)}`);
}

const serverPath = process.argv[2];
const serverArgs = process.argv.slice(3);
const documentUri = 'file:///zr-unresolved-reference-query-smoke.zr';
// 字段访问中只让成员名不存在，以防诊断跨度扩大到接收者或整个表达式。
const documentText = [
    'class Meter {',
    '    var value: int;',
    '}',
    'fn read(meter: Meter): int {',
    '    return meter.missingField;',
    '}',
    'let container = import("zr.container");',
    'fn readIndex(values: container.Array<int>): int {',
    '    var index = 0;',
    '    values[index] = 2;',
    '    return values[index];',
    '}',
    'fn wrongIndex(): int {',
    '    var values = [1, 2];',
    '    let badIndex = "name";',
    '    return values[badIndex];',
    '}',
    'fn useMissingIndex(values: container.Array<int>): int {',
    '    values[missingIndex] = 2;',
    '    return values[missingIndex];',
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
assert(publication, 'Expected unresolved-reference diagnostic publication');

const indexErrors = publication.params.diagnostics.filter((item) =>
    item.severity === 1 && item.range.start.line >= 6 && item.range.start.line <= 11);
assert(indexErrors.length === 0,
    `Expected the native array index read and write to be diagnosed as valid: ${JSON.stringify(indexErrors)}`);
const wrongIndex = publication.params.diagnostics.filter((item) => item.code === 'array_index_type_mismatch');
assert(wrongIndex.length === 1 && wrongIndex[0].range.start.line === 15,
    `Expected one real array index type mismatch: ${JSON.stringify(publication.params.diagnostics)}`);
const missingKeys = publication.params.diagnostics.filter((item) => item.code === 'unresolved_reference')
    .sort((left, right) => left.range.start.line - right.range.start.line);
assert(missingKeys.length === 2 && missingKeys.every((item) => item.severity === 1 &&
    item.message.startsWith("Reference 'missingIndex' could not be resolved")),
    `Expected unresolved variable diagnostics for both missing keys: ${JSON.stringify(publication.params.diagnostics)}`);
assertRange(missingKeys[0].range, 18, 11, 18, 23);
assertRange(missingKeys[1].range, 19, 18, 19, 30);

// 按 canonical code 核对诊断目录的投影、范围与修复 disposition；不能用泛型错误替代。
const diagnostics = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'member_not_found');
assert(diagnostics.length === 1,
    `Expected exactly one canonical member diagnostic: ${JSON.stringify(publication.params.diagnostics)}`);

const diagnostic = diagnostics[0];
const memberRangeDiagnostics = publication.params.diagnostics.filter((item) =>
    item.range &&
    item.range.start.line === 4 &&
    item.range.start.character === 17 &&
    item.range.end.line === 4 &&
    item.range.end.character === 29);
assert(memberRangeDiagnostics.length === 1 &&
    memberRangeDiagnostics[0].code === 'member_not_found',
`Expected the member token to have one canonical diagnostic: ${JSON.stringify(publication.params.diagnostics)}`);
assert(diagnostic.severity === 1 &&
    diagnostic.message ===
        "Member 'missingField' could not be resolved\n" +
        'Cause: Canonical member binding found no field, property, or method for this reference.\n' +
        "Suggestion: Declare the member or use a member exposed by the receiver's canonical type.",
`Expected canonical member diagnostic: ${JSON.stringify(diagnostic)}`);
assertRange(diagnostic.range, 4, 17, 4, 29);
assert(diagnostic.data && diagnostic.data.descriptorId === 2016 &&
    diagnostic.data.noFixReason === 'requires_user_decision',
`Expected descriptor 2016 and no-fix disposition: ${JSON.stringify(diagnostic)}`);
assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
    'Unresolved member diagnostics must not publish a machine fix');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
'Expected registered diagnostic help URI');
