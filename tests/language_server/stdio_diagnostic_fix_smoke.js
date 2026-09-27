const { spawnSync } = require('child_process');

// 断言每个跨 JSON-RPC 边界的回归契约；首个失配即终止此单进程 smoke。
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// 以 LSP Content-Length 帧发送 UTF-8 JSON，确保测试经过真实 stdio 协议解码而非直接调用 C 接口。
function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

// 从服务端 stdout 还原通知与请求响应，供后续按 URI、版本和诊断码验证序列化结果。
// BUG: LSP Content-Length 是 UTF-8 字节数，但此处对已解码 stdout 字符串切片；
// 用含“诊”的首帧接第二帧可复现 JSON.parse 错帧，新增非 ASCII 诊断时 smoke 会误失败。
// TODO: 尾部若缺完整帧头会直接退出；后续应校验已消费完整 stdout，并覆盖截断帧。
// 服务端退出状态先由调用方检查；修正时先按 Buffer 字节边界切帧再解码正文。
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

// CTest 提供当前构建出的 stdio 服务端路径；所有用例在同一进程内观察诊断发布与后续清除。
const serverPath = process.argv[2];
assert(serverPath, 'Expected stdio server executable path');

// 各场景使用独立 URI，避免同一增量解析缓存或旧版本通知污染另一项修复断言。
const documentUri = 'file:///zr-diagnostic-fix-smoke.zr';
const semicolonDocumentUri = 'file:///zr-diagnostic-semicolon-fix-smoke.zr';
const conditionDocumentUri = 'file:///zr-diagnostic-condition-close-fix-smoke.zr';
const indexDocumentUri = 'file:///zr-diagnostic-index-close-fix-smoke.zr';
const parameterListDocumentUri = 'file:///zr-diagnostic-parameter-list-close-fix-smoke.zr';
const callDocumentUri = 'file:///zr-diagnostic-call-close-fix-smoke.zr';
const groupDocumentUri = 'file:///zr-diagnostic-group-close-fix-smoke.zr';
const arrayDocumentUri = 'file:///zr-diagnostic-array-close-fix-smoke.zr';
const objectDocumentUri = 'file:///zr-diagnostic-object-close-fix-smoke.zr';
const objectComputedKeyDocumentUri =
    'file:///zr-diagnostic-object-computed-key-close-fix-smoke.zr';
const objectPropertyColonDocumentUri =
    'file:///zr-diagnostic-object-property-colon-fix-smoke.zr';
const objectPropertySeparatorDocumentUri =
    'file:///zr-diagnostic-object-property-separator-fix-smoke.zr';
const conditionalColonDocumentUri =
    'file:///zr-diagnostic-conditional-colon-fix-smoke.zr';
const conditionalConsequentDocumentUri =
    'file:///zr-diagnostic-conditional-consequent-fix-smoke.zr';
const conditionalAlternateDocumentUri =
    'file:///zr-diagnostic-conditional-alternate-fix-smoke.zr';
const conditionalWithoutAlternateDocumentUri =
    'file:///zr-diagnostic-conditional-without-alternate-fix-smoke.zr';
const arrayElementSeparatorDocumentUri =
    'file:///zr-diagnostic-array-element-separator-fix-smoke.zr';
const arrayElementAssignmentDocumentUri =
    'file:///zr-diagnostic-array-element-assignment-fix-smoke.zr';
const functionCallMismatchDocumentUri =
    'file:///zr-diagnostic-function-call-mismatch-smoke.zr';
const methodCallMismatchDocumentUri =
    'file:///zr-diagnostic-method-call-mismatch-smoke.zr';
const invalidCallableDecoratorDocumentUri =
    'file:///zr-diagnostic-invalid-callable-decorator-smoke.zr';
// 未初始化读取检验语义诊断描述符、帮助链接及需人工填值的占位修复能完整穿过 stdio。
const documentText = [
    'fn choose(flag: bool): int {',
    '    var seed: int;',
    '    if (flag) {',
    '        seed = 1;',
    '    }',
    '    return seed;',
    '}',
    '',
].join('\n');
// 部分缺标点样例配对 didOpen/didChange：第 2 版用预置修复后文本验证诊断消失；
// 无确定编辑的阴性样例仅检查第 1 版未提供自动修复。
const semicolonDocumentText = 'var answer = 42';
const conditionDocumentText = 'if (ready { return 1; }\n';
const indexDocumentText = 'return value[0;\n';
const parameterListDocumentText = 'fn pick(value: int: int { return value; }\n';
const callDocumentText = [
    'fn pick(value: int): int { return value; }',
    'return pick(1 + 2;',
    '',
].join('\n');
const groupDocumentText = 'return (1 + 2;\n';
const arrayDocumentText = 'return [1, 2';
const objectDocumentText = 'return {a: 1';
const objectComputedKeyDocumentText = 'return {[1: 2};';
const objectPropertyColonDocumentText = 'return {a 1};';
const objectPropertySeparatorDocumentText = 'return {a: 1 "b": 2};';
const conditionalColonDocumentText = 'return true ? 1 2;';
const conditionalConsequentDocumentText = 'return true ? : 2;';
const conditionalAlternateDocumentText = 'return true ? 1 : ;';
const conditionalWithoutAlternateDocumentText = 'return true ? 1;';
const arrayElementSeparatorDocumentText = 'return [1 2];';
const arrayElementAssignmentDocumentText = 'return [value = 1];';
// 类型失配核对关联位置与占位修复；无效装饰器核对“需用户决定”的无自动修复处置。
const functionCallMismatchDocumentText = [
    'fn pick(value: int): int { return value; }',
    'fn main(): int {',
    '    pick(2.5);',
    '    return 0;',
    '}',
    '',
].join('\n');
const methodCallMismatchDocumentText = [
    'class Meter {',
    '    pub fn write(value: int): int { return value; }',
    '}',
    'fn main(meter: Meter): int {',
    '    meter.write(2.5);',
    '    return 0;',
    '}',
    '',
].join('\n');
const invalidCallableDecoratorDocumentText = [
    'native extern("fixture") {',
    '    #zr.ffi.callconv(123)#',
    '    fn Bad(): void;',
    '}',
    '',
].join('\n');

