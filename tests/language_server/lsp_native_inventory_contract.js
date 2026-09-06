const assert = require('assert').strict;
const { expectedCapabilities } = require('./stdio_capability_snapshot');

const NATIVE = 1;
const WASM = 2;
const MATERIAL_RESOLVE = 1;
const CORE_IMPLEMENTATION = 1;
const ADAPTER_IMPLEMENTATION = 2;

const CONTROL_METHODS = new Map([
    ['initialize', ['positionEncoding', 'handle_initialize_request']],
    ['textDocument/didChange', ['textDocumentSync', 'handle_did_change']],
    ['workspace/didChangeWorkspaceFolders', ['workspace', 'handle_did_change_workspace_folders']],
]);

const AUXILIARY_ROUTES = [
    ['completionItem/resolve', 'completionProvider', 'resolveProvider', 'handle_completion_item_resolve_request'],
    ['textDocument/rangesFormatting', 'documentRangeFormattingProvider', 'rangesSupport', 'handle_ranges_formatting_request'],
    ['textDocument/willSaveWaitUntil', 'textDocumentSync', 'willSaveWaitUntil', 'handle_formatting_request'],
    ['codeAction/resolve', 'codeActionProvider', 'resolveProvider', 'handle_code_action_resolve_request'],
    ['callHierarchy/incomingCalls', 'callHierarchyProvider', '', 'handle_call_hierarchy_incoming_calls_request'],
    ['callHierarchy/outgoingCalls', 'callHierarchyProvider', '', 'handle_call_hierarchy_outgoing_calls_request'],
    ['typeHierarchy/supertypes', 'typeHierarchyProvider', '', 'handle_type_hierarchy_supertypes_request'],
    ['typeHierarchy/subtypes', 'typeHierarchyProvider', '', 'handle_type_hierarchy_subtypes_request'],
    ['workspace/diagnostic', 'diagnosticProvider', 'workspaceDiagnostics', 'handle_workspace_diagnostic_request'],
    ['workspace/willRenameFiles', 'workspace', 'fileOperations.willRename', 'handle_will_rename_files_request'],
    ['textDocument/semanticTokens/full/delta', 'semanticTokensProvider', 'full.delta', 'handle_semantic_tokens_full_delta_request'],
    ['textDocument/semanticTokens/range', 'semanticTokensProvider', 'range', 'handle_semantic_tokens_range_request'],
    ['textDocument/prepareRename', 'renameProvider', 'prepareProvider', 'handle_prepare_rename_request'],
];

const EXTENSION_ROUTES = [
    ['zr/richHover', 'handle_rich_hover_request'],
    ['zr/nativeDeclarationDocument', 'handle_native_declaration_document_request'],
    ['zr/projectModules', 'handle_project_modules_request'],
];

function present(value) {
    return typeof value === 'string' && value.length > 0;
}

function nested(object, dottedPath) {
    return dottedPath.split('.').reduce((value, key) =>
        value !== null && typeof value === 'object' ? value[key] : undefined, object);
}

function enabled(value) {
    return value === true || (value !== null && typeof value === 'object' && !Array.isArray(value));
}

function uniqueMap(rows, key, name) {
    assert.ok(Array.isArray(rows) && rows.length > 0, name + ' must be a nonempty array');
    const result = new Map();
    for (const row of rows) {
        assert.ok(row && present(row[key]), name + ' has an invalid identity');
        assert.equal(result.has(row[key]), false, name + ' has duplicate ' + row[key]);
        result.set(row[key], row);
    }
    return result;
}

