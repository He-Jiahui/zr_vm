const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');
const { protocolCases } = require('./stdio_protocol_conformance');

const cases = new Map(protocolCases());
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

async function rejectsMutatedEnvelope(serverPath, fixture) {
    const [label, caseName, id, member, mutate] = fixture;
    const originalDispatch = StdioProtocolClient.prototype.dispatch;
    let injected = 0;
    let failure;
    // Alter decoded server output so the production case must reject the bad envelope.
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

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
