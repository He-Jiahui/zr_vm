const assert = require('assert').strict;
const { awaitLspRequestOutcome } = require('./stdio_smoke');

async function main() {
    const result = { items: [] };
    assert.deepEqual(await awaitLspRequestOutcome(Promise.resolve(result)), { result, error: null });
    console.log('Pass - successful LSP results retain their value');

    const error = { code: -32800, message: 'Request cancelled', data: { id: 9 } };
    assert.deepEqual(await awaitLspRequestOutcome(Promise.reject(new Error(JSON.stringify(error)))),
        { result: null, error });
    console.log('Pass - protocol errors retain structured code and data');

    const timeout = new Error('timed out waiting for response id=47 stderr=server evidence');
    await assert.rejects(awaitLspRequestOutcome(Promise.reject(timeout)), (error) => error === timeout);
    console.log('Pass - transport timeout retains its original exception and request evidence');

    const closed = new Error('server closed: exitCode=1 signal=null stderr=server evidence');
    await assert.rejects(awaitLspRequestOutcome(Promise.reject(closed)), (error) => error === closed);
    console.log('Pass - closed transport retains its original exception');
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
