const assert = require('assert').strict;
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { StdioProtocolClient } = require('./stdio_protocol_client');
const { validateNativeInventory } = require('./lsp_native_inventory_contract');
const { checkInventoryMutations } = require('./lsp_native_inventory_mutations');

const REQUEST_TIMEOUT_MS = 10000;

function runJson(command, args) {
    const result = spawnSync(command, args, {
        encoding: 'utf8', timeout: 30000, maxBuffer: 8 * 1024 * 1024, windowsHide: true,
    });
    assert.ifError(result.error);
    assert.equal(result.status, 0, command + ' failed: ' + result.stderr);
    return JSON.parse(result.stdout);
}

async function inspectProfile(serverPath, profile, inventory, registeredTests) {
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;
    try {
        const initialized = await client.request('initialize', {
            capabilities: {
                workspace: { workspaceFolders: true },
                textDocument: Object.assign({},
                    profile.inlineCompletion ? { inlineCompletion: {} } : {},
                    profile.rangesFormatting ? { rangeFormatting: { rangesSupport: true } } : {}),
            },
        }, 'initialize', REQUEST_TIMEOUT_MS);
        assert.equal(initialized.jsonrpc, '2.0');
        assert.equal(initialized.id, 'initialize');
        assert.equal(initialized.error, undefined);
        assert.ok(initialized.result && initialized.result.capabilities);
        const capabilities = initialized.result.capabilities;
        const report = validateNativeInventory(inventory, capabilities, registeredTests, profile);
        const mutations = checkInventoryMutations(inventory, capabilities, registeredTests, profile);
        client.notify('initialized', {});
        for (const method of [
            'workspaceSymbol/resolve', 'inlayHint/resolve', 'documentLink/resolve', 'codeLens/resolve',
            'textDocument/declaration', 'textDocument/typeDefinition',
            'textDocument/documentColor', 'textDocument/colorPresentation',
        ]) {
            const id = 'withdrawn-' + method;
            assert.deepEqual(await client.request(method, {}, id, REQUEST_TIMEOUT_MS), {
                jsonrpc: '2.0', id, error: { code: -32601, message: 'Method not found' },
            }, method + ' must remain unsupported');
        }
        for (const command of ['zr.runCurrentProject', 'zr.showReferences', 'zr.unknown']) {
            const id = 'client-command-' + command;
            assert.deepEqual(await client.request('workspace/executeCommand', {
                command, arguments: ['file:///inventory.zr'],
            }, id, REQUEST_TIMEOUT_MS), {
                jsonrpc: '2.0', id, error: { code: -32601, message: 'Method not found' },
            }, 'client-owned command must not receive a server no-op acknowledgement');
        }
        assert.deepEqual(await client.request('shutdown', undefined, 'shutdown', REQUEST_TIMEOUT_MS), {
            jsonrpc: '2.0', id: 'shutdown', result: null,
        });
        client.notify('exit');
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0);
        assert.equal(client.stderr().trim(), '');
        cleanExit = true;
        return Object.assign({ profile: profile.name, rejectedMutations: mutations }, report);
    } finally {
        if (!cleanExit) await client.terminate();
    }
}

async function main() {
    const [serverPath, probePath, buildDirectory, ctestPath, configuration] = process.argv.slice(2);
    assert.ok(serverPath && probePath && buildDirectory && ctestPath,
              'usage: node stdio_protocol_inventory.js <stdio-server> <inventory-probe> <build-dir> <ctest> [configuration]');
    assert.ok([serverPath, probePath, ctestPath].every(file => fs.existsSync(file)), 'inventory executables must exist');
    const inventory = runJson(probePath, []);
    const repositoryRoot = path.resolve(__dirname, '..', '..');
    const wasmInventory = runJson(process.execPath, [
        path.join(__dirname, 'wasm_capability_inventory.js'), repositoryRoot,
    ]);
    assert.equal(wasmInventory.schemaVersion, 1, 'unsupported WASM inventory schema');
    assert.ok(['wasm-static-contract-mapped', 'wasm-linked-contract-mapped'].includes(wasmInventory.status),
              'WASM capability inventory did not produce a mapped contract');
    const ctest = runJson(ctestPath, ['--test-dir', buildDirectory, '--show-only=json-v1'].concat(
        configuration ? ['-C', configuration] : []));
    assert.ok(Array.isArray(ctest.tests) && ctest.tests.length > 0, 'configured CTest inventory must be nonempty');
    const registeredTests = new Set(ctest.tests.filter(test =>
        Array.isArray(test.command) && test.command.length > 0).map(test => test.name));
    const profiles = [
        { name: '3.17', inlineCompletion: false, rangesFormatting: false },
        { name: 'inline-only', inlineCompletion: true, rangesFormatting: false },
        { name: 'ranges-only', inlineCompletion: false, rangesFormatting: true },
        { name: 'both-3.18', inlineCompletion: true, rangesFormatting: true },
    ];
    const reports = [];
    const failures = [];
    for (const profile of profiles) {
        try {
            reports.push(await inspectProfile(serverPath, profile, inventory, registeredTests));
        } catch (error) {
            failures.push({ profile: profile.name, error: error.stack || String(error) });
        }
    }
    const remaining = ['native control and notification routing'];
    if (!wasmInventory.linkedAssetChecked) {
        remaining.push('WASM linked export table and worker asset loading');
    }
    remaining.push('complete behavioral and integrated semantic acceptance');
    console.log(JSON.stringify({
        status: failures.length ? 'integrated-contract-failed' : 'integrated-contract-mapped',
        reports, wasm: wasmInventory, failures,
        remaining,
    }, null, 2));
    assert.equal(failures.length, 0, 'compiled native inventory profile failures');
}

main().catch(error => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
