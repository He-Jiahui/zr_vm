const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');
const { expectedCapabilities } = require('./stdio_capability_snapshot');

const REQUEST_TIMEOUT_MS = 10000;
const INLINE_METHOD = 'textDocument/inlineCompletion';
const RANGES_METHOD = 'textDocument/rangesFormatting';
const FORMAT_URI = 'file:///optional-capabilities-format.zr';
const INLINE_URI = 'file:///optional-capabilities-inline.zr';
const FORMAT_TEXT = 'fn first(): int {\nreturn 1;\n}\nfn second(): int {\nreturn 2;\n}\n';
const INLINE_TEXT = 'fn main(): int {\n    ret\n}\n';
// 两个不相邻的函数体共用格式化样例：单范围与多范围请求须命中同一组预期编辑。
const RANGES = [1, 4].map((line) => ({
    start: { line, character: 0 }, end: { line, character: 9 },
}));
const EXPECTED_EDITS = [1, 4].map((line, index) => ({
    range: { start: { line, character: 0 }, end: { line: line + 1, character: 0 } },
    newText: `    return ${index + 1};\n`,
}));

// 用最小的客户端声明构造协商输入，让用例分别控制两个可选方法的门禁。
function clientCapabilities(inlineCompletion, rangesFormatting) {
    const textDocument = {};
    if (inlineCompletion) textDocument.inlineCompletion = {};
    if (rangesFormatting) textDocument.rangeFormatting = { rangesSupport: true };
    return { textDocument };
}

// 此组用例包含进程启动、诊断及请求往返，统一放宽共享客户端的默认短超时。
function request(client, method, params, id) {
    return client.request(method, params, id, REQUEST_TIMEOUT_MS);
}

// 与服务端发出的 JSON-RPC 错误信封逐字段比较，防止错误码正确但响应形状退化。
function errorEnvelope(id, code, message) {
    return { jsonrpc: '2.0', id, error: { code, message } };
}

// 每个矩阵项独占服务端会话；成功路径验证 shutdown/exit，失败路径仍回收子进程。
async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;
    try {
        await run(client);
        assert.deepEqual(await request(client, 'shutdown', undefined, 'shutdown'), {
            jsonrpc: '2.0', id: 'shutdown', result: null,
        });
        client.notify('exit');
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0, client.stderr());
        assert.equal(client.stderr().trim(), '', 'stdio stderr must remain empty');
        cleanExit = true;
    } finally {
        if (!cleanExit) await client.terminate();
    }
}

// 完成 initialize 握手并核对信封，再发送 initialized，使后续文档和方法请求处于有效会话。
async function initialize(client, capabilities) {
    const response = await request(client, 'initialize', { capabilities }, 'initialize');
    assert.equal(response.jsonrpc, '2.0');
    assert.equal(response.id, 'initialize');
    assert.equal(response.error, undefined);
    assert.ok(response.result && response.result.capabilities);
    assert.deepEqual(Object.keys(response).sort(), ['id', 'jsonrpc', 'result']);
    assert.deepEqual(Object.keys(response.result).sort(), ['capabilities', 'serverInfo']);
    assert.equal(response.result.serverInfo.name, 'zr_vm_language_server_stdio');
    assert.equal(typeof response.result.serverInfo.version, 'string');
    client.notify('initialized', {});
    return response.result.capabilities;
}

// 以 publishDiagnostics 作为 didOpen 已被处理的屏障，避免随后请求与文档建立竞态。
async function open(client, uri, text) {
    client.notify('textDocument/didOpen', { textDocument: { uri, languageId: 'zr', version: 1, text } });
    const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics', REQUEST_TIMEOUT_MS);
    assert.equal(diagnostics.uri, uri);
    assert.equal(diagnostics.version, 1);
    if (uri === FORMAT_URI) assert.deepEqual(diagnostics.diagnostics, []);
}

// 对同一批已打开文档分别探测两个可选方法：协商成功须返回精确内容，未协商须拒绝分派。
async function checkMethods(client, inlineCompletion, rangesFormatting) {
    const inline = await request(client, INLINE_METHOD, {
        textDocument: { uri: INLINE_URI }, position: { line: 1, character: 7 }, context: { triggerKind: 1 },
    }, 'inline');
    if (inlineCompletion) {
        assert.deepEqual(inline, { jsonrpc: '2.0', id: 'inline', result: [{
            insertText: 'return ', filterText: 'return',
            range: { start: { line: 1, character: 4 }, end: { line: 1, character: 7 } },
        }] }, 'inline completion must provide the exact return replacement');
    } else {
        assert.deepEqual(inline, errorEnvelope('inline', -32601, 'Method not found'));
    }
    const ranges = await request(client, RANGES_METHOD, {
        textDocument: { uri: FORMAT_URI }, ranges: RANGES, options: { tabSize: 4, insertSpaces: true },
    }, 'ranges');
    assert.deepEqual(ranges, rangesFormatting
        ? { jsonrpc: '2.0', id: 'ranges', result: EXPECTED_EDITS }
        : errorEnvelope('ranges', -32601, 'Method not found'));
}

// 协商可选方法不应关闭已有的单范围格式化和普通补全；两者还要保留实际编辑及可见符号。
async function checkOrdinaryMethods(client) {
    const range = await request(client, 'textDocument/rangeFormatting', {
        textDocument: { uri: FORMAT_URI }, range: RANGES[0], options: { tabSize: 4, insertSpaces: true },
    }, 'ordinary-range');
    assert.deepEqual(range, { jsonrpc: '2.0', id: 'ordinary-range', result: [EXPECTED_EDITS[0]] });
    const completion = await request(client, 'textDocument/completion', {
        textDocument: { uri: FORMAT_URI }, position: { line: 4, character: 0 },
    }, 'ordinary-completion');
    assert.equal(completion.error, undefined);
    assert.ok(Array.isArray(completion.result));
    assert.deepEqual(completion.result.map((item) => item.label).sort(), ['first', 'second'],
                     'ordinary completion must retain both visible function declarations');
    const first = completion.result.find((item) => item.label === 'first');
    assert.deepEqual(first.textEdit, {
        range: { start: { line: 4, character: 0 }, end: { line: 4, character: 0 } }, newText: 'first',
    });
}

