const test = require('node:test');
const assert = require('assert').strict;
const fs = require('fs');
const path = require('path');

test('browser diagnostics delegate identity and workspace enumeration to the WASM bridge', () => {
    const worker = fs.readFileSync(
        path.join(__dirname, '..', 'src', 'browser', 'worker', 'server-worker.ts'),
        'utf8',
    );

    assert.doesNotMatch(worker, /function createDiagnosticResultId\(/);
    assert.doesNotMatch(worker, /function hashText\(/);
    assert.match(worker, /bridge\.getDiagnosticReport\(/);
    assert.match(worker, /bridge\.getWorkspaceDiagnosticReports\(/);
    assert.match(worker, /publishedDiagnosticResultIds/);
    assert.match(worker, /published\?\.resultId === report\.resultId && published\.version === version/);
    assert.doesNotMatch(worker, /bridge\.getDiagnostics\(/);
    assert.doesNotMatch(worker, /for \(const \[uri, document\] of documents\.entries\(\)\)/);
});
