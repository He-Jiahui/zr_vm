const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');

const REQUEST_TIMEOUT_MS = 10000;

async function checkProfile(serverPath, capabilities, check) {
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;
    try {
        const initialized = await client.request('initialize', { capabilities }, 'initialize', REQUEST_TIMEOUT_MS);
        assert.equal(initialized.jsonrpc, '2.0');
        assert.equal(initialized.id, 'initialize');
        assert.equal(initialized.error, undefined);
        assert.ok(initialized.result && initialized.result.capabilities);
        client.notify('initialized', {});
        await check('no server command provider', async () => {
            assert.equal(Object.prototype.hasOwnProperty.call(
                initialized.result.capabilities, 'executeCommandProvider'), false);
            assert.equal(initialized.result.capabilities.codeLensProvider.resolveProvider, false);
        });
        for (const command of ['zr.runCurrentProject', 'zr.showReferences', 'zr.unknown']) {
            await check(command + ' is not a server command', async () => {
                assert.deepEqual(await client.request('workspace/executeCommand', {
                    command, arguments: ['file:///client-command-probe.zr'],
                }, command, REQUEST_TIMEOUT_MS), {
                    jsonrpc: '2.0', id: command, error: { code: -32601, message: 'Method not found' },
                });
            });
        }
        assert.deepEqual(await client.request('shutdown', undefined, 'shutdown', REQUEST_TIMEOUT_MS), {
            jsonrpc: '2.0', id: 'shutdown', result: null,
        });
        client.notify('exit');
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0, client.stderr());
        assert.equal(client.stderr().trim(), '');
        cleanExit = true;
    } finally {
        if (!cleanExit) await client.terminate();
    }
}

async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_client_commands_smoke.js <stdio-server>');
    let checks = 0;
    let failures = 0;
    for (const [name, capabilities] of [
        ['empty client', {}],
        ['command-aware client', { workspace: { executeCommand: { dynamicRegistration: false } } }],
    ]) {
        const check = async (label, run) => {
            checks++;
            try {
                await run();
                console.log(`Pass - ${name}: ${label}`);
            } catch (error) {
                failures++;
                console.error(`Fail - ${name}: ${label}\n${error.stack || String(error)}`);
            }
        };
        await check('protocol lifecycle', () => checkProfile(serverPath, capabilities, check));
    }
    assert.equal(failures, 0, `${failures}/${checks} client command checks failed`);
    console.log(`Pass - ${checks}/${checks} client command checks`);
}

main().catch(error => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
