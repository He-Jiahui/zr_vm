const assert = require('assert').strict;
const { awaitLspRequestOutcome } = require('./stdio_smoke');

/** 不启动服务进程，专门固定成功结果、协议错误与传输异常的归一化边界。 */
async function main() {
    const result = { items: [] };
    assert.deepEqual(await awaitLspRequestOutcome(Promise.resolve(result)), { result, error: null });
    console.log('Pass - successful LSP results retain their value');

    const error = { code: -32800, message: 'Request cancelled', data: { id: 9 } };
    assert.deepEqual(await awaitLspRequestOutcome(Promise.reject(new Error(JSON.stringify(error)))),
        { result: null, error });
    console.log('Pass - protocol errors retain structured code and data');

    // 传输错误必须保留同一 Error 身份，便于上层读取请求 id 和 stderr 诊断。
    const timeout = new Error('timed out waiting for response id=47 stderr=server evidence');
    await assert.rejects(awaitLspRequestOutcome(Promise.reject(timeout)), (error) => error === timeout);
    console.log('Pass - transport timeout retains its original exception and request evidence');

    const closed = new Error('server closed: exitCode=1 signal=null stderr=server evidence');
    await assert.rejects(awaitLspRequestOutcome(Promise.reject(closed)), (error) => error === closed);
    console.log('Pass - closed transport retains its original exception');
}

/** 纯 Node 断言用例的失败通过退出码反馈给 CTest。 */
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
