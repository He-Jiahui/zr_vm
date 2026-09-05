const assertStrict = require('assert').strict;
const fs = require('fs');
const path = require('path');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const REQUEST_TIMEOUT_MS = 10000;

const CAPABILITY_PROFILES = {
    textDocumentSync: {
        nativeMarker: 'ZR_LSP_FIELD_TEXT_DOCUMENT_SYNC',
        wasmMarker: 'connection.onDidOpen',
        state: 'implemented',
        owner: 'optimize/01-protocol-lifecycle-and-transport',
    },
    positionEncoding: {
        nativeMarker: 'position_encoding_name',
        wasmMarker: 'connection.onInitialize',
        state: 'implemented',
        owner: 'optimize/01-protocol-lifecycle-and-transport',
    },
    completionProvider: {
        nativeMarker: 'ZR_LSP_FIELD_COMPLETION_PROVIDER',
        wasmMarker: 'connection.onCompletion',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    hoverProvider: {
        nativeMarker: 'ZR_LSP_FIELD_HOVER_PROVIDER',
        wasmMarker: 'connection.onHover',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    signatureHelpProvider: {
        nativeMarker: 'ZR_LSP_FIELD_SIGNATURE_HELP_PROVIDER',
        wasmMarker: 'connection.onSignatureHelp',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    definitionProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DEFINITION_PROVIDER',
        wasmMarker: 'connection.onDefinition',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    referencesProvider: {
        nativeMarker: 'ZR_LSP_FIELD_REFERENCES_PROVIDER',
        wasmMarker: 'connection.onReferences',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    renameProvider: {
        nativeMarker: 'ZR_LSP_FIELD_RENAME_PROVIDER',
        wasmMarker: 'connection.onRenameRequest',
        state: 'implemented',
        owner: 'optimize/02-snapshots-workspaces-and-diagnostics',
    },
    documentSymbolProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DOCUMENT_SYMBOL_PROVIDER',
        wasmMarker: 'connection.onDocumentSymbol',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    workspaceSymbolProvider: {
        nativeMarker: 'ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER',
        wasmMarker: 'connection.onWorkspaceSymbol',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    documentHighlightProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DOCUMENT_HIGHLIGHT_PROVIDER',
        wasmMarker: 'connection.onDocumentHighlight',
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    inlayHintProvider: {
        nativeMarker: 'ZR_LSP_FIELD_INLAY_HINT_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/inlayHint'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    semanticTokensProvider: {
        nativeMarker: 'ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/semanticTokens/full'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    codeActionProvider: {
        nativeMarker: 'ZR_LSP_FIELD_CODE_ACTION_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/codeAction'",
        state: 'implemented',
        owner: 'optimize/02-snapshots-workspaces-and-diagnostics',
    },
    documentFormattingProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DOCUMENT_FORMATTING_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/formatting'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    documentRangeFormattingProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/rangeFormatting'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    documentOnTypeFormattingProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DOCUMENT_ON_TYPE_FORMATTING_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/onTypeFormatting'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    foldingRangeProvider: {
        nativeMarker: 'ZR_LSP_FIELD_FOLDING_RANGE_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/foldingRange'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    selectionRangeProvider: {
        nativeMarker: 'ZR_LSP_FIELD_SELECTION_RANGE_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/selectionRange'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    linkedEditingRangeProvider: {
        nativeMarker: 'ZR_LSP_FIELD_LINKED_EDITING_RANGE_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/linkedEditingRange'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    monikerProvider: {
        nativeMarker: 'ZR_LSP_FIELD_MONIKER_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/moniker'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    inlineValueProvider: {
        nativeMarker: 'ZR_LSP_FIELD_INLINE_VALUE_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/inlineValue'",
        state: 'semantic-payload-gap',
        owner: 'optimize/04-editor-feature-correctness',
    },
    inlineCompletionProvider: {
        nativeMarker: 'ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/inlineCompletion'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    colorProvider: {
        nativeMarker: 'ZR_LSP_FIELD_COLOR_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/documentColor'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    declarationProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DECLARATION_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/declaration'",
        state: 'overclaim-candidate',
        owner: 'optimize/00-baseline-and-contract',
    },
    typeDefinitionProvider: {
        nativeMarker: 'ZR_LSP_FIELD_TYPE_DEFINITION_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/typeDefinition'",
        state: 'overclaim-candidate',
        owner: 'optimize/00-baseline-and-contract',
    },
    implementationProvider: {
        nativeMarker: 'ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/implementation'",
        state: 'overclaim-candidate',
        owner: 'optimize/00-baseline-and-contract',
    },
    callHierarchyProvider: {
        nativeMarker: 'ZR_LSP_FIELD_CALL_HIERARCHY_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/prepareCallHierarchy'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    typeHierarchyProvider: {
        nativeMarker: 'ZR_LSP_FIELD_TYPE_HIERARCHY_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/prepareTypeHierarchy'",
        state: 'implemented',
        owner: 'optimize/04-editor-feature-correctness',
    },
    documentLinkProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/documentLink'",
        state: 'implemented',
        owner: 'optimize/00-baseline-and-contract',
    },
    codeLensProvider: {
        nativeMarker: 'ZR_LSP_FIELD_CODE_LENS_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/codeLens'",
        state: 'implemented',
        owner: 'optimize/00-baseline-and-contract',
    },
    diagnosticProvider: {
        nativeMarker: 'ZR_LSP_FIELD_DIAGNOSTIC_PROVIDER',
        wasmMarker: "connection.onRequest('textDocument/diagnostic'",
        state: 'implemented',
        owner: 'optimize/02-snapshots-workspaces-and-diagnostics',
    },
    workspace: {
        nativeMarker: 'ZR_LSP_FIELD_WORKSPACE',
        wasmMarker: 'connection.onInitialize',
        state: 'workspace-contract-pending',
        owner: 'optimize/02-snapshots-workspaces-and-diagnostics',
    },
};

const RISKY_CONTRACTS = [
    ['workspaceSymbolProvider.resolveProvider', 'identity-only resolve is unsupported'],
    ['inlayHintProvider.resolveProvider', 'identity-only resolve is unsupported'],
    ['documentLinkProvider.resolveProvider', 'identity-only resolve is unsupported'],
    ['codeLensProvider.resolveProvider', 'identity-only resolve is unsupported'],
    ['workspace.workspaceFolders.changeNotifications', 'workspace folder notifications require a handler'],
];

const COMPLETE_INITIAL_RESPONSE_PROVIDERS = [
    'workspaceSymbolProvider',
    'inlayHintProvider',
    'documentLinkProvider',
    'codeLensProvider',
];

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function nestedValue(object, dottedPath) {
    return dottedPath.split('.').reduce((current, key) => {
        if (current === null || typeof current !== 'object') {
            return undefined;
        }
        return current[key];
    }, object);
}

function readSource(sourceRoot, relativePath) {
    const sourcePath = path.join(sourceRoot, relativePath);
    assert(fs.existsSync(sourcePath), `missing source for inventory: ${relativePath}`);
    return fs.readFileSync(sourcePath, 'utf8');
}

async function main() {
    const [serverPath, sourceRoot] = process.argv.slice(2);
    assert(serverPath !== undefined && sourceRoot !== undefined,
           'usage: node stdio_protocol_inventory.js <stdio-server> <source-root>');
    assert(fs.existsSync(serverPath), `stdio server does not exist: ${serverPath}`);

    const initializeSource = readSource(sourceRoot, 'zr_vm_language_server/stdio/stdio_initialize.c');
    const capabilitySource = readSource(sourceRoot, 'zr_vm_language_server/stdio/stdio_initialize_capabilities.c');
    const dispatchSource = readSource(sourceRoot, 'zr_vm_language_server/stdio/stdio_request_dispatch.c');
    const workerSource = readSource(sourceRoot, 'zr_vm_language_server_extension/src/browser/worker/server-worker.ts');
    const nativeSources = `${initializeSource}\n${capabilitySource}\n${dispatchSource}`;
    const client = new StdioProtocolClient(serverPath);

    try {
        const result = await client.requestWithId('initialize', {
            processId: null,
            rootUri: null,
            capabilities: {
                workspace: { workspaceFolders: true },
                textDocument: { semanticTokens: {}, inlayHint: {}, inlineValue: {} },
            },
        }, REQUEST_TIMEOUT_MS).promise;
        assert(result !== null && typeof result === 'object', 'initialize must return an object');
        assert(result.capabilities !== null && typeof result.capabilities === 'object',
               'initialize must return a capabilities object');

        const capabilities = result.capabilities;
        const identityResolveOverclaims = COMPLETE_INITIAL_RESPONSE_PROVIDERS.filter((name) =>
            nestedValue(capabilities, `${name}.resolveProvider`) === true);
        assert(identityResolveOverclaims.length === 0,
               `identity-only resolve must not be advertised: ${identityResolveOverclaims.join(', ')}`);
        for (const name of COMPLETE_INITIAL_RESPONSE_PROVIDERS) {
            const provider = capabilities[name];
            assert(provider === true || (provider !== null && typeof provider === 'object'),
                   `${name} must remain available with complete initial responses`);
        }
        assert(capabilities.codeActionProvider && capabilities.codeActionProvider.resolveProvider === true,
               'native code action resolve must retain snapshot revalidation');
        client.notify('initialized', {});
        for (const method of [
            'workspaceSymbol/resolve', 'inlayHint/resolve',
            'documentLink/resolve', 'codeLens/resolve',
        ]) {
            const id = `withdrawn-${method}`;
            const response = await client.request(method, {}, id, REQUEST_TIMEOUT_MS);
            assertStrict.deepEqual(response, {
                jsonrpc: '2.0',
                id,
                error: { code: -32601, message: 'Method not found' },
            }, `${method} must reject unsupported resolve with MethodNotFound`);
        }
        const declared = Object.keys(capabilities).sort();
        const unclassified = declared.filter((name) => CAPABILITY_PROFILES[name] === undefined);
        const inventory = declared.map((name) => {
            const profile = CAPABILITY_PROFILES[name];
            return {
                capability: name,
                state: profile.state,
                owner: profile.owner,
                nativeMarker: profile.nativeMarker,
                nativeMarkerFound: nativeSources.includes(profile.nativeMarker),
                wasmMarker: profile.wasmMarker,
                wasmMarkerFound: workerSource.includes(profile.wasmMarker),
            };
        });
        const missingNativeMarkers = inventory.filter((entry) => !entry.nativeMarkerFound)
            .map((entry) => entry.capability);
        const wasmGaps = inventory.filter((entry) => !entry.wasmMarkerFound)
            .map((entry) => entry.capability);
        const riskyContracts = RISKY_CONTRACTS.map(([contractPath, reason]) => ({
            contractPath,
            advertised: nestedValue(capabilities, contractPath) === true,
            reason,
            owner: 'optimize/00-baseline-and-contract Task 4',
        }));

        assert(unclassified.length === 0,
               `declared capabilities without an inventory profile: ${unclassified.join(', ')}`);
        assert(missingNativeMarkers.length === 0,
               `declared capabilities without a native source marker: ${missingNativeMarkers.join(', ')}`);
        assert(workerSource.includes('connection.onInitialize') && workerSource.includes('new ZrWasmBridge'),
               'WASM worker must expose its initialization and bridge boundary');

        console.log(JSON.stringify({
            declared,
            inventory,
            riskyContracts,
            wasmGaps,
            status: 'baseline-recorded',
        }, null, 2));
    } finally {
        try {
            await client.requestWithId('shutdown', undefined, REQUEST_TIMEOUT_MS).promise;
            client.notify('exit', undefined);
            client.endInput();
            await client.waitForExit(REQUEST_TIMEOUT_MS);
        } finally {
            await client.terminate();
        }
    }
}

main().catch((error) => {
    console.error(`stdio protocol inventory failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
