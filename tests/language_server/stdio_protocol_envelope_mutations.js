const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');
const { protocolCases } = require('./stdio_protocol_conformance');

// 与正向协议套件共用真实服务器场景，变异结果才能约束同一个响应入口。
const cases = new Map(protocolCases());
// 每项只破坏一个信封契约；元组中的 id 与成员定位要与原用例响应一致。
const mutations = [
    ['duplicate error missing version', 'duplicate request id', 'duplicate-request', 'error',
     (response) => { delete response.jsonrpc; }],
    ['duplicate error includes result', 'duplicate request id', 'duplicate-request', 'error',
     (response) => { response.result = []; }],
    ['duplicate success wrong version', 'duplicate request id', 'duplicate-request', 'result',
     (response) => { response.jsonrpc = '1.0'; }],
    ['duplicate success includes error', 'duplicate request id', 'duplicate-request', 'result',
     (response) => { response.error = null; }],
    ['numeric success missing version', 'distinct typed request ids', 1, 'result',
     (response) => { delete response.jsonrpc; }],
    ['string success missing result', 'distinct typed request ids', '1', 'result',
     (response) => { delete response.result; }],
    ['shutdown success wrong version', 'request after shutdown', 'shutdown', 'result',
     (response) => { response.jsonrpc = '1.0'; }],
    ['error missing message', 'unknown method', 'unknown-method', 'error',
     (response) => { delete response.error.message; }],
    ['error non-string message', 'unknown method', 'unknown-method', 'error',
     (response) => { response.error.message = 17; }],
    ['work-done success missing version', 'request work-done progress', 'work-done-string', 'result',
     (response) => { delete response.jsonrpc; }],
    ['partial success includes error', 'workspace symbol partial results', 'workspace-symbol-partial', 'result',
     (response) => { response.error = null; }],
];

/**
 * 临时替换进程内共享的 dispatch，只改写目标响应后运行原正向用例。
 * monkey patch 在 finally 中恢复，因此此用例必须串行执行，不能并行共享客户端类。
 */
async function rejectsMutatedEnvelope(serverPath, fixture) {
    const [label, caseName, id, member, mutate] = fixture;
    const originalDispatch = StdioProtocolClient.prototype.dispatch;
    let injected = 0;
    let failure;
    // 在客户端分派前改写真实响应，使正向用例必须以信封错误而非超时失败。
    StdioProtocolClient.prototype.dispatch = function (response) {
        if (response && response.id === id &&
            Object.prototype.hasOwnProperty.call(response, member)) {
            mutate(response);
            injected++;
        }
        originalDispatch.call(this, response);
    };
    try {
        await cases.get(caseName)(serverPath);
    } catch (error) {
        failure = error;
    } finally {
        StdioProtocolClient.prototype.dispatch = originalDispatch;
    }
    assert.equal(injected, 1, `${label}: must mutate exactly one real server response`);
    assert.ok(failure, `${label}: conformance accepted an invalid envelope`);
    assert.match(failure.message, /jsonrpc|envelope|message must|must contain result/,
                 `${label}: failure must identify the envelope, not a timeout or semantic assertion`);
}

/** 先运行未变异对照，再逐一核对非法信封由协议断言而非超时拒绝。 */
async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_protocol_envelope_mutations.js <stdio-server>');
    for (const caseName of new Set(mutations.map((fixture) => fixture[1]))) {
        await cases.get(caseName)(serverPath);
        console.log(`Pass - unmodified ${caseName}`);
    }
    let failures = 0;
    for (const fixture of mutations) {
        try {
            await rejectsMutatedEnvelope(serverPath, fixture);
            console.log(`Pass - rejects ${fixture[0]}`);
        } catch (error) {
            failures++;
            console.error(`Fail - ${fixture[0]}: ${error.message}`);
        }
    }
    assert.equal(failures, 0, `${failures}/${mutations.length} envelope mutations escaped conformance`);
    console.log(`Protocol envelope mutations: ${mutations.length}/${mutations.length} rejected`);
}

/** 让所有正向对照或负例断言失败转为 CTest 可见的退出码。 */
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
