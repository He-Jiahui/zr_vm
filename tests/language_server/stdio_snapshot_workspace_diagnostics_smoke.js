const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

// 此脚本由顶层 CTest 的 snapshot workspace diagnostics 目标调用。
// 每个请求都显式设置期限，避免 stdio 无响应时无限等待。
const REQUEST_TIMEOUT_MS = 10000;

/** 调用方只能传入本脚本 mkdtempSync 创建的根目录；函数不校验归属，并保留旧版 Node 的删除回退。 */
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

/** 为多根工作区分别构造带导入依赖和独立文件的工程，再准备一个根外工程。
 * @note withProvider 控制 main 是否依赖另一个未打开的源码；返回 URI 只用于协议请求。
 */
function createProject(rootPath, name, withProvider) {
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, `${name}.zrp`);
    const mainPath = path.join(sourcePath, 'main.zr');
    const providerPath = path.join(sourcePath, 'provider.zr');
    const mainText = withProvider
        ? [
            'module main;',
            'var provider = import("provider");',
            'pub fn entry(): int { return provider.value(); }',
            '',
        ].join('\n')
        : [
            'module main;',
            'pub fn independent(): int { return 1; }',
            '',
        ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name,
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, mainText);
    if (withProvider) {
        fs.writeFileSync(providerPath, [
            'module provider;',
            'pub fn value(): int { return 1; }',
            '',
        ].join('\n'));
    }

    return {
        rootUri: pathToFileURL(rootPath + path.sep).toString(),
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        providerUri: withProvider ? pathToFileURL(providerPath).toString() : null,
        providerPath,
    };
}

/** 在 workspace/diagnostic 聚合结果中按精确 URI 找报告，避免依赖服务器枚举顺序。 */
function reportForUri(workspaceReport, uri) {
    return workspaceReport.items.find((item) => item && item.uri === uri);
}

/** 所有拉取请求共享同一协议客户端和等待期限，测试专注于报告身份而非传输细节。 */
async function request(client, method, params) {
    return client.requestWithId(method, params, REQUEST_TIMEOUT_MS).promise;
}

/** 使用 CTest 提供的服务端路径验证多根快照、依赖重载和根外覆盖层回收。
 * BUG: 三个 fixture 在 try 前连续创建；中途写入失败时可能遗留已建目录，且不会进入 finally。
 */
async function main() {
    const serverPath = process.argv[2];
    const workspaceRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-task7-workspace-diagnostics-'));
    const first = createProject(path.join(workspaceRoot, 'first'), 'first', true);
    const second = createProject(path.join(workspaceRoot, 'second'), 'second', false);
    const outside = createProject(path.join(workspaceRoot, 'outside'), 'outside', false);
    // BUG: 未传服务端路径时，三个临时工程已创建且 spawn(undefined) 会在下方 assert/try 前抛错；
    //      此路径无法进入 finally，留下 zr-task7-workspace-diagnostics 临时目录。
    const client = new StdioProtocolClient(serverPath);

    assert(serverPath, 'expected stdio server path');
    try {
        await request(client, 'initialize', {
            processId: null,
            workspaceFolders: [
                { uri: first.rootUri, name: 'first' },
                { uri: second.rootUri, name: 'second' },
            ],
            capabilities: {},
        });
        client.notify('workspace/didChangeWatchedFiles', {
            changes: [
                { uri: first.projectUri, type: 1 },
                { uri: second.projectUri, type: 1 },
            ],
        });

        // 未打开的两个根仍应被项目快照纳入，且不得虚构编辑器文档版本。
        const initial = await request(client, 'workspace/diagnostic', {});
        const initialMain = reportForUri(initial, first.mainUri);
        const initialProvider = reportForUri(initial, first.providerUri);
        const secondMain = reportForUri(initial, second.mainUri);
        assert(initialMain && initialProvider && secondMain,
            'multi-root diagnostics must include both roots and the unopened provider source');
        assert.equal(initialMain.version, null,
            'unopened importer diagnostics must not claim an editor version');
        assert.equal(initialProvider.version, null,
            'unopened provider diagnostics must not claim an editor version');
        assert.equal(secondMain.version, null,
            'unopened diagnostics from the second root must not claim an editor version');
        assert.equal(typeof initialMain.resultId, 'string');
        assert(initialMain.resultId.length > 0,
            'unopened importer diagnostics must have a snapshot-backed resultId');

        // 只重载 provider；若 importer 的 resultId 不变，跨文件诊断缓存会使用过期事实。
        fs.writeFileSync(first.providerPath, [
            'module provider;',
            'pub fn value(): int { return 1; }',
            '// provider reload generation two',
            '',
        ].join('\n'));
        client.notify('workspace/didChangeWatchedFiles', {
            changes: [{ uri: first.providerUri, type: 2 }],
        });

        const reloaded = await request(client, 'workspace/diagnostic', {
            previousResultIds: [{ uri: first.mainUri, value: initialMain.resultId }],
        });
        const reloadedMain = reportForUri(reloaded, first.mainUri);
        assert(reloadedMain && reloadedMain.resultId !== initialMain.resultId,
            'unopened provider reload must change the importer diagnostic resultId');

        // 根外工程只有显式打开时才临时参与诊断索引；关闭后不能污染注册根。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: outside.mainUri,
                languageId: 'zr',
                version: 7,
                text: 'module main;\npub fn outsideOverlay(): int { return 7; }\n',
            },
        });
        const openedOutside = await request(client, 'workspace/diagnostic', {});
        assert.equal(reportForUri(openedOutside, outside.mainUri).version, 7,
            'an explicitly opened root-external project must publish overlay diagnostics');
        client.notify('textDocument/didClose', {
            textDocument: { uri: outside.mainUri },
        });
        const closedOutside = await request(client, 'workspace/diagnostic', {});
        assert(!reportForUri(closedOutside, outside.mainUri),
            'a released root-external file must leave the project diagnostic index');
        assert(reportForUri(closedOutside, first.mainUri) && reportForUri(closedOutside, second.mainUri),
            'closing an external document must preserve registered workspace diagnostics');
        console.log('stdio snapshot workspace diagnostics smoke: 10/10 Pass');
    // 清理客户端与临时工程，防止后续 CTest 读到前次项目快照。
    // BUG: terminate() 若因等待服务端退出而拒绝，下面的 removePathSync 不会执行；
    //      需让目录清理处于独立 finally，避免失败后的临时工程残留。
    } finally {
        await client.terminate();
        removePathSync(workspaceRoot);
    }
}

// CTest 通过进程退出码消费异步断言结果。
main().catch((error) => {
    console.error(error.stack || error.message || String(error));
    process.exitCode = 1;
});
