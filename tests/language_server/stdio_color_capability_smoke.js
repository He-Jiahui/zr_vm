const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');

// 请求统一等待 10 秒；文本与注释中的十六进制色值用于验证未实现颜色协议时的普通语言语义。
const REQUEST_TIMEOUT_MS = 10000;
const URI = 'file:///stdio-color-capability.zr';
const TEXT = [
    'var accent = "#336699";',
    '// #112233 accent',
    '/* #445566 accent */',
    'var ordinary = accent;',
    'var count = 7;',
    'var copied = count;',
    '',
].join('\n');

// 用 LSP UTF-16 位置构造预期范围，作为 hover、definition 与颜色请求的共用坐标。
function range(line, start, end) {
    return { start: { line, character: start }, end: { line, character: end } };
}

// 保留响应 envelope 和请求 ID，避免只比较 result 而漏掉 JSON-RPC 契约退化。
function response(id, result) {
    return { jsonrpc: '2.0', id, result };
}

// 未注册颜色方法必须返回标准 MethodNotFound，而非空颜色结果。
function methodNotFound(id) {
    return { jsonrpc: '2.0', id, error: { code: -32601, message: 'Method not found' } };
}

// 断言普通字符串和整数悬停仍按语言语义返回精确范围。
function hover(value, hoverRange) {
    return { contents: { kind: 'markdown', value }, range: hoverRange };
}

// 每项断言用独立 ID 与统一超时，便于定位哪种能力/方法发生回归。
function request(client, method, params, id) {
    return client.request(method, params, id, REQUEST_TIMEOUT_MS);
}

// 一个能力档案对应一个服务端进程；正常路径执行 shutdown/exit，失败路径强制回收。
async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;
    try {
        await run(client);
        assert.deepEqual(await request(client, 'shutdown', undefined, 'shutdown'), response('shutdown', null));
        client.notify('exit');
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0, client.stderr());
        assert.equal(client.stderr().trim(), '', 'stdio stderr must remain empty');
        cleanExit = true;
    } finally {
        if (!cleanExit) await client.terminate();
    }
}

// 同时验证未声明与显式声明颜色客户端能力：服务端均不广告、也不处理颜色方法。
// 打开含颜色文本的有效文档后继续核对 hover/definition，防止禁用颜色误伤基础语义。
async function checkProfile(client, capabilities, check) {
    const initialized = await request(client, 'initialize', { capabilities }, 'initialize');
    assert.equal(initialized.jsonrpc, '2.0');
    assert.equal(initialized.id, 'initialize');
    assert.equal(initialized.error, undefined);
    assert.ok(initialized.result && initialized.result.capabilities);
    client.notify('initialized', {});
    client.notify('textDocument/didOpen', {
        textDocument: { uri: URI, languageId: 'zr', version: 1, text: TEXT },
    });
    // 文档发布先于后续查询，避免把尚未完成解析误判为颜色能力问题。
    const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics', REQUEST_TIMEOUT_MS);
    await check('valid string and comment source', async () => {
        assert.deepEqual(diagnostics, { uri: URI, version: 1, diagnostics: [] });
    });
    await check('colorProvider is absent', async () => {
        assert.equal(Object.prototype.hasOwnProperty.call(initialized.result.capabilities, 'colorProvider'), false,
                     'untyped colorProvider must be absent from initialize');
    });
    await check('documentColor is MethodNotFound', async () => {
        const id = 'document-color';
        assert.deepEqual(await request(client, 'textDocument/documentColor', { textDocument: { uri: URI } }, id),
                         methodNotFound(id));
    });
    // 字符串、注释及普通标识符位置都不能让未注册的颜色方法意外生效。
    for (const [name, selectedRange] of [
        ['string', range(0, 14, 21)],
        ['line comment', range(1, 3, 10)],
        ['block comment', range(2, 3, 10)],
        ['ordinary identifier', range(4, 4, 9)],
    ]) {
        await check(`colorPresentation for ${name} is MethodNotFound`, async () => {
            const id = `color-presentation-${name}`;
            assert.deepEqual(await request(client, 'textDocument/colorPresentation', {
                textDocument: { uri: URI }, color: { red: 0.2, green: 0.4, blue: 0.6, alpha: 1 },
                range: selectedRange,
            }, id), methodNotFound(id));
        });
    }
    // 颜色协议的负面边界之外，保留字符串、数值、定义及注释位置的正面语义断言。
    const queries = [
        ['hex text remains an ordinary string literal', 'textDocument/hover', { line: 0, character: 16 },
            hover('**expression**\n\nType: string\n\nExpression: literal exact\n\nConstant: "#336699"',
                  range(0, 13, 22))],
        ['string variable hover retains its exact type and range', 'textDocument/hover', { line: 3, character: 17 },
            hover('**variable**: accent\n\nResolved Type: string\n\nExpression: identifier exact\n\n' +
                  'Reference: read\n\nSymbol: accent\n\nDeclared at: 1:5', range(3, 15, 21))],
        ['string variable definition retains its exact target', 'textDocument/definition', { line: 3, character: 17 },
            [{ uri: URI, range: range(0, 4, 10) }]],
        ['ordinary identifier hover retains its value and type', 'textDocument/hover', { line: 5, character: 15 },
            hover('**variable**: count\n\nResolved Type: int\n\nExpression: identifier exact\n\n' +
                  'Numeric range: 7..7\n\nUnsigned range: 7..7\n\nReference: read\n\n' +
                  'Symbol: count\n\nDeclared at: 5:5', range(5, 13, 18))],
        ['ordinary identifier definition retains its exact target', 'textDocument/definition', { line: 5, character: 15 },
            [{ uri: URI, range: range(4, 4, 9) }]],
        ['line comment does not become a symbol reference', 'textDocument/definition', { line: 1, character: 13 }, []],
        ['block comment does not become a symbol reference', 'textDocument/definition', { line: 2, character: 13 }, []],
    ];
    for (const [name, method, position, result] of queries) {
        await check(name, async () => {
            assert.deepEqual(await request(client, method, { textDocument: { uri: URI }, position }, name),
                             response(name, result));
        });
    }
}

// CTest 入口分别启动两个客户端能力档案，汇总软断言后统一报告失败。
async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_color_capability_smoke.js <stdio-server>');
    let checks = 0;
    let failures = 0;
    for (const [profile, capabilities] of [
        ['empty client', {}],
        ['explicit color client', { textDocument: { colorProvider: { dynamicRegistration: false } } }],
    ]) {
        // 一项语义检查失败不遮蔽其他方法的结果，最终由 failures 决定退出码。
        const check = async (name, run) => {
            checks++;
            try {
                await run();
                console.log(`Pass - ${profile}: ${name}`);
            } catch (error) {
                failures++;
                console.error(`Fail - ${profile}: ${name}\n${error.stack || String(error)}`);
            }
        };
        await check('protocol lifecycle', async () => withClient(serverPath, async (client) => {
            await checkProfile(client, capabilities, check);
        }));
    }
    assert.equal(failures, 0, `${failures}/${checks} color capability checks failed`);
    console.log(`Pass - ${checks}/${checks} color capability checks`);
}

// 握手、退出与回收异常仍须使 CTest 得到失败退出码。
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