// 每次文档通知后穿插 documentSymbol 请求，让服务端按同一会话顺序处理文档和后续请求；
// 最后按 shutdown/exit 握手关闭，使退出码也进入协议回归断言。
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
        params: { textDocument: { uri: documentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: semicolonDocumentUri,
                languageId: 'zr',
                version: 1,
                text: semicolonDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 3,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: semicolonDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: semicolonDocumentUri, version: 2 },
            contentChanges: [{ text: 'var answer = 42;' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 4,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: semicolonDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: conditionDocumentUri,
                languageId: 'zr',
                version: 1,
                text: conditionDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 5,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: conditionDocumentUri, version: 2 },
            contentChanges: [{ text: 'if (ready) { return 1; }\n' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 6,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: indexDocumentUri,
                languageId: 'zr',
                version: 1,
                text: indexDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 7,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: indexDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: indexDocumentUri, version: 2 },
            contentChanges: [{ text: 'return value[0];\n' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 8,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: indexDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: parameterListDocumentUri,
                languageId: 'zr',
                version: 1,
                text: parameterListDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 9,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: parameterListDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: parameterListDocumentUri, version: 2 },
            contentChanges: [{
                text: 'fn pick(value: int): int { return value; }\n',
            }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 10,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: parameterListDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: callDocumentUri,
                languageId: 'zr',
                version: 1,
                text: callDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 11,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: callDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: callDocumentUri, version: 2 },
            contentChanges: [{
                text: [
                    'fn pick(value: int): int { return value; }',
                    'return pick(1 + 2);',
                    '',
                ].join('\n'),
            }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 12,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: callDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: groupDocumentUri,
                languageId: 'zr',
                version: 1,
                text: groupDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 13,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: groupDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: groupDocumentUri, version: 2 },
            contentChanges: [{ text: 'return (1 + 2);\n' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 14,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: groupDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: arrayDocumentUri,
                languageId: 'zr',
                version: 1,
                text: arrayDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 15,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: arrayDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: arrayDocumentUri, version: 2 },
            contentChanges: [{ text: 'return [1, 2];' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 16,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: arrayDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: objectDocumentUri,
                languageId: 'zr',
                version: 1,
                text: objectDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 17,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: objectDocumentUri, version: 2 },
            contentChanges: [{ text: 'return {a: 1};' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 18,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: objectComputedKeyDocumentUri,
                languageId: 'zr',
                version: 1,
                text: objectComputedKeyDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 19,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectComputedKeyDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: objectComputedKeyDocumentUri, version: 2 },
            contentChanges: [{ text: 'return {[1]: 2};' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 20,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectComputedKeyDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: objectPropertyColonDocumentUri,
                languageId: 'zr',
                version: 1,
                text: objectPropertyColonDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 21,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectPropertyColonDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: objectPropertyColonDocumentUri, version: 2 },
            contentChanges: [{ text: 'return {a: 1};' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 22,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectPropertyColonDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: objectPropertySeparatorDocumentUri,
                languageId: 'zr',
                version: 1,
                text: objectPropertySeparatorDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 23,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectPropertySeparatorDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: objectPropertySeparatorDocumentUri, version: 2 },
            contentChanges: [{ text: 'return {a: 1, "b": 2};' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 24,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: objectPropertySeparatorDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: conditionalColonDocumentUri,
                languageId: 'zr',
                version: 1,
                text: conditionalColonDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 25,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionalColonDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: conditionalColonDocumentUri, version: 2 },
            contentChanges: [{ text: 'return true ? 1 : 2;' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 26,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionalColonDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: conditionalConsequentDocumentUri,
                languageId: 'zr',
                version: 1,
                text: conditionalConsequentDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 27,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionalConsequentDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: conditionalAlternateDocumentUri,
                languageId: 'zr',
                version: 1,
                text: conditionalAlternateDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 28,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionalAlternateDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: conditionalWithoutAlternateDocumentUri,
                languageId: 'zr',
                version: 1,
                text: conditionalWithoutAlternateDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 29,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: conditionalWithoutAlternateDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: arrayElementSeparatorDocumentUri,
                languageId: 'zr',
                version: 1,
                text: arrayElementSeparatorDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 30,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: arrayElementSeparatorDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didChange',
        params: {
            textDocument: { uri: arrayElementSeparatorDocumentUri, version: 2 },
            contentChanges: [{ text: 'return [1, 2];' }],
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 31,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: arrayElementSeparatorDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: arrayElementAssignmentDocumentUri,
                languageId: 'zr',
                version: 1,
                text: arrayElementAssignmentDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 32,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: arrayElementAssignmentDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: functionCallMismatchDocumentUri,
                languageId: 'zr',
                version: 1,
                text: functionCallMismatchDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 33,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: functionCallMismatchDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: methodCallMismatchDocumentUri,
                languageId: 'zr',
                version: 1,
                text: methodCallMismatchDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 34,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: methodCallMismatchDocumentUri } },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: invalidCallableDecoratorDocumentUri,
                languageId: 'zr',
                version: 1,
                text: invalidCallableDecoratorDocumentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 35,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: invalidCallableDecoratorDocumentUri } },
    }),
    createMessage({ jsonrpc: '2.0', id: 36, method: 'shutdown', params: {} }),
    createMessage({ jsonrpc: '2.0', method: 'exit', params: {} }),
]);

// 一次性输入保持消息顺序，测试真实启动、增量更新和干净退出，不依赖共享磁盘 fixture。
const result = spawnSync(serverPath, [], {
    input: payload,
    encoding: 'utf8',
    timeout: 10000,
    windowsHide: true,
});

assert(result.status === 0,
    `Expected stdio server to exit cleanly, got status=${result.status} signal=${result.signal}`);

// 响应顺序可与通知交错；以下断言以 URI、版本和诊断码定位目标发布。
const messages = parseMessages(result.stdout);
// 语义诊断保留注册表身份、帮助文档和占位修复的精确范围，供编辑器安全呈现代码操作。
const publication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === documentUri &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((diagnostic) =>
        diagnostic.code === 'possibly_uninitialized_read'));
assert(publication, 'Expected possibly_uninitialized_read publication');

const diagnostic = publication.params.diagnostics.find((entry) =>
    entry.code === 'possibly_uninitialized_read');
assert(diagnostic.data && diagnostic.data.descriptorId === 3003,
    'Expected the registered descriptorId to survive Diagnostic.data serialization');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected the registered diagnostic help URI to survive codeDescription serialization');
assert(!Object.prototype.hasOwnProperty.call(diagnostic.data, 'noFixReason'),
    'Expected a diagnostic with a typed fix to omit noFixReason');
assert(Array.isArray(diagnostic.data.fixes) && diagnostic.data.fixes.length === 1,
    'Expected one serialized diagnostic fix');

const fix = diagnostic.data.fixes[0];
assert(fix.title === 'Replace with an initialized value',
    'Expected serialized diagnostic fix title');
assert(fix.edit && fix.edit.newText === '<value>',
    'Expected serialized placeholder edit text');
assert(fix.applicability === 2,
    'Expected HAS_PLACEHOLDERS applicability');
assert(fix.edit.range.start.line === 5 && fix.edit.range.start.character === 11 &&
    fix.edit.range.end.line === 5 && fix.edit.range.end.character === 15,
    'Expected serialized fix range for seed read');

// EOF 分号修复必须是零宽插入；预置修复后文本的第 2 版不能沿用第 1 版的错误诊断。
const semicolonPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === semicolonDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_statement_semicolon'));
assert(semicolonPublication,
    'Expected EOF missing_statement_semicolon publication');

const semicolonDiagnostic = semicolonPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_statement_semicolon');
assert(semicolonDiagnostic.data &&
    Array.isArray(semicolonDiagnostic.data.fixes) &&
    semicolonDiagnostic.data.fixes.length === 1,
    'Expected one serialized semicolon diagnostic fix');

const semicolonFix = semicolonDiagnostic.data.fixes[0];
assert(semicolonFix.title === 'Insert missing semicolon' &&
    semicolonFix.applicability === 1 &&
    semicolonFix.edit &&
    semicolonFix.edit.newText === ';',
    'Expected a machine-applicable serialized semicolon edit');
assert(semicolonFix.edit.range.start.line === 0 &&
    semicolonFix.edit.range.start.character === 15 &&
    semicolonFix.edit.range.end.line === 0 &&
    semicolonFix.edit.range.end.character === 15,
    'Expected the semicolon edit at the previous token end');

const fixedSemicolonPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === semicolonDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedSemicolonPublication &&
    !fixedSemicolonPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_statement_semicolon'),
    'Expected the applied semicolon fix to clear the diagnostic');

// 条件右括号应插在块开始前，避免将修复放到 if 条件内部或行末。
const conditionPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === conditionDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_condition_close'));
assert(conditionPublication,
    'Expected missing_condition_close publication');

const conditionDiagnostic = conditionPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_condition_close');
assert(conditionDiagnostic.data &&
    Array.isArray(conditionDiagnostic.data.fixes) &&
    conditionDiagnostic.data.fixes.length === 1,
    'Expected one serialized condition-close diagnostic fix');

const conditionFix = conditionDiagnostic.data.fixes[0];
assert(conditionFix.title === "Insert missing ')'" &&
    conditionFix.applicability === 1 &&
    conditionFix.edit &&
    conditionFix.edit.newText === ')',
    'Expected a machine-applicable serialized condition-close edit');
assert(conditionFix.edit.range.start.line === 0 &&
    conditionFix.edit.range.start.character === 10 &&
    conditionFix.edit.range.end.line === 0 &&
    conditionFix.edit.range.end.character === 10,
    'Expected the condition-close edit before the block opener');

const fixedConditionPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === conditionDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedConditionPublication &&
    !fixedConditionPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_condition_close'),
    'Expected the applied condition-close fix to clear the diagnostic');

// 下标右括号位于语句终止符前；版本更新同时验证诊断清除。
const indexPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === indexDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_index_close'));
assert(indexPublication,
    'Expected missing_index_close publication');

const indexDiagnostic = indexPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_index_close');
assert(indexDiagnostic.data &&
    Array.isArray(indexDiagnostic.data.fixes) &&
    indexDiagnostic.data.fixes.length === 1,
    'Expected one serialized index-close diagnostic fix');

const indexFix = indexDiagnostic.data.fixes[0];
assert(indexFix.title === "Insert missing ']'" &&
    indexFix.applicability === 1 &&
    indexFix.edit &&
    indexFix.edit.newText === ']',
    'Expected a machine-applicable serialized index-close edit');
assert(indexFix.edit.range.start.line === 0 &&
    indexFix.edit.range.start.character === 14 &&
    indexFix.edit.range.end.line === 0 &&
    indexFix.edit.range.end.character === 14,
    'Expected the index-close edit before the statement terminator');

const fixedIndexPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === indexDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedIndexPublication &&
    !fixedIndexPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_index_close'),
    'Expected the applied index-close fix to clear the diagnostic');

// 形参列表右括号应在返回类型冒号之前，区分函数声明恢复与表达式恢复。
const parameterListPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === parameterListDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_parameter_list_close'));
assert(parameterListPublication,
    'Expected missing_parameter_list_close publication');

const parameterListDiagnostic = parameterListPublication.params.diagnostics.find(
    (entry) => entry.code === 'missing_parameter_list_close');
assert(parameterListDiagnostic.data &&
    Array.isArray(parameterListDiagnostic.data.fixes) &&
    parameterListDiagnostic.data.fixes.length === 1,
    'Expected one serialized parameter-list-close diagnostic fix');

const parameterListFix = parameterListDiagnostic.data.fixes[0];
assert(parameterListFix.title === "Insert missing ')'" &&
    parameterListFix.applicability === 1 &&
    parameterListFix.edit &&
    parameterListFix.edit.newText === ')',
    'Expected a machine-applicable serialized parameter-list-close edit');
assert(parameterListFix.edit.range.start.line === 0 &&
    parameterListFix.edit.range.start.character === 18 &&
    parameterListFix.edit.range.end.line === 0 &&
    parameterListFix.edit.range.end.character === 18,
    'Expected the parameter-list-close edit before the return-type colon');

const fixedParameterListPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === parameterListDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedParameterListPublication &&
    !fixedParameterListPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_parameter_list_close'),
    'Expected the applied parameter-list-close fix to clear the diagnostic');

