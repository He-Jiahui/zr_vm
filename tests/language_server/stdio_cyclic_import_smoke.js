const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

// CTest 入口构造真实三文件循环导入项目，验证诊断、符号和 token 查询仍能完成。
// 项目只存在于临时目录，退出路径必须同时回收服务端和磁盘样例。
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server path');
    // 三个模块形成 main→helper→cycle→helper 的回边；源文件中的箭头属于被测语言样例。
    const projectRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-lsp-cyclic-import-'));
    const sourceRoot = path.join(projectRoot, 'src');
    fs.mkdirSync(sourceRoot);
    fs.writeFileSync(path.join(projectRoot, 'cyclic.zrp'), JSON.stringify({
        name: 'cyclic', source: 'src', binary: 'bin', entry: 'main',
    }));
    fs.writeFileSync(path.join(sourceRoot, 'main.zr'), [
        'let helper = import("helper");',
        'class StructureHero {',
        '    pub fn total(): int { return helper.value(); }',
        '}',
        'return helper.value();',
        '',
    ].join('\n'));
    fs.writeFileSync(path.join(sourceRoot, 'helper.zr'), [
        'let cycle = import("cycle");',
        'pub var value = fn() => { return cycle.answer(); };',
        '',
    ].join('\n'));
    fs.writeFileSync(path.join(sourceRoot, 'cycle.zr'), [
        'let helper = import("helper");',
        'pub var answer = fn() => { return 42; };',
        '',
    ].join('\n'));

    // BUG: serverPath 不存在时，共享客户端的 spawn 发出未监听的 error；该异步事件不进入下方
    // async try/finally，已创建的 projectRoot 因而遗留。共享客户端已记录 ENOENT 触发路径。
    const client = new StdioProtocolClient(serverPath);
    // 测试只消费 JSON-RPC result；保留请求超时和协议错误由共享客户端传播。
    const request = async (method, params) =>
        client.requestWithId(method, params, 10000).promise;
    try {
        const projectUri = pathToFileURL(path.join(projectRoot, 'cyclic.zrp')).toString();
        const rootUri = pathToFileURL(projectRoot).toString();
        const documentUri = pathToFileURL(path.join(sourceRoot, 'main.zr')).toString();
        // 选中真实项目根，确保导入环从项目索引进入，而非仅测试打开文档的文本解析。
        const initialized = await request('initialize', {
            processId: null,
            rootUri,
            workspaceFolders: [{ uri: rootUri, name: 'cyclic' }],
            capabilities: { textDocument: { diagnostic: {}, semanticTokens: {} } },
            initializationOptions: { zrSelectedProjectUri: projectUri },
        });
        assert(initialized?.capabilities?.documentSymbolProvider);
        client.notify('initialized', {});
        client.notify('textDocument/didOpen', { textDocument: {
            uri: documentUri, languageId: 'zr', version: 1,
            text: fs.readFileSync(path.join(sourceRoot, 'main.zr'), 'utf8'),
        } });

        // 同一轮打开文档后依次查询三个语义视图，验证导入环不使服务端卡死或丢失声明。
        const diagnostics = await request('textDocument/diagnostic', {
            textDocument: { uri: documentUri },
        });
        assert(Array.isArray(diagnostics?.items), 'Cyclic imports must return diagnostics');
        const symbols = await request('textDocument/documentSymbol', {
            textDocument: { uri: documentUri },
        });
        assert(Array.isArray(symbols), 'Cyclic imports must keep document symbols responsive');
        assert(symbols.some((symbol) => symbol.name === 'StructureHero'),
            'Document symbols must retain declarations beside a cyclic import');
        const tokens = await request('textDocument/semanticTokens/full', {
            textDocument: { uri: documentUri },
        });
        assert(Array.isArray(tokens?.data), 'Semantic tokens must remain responsive');
        assert(!client.closed, 'Language server must survive cyclic imports');
        console.log('Cyclic import stdio smoke passed');
    } finally {
        // TODO: kill 后立即删除目录，尚未等待 close；需在 Windows 检查子进程仍持有项目文件时的回收竞态。
        client.child.kill();
        fs.rmSync(projectRoot, { recursive: true, force: true });
    }
}

// 可进入 Promise 拒绝路径的异常交由 CTest 的非零退出码报告。
main().catch((error) => { console.error(error); process.exitCode = 1; });
