const { StdioProtocolClient } = require('./stdio_protocol_client');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');

// 各协议断言需要在有限窗口内得到明确响应，避免同步失效被长时间挂起掩盖。
const RESPONSE_TIMEOUT_MS = 3000;

// 本套用例把响应形状与状态不变量失败汇总为同一异常出口。
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// 每组同步状态用独立服务端进程，防止前一组 desynchronized 标记污染下一组编码测试。
// 无论回调成功或失败都请求终止子进程。
async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    try {
        return await run(client);
    } finally {
        // TODO: terminate 失败被吞掉；需用 kill/close 故障注入核对 CTest 是否可能在服务端未回收时通过。
        await client.terminate().catch(() => {});
    }
}

// 为 didSave 无 text 的磁盘刷新路径创建独立真实文件，用完即删除。
async function withTemporaryDiskDocument(run) {
    // BUG: 文件名仅由 PID 决定；若该路径已存在，writeFileSync 会覆盖其内容，finally 随后删掉原文件。
    // 该用例没有独占创建或预存检查，PID 复用即可触发测试外数据丢失。
    const filePath = path.join(os.tmpdir(), `zr-vm-document-sync-${process.pid}.zr`);

    fs.writeFileSync(filePath, 'struct DidSaveRefreshesDiskDocument { pub var value: int; }', 'utf8');
    try {
        return await run(pathToFileURL(filePath).href);
    } finally {
        if (fs.existsSync(filePath)) {
            fs.unlinkSync(filePath);
        }
    }
}

// 先完成 initialize/initialized 握手，再允许各场景发送文档通知与查询。
// 返回协商后的能力供 UTF-8 场景确认位置编码确已生效。
async function initialize(client, capabilities = {}, rootUri = null) {
    const response = await client.request('initialize', {
        processId: null,
        rootUri,
        capabilities,
    }, 'document-sync-initialize', RESPONSE_TIMEOUT_MS);
    assert(response && response.result && response.result.capabilities,
           `initialize must succeed, actual=${JSON.stringify(response)}`);
    client.notify('initialized', {});
    return response.result.capabilities;
}

// 工作区索引查询统一要求正常数组结果，使后续空/非空断言只比较同步状态。
async function queryWorkspaceSymbols(client, query, id) {
    const response = await client.request('workspace/symbol', { query }, id, RESPONSE_TIMEOUT_MS);
    assert(response && !response.error && Array.isArray(response.result),
           `workspace/symbol must return a normal result, actual=${JSON.stringify(response)}`);
    return response.result;
}

// 文档符号用于区分关闭虚拟 overlay 与恢复已索引磁盘文档两种所有权。
async function queryDocumentSymbols(client, uri, id) {
    const response = await client.request('textDocument/documentSymbol', {
        textDocument: { uri },
    }, id, RESPONSE_TIMEOUT_MS);
    assert(response && !response.error && Array.isArray(response.result),
           `textDocument/documentSymbol must return an array, actual=${JSON.stringify(response)}`);
    return response.result;
}

// 固定有效位置的轻量查询，触发分派器对 desynchronized 文档的 ContentModified 门禁。
async function queryHover(client, uri, id) {
    return queryHoverAt(client, uri, 0, 7, id);
}

// 保留自定义位置入口，以验证越界请求不能被静默钳到有效范围。
async function queryHoverAt(client, uri, line, character, id) {
    return client.request('textDocument/hover', {
        textDocument: { uri },
        position: { line, character },
    }, id, RESPONSE_TIMEOUT_MS);
}

// 一旦通知使文档失去可信快照，后续读取必须以 ContentModified 失败关闭。
function assertContentModified(response, label) {
    assert(response && response.error && response.error.code === -32801,
           `${label} must fail closed with ContentModified, actual=${JSON.stringify(response)}`);
}

// 完整替换或合法增量应恢复可查询状态，不能沿用旧失同步标记。
function assertNotContentModified(response, label) {
    // BUG: 这里只排除 -32801；若服务端回 -32602/-32603 等其他错误，调用方的“同步已恢复”断言仍通过。
    // 例如越界处理退化为 Invalid params 时，下方合法增量恢复场景会被误报通过。
    assert(!(response && response.error && response.error.code === -32801),
           `${label} must leave the document synchronized, actual=${JSON.stringify(response)}`);
}