// 调用表达式同时检查主诊断定位开括号、修复定位结束标记，两种范围不能混用。
const callPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === callDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_call_close'));
assert(callPublication,
    'Expected missing_call_close publication');

const callDiagnostic = callPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_call_close');
assert(callDiagnostic.range.start.line === 1 &&
    callDiagnostic.range.start.character === 11 &&
    callDiagnostic.range.end.line === 1 &&
    callDiagnostic.range.end.character === 12,
    'Expected the call-close primary range to remain on the opening parenthesis');
assert(callDiagnostic.data &&
    Array.isArray(callDiagnostic.data.fixes) &&
    callDiagnostic.data.fixes.length === 1,
    'Expected one serialized call-close diagnostic fix');

const callFix = callDiagnostic.data.fixes[0];
assert(callFix.title === "Insert missing ')'" &&
    callFix.applicability === 1 &&
    callFix.edit &&
    callFix.edit.newText === ')',
    'Expected a machine-applicable serialized call-close edit');
assert(callFix.edit.range.start.line === 1 &&
    callFix.edit.range.start.character === 17 &&
    callFix.edit.range.end.line === 1 &&
    callFix.edit.range.end.character === 17,
    'Expected the call-close edit before the statement terminator');

const fixedCallPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === callDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedCallPublication &&
    !fixedCallPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_call_close'),
    'Expected the applied call-close fix to clear the diagnostic');

