const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

// TODO: 当前 CTest 注册表未直接调用本脚本；核查其他正式验收入口，避免代际回归长期只靠手工运行。
// 所有请求共用该等待上限；协议客户端负责在超时后拒绝未完成的请求。
const REQUEST_TIMEOUT_MS = 10000;

/** 调用方只能传入本探针 mkdtempSync 创建的临时根目录；函数不校验归属，并兼容没有 fs.rmSync 的 Node 版本。 */
function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (!fs.existsSync(targetPath)) {
        return;
    }
    if (fs.statSync(targetPath).isDirectory()) {
        for (const entry of fs.readdirSync(targetPath)) {
            removePathSync(path.join(targetPath, entry));
        }
        fs.rmdirSync(targetPath);
        return;
    }
    fs.unlinkSync(targetPath);
}

/** 建立包含 importer 与 provider 的临时工程，用于验证依赖文件变化会推进诊断身份。
 * @note main 的 finally 尝试删除返回的 rootPath；terminate 拒绝时会跳过删除。URI 只供 LSP 请求。
 */
function createWorkspaceDiagnosticFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-task6-workspace-diagnostics-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'workspace_diagnostics.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const providerPath = path.join(sourcePath, 'provider.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'workspace_diagnostics',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    const mainText = [
        'module main;',
        'var provider = import("provider");',
        'pub fn entry(): int { return provider.value(); }',
        '',
    ].join('\n');
    const providerText = [
        'module provider;',
        'pub fn value(): int { return missing_provider_value; }',
        '',
    ].join('\n');
    fs.writeFileSync(mainPath, mainText);
    fs.writeFileSync(providerPath, providerText);

    return {
        rootPath,
        rootUri: pathToFileURL(rootPath + path.sep).toString(),
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        providerUri: pathToFileURL(providerPath).toString(),
        mainText,
        providerText,
    };
}

/** 使用服务端二进制验证 full/unchanged 身份、开放覆盖层和跨文件依赖失效。
 * BUG: 客户端启动和 fixture 创建发生在 try 前；fixture 创建失败时可能遗留进程或已建临时目录。
 */
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'expected stdio server path');

    const client = new StdioProtocolClient(serverPath);
    const workspaceFixture = createWorkspaceDiagnosticFixture();
    try {
        const initialize = await client.requestWithId('initialize', {
            processId: null,
            workspaceFolders: [{ uri: workspaceFixture.rootUri, name: 'diagnostics' }],
            rootUri: workspaceFixture.rootUri,
            capabilities: {},
        }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(initialize.capabilities.diagnosticProvider.interFileDependencies, true);

        // 非法参数必须按 JSON-RPC 错误返回，不能被当成空诊断报告。
        await assert.rejects(
            client.requestWithId('textDocument/diagnostic', {}, REQUEST_TIMEOUT_MS).promise,
            (error) => {
                const response = JSON.parse(error.message);
                return response.code === -32602;
            },
            'invalid document diagnostic params must return InvalidParams',
        );

        const uri = 'file:///tmp/zr-task6-diagnostic-generation.zr';
        const text = 'fn broken(): int { return missing; }\n';
        client.notify('textDocument/didOpen', {
            textDocument: { uri, languageId: 'zr', version: 1, text },
        });
        // 同一快照首次返回 full，随后携带 resultId 的请求应返回 unchanged。
        const first = await client.requestWithId(
            'textDocument/diagnostic', { textDocument: { uri } }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(first.kind, 'full');
        assert.equal(typeof first.resultId, 'string');
        assert(first.resultId.length > 0);

        const unchanged = await client.requestWithId('textDocument/diagnostic', {
            textDocument: { uri },
            previousResultId: first.resultId,
        }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(unchanged.kind, 'unchanged');
        assert.equal(unchanged.resultId, first.resultId);

        // 非语义文档也有快照身份；空诊断不应复用固定、无代际的 resultId。
        const emptyUri = 'file:///tmp/zr-task6-diagnostic-empty.zrp';
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: emptyUri,
                languageId: 'json',
                version: 1,
                text: '{ "name": "diagnostic-empty", "source": "src" }\n',
            },
        });
        const empty = await client.requestWithId(
            'textDocument/diagnostic', { textDocument: { uri: emptyUri } }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(empty.kind, 'full');
        assert.equal(typeof empty.resultId, 'string');
        assert(empty.resultId.length > 0,
            'non-semantic documents must use their document snapshot for diagnostics identity');
        assert.deepEqual(empty.items, []);

        // 工作区报告要同时覆盖尚未打开的 provider 与工程索引外的已打开文档。
        client.notify('workspace/didChangeWatchedFiles', {
            changes: [{ uri: workspaceFixture.projectUri, type: 1 }],
        });
        const workspace = await client.requestWithId(
            'workspace/diagnostic', {}, REQUEST_TIMEOUT_MS).promise;
        const providerReport = workspace.items.find((item) => item && item.uri === workspaceFixture.providerUri);
        const overlayReport = workspace.items.find((item) => item && item.uri === emptyUri);
        assert(providerReport, 'workspace diagnostics must include an unopened indexed source file');
        assert(overlayReport, 'workspace diagnostics must include an open document outside the project index');
        assert.equal(providerReport.version, null,
            'unopened indexed sources must not claim an editor document version');
        assert.equal(overlayReport.version, 1,
            'open overlay diagnostics must retain the editor document version');
        assert.equal(typeof providerReport.resultId, 'string');
        assert(providerReport.resultId.length > 0);

        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: workspaceFixture.mainUri,
                languageId: 'zr',
                version: 1,
                text: workspaceFixture.mainText,
            },
        });
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: workspaceFixture.providerUri,
                languageId: 'zr',
                version: 1,
                text: workspaceFixture.providerText,
            },
        });
        // provider 的未保存改动会使 importer 分析快照失效，即使 importer 自身文本未变。
        const dependencyBaseline = await client.requestWithId('textDocument/diagnostic', {
            textDocument: { uri: workspaceFixture.mainUri },
        }, REQUEST_TIMEOUT_MS).promise;
        client.notify('textDocument/didChange', {
            textDocument: { uri: workspaceFixture.providerUri, version: 2 },
            contentChanges: [{ text: workspaceFixture.providerText + '// dependency generation two\n' }],
        });
        const dependencyChanged = await client.requestWithId('textDocument/diagnostic', {
            textDocument: { uri: workspaceFixture.mainUri },
        }, REQUEST_TIMEOUT_MS).promise;
        assert.notEqual(dependencyChanged.resultId, dependencyBaseline.resultId,
            'dependency changes must invalidate a stable importer diagnostic resultId');
    // 测试失败也必须关闭服务端和删除临时工程，避免下一次运行复用旧索引。
    // BUG: terminate() 若因等待服务端退出而拒绝，下面的 removePathSync 不会执行；
    //      需让目录清理处于独立 finally，避免失败后的临时工程残留。
    } finally {
        await client.terminate();
        removePathSync(workspaceFixture.rootPath);
    }
}

// 把异步协议失败交给 Node 退出状态，供人工与自动入口统一识别。
main().catch((error) => {
    console.error(error.stack || error.message || String(error));
    process.exitCode = 1;
});
