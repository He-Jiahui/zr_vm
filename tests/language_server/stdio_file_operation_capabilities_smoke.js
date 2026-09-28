const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { fileURLToPath, pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

// 请求统一等待 5 秒；同一符号名贯穿创建、重命名和删除阶段，便于发现旧索引残留。
const RESPONSE_TIMEOUT_MS = 5000;
const SYMBOL_NAME = 'file_operation_value';

// 仅清理本测试创建的工作区；旧 Node 缺少 rmSync 时逐项递归回收。
function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (!fs.existsSync(targetPath)) {
        return;
    }
    if (fs.statSync(targetPath).isDirectory()) {
        // 回调执行文件系统副作用，必须在移除父目录前完成所有子路径清理。
        fs.readdirSync(targetPath).forEach((entry) => {
            removePathSync(path.join(targetPath, entry));
        });
        fs.rmdirSync(targetPath);
        return;
    }
    fs.unlinkSync(targetPath);
}

// 对文件操作响应按宿主路径身份比较，容忍 Windows 驱动器与文件名大小写差异。
function comparableUri(uri) {
    const nativePath = fileURLToPath(uri);
    return process.platform === 'win32' ? nativePath.toLowerCase() : nativePath;
}

// 写入尚未由编辑器打开的项目，验证 didCreateFiles 可从磁盘发现项目和导出符号。
// 返回路径与 URI 的配对供后续 willRename/didRename 两阶段复用。
function writeProject(rootPath) {
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'file_operations.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const providerPath = path.join(sourcePath, 'legacy.zr');
    const renamedProviderPath = path.join(sourcePath, 'modern.zr');
    const mainText = [
        'var legacy = import("legacy");',
        `var cached = legacy.${SYMBOL_NAME}();`,
        'return cached;',
        '',
    ].join('\n');
    const providerText = [
        'module legacy;',
        `pub fn ${SYMBOL_NAME}(): int {`,
        '    return 1;',
        '}',
        '',
    ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'file_operations',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, mainText);
    fs.writeFileSync(providerPath, providerText);
    return {
        projectPath,
        mainPath,
        providerPath,
        renamedProviderPath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        providerUri: pathToFileURL(providerPath).toString(),
        renamedProviderUri: pathToFileURL(renamedProviderPath).toString(),
        mainText,
        providerText,
    };
}

// 核对成功请求的 result；协议错误由原始 envelope 断言路径单独处理。
async function request(client, method, params) {
    return client.requestWithId(method, params, RESPONSE_TIMEOUT_MS).promise;
}

// 用同一查询证明文件操作前后索引的增加、迁移和清除，没有旧 URI 残留。
async function assertWorkspaceSymbol(client, expectedUri, message) {
    const symbols = await request(client, 'workspace/symbol', { query: SYMBOL_NAME });
    assert(Array.isArray(symbols), `${message}: ${JSON.stringify(symbols)}`);
    const locations = symbols.filter((symbol) => symbol.name === SYMBOL_NAME)
        .map((symbol) => comparableUri(symbol.location.uri));
    assert.deepStrictEqual(locations, expectedUri === null ? [] : [comparableUri(expectedUri)], message);
}

// willRenameFiles 必须同时改导入字符串与模块声明；打开文档用当前版本，磁盘文件用 null。
// 两次调用分别覆盖 didOpen 版本和 didChange 后版本，排序只用于忽略响应数组顺序。
function assertRenameEdit(edit, fixture, mainVersion, importLine) {
    assert(edit && Array.isArray(edit.documentChanges),
        `willRenameFiles must return documentChanges: ${JSON.stringify(edit)}`);
    const actual = edit.documentChanges.map((change) => ({
        uri: comparableUri(change.textDocument.uri),
        version: change.textDocument.version,
        edits: change.edits,
    })).sort((left, right) => left.uri.localeCompare(right.uri));
    const expected = [
        {
            uri: comparableUri(fixture.mainUri),
            version: mainVersion,
            edits: [{
                range: {
                    start: { line: importLine, character: 21 },
                    end: { line: importLine, character: 27 },
                },
                newText: 'modern',
            }],
        },
        {
            uri: comparableUri(fixture.providerUri),
            version: null,
            edits: [{
                range: {
                    start: { line: 0, character: 7 },
                    end: { line: 0, character: 13 },
                },
                newText: 'modern',
            }],
        },
    ].sort((left, right) => left.uri.localeCompare(right.uri));
    assert.deepStrictEqual(actual, expected,
        'willRenameFiles must edit exactly the import target and module declaration at their current versions');
}

