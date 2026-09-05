const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');

const REQUEST_TIMEOUT_MS = 10000;

function assertRange(range, label) {
    assert.ok(range && range.start && range.end, `${label} must have a complete range`);
    for (const position of [range.start, range.end]) {
        assert.ok(Number.isInteger(position.line) && position.line >= 0 &&
                  Number.isInteger(position.character) && position.character >= 0,
                  `${label} must have valid positions`);
    }
    assert.ok(range.end.line > range.start.line ||
              (range.end.line === range.start.line && range.end.character >= range.start.character),
              `${label} must have an ordered range`);
}

async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_resolve_capabilities_smoke.js <stdio-server>');
    const client = new StdioProtocolClient(serverPath);
    const request = (method, params) => client.requestWithId(method, params, REQUEST_TIMEOUT_MS).promise;
    const probeUri = 'file:///zr-resolve-capabilities-probe.zr';
    const importsUri = 'file:///zr-resolve-capabilities-imports.zr';
    const probeText = [
        '#zr.testing.test#',
        'fn resolveProbe(): void {',
        '    var inferred = 1;',
        '}',
        '',
    ].join('\n');
    const importsText = [
        'let system = import("zr.system");',
        'let math = import("zr.math");',
        '',
    ].join('\n');
    let cleanExit = false;

    try {
        const initialize = await request('initialize', { capabilities: {} });
        assert.ok(initialize && initialize.capabilities, 'initialize must return capabilities');
        client.notify('initialized', {});
        for (const [uri, text] of [[probeUri, probeText], [importsUri, importsText]]) {
            client.notify('textDocument/didOpen', {
                textDocument: { uri, languageId: 'zr', version: 0, text },
            });
            const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics', REQUEST_TIMEOUT_MS);
            assert.equal(diagnostics.uri, uri);
        }

        const links = await request('textDocument/documentLink', { textDocument: { uri: importsUri } });
        assert.ok(Array.isArray(links));
        for (const moduleName of ['zr.system', 'zr.math']) {
            const link = links.find((item) => item.target === `zr-decompiled:/${moduleName}.zr`);
            assert.ok(link, `initial document links must include ${moduleName}`);
            assertRange(link.range, 'document link');
        }

        const lenses = await request('textDocument/codeLens', { textDocument: { uri: probeUri } });
        assert.ok(Array.isArray(lenses));
        const runLens = lenses.find((item) => item.command && item.command.command === 'zr.runCurrentProject');
        assert.ok(runLens, 'initial code lenses must include an executable test command');
        assertRange(runLens.range, 'code lens');
        assert.ok(typeof runLens.command.title === 'string' && runLens.command.title.length > 0);
        assert.ok(Array.isArray(runLens.command.arguments));
        assert.equal(runLens.command.arguments[0], probeUri);

        const hints = await request('textDocument/inlayHint', {
            textDocument: { uri: probeUri },
            range: { start: { line: 0, character: 0 }, end: { line: 4, character: 0 } },
        });
        assert.ok(Array.isArray(hints));
        const hint = hints.find((item) => typeof item.label === 'string' &&
            (item.label === ': int' || item.label.startsWith(': int, ')));
        assert.ok(hint, `initial inlay hints must contain the inferred type label: ${JSON.stringify(hints)}`);
        assert.deepEqual(hint.position, { line: 2, character: 16 });
        assert.equal(hint.kind, 1);

        const symbols = await request('workspace/symbol', { query: 'resolveProbe' });
        assert.ok(Array.isArray(symbols));
        const symbol = symbols.find((item) => item.name === 'resolveProbe');
        assert.ok(symbol && symbol.location, 'initial workspace symbols must contain a location');
        assert.equal(symbol.location.uri, probeUri);
        assertRange(symbol.location.range, 'workspace symbol');
        assert.ok(symbol.location.range.start.line <= 1 && symbol.location.range.end.line >= 1,
                  'workspace symbol range must cover its annotated function declaration');
        console.log('Pass - complete initial documentLink/codeLens/inlayHint/workspaceSymbol payloads');

        const codeActionParams = {
            textDocument: { uri: importsUri },
            range: { start: { line: 0, character: 0 }, end: { line: 3, character: 0 } },
            context: { diagnostics: [], only: ['source.organizeImports'] },
        };
        const actions = await request('textDocument/codeAction', codeActionParams);
        assert.ok(Array.isArray(actions));
        const action = actions.find((item) => item.kind === 'source.organizeImports' && item.edit);
        assert.ok(action && action.data && action.data.snapshot,
                  'initial code actions must include edits and snapshot identity');
        assert.equal(action.data.snapshot.version, 0);
        assert.equal(action.edit.changes, undefined);
        assert.ok(action.edit.documentChanges.some((change) =>
            change.textDocument.uri === importsUri && change.textDocument.version === 0 &&
            change.edits.some((edit) => edit.newText.includes(
                'let math = import("zr.math");\nlet system = import("zr.system");'))));
        const resolved = await request('codeAction/resolve', action);
        assert.deepEqual(resolved.edit, action.edit);
        assert.equal(resolved.disabled, undefined);

        client.notify('textDocument/didChange', {
            textDocument: { uri: importsUri, version: 1 },
            contentChanges: [{ text: importsText }],
        });
        const stale = await request('codeAction/resolve', action);
        assert.equal(stale.edit, undefined, 'stale code actions must remove their edits');
        assert.deepEqual(stale.disabled, { reason: 'Document changed since this code action was computed' });
        const freshActions = await request('textDocument/codeAction', codeActionParams);
        const fresh = freshActions.find((item) => item.kind === 'source.organizeImports' && item.edit);
        assert.ok(fresh && fresh.data && fresh.data.snapshot);
        assert.equal(fresh.data.snapshot.version, 1);
        const freshResolved = await request('codeAction/resolve', fresh);
        assert.deepEqual(freshResolved.edit, fresh.edit);
        assert.equal(freshResolved.disabled, undefined);
        console.log('Pass - native codeAction resolve revalidates current, stale and refreshed snapshots');

        const capabilities = initialize.capabilities;
        for (const name of ['documentLinkProvider', 'codeLensProvider', 'inlayHintProvider', 'workspaceSymbolProvider']) {
            const provider = capabilities[name];
            assert.ok(provider === true || (provider !== null && typeof provider === 'object'), name);
            assert.notEqual(provider.resolveProvider, true, `${name} must not advertise identity resolve`);
        }
        assert.equal(capabilities.codeActionProvider.resolveProvider, true);
        assert.equal(capabilities.completionProvider.resolveProvider, true);
        for (const [method, payload] of [
            ['documentLink/resolve', links[0]],
            ['codeLens/resolve', runLens],
            ['inlayHint/resolve', hint],
            ['workspaceSymbol/resolve', symbol],
        ]) {
            const id = `withdrawn-${method}`;
            const response = await client.request(method, payload, id, REQUEST_TIMEOUT_MS);
            assert.deepEqual(response, {
                jsonrpc: '2.0', id, error: { code: -32601, message: 'Method not found' },
            }, `${method} must return MethodNotFound`);
        }
        console.log('Pass - initialize and dispatch withdraw identity resolve while retaining material resolve');

        assert.equal(await request('shutdown', undefined), null);
        client.notify('exit', undefined);
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0, client.stderr());
        assert.equal(client.stderr().trim(), '', 'stdio stderr must remain empty');
        cleanExit = true;
    } finally {
        if (!cleanExit) {
            await client.terminate();
        }
    }
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