function validateMetadata(descriptor, registeredTests) {
    const key = descriptor.capabilityKey;
    assert.ok(present(descriptor.method) && present(descriptor.clientCapabilityPath), key + ' is missing protocol metadata');
    assert.ok(present(descriptor.testId) && registeredTests.has(descriptor.testId), key + ' has an unregistered test ID');
    assert.ok(Number.isInteger(descriptor.runtimeMask) && descriptor.runtimeMask > 0 &&
              (descriptor.runtimeMask & ~(NATIVE | WASM)) === 0, key + ' has invalid runtime coverage');
    for (const [runtime, field] of [[NATIVE, 'nativeAdapter'], [WASM, 'wasmExport']]) {
        assert.ok((descriptor.runtimeMask & runtime) ? present(descriptor[field]) : descriptor[field] === null,
                  key + ' has inconsistent ' + field);
    }
    if (descriptor.implementationLayer === CORE_IMPLEMENTATION) {
        assert.ok(present(descriptor.coreEntryPoint), key + ' is missing its core entry point');
    } else {
        assert.equal(descriptor.implementationLayer, ADAPTER_IMPLEMENTATION, key + ' has unknown implementation ownership');
        assert.equal(descriptor.coreEntryPoint, null, key + ' adapter must not invent a core entry point');
        assert.equal(descriptor.runtimeMask, NATIVE, key + ' adapter must be native-only');
    }
    assert.equal(descriptor.minimumMajor, 3, key + ' is outside the protocol baseline');
    assert.ok(descriptor.minimumMinor === 17 || descriptor.minimumMinor === 18, key + ' has an unknown protocol version');
    assert.equal(typeof descriptor.isExperimental, 'boolean', key + ' experimental flag must be boolean');
    assert.ok(descriptor.minimumMinor < 18 || descriptor.isExperimental, key + ' 3.18 capability must be experimental');
    assert.equal(typeof descriptor.hasResolve, 'boolean', key + ' resolve flag must be boolean');
    assert.ok(Number.isInteger(descriptor.resolveRuntimeMask) && descriptor.resolveRuntimeMask >= 0 &&
              (descriptor.resolveRuntimeMask & ~descriptor.runtimeMask) === 0, key + ' has invalid resolve runtimes');
    if (descriptor.hasResolve) {
        assert.equal(descriptor.resolveBehavior, MATERIAL_RESOLVE, key + ' resolve must do material work');
        assert.ok(descriptor.resolveRuntimeMask !== 0, key + ' has no resolve runtime');
    } else {
        assert.equal(descriptor.resolveBehavior, 0, key + ' has contradictory resolve behavior');
        assert.equal(descriptor.resolveRuntimeMask, 0, key + ' has contradictory resolve coverage');
    }
}

function validateWasmRegistryMapping(descriptors, inventory, wasm) {
    assert.ok(wasm && wasm.schemaVersion === 2, 'WASM adapter evidence is required');
    const worker = wasm.worker;
    assert.ok(worker && worker.mockedWasm === true && worker.workerAssetLoaded === false,
              'WASM adapter evidence must identify its mocked execution boundary');
    const routes = uniqueMap(worker.featureRoutes.concat(worker.documentRoutes), 'method', 'WASM worker routes');
    const expectedKeys = [];
    for (const descriptor of descriptors.values()) {
        if (!(descriptor.runtimeMask & WASM)) continue;
        expectedKeys.push(descriptor.capabilityKey);
        const route = routes.get(descriptor.method);
        assert.ok(route, 'missing WASM worker route for ' + descriptor.method);
        assert.ok(wasm.runtimeExportNames.includes(descriptor.wasmExport),
                  'registry names missing WASM export ' + descriptor.wasmExport);
        assert.ok(route.exportNames.includes(descriptor.wasmExport),
                  'registry WASM export disagrees with worker for ' + descriptor.method);
        const provider = worker.capabilities[descriptor.capabilityKey];
        assert.ok(provider !== undefined, 'missing WASM initialize capability ' + descriptor.capabilityKey);
        assert.equal(provider.resolveProvider === true, Boolean(descriptor.resolveRuntimeMask & WASM),
                     'WASM resolve publication disagrees with registry ' + descriptor.capabilityKey);
    }
    assert.deepEqual(Object.keys(worker.capabilities).sort(), expectedKeys.sort(),
                     'WASM capabilities disagree with registry runtime coverage');
    assert.deepEqual(worker.capabilities.semanticTokensProvider.legend.tokenTypes, inventory.semanticTokenTypes,
                     'worker token ordering disagrees with compiled core legend');
    return expectedKeys.length;
}

