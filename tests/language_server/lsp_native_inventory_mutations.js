const assert = require('assert').strict;
const { validateNativeInventory } = require('./lsp_native_inventory_contract');

function checkInventoryMutations(inventory, capabilities, registeredTests, negotiation, wasm) {
    const cases = [
        ['missing primary handler', data => data.inventory.nativeFeatureRoutes.splice(0, 1), /missing native feature route/],
        ['orphan handler', data => data.inventory.nativeFeatureRoutes.push({
            method: 'textDocument/unregistered', handler: 'handle_unregistered_request',
            requiresInlineCompletion: false, requiresRangesFormatting: false,
        }), /without a capability or extension contract/],
        ['wrong handler', data => { data.inventory.nativeFeatureRoutes[0].handler = 'handle_wrong_request'; }, /wrong native handler/],
        ['duplicate route', data => data.inventory.nativeFeatureRoutes.push(data.inventory.nativeFeatureRoutes[0]), /has duplicate/],
        ['unregistered declaration', data => { data.capabilities.unknownProvider = true; }, /unregistered capability/],
        ['missing declaration', data => { delete data.capabilities.hoverProvider; }, /missing native initialize capability/],
        ['wrong token type', data => { data.capabilities.semanticTokensProvider.legend.tokenTypes[0] = 'wrong'; }, /compiled core legend/],
        ['wrong token modifier', data => { data.capabilities.semanticTokensProvider.legend.tokenModifiers = []; }, /native token encoder/],
        ['object prepare flag', data => { data.capabilities.renameProvider.prepareProvider = {}; }, /capability option/],
        ['object delta flag', data => { data.capabilities.semanticTokensProvider.full.delta = {}; }, /capability option/],
        ['object workspace diagnostics flag', data => { data.capabilities.diagnosticProvider.workspaceDiagnostics = {}; }, /capability option/],
        ['missing workspace folders', data => { delete data.capabilities.workspace.workspaceFolders; }, /capability snapshot/],
        ['missing format trigger', data => { delete data.capabilities.documentOnTypeFormattingProvider.firstTriggerCharacter; }, /capability snapshot/],
        ['unnegotiated encoding', data => { data.capabilities.positionEncoding = 'utf-8'; }, /capability snapshot/],
        ['unhandled willSave', data => { data.capabilities.textDocumentSync.willSave = true; }, /supported notifications/],
        ['missing auxiliary handler', data => {
            data.inventory.nativeFeatureRoutes = data.inventory.nativeFeatureRoutes.filter(route => route.method !== 'textDocument/prepareRename');
        }, /missing native feature route/],
        ['wrong optional gate', data => {
            data.inventory.nativeFeatureRoutes.find(route => route.method === 'textDocument/rangesFormatting').requiresRangesFormatting = false;
        }, /incorrect ranges capability gate/],
        ['unregistered test', data => { data.inventory.capabilities[0].testId = 'missing_test'; }, /unregistered test ID/],
        ['duplicate descriptor', data => data.inventory.capabilities.push(data.inventory.capabilities[0]), /has duplicate/],
        ['missing WASM export metadata', data => { data.inventory.capabilities[0].wasmExport = null; }, /inconsistent wasmExport/],
        ['wrong WASM export metadata', data => {
            data.inventory.capabilities.find(row => row.capabilityKey === 'hoverProvider').wasmExport = 'wasm_ZrLspGetCompletion';
        }, /WASM export disagrees with worker/],
        ['nonexistent WASM export metadata', data => {
            data.inventory.capabilities.find(row => row.capabilityKey === 'hoverProvider').wasmExport = 'wasm_missing';
        }, /registry names missing WASM export/],
        ['missing WASM runtime coverage', data => {
            const descriptor = data.inventory.capabilities.find(row => row.capabilityKey === 'hoverProvider');
            descriptor.runtimeMask = 1;
            descriptor.wasmExport = null;
        }, /WASM capabilities disagree with registry/],
        ['missing worker observation', data => {
            data.wasm.worker.featureRoutes = data.wasm.worker.featureRoutes.filter(row => row.method !== 'textDocument/inlayHint');
        }, /missing WASM worker route/],
        ['mismatched worker legend', data => {
            data.wasm.worker.capabilities.semanticTokensProvider.legend.tokenTypes.reverse();
        }, /worker token ordering/],
        ['unknown runtime', data => { data.inventory.capabilities[0].runtimeMask = 4; }, /invalid runtime coverage/],
        ['missing core entry', data => { data.inventory.capabilities[0].coreEntryPoint = null; }, /missing its core entry point/],
        ['identity resolve', data => {
            data.inventory.capabilities.find(row => row.capabilityKey === 'completionProvider').resolveBehavior = 2;
        }, /resolve must do material work/],
        ['resolve overclaim', data => { data.capabilities.documentLinkProvider.resolveProvider = true; }, /resolve publication/],
        ['invalid resolve runtime', data => { data.inventory.capabilities[0].resolveRuntimeMask = 4; }, /invalid resolve runtimes/],
        ['missing experimental marker', data => {
            data.inventory.capabilities.find(row => row.capabilityKey === 'inlineCompletionProvider').isExperimental = false;
        }, /must be experimental/],
    ];
    if (negotiation.rangesFormatting) cases.push([
        'object ranges flag', data => { data.capabilities.documentRangeFormattingProvider.rangesSupport = {}; }, /capability option/,
    ]);
    for (const [name, mutate, error] of cases) {
        const data = JSON.parse(JSON.stringify({ inventory, capabilities, wasm }));
        mutate(data);
        assert.throws(() => validateNativeInventory(data.inventory, data.capabilities, registeredTests, negotiation, data.wasm),
                      error, 'inventory must reject ' + name);
    }
    return cases.length;
}

module.exports = { checkInventoryMutations };