// 绕开正常 JSON.stringify，构造帧长度正确但正文含非法 UTF-8 的 didChange 负例。
// 服务端必须拒绝该版本，之后允许更高版本的完整内容替换恢复同步。
function notifyInvalidUtf8DidChange(client, uri, version) {
    const prefix = Buffer.from(
        `{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":${JSON.stringify(uri)},"version":${version}},"contentChanges":[{"text":"class InvalidUtf8`,
        'ascii');
    const suffix = Buffer.from(' { }"}]}}', 'ascii');
    const body = Buffer.concat([prefix, Buffer.from([0xc3]), suffix]);
    const header = Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii');

    client.sendRawFrame(Buffer.concat([header, body]));
}

// CTest 入口依次核对文档版本、增量原子性、编码边界及关闭/保存后的磁盘回退。
// 不修改仓内 fixture；唯一磁盘写入由 withTemporaryDiskDocument 限定在系统临时目录。
async function main() {
    const serverPath = process.argv[2];
    const uri = 'file:///stdio-document-sync-conformance.zr';
    const invalidOpenUri = 'file:///stdio-document-sync-invalid-open.zr';
    const missingTextOpenUri = 'file:///stdio-document-sync-missing-text-open.zr';
    const unopenedSaveUri = 'file:///stdio-document-sync-unopened-save.zr';
    const indexedWorkspacePath = path.resolve(__dirname, '..', 'fixtures', 'projects', 'classes');
    const indexedWorkspaceRootUri = pathToFileURL(indexedWorkspacePath).href;
    const indexedFixtureUri = pathToFileURL(path.join(indexedWorkspacePath, 'src', 'main.zr')).href;
    const versionTwoText = 'class DocumentSyncVersionTwo { }';

    assert(serverPath, 'usage: node stdio_document_sync_conformance.js <stdio-server>');
    // 第一进程只验证默认 UTF-16 档案下的虚拟文档版本与失同步生命周期。
    await withClient(serverPath, async (client) => {
        await initialize(client);
        // didOpen 缺版本或缺正文都不得建立可查询的工作区符号。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: invalidOpenUri,
                languageId: 'zr',
                text: 'class InvalidOpenMustNotCreateOverlay { }',
            },
        });
        const invalidOpenSymbols = await queryWorkspaceSymbols(
            client,
            'InvalidOpenMustNotCreateOverlay',
            'document-sync-invalid-open');
        assert(invalidOpenSymbols.length === 0,
               `didOpen without an integer version must not create an overlay, actual=${JSON.stringify(invalidOpenSymbols)}`);
        // TODO: 缺 text 的 didOpen 只查询本来就未出现在请求中的名字，
        // 需另用可观察的 overlay 状态或后续有效 didOpen 证明空叠层未被创建。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: missingTextOpenUri,
                languageId: 'zr',
                version: 1,
            },
        });
        const missingTextOpenSymbols = await queryWorkspaceSymbols(
            client,
            'MissingTextMustNotCreateOverlay',
            'document-sync-missing-text-open');
        assert(missingTextOpenSymbols.length === 0,
               `didOpen without string text must not create an overlay, actual=${JSON.stringify(missingTextOpenSymbols)}`);
        // 有效版本一作为后续增量的基线；同版本通知必须使读取失败关闭。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'class DocumentSyncVersionOne { }',
            },
        });

        const versionOne = await queryWorkspaceSymbols(
            client,
            'DocumentSyncVersionOne',
            'document-sync-v1');
        assert(versionOne.some((symbol) => symbol && symbol.name === 'DocumentSyncVersionOne'),
               `didOpen version 1 must be queryable, actual=${JSON.stringify(versionOne)}`);

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 1 },
            contentChanges: [{ text: 'class SameVersionMustDesynchronize { }' }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-same-version'),
            'same-version didChange');

        // 更高版本的完整内容替换可从失同步恢复，范围编辑则不能用旧快照继续应用。
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 2 },
            contentChanges: [{ text: versionTwoText }],
        });

        const versionTwo = await queryWorkspaceSymbols(
            client,
            'DocumentSyncVersionTwo',
            'document-sync-v2');
        assert(versionTwo.some((symbol) => symbol && symbol.name === 'DocumentSyncVersionTwo'),
               `didChange version 2 must replace the queryable document content, actual=${JSON.stringify(versionTwo)}`);

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 3 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: 0 },
                    end: { line: 0, character: 0 },
                },
                rangeLength: 1,
                text: 'X',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-range-length'),
            'mismatched rangeLength didChange');

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 4 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: 0 },
                    end: { line: 0, character: versionTwoText.length },
                },
                text: 'class IllegalWhileDesynchronized { }',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-ranged-while-desynchronized'),
            'ranged didChange while desynchronized');
        const illegalWhileDesynchronized = await queryWorkspaceSymbols(
            client,
            'IllegalWhileDesynchronized',
            'document-sync-ranged-while-desynchronized-symbols');
        assert(illegalWhileDesynchronized.length === 0,
               `ranged didChange while desynchronized must not replace the old snapshot, actual=${JSON.stringify(illegalWhileDesynchronized)}`);

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 5 },
            contentChanges: [{ text: 'class DocumentSyncRecovered { }' }],
        });
        assertNotContentModified(
            await queryHover(client, uri, 'document-sync-full-recovery'),
            'full-content didChange recovery');
        const recovered = await queryWorkspaceSymbols(
            client,
            'DocumentSyncRecovered',
            'document-sync-recovered');
        assert(recovered.some((symbol) => symbol && symbol.name === 'DocumentSyncRecovered'),
               `full-content didChange must recover the document, actual=${JSON.stringify(recovered)}`);

        // 两个变更共属一个通知；后一个越界时前一个也不得提交到工作区索引。
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 6 },
            contentChanges: [{ text: 'class AtomicBefore { }' }],
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 7 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: 6 },
                    end: { line: 0, character: 18 },
                },
                text: 'AtomicAfter',
            }, {
                range: {
                    start: { line: 9, character: 0 },
                    end: { line: 9, character: 0 },
                },
                text: 'x',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-atomic-rollback'),
            'partially invalid multi-change');
        const atomicBefore = await queryWorkspaceSymbols(
            client,
            'AtomicBefore',
            'document-sync-atomic-before');
        const atomicAfter = await queryWorkspaceSymbols(
            client,
            'AtomicAfter',
            'document-sync-atomic-after');
        assert(atomicBefore.some((symbol) => symbol && symbol.name === 'AtomicBefore') &&
               atomicAfter.length === 0,
               `invalid multi-change must atomically preserve the old snapshot, before=${JSON.stringify(atomicBefore)}, after=${JSON.stringify(atomicAfter)}`);

        // 越界、反向范围和 UTF-16 代理对中点都必须拒绝，避免静默钳制编辑位置。
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 8 },
            contentChanges: [{ text: 'class RangeMatrix { }' }],
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 9 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: 0 },
                    end: { line: 0, character: 999 },
                },
                text: 'x',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-out-of-bounds-range'),
            'out-of-bounds range');

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 10 },
            contentChanges: [{ text: 'class ReverseRange { }' }],
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 11 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: 12 },
                    end: { line: 0, character: 0 },
                },
                text: 'x',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-reversed-range'),
            'reversed range');

        const astralText = 'class AstralRange { let marker = "😀"; }';
        const astralMiddle = astralText.indexOf('😀') + 1;
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 12 },
            contentChanges: [{ text: astralText }],
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 13 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: astralMiddle },
                    end: { line: 0, character: astralMiddle },
                },
                text: 'x',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-surrogate-middle'),
            'UTF-16 surrogate middle range');

        // 组合附标是独立码点；它与拆开代理对的错误边界不同，应允许精确删除。
        const combiningText = 'class CombiningRange { let marker = "e\u0301"; }';
        const combiningAccent = combiningText.indexOf('\u0301');
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 14 },
            contentChanges: [{ text: combiningText }],
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 15 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: combiningAccent },
                    end: { line: 0, character: combiningAccent + 1 },
                },
                text: '',
            }],
        });
        assertNotContentModified(
            await queryHover(client, uri, 'document-sync-combining-range'),
            'combining code point boundary range');

        // 连续编辑混合 CRLF、孤立 CR 和 LF 文档，验证每个范围都相对前一变更后的内容。
        const mixedLineEndings = 'class CrLfA { }\r\nclass CrOnlyA { }\rclass LfA { }\n';
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 16 },
            contentChanges: [{ text: mixedLineEndings }],
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 17 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: 6 },
                    end: { line: 0, character: 11 },
                },
                rangeLength: 5,
                text: 'CrLfB',
            }, {
                range: {
                    start: { line: 1, character: 6 },
                    end: { line: 1, character: 13 },
                },
                rangeLength: 7,
                text: 'CrOnlyB',
            }, {
                range: {
                    start: { line: 2, character: 6 },
                    end: { line: 2, character: 9 },
                },
                rangeLength: 3,
                text: 'LfB',
            }],
        });
        assertNotContentModified(
            await queryHover(client, uri, 'document-sync-mixed-line-endings'),
            'sequential CR/LF/CRLF changes');
        const crLf = await queryWorkspaceSymbols(client, 'CrLfB', 'document-sync-crlf');
        const crOnly = await queryWorkspaceSymbols(client, 'CrOnlyB', 'document-sync-cr');
        const lf = await queryWorkspaceSymbols(client, 'LfB', 'document-sync-lf');
        assert(crLf.some((symbol) => symbol && symbol.name === 'CrLfB') &&
               crOnly.some((symbol) => symbol && symbol.name === 'CrOnlyB') &&
               lf.some((symbol) => symbol && symbol.name === 'LfB'),
               `mixed line ending changes must use each change's current content, crlf=${JSON.stringify(crLf)}, cr=${JSON.stringify(crOnly)}, lf=${JSON.stringify(lf)}`);

        // 空变更数组失同步；随后完整替换恢复，再试重复 didOpen 不能覆盖当前 overlay。
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 18 },
            contentChanges: [],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-empty-changes'),
            'empty contentChanges');

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 19 },
            contentChanges: [{ text: 'class DuplicateOpenOriginal { }' }],
        });
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 20,
                text: 'class DuplicateOpenReplacement { }',
            },
        });
        assertNotContentModified(
            await queryHover(client, uri, 'document-sync-duplicate-open'),
            'duplicate didOpen');
        const duplicateOriginal = await queryWorkspaceSymbols(
            client,
            'DuplicateOpenOriginal',
            'document-sync-duplicate-open-original');
        const duplicateReplacement = await queryWorkspaceSymbols(
            client,
            'DuplicateOpenReplacement',
            'document-sync-duplicate-open-replacement');
        assert(duplicateOriginal.some((symbol) => symbol && symbol.name === 'DuplicateOpenOriginal') &&
               duplicateReplacement.length === 0,
               `duplicate didOpen must leave the original overlay intact, original=${JSON.stringify(duplicateOriginal)}, replacement=${JSON.stringify(duplicateReplacement)}`);

        // didSave 携带 text 只触发诊断，不等价于新版本 didChange，也不能创建未打开 overlay。
        client.notify('textDocument/didSave', {
            textDocument: { uri },
            text: 'class DidSaveMustNotReplaceClientSnapshot { }',
        });
        const savedOriginal = await queryWorkspaceSymbols(
            client,
            'DuplicateOpenOriginal',
            'document-sync-save-original');
        const savedReplacement = await queryWorkspaceSymbols(
            client,
            'DidSaveMustNotReplaceClientSnapshot',
            'document-sync-save-replacement');
        assert(savedOriginal.some((symbol) => symbol && symbol.name === 'DuplicateOpenOriginal') &&
               savedReplacement.length === 0,
               `didSave text must not reuse the current version as a didChange, original=${JSON.stringify(savedOriginal)}, replacement=${JSON.stringify(savedReplacement)}`);

        client.notify('textDocument/didSave', {
            textDocument: { uri: unopenedSaveUri },
            text: 'class DidSaveMustNotCreateOverlay { }',
        });
        const unopenedSaveSymbols = await queryWorkspaceSymbols(
            client,
            'DidSaveMustNotCreateOverlay',
            'document-sync-unopened-save');
        assert(unopenedSaveSymbols.length === 0,
               `didSave text for an unopened document must not create an overlay, actual=${JSON.stringify(unopenedSaveSymbols)}`);

        client.notify('textDocument/didChange', {
            textDocument: { uri: unopenedSaveUri, version: 1 },
            contentChanges: [{ text: 'class UnopenedChangeMustFailClosed { }' }],
        });
        assertContentModified(
            await queryHover(client, unopenedSaveUri, 'document-sync-unopened-change'),
            'didChange for an unopened document');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: unopenedSaveUri,
                languageId: 'zr',
                version: 2,
                text: 'class OpenRebuildsDesynchronizedDocument { }',
            },
        });
        assertNotContentModified(
            await queryHover(client, unopenedSaveUri, 'document-sync-open-rebuild'),
            'didOpen after an unopened didChange');
        // 非法 UTF-8 原始帧不得污染文档；更高版本完整替换是恢复入口。
        notifyInvalidUtf8DidChange(client, unopenedSaveUri, 3);
        assertContentModified(
            await queryHover(client, unopenedSaveUri, 'document-sync-invalid-utf8'),
            'invalid UTF-8 didChange');
        client.notify('textDocument/didChange', {
            textDocument: { uri: unopenedSaveUri, version: 4 },
            contentChanges: [{ text: 'class ValidUtf8Recovery { }' }],
        });
        assertNotContentModified(
            await queryHover(client, unopenedSaveUri, 'document-sync-invalid-utf8-recovery'),
            'full-content recovery after invalid UTF-8');
        const invalidPosition = await queryHoverAt(
            client,
            unopenedSaveUri,
            99,
            0,
            'document-sync-invalid-request-position');
        assert((invalidPosition && invalidPosition.error && invalidPosition.error.code === -32602) ||
               (invalidPosition && invalidPosition.result === null),
               `out-of-bounds request positions must not be clamped, actual=${JSON.stringify(invalidPosition)}`);

        // 关闭未索引的虚拟文档后不应再返回旧符号，历史版本也不得残留在索引。
        client.notify('textDocument/didClose', {
            textDocument: { uri },
        });
        const closedVirtualSymbols = await queryDocumentSymbols(
            client,
            uri,
            'document-sync-virtual-close');
        assert(closedVirtualSymbols.length === 0,
               `didClose must remove an unindexed virtual overlay, actual=${JSON.stringify(closedVirtualSymbols)}`);

        const stale = await queryWorkspaceSymbols(
            client,
            'DocumentSyncVersionOne',
            'document-sync-stale');
        assert(stale.length === 0,
               `replaced document content must not remain in the workspace index, actual=${JSON.stringify(stale)}`);
    });
    // 第二进程协商 UTF-8，验证字符偏移与 rangeLength 使用字节而非 UTF-16 单元。
    await withClient(serverPath, async (client) => {
        const capabilities = await initialize(client, {
            general: { positionEncodings: ['utf-8'] },
        });
        const uri = 'file:///stdio-document-sync-utf8.zr';
        const source = 'class Utf8Encoding { let marker = "😀"; }';
        const emojiByteOffset = Buffer.byteLength(source.slice(0, source.indexOf('😀')), 'utf8');

        assert(capabilities.positionEncoding === 'utf-8',
               `initialize must negotiate utf-8, actual=${JSON.stringify(capabilities)}`);
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: source,
            },
        });
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 2 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: emojiByteOffset },
                    end: { line: 0, character: emojiByteOffset + 4 },
                },
                rangeLength: 4,
                text: 'x',
            }],
        });
        assertNotContentModified(
            await queryHover(client, uri, 'document-sync-utf8-range-length'),
            'utf-8 rangeLength');
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 3 },
            contentChanges: [{
                range: {
                    start: { line: 0, character: emojiByteOffset },
                    end: { line: 0, character: emojiByteOffset + 1 },
                },
                rangeLength: 4,
                text: 'y',
            }],
        });
        assertContentModified(
            await queryHover(client, uri, 'document-sync-utf8-range-length-mismatch'),
            'utf-8 rangeLength mismatch');
    });
    // 第三进程以仓内只读项目为根：关闭已索引文件时应恢复磁盘版本，而非清空它。
    await withClient(serverPath, async (client) => {
        await initialize(client, {}, indexedWorkspaceRootUri);
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: indexedFixtureUri,
                languageId: 'zr',
                version: 1,
                text: 'class IndexedOverlayMustBeDiscardedOnClose { }',
            },
        });
        const indexedOverlay = await queryWorkspaceSymbols(
            client,
            'IndexedOverlayMustBeDiscardedOnClose',
            'document-sync-indexed-overlay');
        assert(indexedOverlay.some((symbol) => symbol && symbol.name === 'IndexedOverlayMustBeDiscardedOnClose'),
               `didOpen must expose the indexed file overlay, actual=${JSON.stringify(indexedOverlay)}`);
        client.notify('textDocument/didClose', {
            textDocument: { uri: indexedFixtureUri },
        });
        const indexedDiskSymbols = await queryDocumentSymbols(
            client,
            indexedFixtureUri,
            'document-sync-indexed-close');
        assert(indexedDiskSymbols.some((symbol) => symbol && symbol.name === 'BaseCounter'),
               `didClose must restore an indexed file's disk snapshot, actual=${JSON.stringify(indexedDiskSymbols)}`);
    });
    // 最后在临时真实文件上核对 didSave 无 text 的磁盘刷新分支。
    await withTemporaryDiskDocument(async (diskUri) => {
        await withClient(serverPath, async (client) => {
            await initialize(client);
            client.notify('textDocument/didSave', {
                textDocument: { uri: diskUri },
            });
            const diskSymbols = await queryDocumentSymbols(
                client,
                diskUri,
                'document-sync-save-disk-refresh');
            assert(diskSymbols.some((symbol) => symbol && symbol.name === 'DidSaveRefreshesDiskDocument'),
                   `didSave without text must refresh the disk document generation, actual=${JSON.stringify(diskSymbols)}`);
        });
    });
    console.log('stdio document sync conformance passed');
}

// 所有场景结束后由顶层异常出口把失败交给 CTest。
main().catch((error) => {
    console.error(`stdio document sync conformance failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
