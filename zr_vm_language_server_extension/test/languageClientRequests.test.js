const test = require('node:test');
const assert = require('node:assert/strict');

const {
    isIgnorableLanguageServerRequestError,
    sendLanguageServerRequest,
    setLanguageClientRequestClient,
} = require('../out/languageClientRequests.js');

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

test('language server client errors identify shutdown races', () => {
    assert.equal(isIgnorableLanguageServerRequestError(new Error('Client is not running')), true);
    assert.equal(isIgnorableLanguageServerRequestError(new Error('Cannot call write after a stream was destroyed')), true);
    assert.equal(isIgnorableLanguageServerRequestError(new Error('real protocol failure')), false);
});