// 分组表达式与调用共享右括号字符，但须归属不同诊断码和原始开括号。
const groupPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === groupDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_group_close'));
assert(groupPublication,
    'Expected missing_group_close publication');

const groupDiagnostic = groupPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_group_close');
assert(groupDiagnostic.range.start.line === 0 &&
    groupDiagnostic.range.start.character === 7 &&
    groupDiagnostic.range.end.line === 0 &&
    groupDiagnostic.range.end.character === 8,
    'Expected the group-close primary range to remain on the opening parenthesis');
assert(groupDiagnostic.data &&
    Array.isArray(groupDiagnostic.data.fixes) &&
    groupDiagnostic.data.fixes.length === 1,
    'Expected one serialized group-close diagnostic fix');

const groupFix = groupDiagnostic.data.fixes[0];
assert(groupFix.title === "Insert missing ')'" &&
    groupFix.applicability === 1 &&
    groupFix.edit &&
    groupFix.edit.newText === ')',
    'Expected a machine-applicable serialized group-close edit');
assert(groupFix.edit.range.start.line === 0 &&
    groupFix.edit.range.start.character === 13 &&
    groupFix.edit.range.end.line === 0 &&
    groupFix.edit.range.end.character === 13,
    'Expected the group-close edit before the statement terminator');

const fixedGroupPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === groupDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedGroupPublication &&
    !fixedGroupPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_group_close'),
    'Expected the applied group-close fix to clear the diagnostic');

