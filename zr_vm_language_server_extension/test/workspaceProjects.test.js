const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

/**
 * ## Summary
 * 为项目发现用例建立一次隔离的 VS Code 替身，并在 VM 中执行仓库当前的 `workspaceProjects.ts` 源码；它验证单清单创建路径，不模拟真实扩展宿主。
 *
 * ## Parameters
 * @param openText 未定义表示没有打开文档；传入字符串时创建 URI 相同的文档，并让其 `getText` 返回该未保存文本。
 *
 * ## Returns
 * @returns `uri` 是唯一的文件 URI 替身，`api` 是 VM 导出的项目发现函数，`diskReads` 是读取计数器。每次调用新建状态与计数，不与其他测试共享。
 *
 * ## Constraints and lifecycle
 * `vscode` 只实现当前三条 `createWorkspaceProject` 用例所需的 workspace 接口；固定 folder 对任何 URI 都返回同一根，文件读取总是返回 `manifest(...)` 的 UTF-8 Buffer，且 `openTextDocument` 是禁止调用的失败哨兵。VM 对 workspaceProjects.ts 做内存 `transpileModule`，把 `vscode` 映射到此替身、其他模块名映射到现有编译输出中的 `projectSupport`；不启动真正的 VS Code、不模拟多根选择/发现扫描/读取拒绝，也不改变源树输出。
 *
 * ## References
 * `project discovery reads unopened...`、`preserves unsaved...` 和 `an invalid unsaved...` 三例通过该 helper 调用真实 `createWorkspaceProject` 源码；helper 的宿主边界不代表完整 extension 集成覆盖。
 */
function projectHost(openText) {
    const uri = {
        path: '/workspace/demo.zrp',
        fsPath: '/workspace/demo.zrp',
        // 同一 URI 在文档匹配和项目 ID 中需要稳定字符串；不模拟 vscode.Uri 的编码规则。
        toString: () => 'file:///workspace/demo.zrp'
    };
    const workspaceFolder = { uri: { path: '/workspace', fsPath: '/workspace' } };
    let diskReads = 0;
    const vscode = {
        EventEmitter: class {
            // 模块初始化只读取 event 成员；这些用例不订阅或验证选择变化事件。
            event() {}
        },
        workspace: {
            // 所有样例 URI 都被视为同一工作区根；不覆盖根外 URI 或多根选择。
            getWorkspaceFolder: () => workspaceFolder,
            textDocuments: openText === undefined ? [] : [{
                uri,
                // 返回闭包持有的未保存文本；字符串由测试调用方提供，不从磁盘刷新。
                getText: () => openText
            }],
            // 若产品意外打开编辑器文档便立即失败；当前用例要求只读已开文档或 fs.readFile。
            openTextDocument: () => assert.fail('Project discovery must not open a document or activate its language extensions'),
            fs: {
                // 只记录调用次数并回传固定清单字节；不访问真实文件、检查 URI 或注入读失败。
                readFile: async () => { diskReads++; return Buffer.from(manifest('磁盘项目')); }
            },
        },
    };
    const source = path.join(__dirname, '../src/workspaceProjects.ts');
    const exports = {};
    vm.runInNewContext(ts.transpileModule(fs.readFileSync(source, 'utf8'), {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    }).outputText, {
        exports, TextDecoder,
        // 当前唯一非 vscode 依赖是 projectSupport；其他名字也会被此替身路由到同一编译模块。
        require: name => name === 'vscode' ? vscode : require('../out/projectSupport'),
    }, { filename: source });
    return {
        uri,
        api: exports,
        // 只暴露读取次数，不给用例修改闭包中的计数器。
        diskReads: () => diskReads
    };
}

/**
 * ## Summary
 * 生成当前工作区项目测试共用的最小 `.zrp` JSON 文本，用于通过 `name` 区分磁盘版本和未保存版本。
 *
 * ## Parameters
 * @param name 写入 manifest 的项目名称字符串。
 *
 * ## Returns
 * @returns 含固定 `source`、`binary`、`entry` 和传入 `name` 的序列化 JSON 文本。
 *
 * ## Constraints and lifecycle
 * 该 helper 只构造字符串，不写文件、不校验 schema，也不覆盖别名或错误输入场景；返回值由调用方作为编辑器文本或假 fs 字节使用。
 *
 * ## References
 * `projectHost` 的假 `readFile` 和未保存文本用例调用此 helper；三项测试断言项目创建读取来源。
 */
function manifest(name) {
    return JSON.stringify({ name, source: 'src', binary: 'bin', entry: 'main' });
}

/** 未打开清单应从工作区文件系统读取，且不调用 openTextDocument。 */
test('project discovery reads unopened UTF-8 manifests without creating editor documents', async () => {
    const host = projectHost();
    const project = await host.api.createWorkspaceProject(host.uri);
    assert.equal(project.label, '磁盘项目');
    assert.equal(project.sourceRootPath, '/workspace/src');
    assert.equal(host.diskReads(), 1);
});

/** 断言项目标签取自编辑器未保存文本且磁盘零读取；两份清单的源码根相同，本例不验证源码根变化。 */
test('project discovery preserves unsaved manifest text instead of reading its disk version', async () => {
    const host = projectHost(manifest('unsaved'));
    const project = await host.api.createWorkspaceProject(host.uri);
    assert.equal(project.label, 'unsaved');
    assert.equal(host.diskReads(), 0);
});

/** 未保存文本无效时不回退磁盘旧版本，避免调试与 LSP 选到非用户所见的项目。 */
test('an invalid unsaved manifest is not replaced with a valid stale disk manifest', async () => {
    const host = projectHost('{');
    assert.equal(await host.api.createWorkspaceProject(host.uri), undefined);
    assert.equal(host.diskReads(), 0);
});
