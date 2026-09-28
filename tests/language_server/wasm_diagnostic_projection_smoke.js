const fs = require('fs');
const path = require('path');

/** 对 WASM JSON 响应失败立即停止，避免后续字段检查误报成不同问题。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** C ABI 接收 UTF-8 字节长度；JS 字符数不能代替协议长度。 */
function byteLength(text) {
    return Buffer.byteLength(text, 'utf8');
}

/**
 * 用生成的 JS/WASM 对验证诊断投影：同一文档的 push、pull 与 workspace 结果
 * 应保持 typed fix 或 no-fix disposition；调用方须先构建并传入一对产物路径。
 */
async function main() {
    const modulePath = process.argv[2];
    const wasmPath = process.argv[3];

    assert(modulePath, 'Expected generated WASM JavaScript path');
    assert(wasmPath, 'Expected generated WASM binary path');

    const createModule = require(path.resolve(modulePath));
    const module = await createModule({
        wasmBinary: fs.readFileSync(path.resolve(wasmPath)),
    });
    /** 统一读取 ABI JSON 响应并管理 C 分配的指针；异常解析路径仍待核对。 */
    // TODO: JSON.parse 若遇到非 JSON 响应会绕过 _free；核对 WASM 导出是否保证 JSON，再决定是否用 finally 释放。
    const invoke = (name, argTypes, args) => {
        const pointer = module.ccall(name, 'number', argTypes, args);
        assert(pointer !== 0, `${name} returned a null response pointer`);
        const response = JSON.parse(module.UTF8ToString(pointer));
        module._free(pointer);
        return response;
    };
    /** 将测试源码交给真实 WASM 语义管线，供后续诊断查询。 */
    const updateDocument = (context, uri, source) => invoke(
        'wasm_ZrLspUpdateDocument',
        ['number', 'string', 'number', 'string', 'number', 'number'],
        [context, uri, byteLength(uri), source, byteLength(source), 1],
    );
    /** 读取文档诊断，与 pull/workspace 投影比较。 */
    const getDiagnostics = (context, uri) => invoke(
        'wasm_ZrLspGetDiagnostics',
        ['number', 'string', 'number'],
        [context, uri, byteLength(uri)],
    );
    /** 请求 pull 诊断报告，确认 typed-fix 元数据没有在封装层丢失。 */
    const getDiagnosticReport = (context, uri) => invoke(
        'wasm_ZrLspGetDiagnosticReport',
        ['number', 'string', 'number'],
        [context, uri, byteLength(uri)],
    );
    /** 查询工作区报告，验证多文档诊断仍携带各自的修复处置。 */
    const getWorkspaceDiagnosticReports = (context) => invoke(
        'wasm_ZrLspGetWorkspaceDiagnosticReports',
        ['number'],
        [context],
    );
    const context = module.ccall('wasm_ZrLspContextNew', 'number', [], []);

    assert(context !== 0, 'Failed to create WASM LSP context');
    try {
        // 可提供类型化修复的未初始化读取，三种视图都应保留相同 descriptor 身份。
        const fixUri = 'file:///wasm-diagnostic-fix-smoke.zr';
        const fixSource = [
            'fn choose(flag: bool): int {',
            '    var seed: int;',
            '    if (flag) {',
            '        seed = 1;',
            '    }',
            '    return seed;',
            '}',
            '',
        ].join('\n');
        const updateFix = updateDocument(context, fixUri, fixSource);
        assert(updateFix.success && updateFix.data.updated,
            'Failed to update typed-fix document');

        const fixResponse = getDiagnostics(context, fixUri);
        assert(fixResponse.success && Array.isArray(fixResponse.data),
            'Typed-fix diagnostics request failed');
        const fixDiagnostic = fixResponse.data.find((diagnostic) =>
            diagnostic.code === 'possibly_uninitialized_read');
        assert(fixDiagnostic, 'Expected possibly_uninitialized_read diagnostic');
        assert(fixDiagnostic.source === 'zr',
            'Expected canonical diagnostic source');
        assert(fixDiagnostic.codeDescription &&
            fixDiagnostic.codeDescription.href ===
                'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
        'Expected canonical diagnostic help URI');
        assert(fixDiagnostic.data && fixDiagnostic.data.uri === fixUri &&
            fixDiagnostic.data.descriptorId === 3003 &&
            fixDiagnostic.data.code === 'possibly_uninitialized_read',
        'Expected canonical typed-fix diagnostic data');
        assert(!Object.prototype.hasOwnProperty.call(
            fixDiagnostic.data, 'noFixReason'),
        'Typed-fix diagnostic must omit noFixReason');
        assert(Array.isArray(fixDiagnostic.data.fixes) &&
            fixDiagnostic.data.fixes.length === 1,
        'Expected one typed diagnostic fix');
        assert(fixDiagnostic.data.fixes[0].title ===
            'Replace with an initialized value' &&
            fixDiagnostic.data.fixes[0].edit.newText === '<value>' &&
            fixDiagnostic.data.fixes[0].applicability === 2,
        'Expected exact typed diagnostic fix payload');

        const pullResponse = getDiagnosticReport(context, fixUri);
        assert(pullResponse.success && pullResponse.data.resultId &&
            Array.isArray(pullResponse.data.items),
        'Typed-fix pull diagnostic report failed');
        const pullFix = pullResponse.data.items.find((diagnostic) =>
            diagnostic.code === 'possibly_uninitialized_read');
        assert(pullFix && pullFix.data &&
            pullFix.data.descriptorId === 3003 &&
            Array.isArray(pullFix.data.fixes) &&
            pullFix.data.fixes.length === 1,
        'Pull diagnostics must retain canonical typed-fix data');

        // 需要用户决策的诊断必须说明 no-fix 原因，不能虚构自动修复建议。
        const noFixUri = 'file:///wasm-diagnostic-no-fix-smoke.zr';
        const noFixSource = 'return [value = 1];';
        const updateNoFix = updateDocument(context, noFixUri, noFixSource);
        assert(updateNoFix.success && updateNoFix.data.updated,
            'Failed to update no-fix document');

        const noFixResponse = getDiagnostics(context, noFixUri);
        assert(noFixResponse.success && Array.isArray(noFixResponse.data),
            'No-fix diagnostics request failed');
        const noFixDiagnostic = noFixResponse.data.find((diagnostic) =>
            diagnostic.code === 'array_element_assignment');
        assert(noFixDiagnostic, 'Expected array_element_assignment diagnostic');
        assert(noFixDiagnostic.data &&
            noFixDiagnostic.data.noFixReason === 'requires_user_decision',
        'Expected canonical no-fix disposition');
        assert(!Array.isArray(noFixDiagnostic.data.fixes) ||
            noFixDiagnostic.data.fixes.length === 0,
        'No-fix diagnostic must not expose a typed fix');
        assert(noFixDiagnostic.codeDescription &&
            noFixDiagnostic.codeDescription.href ===
                'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
        'Expected no-fix diagnostic help URI');

        // 跨文档汇总不能丢失单文档的 typed-fix 与 no-fix 处置。
        const workspaceResponse = getWorkspaceDiagnosticReports(context);
        assert(workspaceResponse.success && Array.isArray(workspaceResponse.data),
            'Workspace diagnostics request failed');
        const fixWorkspaceReport = workspaceResponse.data.find((report) =>
            report.uri === fixUri);
        const noFixWorkspaceReport = workspaceResponse.data.find((report) =>
            report.uri === noFixUri);
        assert(fixWorkspaceReport && Array.isArray(fixWorkspaceReport.items) &&
            fixWorkspaceReport.items.some((diagnostic) =>
                diagnostic.code === 'possibly_uninitialized_read' &&
                diagnostic.data && diagnostic.data.descriptorId === 3003 &&
                Array.isArray(diagnostic.data.fixes) &&
                diagnostic.data.fixes.length === 1),
        'Workspace diagnostics must retain canonical typed-fix data');
        assert(noFixWorkspaceReport &&
            Array.isArray(noFixWorkspaceReport.items) &&
            noFixWorkspaceReport.items.some((diagnostic) =>
                diagnostic.code === 'array_element_assignment' &&
                diagnostic.data &&
                diagnostic.data.noFixReason === 'requires_user_decision'),
        'Workspace diagnostics must retain canonical no-fix disposition');
    } finally {
        module.ccall(
            'wasm_ZrLspContextFree',
            null,
            ['number'],
            [context],
        );
    }

    console.log('PASS: WASM diagnostic JSON matches canonical LSP projection');
}

main().catch((error) => {
    console.error(error);
    process.exit(1);
});