// 数组在 EOF 缺少右方括号时可在末尾安全插入，并由下一版本验证诊断失效。
const arrayPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === arrayDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_array_close'));
assert(arrayPublication,
    'Expected missing_array_close publication');

const arrayDiagnostic = arrayPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_array_close');
assert(arrayDiagnostic.range.start.line === 0 &&
    arrayDiagnostic.range.start.character === 7 &&
    arrayDiagnostic.range.end.line === 0 &&
    arrayDiagnostic.range.end.character === 8,
    'Expected the array-close primary range to remain on the opening bracket');
assert(arrayDiagnostic.data &&
    Array.isArray(arrayDiagnostic.data.fixes) &&
    arrayDiagnostic.data.fixes.length === 1,
    'Expected one serialized array-close diagnostic fix');

const arrayFix = arrayDiagnostic.data.fixes[0];
assert(arrayFix.title === "Insert missing ']'" &&
    arrayFix.applicability === 1 &&
    arrayFix.edit &&
    arrayFix.edit.newText === ']',
    'Expected a machine-applicable serialized array-close edit');
assert(arrayFix.edit.range.start.line === 0 &&
    arrayFix.edit.range.start.character === 12 &&
    arrayFix.edit.range.end.line === 0 &&
    arrayFix.edit.range.end.character === 12,
    'Expected the array-close edit at end of file');

const fixedArrayPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === arrayDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedArrayPublication &&
    !fixedArrayPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_array_close'),
    'Expected the applied array-close fix to clear the diagnostic');

// 对象在 EOF 缺少右花括号时使用同样的增量修复链，保持对象专属诊断身份。
const objectPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_close'));
assert(objectPublication,
    'Expected missing_object_close publication');

const objectDiagnostic = objectPublication.params.diagnostics.find((entry) =>
    entry.code === 'missing_object_close');
assert(objectDiagnostic.range.start.line === 0 &&
    objectDiagnostic.range.start.character === 7 &&
    objectDiagnostic.range.end.line === 0 &&
    objectDiagnostic.range.end.character === 8,
    'Expected the object-close primary range to remain on the opening brace');
assert(objectDiagnostic.data &&
    Array.isArray(objectDiagnostic.data.fixes) &&
    objectDiagnostic.data.fixes.length === 1,
    'Expected one serialized object-close diagnostic fix');

const objectFix = objectDiagnostic.data.fixes[0];
assert(objectFix.title === "Insert missing '}'" &&
    objectFix.applicability === 1 &&
    objectFix.edit &&
    objectFix.edit.newText === '}',
    'Expected a machine-applicable serialized object-close edit');
assert(objectFix.edit.range.start.line === 0 &&
    objectFix.edit.range.start.character === 12 &&
    objectFix.edit.range.end.line === 0 &&
    objectFix.edit.range.end.character === 12,
    'Expected the object-close edit at end of file');

const fixedObjectPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedObjectPublication &&
    !fixedObjectPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_close'),
    'Expected the applied object-close fix to clear the diagnostic');

// 计算属性键的右方括号位于冒号之前；主范围仍需指向原开括号以便定位问题。
const objectComputedKeyPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectComputedKeyDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_computed_key_close'));
assert(objectComputedKeyPublication,
    'Expected missing_object_computed_key_close publication');

const objectComputedKeyDiagnostic =
    objectComputedKeyPublication.params.diagnostics.find((entry) =>
        entry.code === 'missing_object_computed_key_close');
assert(objectComputedKeyDiagnostic.range.start.line === 0 &&
    objectComputedKeyDiagnostic.range.start.character === 8 &&
    objectComputedKeyDiagnostic.range.end.line === 0 &&
    objectComputedKeyDiagnostic.range.end.character === 9,
    'Expected the computed-key close primary range on the opening bracket');
assert(objectComputedKeyDiagnostic.data &&
    Array.isArray(objectComputedKeyDiagnostic.data.fixes) &&
    objectComputedKeyDiagnostic.data.fixes.length === 1,
    'Expected one serialized computed-key close diagnostic fix');

const objectComputedKeyFix = objectComputedKeyDiagnostic.data.fixes[0];
assert(objectComputedKeyFix.title === "Insert missing ']'" &&
    objectComputedKeyFix.applicability === 1 &&
    objectComputedKeyFix.edit &&
    objectComputedKeyFix.edit.newText === ']',
    'Expected a machine-applicable serialized computed-key close edit');
assert(objectComputedKeyFix.edit.range.start.line === 0 &&
    objectComputedKeyFix.edit.range.start.character === 10 &&
    objectComputedKeyFix.edit.range.end.line === 0 &&
    objectComputedKeyFix.edit.range.end.character === 10,
    'Expected the computed-key close edit before the property colon');

const fixedObjectComputedKeyPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectComputedKeyDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedObjectComputedKeyPublication &&
    !fixedObjectComputedKeyPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_computed_key_close'),
    'Expected the applied computed-key close fix to clear the diagnostic');

// 属性名与值之间缺失冒号时，修复只插入标点而不替换值 token。
const objectPropertyColonPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectPropertyColonDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_property_colon'));
assert(objectPropertyColonPublication,
    'Expected missing_object_property_colon publication');

const objectPropertyColonDiagnostic =
    objectPropertyColonPublication.params.diagnostics.find((entry) =>
        entry.code === 'missing_object_property_colon');