// CTest 入口按能力发布、创建、预重命名、落盘重命名和删除顺序验证文件操作协议。
// 一个服务端进程贯穿所有阶段，保证索引与打开文档状态确实被沿用。
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server executable path');
    const workspaceRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-lsp-file-operations-'));
    // BUG: 传入不存在的服务端路径时共享客户端发出未监听的 spawn error；
    // 异常不进入下方 async finally，已创建的 workspaceRoot 因而遗留。
    const client = new StdioProtocolClient(serverPath);

    try {
        const initialized = await request(client, 'initialize', {
            processId: null,
            rootUri: pathToFileURL(workspaceRoot + path.sep).toString(),
            capabilities: {
                workspace: {
                    workspaceEdit: { documentChanges: true },
                    fileOperations: {
                        willCreate: true,
                        didCreate: true,
                        willRename: true,
                        didRename: true,
                        willDelete: true,
                        didDelete: true,
                    },
                },
            },
        });
        client.notify('initialized', {});
        // 只广告确有处理链的四种操作；willCreate/willDelete 保持 MethodNotFound。
        const registrations = initialized.capabilities.workspace.fileOperations;
        for (const method of ['didCreate', 'didDelete', 'didRename', 'willRename']) {
            assert.deepStrictEqual(registrations[method], {
                filters: [{ pattern: { glob: '**/*.{zr,zrp,zro,dll,so,dylib}' } }],
            }, `${method} must retain its file-operation registration`);
        }

        const withdrawnResponses = [];
        for (const method of ['workspace/willCreateFiles', 'workspace/willDeleteFiles']) {
            const response = await client.request(method, {
                files: [{ uri: pathToFileURL(path.join(workspaceRoot, 'future.zr')).toString() }],
            }, method, RESPONSE_TIMEOUT_MS);
            withdrawnResponses.push(response);
            console.log(`${method}: ${JSON.stringify(response)}`);
        }

        // 先证实空索引，再创建未打开项目，避免 didCreate 的成功被既有缓存伪装。
        await assertWorkspaceSymbol(client, null,
            'the workspace must start without the future project symbol');
        const fixture = writeProject(workspaceRoot);
        client.notify('workspace/didCreateFiles', {
            files: [{ uri: fixture.projectUri }],
        });
        await assertWorkspaceSymbol(client, fixture.providerUri,
            'didCreateFiles must index newly created unopened project sources');

        client.notify('textDocument/didOpen', {
            textDocument: { uri: fixture.mainUri, languageId: 'zr', version: 7, text: fixture.mainText },
        });
        const renameParams = {
            files: [{ oldUri: fixture.providerUri, newUri: fixture.renamedProviderUri }],
        };
        assertRenameEdit(await request(client, 'workspace/willRenameFiles', renameParams), fixture, 7, 0);

        // 预重命名必须读取最新 overlay 版本和位移后的导入行，而非旧磁盘快照。
        const changedMainText = '// current overlay\n' + fixture.mainText;
        client.notify('textDocument/didChange', {
            textDocument: { uri: fixture.mainUri, version: 8 },
            contentChanges: [{ text: changedMainText }],
        });
        assertRenameEdit(await request(client, 'workspace/willRenameFiles', renameParams), fixture, 8, 1);

        // 未打开提供者文件若在磁盘上变化，应拒绝用缓存范围写入可能已变的源文件。
        fs.writeFileSync(fixture.providerPath, fixture.providerText.replace('return 1;', 'return 2;'));
        assert.strictEqual(await request(client, 'workspace/willRenameFiles', renameParams), null,
            'willRenameFiles must reject unopened disk content that no longer matches the cached snapshot');
        fs.writeFileSync(fixture.providerPath, fixture.providerText);
        assertRenameEdit(await request(client, 'workspace/willRenameFiles', renameParams), fixture, 8, 1);
        assert.strictEqual(await request(client, 'workspace/willRenameFiles', {
            files: [{ oldUri: fixture.providerUri, newUri: fixture.providerUri }],
        }), null, 'a same-URI willRenameFiles request must not produce edits');

        // 先模拟磁盘重命名与客户端编辑，再通知服务端，验证新旧模块身份转移。
        const renamedMainText = changedMainText.replace('import("legacy")', 'import("modern")');
        fs.writeFileSync(fixture.mainPath, renamedMainText);
        fs.renameSync(fixture.providerPath, fixture.renamedProviderPath);
        fs.writeFileSync(fixture.renamedProviderPath,
            fixture.providerText.replace('module legacy;', 'module modern;'));
        client.notify('workspace/didRenameFiles', renameParams);
        client.notify('textDocument/didChange', {
            textDocument: { uri: fixture.mainUri, version: 9 },
            contentChanges: [{ text: renamedMainText }],
        });
        await assertWorkspaceSymbol(client, fixture.renamedProviderUri,
            'didRenameFiles must move the source symbol to the new URI without retaining the old index');
        const definitions = await request(client, 'textDocument/definition', {
            textDocument: { uri: fixture.mainUri },
            position: { line: 2, character: 20 },
        });
        assert(Array.isArray(definitions), `Expected definition locations: ${JSON.stringify(definitions)}`);
        assert.deepStrictEqual(definitions.map((location) => comparableUri(location.uri)),
            [comparableUri(fixture.renamedProviderUri)],
            'didRenameFiles must resolve the updated import to the renamed provider');

        // 关闭 overlay 后删除项目通知应同时清除项目索引，不依赖文件仍处于打开态。
        client.notify('textDocument/didClose', { textDocument: { uri: fixture.mainUri } });
        fs.unlinkSync(fixture.projectPath);
        client.notify('workspace/didDeleteFiles', { files: [{ uri: fixture.projectUri }] });
        await assertWorkspaceSymbol(client, null,
            'didDeleteFiles must remove the deleted project index');

        // 完整比对未注册方法的 JSON-RPC error envelope，防止只凭空 result 误判拒绝。
        assert.deepStrictEqual({
            registrations: Object.keys(registrations).sort(),
            withdrawnResponses,
        }, {
            registrations: ['didCreate', 'didDelete', 'didRename', 'willRename'],
            withdrawnResponses: ['workspace/willCreateFiles', 'workspace/willDeleteFiles'].map((method) => ({
                jsonrpc: '2.0',
                id: method,
                error: { code: -32601, message: 'Method not found' },
            })),
        }, 'null-only file-operation requests must be unregistered and return exact MethodNotFound envelopes');

        await request(client, 'shutdown', {});
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert.strictEqual(exitCode, 0, `Expected clean server exit: ${client.stderr()}`);
        console.log('stdio file operation capabilities smoke: Pass');
    } finally {
        // 服务端停止后才删除临时目录，避免子进程继续读取测试文件。
        if (!client.closed) {
            await client.terminate();
        }
        removePathSync(workspaceRoot);
    }
}

// 顶层断言和回收失败均转成 CTest 可见的非零退出码。
main().catch((error) => {
    console.error(`stdio file operation capabilities smoke failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
