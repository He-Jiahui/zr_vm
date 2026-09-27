const test = require('node:test');
const assert = require('node:assert/strict');
const { loadWorker } = require('./helpers/workerHost');

// 同一后端报告用于推送和拉取；保存通知不得用磁盘文本替换已同步的编辑器版本。
test('Web push and pull diagnostics use backend identity and save preserves the editor version', async () => {
    const uri = 'file:///workspace/main.zr';
    const report = { resultId: 'backend-result', items: [{ message: 'problem' }] };
    const worker = loadWorker({ getDiagnosticReport: report });
    await worker.handlers.get('onDidOpenTextDocument')({ textDocument: { uri, text: 'abc', version: 7 } });
    await worker.handlers.get('onDidSaveTextDocument')({ textDocument: { uri }, text: 'disk contents' });
    assert.equal(worker.bridgeCalls.filter(([name]) => name === 'updateDocument').length, 1);
    assert.equal(worker.diagnostics.length, 1);
    assert.equal(worker.diagnostics[0].version, 7);
    assert.equal(worker.diagnostics[0].diagnostics[0].severity, 1);
    const pull = await worker.requests.get('textDocument/diagnostic')({ textDocument: { uri }, previousResultId: report.resultId });
    assert.equal(pull.kind, 'unchanged');
    assert.equal(pull.resultId, report.resultId);
});
