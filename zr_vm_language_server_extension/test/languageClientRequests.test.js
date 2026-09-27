// 用可替换的请求客户端验证可选请求与严格请求在重启、协议差异下的边界。
const test = require('node:test');
const assert = require('node:assert/strict');

const {
    isIgnorableLanguageServerRequestError,
    sendLanguageServerRequest,
    setLanguageClientRequestClient,
} = require('../out/languageClientRequests.js');

// 模块级客户端会跨测试保留；每例结束时撤销它，避免顺序影响无客户端断言。
test.afterEach(() => {
    setLanguageClientRequestClient(undefined);
});

test('sendLanguageServerRequest returns request results when the language client succeeds', async () => {
    setLanguageClientRequestClient({
        sendRequest: async (method, params) => ({
            method,
            params,
            ok: true,
        }),
    });

    const result = await sendLanguageServerRequest('zr/projectModules', {
        uri: 'file:///demo.zrp',
    });

    assert.deepEqual(result, {
        method: 'zr/projectModules',
        params: { uri: 'file:///demo.zrp' },
        ok: true,
    });
});

test('sendLanguageServerRequest suppresses language server method-not-found errors', async () => {
    setLanguageClientRequestClient({
        sendRequest: async () => {
            const error = new Error('Method not found');
            error.code = -32601;
            throw error;
        },
    });

    const result = await sendLanguageServerRequest('zr/projectModules', {
        uri: 'file:///demo.zrp',
    });

    assert.equal(result, undefined);
});

test('sendLanguageServerRequest suppresses disposed transport errors during restart', async () => {
    setLanguageClientRequestClient({
        sendRequest: async () => {
            throw new Error('Pending response rejected since connection got disposed');
        },
    });

    const result = await sendLanguageServerRequest('zr/nativeDeclarationDocument', {
        uri: 'zr-decompiled:/zr.system.zr',
    });

    assert.equal(result, undefined);
});

test('strict language server requests preserve protocol errors for callers', async () => {
    const error = Object.assign(new Error('Method not found'), { code: -32601 });
    setLanguageClientRequestClient({ sendRequest: async () => { throw error; } });
    await assert.rejects(sendLanguageServerRequest('workspace/diagnostic', {}, { strict: true }),
        actual => actual === error);
});

test('strict language server requests fail when the client is unavailable', async () => {
    await assert.rejects(sendLanguageServerRequest('textDocument/diagnostic', {}, { strict: true }),
        /client is not running/i);
    assert.equal(await sendLanguageServerRequest('textDocument/diagnostic', {}), undefined);
});

test('language server client errors identify shutdown races', () => {
    assert.equal(isIgnorableLanguageServerRequestError(new Error('Client is not running')), true);
    assert.equal(isIgnorableLanguageServerRequestError(new Error('Cannot call write after a stream was destroyed')), true);
    assert.equal(isIgnorableLanguageServerRequestError(new Error('real protocol failure')), false);
});