// CTest 传入 stdio 服务端路径；逐项创建新进程，避免前一会话的能力状态掩盖协商差异。
async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_optional_capabilities_smoke.js <stdio-server>');
    const tests = [];
    // 正例覆盖独立及组合启用、动态注册声明和未知扩展字段；负例保证畸形字段只失效对应可选能力。
    const matrix = [
        ['3.17 empty client', {}, false, false],
        ['inline completion only', clientCapabilities(true, false), true, false],
        ['multi-range formatting only', clientCapabilities(false, true), false, true],
        ['both optional providers', clientCapabilities(true, true), true, true],
        ['static inline registration', { textDocument: { inlineCompletion: { dynamicRegistration: false } } }, true, false],
        ['dynamic registration with optional providers', { textDocument: {
            inlineCompletion: { dynamicRegistration: true },
            rangeFormatting: { dynamicRegistration: true, rangesSupport: true },
        } }, true, true],
        ['unknown properties inside valid capabilities', { textDocument: {
            inlineCompletion: { futureProperty: true },
            rangeFormatting: { dynamicRegistration: false, rangesSupport: true, futureProperty: [] },
        } }, true, true],
        ['new server resets optional providers', {}, false, false],
        ['unknown capability paths', { experimental: { inlineCompletion: {}, rangesSupport: true } }, false, false],
        ['null client capabilities', null, false, false],
        ['nonobject client capabilities', true, false, false],
        ['array textDocument capabilities', { textDocument: [] }, false, false],
        ['false optional fields', { textDocument: { inlineCompletion: false, rangeFormatting: { rangesSupport: false } } }, false, false],
        ['boolean inline and nonobject range', { textDocument: { inlineCompletion: true, rangeFormatting: true } }, false, false],
        ['array inline and string ranges flag', { textDocument: { inlineCompletion: [], rangeFormatting: { rangesSupport: 'true' } } }, false, false],
        ['null inline and numeric ranges flag', { textDocument: { inlineCompletion: null, rangeFormatting: { rangesSupport: 1 } } }, false, false],
        ['string inline and object ranges flag', { textDocument: { inlineCompletion: 'enabled', rangeFormatting: { rangesSupport: {} } } }, false, false],
        ['string dynamic registration', { textDocument: {
            inlineCompletion: { dynamicRegistration: 'false' },
            rangeFormatting: { dynamicRegistration: 'true', rangesSupport: true },
        } }, false, false],
        ['null dynamic registration', { textDocument: {
            inlineCompletion: { dynamicRegistration: null },
            rangeFormatting: { dynamicRegistration: null, rangesSupport: true },
        } }, false, false],
        ['numeric dynamic registration', { textDocument: {
            inlineCompletion: { dynamicRegistration: 0 },
            rangeFormatting: { dynamicRegistration: 1, rangesSupport: true },
        } }, false, false],
        ['structured dynamic registration', { textDocument: {
            inlineCompletion: { dynamicRegistration: {} },
            rangeFormatting: { dynamicRegistration: [], rangesSupport: true },
        } }, false, false],
    ];
    // 完整能力快照由测试辅助工具独立提供，不能从服务端注册表反推预期值。
    for (const [name, capabilities, inlineCompletion, rangesFormatting] of matrix) {
        tests.push([name, async () => withClient(serverPath, async (client) => {
            assert.deepEqual(await initialize(client, capabilities),
                             expectedCapabilities(inlineCompletion, rangesFormatting));
        })]);
    }
    // 四种协商组合都发真实请求；非法的第二次 initialize 不得改变首次协商的分派结果。
    for (const [inlineCompletion, rangesFormatting] of [[false, false], [true, false], [false, true], [true, true]]) {
        tests.push([`requests inline=${inlineCompletion} ranges=${rangesFormatting}`, async () =>
            withClient(serverPath, async (client) => {
                await initialize(client, clientCapabilities(inlineCompletion, rangesFormatting));
                await open(client, FORMAT_URI, FORMAT_TEXT);
                await open(client, INLINE_URI, INLINE_TEXT);
                await checkMethods(client, inlineCompletion, rangesFormatting);
                await checkOrdinaryMethods(client);
                const repeated = await request(client, 'initialize', {
                    capabilities: clientCapabilities(!inlineCompletion, !rangesFormatting),
                }, 'reinitialize');
                assert.deepEqual(repeated, errorEnvelope('reinitialize', -32600, 'Invalid Request'));
                await checkMethods(client, inlineCompletion, rangesFormatting);
            })]);
    }
    let failures = 0;
    // 保留所有场景的失败明细，最终再让 CTest 用非零退出状态判定整组失败。
    for (const [name, run] of tests) {
        try {
            await run();
            console.log(`Pass - ${name}`);
        } catch (error) {
            failures++;
            console.error(`Fail - ${name}\n${error.stack || String(error)}`);
        }
    }
    assert.equal(failures, 0, `${failures}/${tests.length} optional capability checks failed`);
    console.log(`Pass - ${tests.length}/${tests.length} optional capability checks`);
}

// 入口异常也必须使 CTest 失败，不能把服务进程或协议错误当作通过。
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
