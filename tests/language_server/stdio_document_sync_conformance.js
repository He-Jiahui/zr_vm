const { StdioProtocolClient } = require('./stdio_protocol_client');
const fs = require('fs');

const RESPONSE_TIMEOUT_MS = 3000;

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    try {
        return await run(client);
    } finally {
        await client.terminate().catch(() => {});
    }
}

async function withTemporaryDiskDocument(run) {
    const path = `/tmp/zr-vm-document-sync-${process.pid}.zr`;

    fs.writeFileSync(path, 'struct DidSaveRefreshesDiskDocument { pub var value: int; }', 'utf8');
    try {
        return await run(`file://${path}`);
    } finally {
        if (fs.existsSync(path)) {
            fs.unlinkSync(path);
        }
    }
}

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

async function queryWorkspaceSymbols(client, query, id) {
    const response = await client.request('workspace/symbol', { query }, id, RESPONSE_TIMEOUT_MS);
    assert(response && !response.error && Array.isArray(response.result),
           `workspace/symbol must return a normal result, actual=${JSON.stringify(response)}`);
    return response.result;
}

async function queryDocumentSymbols(client, uri, id) {
    const response = await client.request('textDocument/documentSymbol', {
        textDocument: { uri },
    }, id, RESPONSE_TIMEOUT_MS);
    assert(response && !response.error && Array.isArray(response.result),
           `textDocument/documentSymbol must return an array, actual=${JSON.stringify(response)}`);
    return response.result;
}

async function queryHover(client, uri, id) {
    return queryHoverAt(client, uri, 0, 7, id);
}

async function queryHoverAt(client, uri, line, character, id) {
    return client.request('textDocument/hover', {
        textDocument: { uri },
        position: { line, character },
    }, id, RESPONSE_TIMEOUT_MS);
}

function assertContentModified(response, label) {
    assert(response && response.error && response.error.code === -32801,
           `${label} must fail closed with ContentModified, actual=${JSON.stringify(response)}`);
}

function assertNotContentModified(response, label) {
    assert(!(response && response.error && response.error.code === -32801),
           `${label} must leave the document synchronized, actual=${JSON.stringify(response)}`);
}

function notifyInvalidUtf8DidChange(client, uri, version) {
    const prefix = Buffer.from(
        `{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":${JSON.stringify(uri)},"version":${version}},"contentChanges":[{"text":"class InvalidUtf8`,
        'ascii');
    const suffix = Buffer.from(' { }"}]}}', 'ascii');
    const body = Buffer.concat([prefix, Buffer.from([0xc3]), suffix]);
    const header = Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii');

    client.sendRawFrame(Buffer.concat([header, body]));
}

async function main() {
    const serverPath = process.argv[2];
    const uri = 'file:///stdio-document-sync-conformance.zr';
    const invalidOpenUri = 'file:///stdio-document-sync-invalid-open.zr';
    const missingTextOpenUri = 'file:///stdio-document-sync-missing-text-open.zr';
    const unopenedSaveUri = 'file:///stdio-document-sync-unopened-save.zr';
    const indexedWorkspaceRootUri = 'file:///mnt/e/Git/zr_vm/tests/fixtures/projects/classes';
    const indexedFixtureUri = 'file:///mnt/e/Git/zr_vm/tests/fixtures/projects/classes/src/main.zr';
    const versionTwoText = 'class DocumentSyncVersionTwo { }';

    assert(serverPath, 'usage: node stdio_document_sync_conformance.js <stdio-server>');
    await withClient(serverPath, async (client) => {
        await initialize(client);
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

main().catch((error) => {
    console.error(`stdio document sync conformance failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
