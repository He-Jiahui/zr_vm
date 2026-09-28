const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');

// 两种客户端能力档案都须在同一等待窗口内完成独立的服务端握手。
const REQUEST_TIMEOUT_MS = 10000;

// 命令属于扩展客户端而非 stdio 服务端；每种客户端能力声明都必须得到相同拒绝结果。
// 本函数独占一个子进程，完成 shutdown/exit 才算档案通过，异常时由 finally 回收。
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
        // 广告能力与实际分派必须一致：不公布命令 provider，也不承诺 CodeLens resolve。
        await check('no server command provider', async () => {
            assert.equal(Object.prototype.hasOwnProperty.call(
                initialized.result.capabilities, 'executeCommandProvider'), false);
            assert.equal(initialized.result.capabilities.codeLensProvider.resolveProvider, false);
        });
        // 已知客户端命令和未知命令均走未实现方法响应，不能被服务端代执行。
        for (const command of ['zr.runCurrentProject', 'zr.showReferences', 'zr.unknown']) {
            await check(command + ' is not a server command', async () => {
                assert.deepEqual(await client.request('workspace/executeCommand', {
                    command, arguments: ['file:///client-command-probe.zr'],
                }, command, REQUEST_TIMEOUT_MS), {
                    jsonrpc: '2.0', id: command, error: { code: -32601, message: 'Method not found' },
                });
            });
        }
        // 每个档案都验证完整协议收尾，防止能力断言通过但服务端留在运行态。
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

// CTest 入口对空能力和显式命令能力分别运行档案，累积软断言后统一失败。
async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_client_commands_smoke.js <stdio-server>');
    // 单项失败继续测试另一个档案，但最终退出码必须反映全部失败数。
    let checks = 0;
    let failures = 0;
    for (const [name, capabilities] of [
        ['empty client', {}],
        ['command-aware client', { workspace: { executeCommand: { dynamicRegistration: false } } }],
    ]) {
        // 以档案名标记每个失败，避免一个断言中断后续命令边界检查。
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

// 顶层异常包括握手和进程回收错误；反馈给 CTest 的进程退出码。
main().catch(error => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