function validateNativeInventory(inventory, capabilities, registeredTests, negotiation, wasm) {
    assert.equal(inventory.schemaVersion, 1, 'unsupported compiled inventory schema');
    const descriptors = uniqueMap(inventory.capabilities, 'capabilityKey', 'registry');
    const routes = uniqueMap(inventory.nativeFeatureRoutes, 'method', 'native routes');
    const assigned = new Set();
    const declared = Object.keys(capabilities).sort();
    for (const key of declared) {
        assert.ok(descriptors.has(key), 'initialize declares unregistered capability ' + key);
    }
    function assign(method, handler) {
        assert.equal(assigned.has(method), false, 'native route is assigned twice: ' + method);
        const route = routes.get(method);
        assert.ok(route, 'missing native feature route ' + method);
        assert.equal(route.handler, handler, 'wrong native handler for ' + method);
        assert.equal(route.requiresInlineCompletion, method === 'textDocument/inlineCompletion',
                     'incorrect inline capability gate for ' + method);
        assert.equal(route.requiresRangesFormatting, method === 'textDocument/rangesFormatting',
                     'incorrect ranges capability gate for ' + method);
        assigned.add(method);
    }
    for (const descriptor of descriptors.values()) {
        validateMetadata(descriptor, registeredTests);
        const key = descriptor.capabilityKey;
        const provider = capabilities[key];
        if (!(descriptor.runtimeMask & NATIVE)) {
            assert.equal(provider, undefined, key + ' is unavailable in native');
            continue;
        }
        const expected = key !== 'inlineCompletionProvider' || negotiation.inlineCompletion;
        if (expected) {
            assert.ok(key === 'positionEncoding' ? ['utf-8', 'utf-16'].includes(provider) : enabled(provider),
                      'missing native initialize capability ' + key);
        } else {
            assert.equal(provider, undefined, key + ' must be negotiated');
        }
        if (provider !== undefined) {
            assert.equal(provider.resolveProvider === true, Boolean(descriptor.resolveRuntimeMask & NATIVE),
                         key + ' resolve publication disagrees with registry');
        }
        const control = CONTROL_METHODS.get(descriptor.method);
        if (control) {
            assert.deepEqual([key, descriptor.nativeAdapter], control, 'incorrect control-plane descriptor');
        } else {
            assign(descriptor.method, descriptor.nativeAdapter);
        }
    }
    for (const [method, key, option, handler] of AUXILIARY_ROUTES) {
        const descriptor = descriptors.get(key);
        assert.ok(descriptor && (descriptor.runtimeMask & NATIVE), method + ' has no native capability owner');
        const value = option ? nested(capabilities[key], option) : capabilities[key];
        const expected = method !== 'textDocument/rangesFormatting' || negotiation.rangesFormatting;
        if (option && method !== 'workspace/willRenameFiles') {
            assert.equal(value, expected ? true : undefined,
                         method + ' publication disagrees with its boolean capability option');
        } else {
            assert.equal(enabled(value), expected, method + ' publication disagrees with its capability option');
        }
        assign(method, handler);
    }
    for (const [method, handler] of EXTENSION_ROUTES) assign(method, handler);
    const orphaned = Array.from(routes.keys()).filter(method => !assigned.has(method));
    assert.deepEqual(orphaned, [], 'native feature routes without a capability or extension contract');
    assert.deepEqual(capabilities.textDocumentSync, {
        openClose: true, change: 2, willSaveWaitUntil: true, save: { includeText: false },
    }, 'document sync publication must match supported notifications and requests');
    assert.ok(Array.isArray(inventory.semanticTokenTypes) && inventory.semanticTokenTypes.length > 0,
              'compiled token legend is missing');
    assert.equal(new Set(inventory.semanticTokenTypes).size, inventory.semanticTokenTypes.length, 'duplicate token type');
    assert.deepEqual(capabilities.semanticTokensProvider.legend.tokenTypes, inventory.semanticTokenTypes,
                     'initialize token types disagree with the compiled core legend');
    assert.deepEqual(capabilities.semanticTokensProvider.legend.tokenModifiers, ['declaration'],
                     'initialize token modifiers disagree with the native token encoder');
    assert.deepEqual(capabilities, expectedCapabilities(negotiation.inlineCompletion, negotiation.rangesFormatting),
                     'initialize capability snapshot disagrees with the negotiated contract');
    const wasmRegistryEntries = validateWasmRegistryMapping(descriptors, inventory, wasm);
    return {
        registryEntries: descriptors.size,
        wasmRegistryEntries,
        declared,
        featureRoutes: routes.size,
        controlDescriptorMetadataOnly: CONTROL_METHODS.size,
        orphaned,
        status: 'native-contract-mapped',
    };
}

module.exports = { validateNativeInventory };