assert(objectPropertyColonDiagnostic.range.start.line === 0 &&
    objectPropertyColonDiagnostic.range.start.character === 10 &&
    objectPropertyColonDiagnostic.range.end.line === 0 &&
    objectPropertyColonDiagnostic.range.end.character === 11,
    'Expected the property-colon primary range on the value token');
assert(objectPropertyColonDiagnostic.data &&
    Array.isArray(objectPropertyColonDiagnostic.data.fixes) &&
    objectPropertyColonDiagnostic.data.fixes.length === 1,
    'Expected one serialized property-colon diagnostic fix');

const objectPropertyColonFix = objectPropertyColonDiagnostic.data.fixes[0];
assert(objectPropertyColonFix.title === "Insert missing ':'" &&
    objectPropertyColonFix.applicability === 1 &&
    objectPropertyColonFix.edit &&
    objectPropertyColonFix.edit.newText === ':',
    'Expected a machine-applicable serialized property-colon edit');
assert(objectPropertyColonFix.edit.range.start.line === 0 &&
    objectPropertyColonFix.edit.range.start.character === 10 &&
    objectPropertyColonFix.edit.range.end.line === 0 &&
    objectPropertyColonFix.edit.range.end.character === 10,
    'Expected the property-colon edit before the value token');

const fixedObjectPropertyColonPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectPropertyColonDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedObjectPropertyColonPublication &&
    !fixedObjectPropertyColonPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_property_colon'),
    'Expected the applied property-colon fix to clear the diagnostic');

// 相邻对象属性缺逗号时，编辑锚在后一个键之前，避免触及前一属性的值。
const objectPropertySeparatorPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectPropertySeparatorDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_property_separator'));
assert(objectPropertySeparatorPublication,
    'Expected missing_object_property_separator publication');

const objectPropertySeparatorDiagnostic =
    objectPropertySeparatorPublication.params.diagnostics.find((entry) =>
        entry.code === 'missing_object_property_separator');
assert(objectPropertySeparatorDiagnostic.range.start.line === 0 &&
    objectPropertySeparatorDiagnostic.range.start.character === 13 &&
    objectPropertySeparatorDiagnostic.range.end.line === 0 &&
    objectPropertySeparatorDiagnostic.range.end.character === 16,
    'Expected the property-separator primary range on the next key token');
assert(objectPropertySeparatorDiagnostic.data &&
    Array.isArray(objectPropertySeparatorDiagnostic.data.fixes) &&
    objectPropertySeparatorDiagnostic.data.fixes.length === 1,
    'Expected one serialized property-separator diagnostic fix');

const objectPropertySeparatorFix =
    objectPropertySeparatorDiagnostic.data.fixes[0];
assert(objectPropertySeparatorFix.title === "Insert missing ','" &&
    objectPropertySeparatorFix.applicability === 1 &&
    objectPropertySeparatorFix.edit &&
    objectPropertySeparatorFix.edit.newText === ',',
    'Expected a machine-applicable serialized property-separator edit');
assert(objectPropertySeparatorFix.edit.range.start.line === 0 &&
    objectPropertySeparatorFix.edit.range.start.character === 13 &&
    objectPropertySeparatorFix.edit.range.end.line === 0 &&
    objectPropertySeparatorFix.edit.range.end.character === 13,
    'Expected the property-separator edit before the next key token');

const fixedObjectPropertySeparatorPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === objectPropertySeparatorDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedObjectPropertySeparatorPublication &&
    !fixedObjectPropertySeparatorPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_object_property_separator'),
    'Expected the applied property-separator fix to clear the diagnostic');

// 三元表达式的缺失冒号有确定插入点；下方无分支表达式的案例则不得提供盲目标点修复。
const conditionalColonPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === conditionalColonDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_conditional_colon'));
assert(conditionalColonPublication,
    'Expected missing_conditional_colon publication');

const conditionalColonDiagnostic =
    conditionalColonPublication.params.diagnostics.find((entry) =>
        entry.code === 'missing_conditional_colon');
assert(conditionalColonDiagnostic.range.start.line === 0 &&
    conditionalColonDiagnostic.range.start.character === 12 &&
    conditionalColonDiagnostic.range.end.line === 0 &&
    conditionalColonDiagnostic.range.end.character === 13,
    'Expected the conditional-colon primary range on the question token');
assert(conditionalColonDiagnostic.data &&
    Array.isArray(conditionalColonDiagnostic.data.fixes) &&
    conditionalColonDiagnostic.data.fixes.length === 1,
    'Expected one serialized conditional-colon diagnostic fix');

const conditionalColonFix = conditionalColonDiagnostic.data.fixes[0];
assert(conditionalColonFix.title === "Insert missing ':'" &&
    conditionalColonFix.applicability === 1 &&
    conditionalColonFix.edit &&
    conditionalColonFix.edit.newText === ':',
    'Expected a machine-applicable serialized conditional-colon edit');
assert(conditionalColonFix.edit.range.start.line === 0 &&
    conditionalColonFix.edit.range.start.character === 16 &&
    conditionalColonFix.edit.range.end.line === 0 &&
    conditionalColonFix.edit.range.end.character === 16,
    'Expected the conditional-colon edit before the alternate expression');

const fixedConditionalColonPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === conditionalColonDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedConditionalColonPublication &&
    !fixedConditionalColonPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_conditional_colon'),
    'Expected the applied conditional-colon fix to clear the diagnostic');

// 缺少 consequent/alternate 的三元表达式需要用户决定表达式内容，不发布可直接应用的修复。
for (const [uri, code] of [
    [conditionalConsequentDocumentUri, 'missing_conditional_consequent'],
    [conditionalAlternateDocumentUri, 'missing_conditional_alternate'],
    [conditionalWithoutAlternateDocumentUri, 'missing_conditional_colon'],
]) {
    const publication = messages.find((message) =>
        message.method === 'textDocument/publishDiagnostics' &&
        message.params &&
        message.params.uri === uri &&
        message.params.version === 1 &&
        Array.isArray(message.params.diagnostics) &&
        message.params.diagnostics.some((entry) => entry.code === code));
    assert(publication, `Expected ${code} publication`);
    const branchDiagnostic = publication.params.diagnostics.find(
        (entry) => entry.code === code);
    assert(!branchDiagnostic.data ||
        !Array.isArray(branchDiagnostic.data.fixes) ||
        branchDiagnostic.data.fixes.length === 0,
        `Expected ${code} to publish no machine-applicable punctuation fix`);
}

// 两个数组元素之间的缺失逗号可在后一元素前插入；第 2 版验证该错误被清除。
const arrayElementSeparatorPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === arrayElementSeparatorDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'missing_array_element_separator'));
assert(arrayElementSeparatorPublication,
    'Expected missing_array_element_separator publication');

const arrayElementSeparatorDiagnostic =
    arrayElementSeparatorPublication.params.diagnostics.find((entry) =>
        entry.code === 'missing_array_element_separator');
assert(arrayElementSeparatorDiagnostic.range.start.line === 0 &&
    arrayElementSeparatorDiagnostic.range.start.character === 10 &&
    arrayElementSeparatorDiagnostic.range.end.line === 0 &&
    arrayElementSeparatorDiagnostic.range.end.character === 11,
    'Expected the array-element-separator primary range on the next element');
assert(arrayElementSeparatorDiagnostic.data &&
    Array.isArray(arrayElementSeparatorDiagnostic.data.fixes) &&
    arrayElementSeparatorDiagnostic.data.fixes.length === 1,
    'Expected one serialized array-element-separator diagnostic fix');

const arrayElementSeparatorFix =
    arrayElementSeparatorDiagnostic.data.fixes[0];
assert(arrayElementSeparatorFix.title === "Insert missing ','" &&
    arrayElementSeparatorFix.applicability === 1 &&
    arrayElementSeparatorFix.edit &&
    arrayElementSeparatorFix.edit.newText === ',',
    'Expected a machine-applicable serialized array-element-separator edit');
assert(arrayElementSeparatorFix.edit.range.start.line === 0 &&
    arrayElementSeparatorFix.edit.range.start.character === 10 &&
    arrayElementSeparatorFix.edit.range.end.line === 0 &&
    arrayElementSeparatorFix.edit.range.end.character === 10,
    'Expected the array-element-separator edit before the next element');

const fixedArrayElementSeparatorPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === arrayElementSeparatorDocumentUri &&
    message.params.version === 2 &&
    Array.isArray(message.params.diagnostics));
assert(fixedArrayElementSeparatorPublication &&
    !fixedArrayElementSeparatorPublication.params.diagnostics.some((entry) =>
        entry.code === 'missing_array_element_separator'),
    'Expected the applied array-element-separator fix to clear the diagnostic');

// 数组元素中的赋值属于语义选择，必须保留 noFixReason 而不是猜测自动编辑。
const arrayElementAssignmentPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === arrayElementAssignmentDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((entry) =>
        entry.code === 'array_element_assignment'));
assert(arrayElementAssignmentPublication,
    'Expected array_element_assignment publication');
const arrayElementAssignmentDiagnostic =
    arrayElementAssignmentPublication.params.diagnostics.find((entry) =>
        entry.code === 'array_element_assignment');
assert(!arrayElementAssignmentDiagnostic.data ||
    !Array.isArray(arrayElementAssignmentDiagnostic.data.fixes) ||
    arrayElementAssignmentDiagnostic.data.fixes.length === 0,
    'Expected array_element_assignment to publish no machine-applicable fix');
assert(arrayElementAssignmentDiagnostic.data &&
    arrayElementAssignmentDiagnostic.data.noFixReason === 'requires_user_decision',
    'Expected array_element_assignment to publish its canonical no-fix reason');
assert(arrayElementAssignmentDiagnostic.codeDescription &&
    arrayElementAssignmentDiagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected array_element_assignment to publish its registered code description');

// 普通函数实参类型失配应只产生一条规范诊断，并指向实参与参数类型的两端。
const functionCallMismatchPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === functionCallMismatchDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics));
assert(functionCallMismatchPublication,
    'Expected function-call mismatch publication');
const functionCallMismatchDiagnostics =
    functionCallMismatchPublication.params.diagnostics.filter((entry) =>
        entry.code === 'type_mismatch');
assert(functionCallMismatchDiagnostics.length === 1,
    'Expected exactly one canonical function-call type mismatch');
const functionCallMismatchDiagnostic = functionCallMismatchDiagnostics[0];
assert(functionCallMismatchDiagnostic.range.start.line === 2 &&
    functionCallMismatchDiagnostic.range.start.character === 9 &&
    functionCallMismatchDiagnostic.range.end.line === 2 &&
    functionCallMismatchDiagnostic.range.end.character === 12,
    'Expected the function-call mismatch primary range on the argument');
assert(Array.isArray(functionCallMismatchDiagnostic.relatedInformation) &&
    functionCallMismatchDiagnostic.relatedInformation.length === 1 &&
    functionCallMismatchDiagnostic.relatedInformation[0].location &&
    functionCallMismatchDiagnostic.relatedInformation[0].location.range.start.line === 0 &&
    functionCallMismatchDiagnostic.relatedInformation[0].location.range.start.character === 15 &&
    functionCallMismatchDiagnostic.relatedInformation[0].location.range.end.line === 0 &&
    functionCallMismatchDiagnostic.relatedInformation[0].location.range.end.character === 18,
    'Expected the function-call mismatch relation on the parameter type');
assert(functionCallMismatchDiagnostic.data &&
    Array.isArray(functionCallMismatchDiagnostic.data.fixes) &&
    functionCallMismatchDiagnostic.data.fixes.length === 1,
    'Expected one canonical function-call mismatch fix');
const functionCallMismatchFix = functionCallMismatchDiagnostic.data.fixes[0];
assert(functionCallMismatchFix.applicability === 2 &&
    functionCallMismatchFix.edit &&
    functionCallMismatchFix.edit.newText === '<int> <expression>',
    'Expected the typed placeholder fix from the parser diagnostic fact');

// 方法调用沿相同事实投影链，另验证描述符身份与占位类型修复未在 stdio 层丢失。
const methodCallMismatchPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === methodCallMismatchDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics));
assert(methodCallMismatchPublication,
    'Expected method-call mismatch publication');
const methodCallMismatchDiagnostics =
    methodCallMismatchPublication.params.diagnostics.filter((entry) =>
        entry.code === 'type_mismatch');
assert(methodCallMismatchDiagnostics.length === 1,
    'Expected exactly one canonical method-call type mismatch');
const methodCallMismatchDiagnostic = methodCallMismatchDiagnostics[0];
assert(methodCallMismatchDiagnostic.range.start.line === 4 &&
    methodCallMismatchDiagnostic.range.start.character === 16 &&
    methodCallMismatchDiagnostic.range.end.line === 4 &&
    methodCallMismatchDiagnostic.range.end.character === 19,
    'Expected the method-call mismatch primary range on the argument');
assert(Array.isArray(methodCallMismatchDiagnostic.relatedInformation) &&
    methodCallMismatchDiagnostic.relatedInformation.length === 1 &&
    methodCallMismatchDiagnostic.relatedInformation[0].location &&
    methodCallMismatchDiagnostic.relatedInformation[0].location.range.start.line === 1 &&
    methodCallMismatchDiagnostic.relatedInformation[0].location.range.start.character === 24 &&
    methodCallMismatchDiagnostic.relatedInformation[0].location.range.end.line === 1 &&
    methodCallMismatchDiagnostic.relatedInformation[0].location.range.end.character === 27,
    'Expected the method-call mismatch relation on the parameter type');
assert(methodCallMismatchDiagnostic.data &&
    methodCallMismatchDiagnostic.data.descriptorId === 2011 &&
    Array.isArray(methodCallMismatchDiagnostic.data.fixes) &&
    methodCallMismatchDiagnostic.data.fixes.length === 1,
    'Expected one registered method-call mismatch fix');
const methodCallMismatchFix = methodCallMismatchDiagnostic.data.fixes[0];
assert(methodCallMismatchFix.applicability === 2 &&
    methodCallMismatchFix.edit &&
    methodCallMismatchFix.edit.newText === '<int> <expression>',
    'Expected the method-call typed placeholder from the parser query fact');

// FFI 调用约定装饰器参数无效时，保留精确装饰器范围和“需用户决定”的无修复处置。
const invalidCallableDecoratorPublication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === invalidCallableDecoratorDocumentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics));
assert(invalidCallableDecoratorPublication,
    'Expected invalid callable decorator publication');
const invalidCallableDecoratorDiagnostics =
    invalidCallableDecoratorPublication.params.diagnostics.filter((entry) =>
        entry.code === 'invalid_decorator');
assert(invalidCallableDecoratorDiagnostics.length === 1,
    'Expected exactly one canonical invalid callable decorator diagnostic');
const invalidCallableDecoratorDiagnostic = invalidCallableDecoratorDiagnostics[0];
assert(invalidCallableDecoratorDiagnostic.range.start.line === 1 &&
    invalidCallableDecoratorDiagnostic.range.start.character === 4 &&
    invalidCallableDecoratorDiagnostic.range.end.line === 1 &&
    invalidCallableDecoratorDiagnostic.range.end.character === 26,
    `Expected the invalid callable decorator range on the exact decorator, got ${JSON.stringify(invalidCallableDecoratorDiagnostic.range)}`);
assert(invalidCallableDecoratorDiagnostic.data &&
    invalidCallableDecoratorDiagnostic.data.descriptorId === 2019 &&
    invalidCallableDecoratorDiagnostic.data.noFixReason === 'requires_user_decision' &&
    (!Array.isArray(invalidCallableDecoratorDiagnostic.data.fixes) ||
        invalidCallableDecoratorDiagnostic.data.fixes.length === 0),
    'Expected the parser-owned invalid decorator disposition without a fix');
assert(invalidCallableDecoratorDiagnostic.codeDescription &&
    invalidCallableDecoratorDiagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected the registered invalid decorator code description');
