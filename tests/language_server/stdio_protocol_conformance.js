const { StdioProtocolClient, encodeFrame } = require('./stdio_protocol_client');

/** 响应等待与静默观察分别计时；后者只用于确认通知不产生意外输出。 */
const RESPONSE_TIMEOUT_MS = 3000;
const NO_RESPONSE_TIMEOUT_MS = 150;
/** initialize 能力矩阵的精确契约；新增能力需同步审阅客户端协商。 */
const EXPECTED_CAPABILITY_KEYS = [
    'callHierarchyProvider', 'codeActionProvider', 'codeLensProvider',
    'completionProvider', 'definitionProvider', 'diagnosticProvider',
    'documentFormattingProvider', 'documentHighlightProvider', 'documentLinkProvider',
    'documentOnTypeFormattingProvider', 'documentRangeFormattingProvider',
    'documentSymbolProvider', 'foldingRangeProvider', 'hoverProvider', 'implementationProvider',
    'inlayHintProvider', 'inlineValueProvider',
    'linkedEditingRangeProvider', 'monikerProvider', 'positionEncoding', 'referencesProvider',
    'renameProvider', 'selectionRangeProvider', 'semanticTokensProvider', 'signatureHelpProvider',
    'textDocumentSync', 'typeHierarchyProvider', 'workspace',
    'workspaceSymbolProvider',
].sort();

/** 统一把协议断言失败变成单场景失败，供用例入口汇总。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** 核对响应只含版本、原样 id 与指定结果分支，防止语义正确但信封损坏。 */
function assertResponseEnvelope(response, id, member, label) {
    assert(response && typeof response === 'object' && !Array.isArray(response),
           `${label}: response envelope must be an object`);
    assert(response.jsonrpc === '2.0', `${label}: response must have jsonrpc=2.0`);
    assert(Object.prototype.hasOwnProperty.call(response, 'id') && response.id === id,
           `${label}: response envelope id must exactly match ${String(id)}`);
    assert(JSON.stringify(Object.keys(response).sort()) ===
           JSON.stringify(['id', 'jsonrpc', member].sort()),
           `${label}: response envelope must contain only jsonrpc, id and ${member}`);
}

/** 成功响应共用严格信封约束，供生命周期和功能用例复用。 */
function assertSuccessEnvelope(response, id, label) {
    assertResponseEnvelope(response, id, 'result', label);
}

/** 错误响应同时约束错误码与消息类型，区分不同协议失败。 */
function assertErrorEnvelope(response, id, code, label) {
    assertResponseEnvelope(response, id, 'error', label);
    assert(response.error && typeof response.error === 'object' && !Array.isArray(response.error) &&
           response.error.code === code,
           `${label}: expected error ${code}, actual=${JSON.stringify(response)}`);
    assert(typeof response.error.message === 'string', `${label}: error message must be a string`);
}

/** 提供最小合法初始化请求，让后续变体只改变待测字段。 */
function initializePayload(id) {
    return {
        jsonrpc: '2.0',
        id,
        method: 'initialize',
        params: {
            processId: null,
            rootUri: null,
            capabilities: {},
        },
    };
}

/** 保留原始 JSON 数字等精确字节，绕开 JSON.stringify 的数值归一化。 */
function encodeRawJsonFrame(payload) {
    const body = Buffer.from(payload, 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

/** 建立合法生命周期前置条件，并核对 initialize 的响应信封。 */
async function initialize(client, id = 'initialize') {
    const response = await client.requestEnvelope(initializePayload(id), RESPONSE_TIMEOUT_MS);
    assertSuccessEnvelope(response, id, 'initialize');
    assert(response && response.jsonrpc === '2.0' && response.id === id && response.result,
           `initialize must return a JSON-RPC success envelope, actual=${JSON.stringify(response)}`);
    return response.result;
}

/** 对信封级无效请求核对 Invalid Request 与响应 id。 */
async function expectInvalidRequest(client, payload, id, label) {
    client.sendPayload(payload);
    const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
    assertErrorEnvelope(response, id, -32600, label);
}

/** 每个场景独占服务器进程，失败时也回收，避免状态污染。 */
async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    try {
        return await run(client);
    } finally {
        await client.terminate().catch(() => {});
    }
}

/** 固定 initialize 宣告的能力矩阵，防止客户端协商与实现漂移。 */
async function testCapabilityMatrix(serverPath) {
    await withClient(serverPath, async (client) => {
        const result = await initialize(client, 'matrix');
        assert(result.capabilities && typeof result.capabilities === 'object',
               'initialize must return a capabilities object');
        const keys = Object.keys(result.capabilities).sort();
        assert(JSON.stringify(keys) === JSON.stringify(EXPECTED_CAPABILITY_KEYS),
               `LSP 3.17 capability matrix changed: ${JSON.stringify(keys)}`);
    });
}

/** 验证初始化前的普通请求被生命周期门禁拒绝。 */
async function testRequestBeforeInitialize(serverPath) {
    await withClient(serverPath, async (client) => {
        const response = await client.request('textDocument/hover', {
            textDocument: { uri: 'file:///before-initialize.zr' },
            position: { line: 0, character: 0 },
        }, 'before-initialize', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'before-initialize', -32002, 'request before initialize');
    });
}

/** 验证初始化前通知不能提前修改工作区索引。 */
async function testNotificationBeforeInitializeIsIgnored(serverPath) {
    await withClient(serverPath, async (client) => {
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///ignored-before-initialize.zr',
                languageId: 'zr',
                version: 1,
                text: 'class IgnoredBeforeInitialize { }',
            },
        });
        await initialize(client, 'notification-before-initialize');
        client.notify('initialized', {});

        const response = await client.request('workspace/symbol', {
            query: 'IgnoredBeforeInitialize',
        }, 'ignored-before-initialize', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(response, 'ignored-before-initialize', 'ignored notification');
        assert(response && Array.isArray(response.result) && response.result.length === 0,
               `notification before initialize must be ignored, actual=${JSON.stringify(response)}`);
    });
}

/** 验证重复 initialize 不会重新建立会话。 */
async function testRepeatedInitialize(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'first-initialize');
        const response = await client.requestEnvelope(
            initializePayload('second-initialize'), RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'second-initialize', -32600, 'repeated initialize');
    });
}

/** 验证未 shutdown 的 exit 以失败状态结束。 */
async function testExitBeforeShutdown(serverPath) {
    await withClient(serverPath, async (client) => {
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode === 1, `exit before shutdown must exit 1, actual=${exitCode}`);
    });
}

/** 区分 shutdown 前后 exit 的进程状态契约。 */
async function testShutdownExitOrdering(serverPath) {
    await withClient(serverPath, async (client) => {
        const beforeInitialize = await client.request('shutdown', undefined,
                                                       'shutdown-before-initialize');
        assertErrorEnvelope(beforeInitialize, 'shutdown-before-initialize', -32002,
                            'shutdown before initialize');
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode === 1, `exit before successful shutdown must exit 1, actual=${exitCode}`);
    });

    await withClient(serverPath, async (client) => {
        await initialize(client, 'shutdown-exit-initialize');
        const shutdown = await client.request('shutdown', undefined, 'shutdown-exit-shutdown');
        assertSuccessEnvelope(shutdown, 'shutdown-exit-shutdown', 'shutdown before exit');
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode === 0, `exit after successful shutdown must exit 0, actual=${exitCode}`);
    });
}

/** 验证 shutdown 后通知静默且请求仍有错误响应。 */
async function testRequestAfterShutdown(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'shutdown-initialize');
        const shutdown = await client.request('shutdown', undefined, 'shutdown', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(shutdown, 'shutdown', 'shutdown');
        assert(shutdown && shutdown.result === null,
               `shutdown must return a null result envelope, actual=${JSON.stringify(shutdown)}`);
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///ignored-after-shutdown.zr',
                languageId: 'zr',
                version: 1,
                text: 'class IgnoredAfterShutdown { }',
            },
        });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        const response = await client.request('workspace/symbol', { query: '' }, 'after-shutdown', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'after-shutdown', -32600, 'request after shutdown');
    });
}

/** 验证缺少 jsonrpc 版本的请求在分派前被拒绝。 */
async function testMissingJsonRpc(serverPath) {
    await withClient(serverPath, async (client) => {
        const response = await client.requestEnvelope({
            id: 'missing-jsonrpc',
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'missing-jsonrpc', -32600, 'missing jsonrpc');
    });
}

/** 验证错误 jsonrpc 版本不能进入初始化处理。 */
async function testWrongJsonRpc(serverPath) {
    await withClient(serverPath, async (client) => {
        const response = await client.requestEnvelope({
            jsonrpc: '1.0',
            id: 'wrong-jsonrpc',
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'wrong-jsonrpc', -32600, 'wrong jsonrpc');
    });
}

/** 防止布尔 id 被宽松数值转换误收为请求身份。 */
async function testInvalidBooleanId(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, {
            jsonrpc: '2.0',
            id: true,
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, null, 'boolean request id');
    });
}

/** 验证对象和数组 id 被拒绝且错误响应使用 null id。 */
async function testInvalidStructuredIds(serverPath) {
    const invalidIds = [
        ['object request id', {}],
        ['array request id', []],
    ];

    for (const [label, id] of invalidIds) {
        await withClient(serverPath, async (client) => {
            await expectInvalidRequest(client, {
                jsonrpc: '2.0',
                id,
                method: 'initialize',
                params: initializePayload('ignored').params,
            }, null, label);
        });
    }
}

/** 防止小数 id 被截断后误收为整数身份。 */
async function testInvalidFractionalRequestId(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, {
            jsonrpc: '2.0',
            id: 1.5,
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, null, 'fractional request id');
    });
}

/** 验证 JSON 顶层必须是请求对象。 */
async function testInvalidTopLevelMessages(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, [], null, 'array top-level message');
        await expectInvalidRequest(client, 17, null, 'scalar top-level message');
    });
}

/** 验证 initialize 参数缺失、null 与伪对象都触发参数错误。 */
async function testInvalidParams(serverPath) {
    const invalidParams = [
        ['missing params', undefined],
        ['scalar params', 'not-an-object'],
        ['null params', null],
        ['array params', []],
    ];

    for (const [label, params] of invalidParams) {
        await withClient(serverPath, async (client) => {
            const response = await client.requestEnvelope({
                jsonrpc: '2.0',
                id: `invalid-params-${label}`,
                method: 'initialize',
                params,
            }, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, `invalid-params-${label}`, -32602, label);
        });
    }
}

/** 验证位置为有界非负整数且范围方向正确。 */
async function testInvalidPositionAndRangeNumbers(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///invalid-position.zr';
        const invalidPositions = [
            ['fractional line', { line: 0.5, character: 0 }],
            ['negative character', { line: 0, character: -1 }],
            ['overflow line', { line: 2147483648, character: 0 }],
        ];

        await initialize(client, 'invalid-position-initialize');
        for (const [label, position] of invalidPositions) {
            const id = `invalid-position-${label}`;
            const response = await client.request('textDocument/hover', {
                textDocument: { uri },
                position,
            }, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }

        const reverseRange = await client.request('textDocument/rangeFormatting', {
            textDocument: { uri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 0, character: 0 },
            },
            options: {},
        }, 'reverse-range', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(reverseRange, 'reverse-range', -32602, 'reverse range');
    });
}

/** 验证调用和类型层次入口要求结构化 item 与位置。 */
async function testInvalidHierarchyParams(serverPath) {
    const cases = [
        ['call hierarchy prepare', 'textDocument/prepareCallHierarchy', {}],
        ['call hierarchy incoming', 'callHierarchy/incomingCalls', { item: {} }],
        ['call hierarchy outgoing', 'callHierarchy/outgoingCalls', { item: {} }],
        ['type hierarchy prepare', 'textDocument/prepareTypeHierarchy', {}],
        ['type hierarchy supertypes', 'typeHierarchy/supertypes', { item: {} }],
        ['type hierarchy subtypes', 'typeHierarchy/subtypes', { item: {} }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-hierarchy-initialize');
        for (const [label, method, params] of cases) {
            const id = `invalid-hierarchy-${label}`;
            const response = await client.request(method, params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证多个编辑器查询入口不会接受空文档请求。 */
async function testInvalidEditorFeatureParams(serverPath) {
    const cases = [
        ['implementation', 'textDocument/implementation'],
        ['folding range', 'textDocument/foldingRange'],
        ['selection range', 'textDocument/selectionRange'],
        ['document link', 'textDocument/documentLink'],
        ['code lens', 'textDocument/codeLens'],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-editor-feature-initialize');
        for (const [label, method] of cases) {
            const id = `invalid-editor-feature-${label}`;
            const response = await client.request(method, {}, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证格式化和代码动作入口要求完整参数。 */
async function testInvalidEditingParams(serverPath) {
    const cases = [
        ['formatting', 'textDocument/formatting'],
        ['on type formatting', 'textDocument/onTypeFormatting'],
        ['code action', 'textDocument/codeAction'],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-editing-initialize');
        for (const [label, method] of cases) {
            const id = `invalid-editing-${label}`;
            const response = await client.request(method, {}, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 在真实打开文档上单独验证 codeAction 范围门禁。 */
async function testInvalidCodeActionRange(serverPath) {
    const uri = 'file:///invalid-code-action-range.zr';
    const cases = [
        ['null range', null],
        ['scalar range', 'not-a-range'],
        ['array range', []],
        ['reverse range', {
            start: { line: 1, character: 0 },
            end: { line: 0, character: 0 },
        }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-code-action-range-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'var answer = 1;\n',
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics');
        for (const [label, range] of cases) {
            const params = {
                textDocument: { uri },
                range,
                context: { diagnostics: [] },
            };
            const id = `invalid-code-action-range-${label}`;
            const response = await client.request('textDocument/codeAction', params, id,
                                                  RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 用合法文档与范围隔离检查 codeAction.context 形状。 */
async function testInvalidCodeActionContext(serverPath) {
    const uri = 'file:///invalid-code-action-context.zr';
    const contexts = [
        ['missing context', undefined],
        ['null context', null],
        ['scalar context', 'not-a-context'],
        ['array context', []],
        ['missing diagnostics', {}],
        ['null diagnostics', { diagnostics: null }],
        ['scalar diagnostics', { diagnostics: 'not-an-array' }],
        ['malformed diagnostic item', { diagnostics: [1] }],
        ['scalar only', { diagnostics: [], only: 'quickfix' }],
        ['malformed only item', { diagnostics: [], only: [1] }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-code-action-context-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'var answer = 1;\n',
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics');
        for (const [label, context] of contexts) {
            const params = {
                textDocument: { uri },
                range: {
                    start: { line: 0, character: 0 },
                    end: { line: 0, character: 15 },
                },
            };
            if (context !== undefined) {
                params.context = context;
            }
            const id = `invalid-code-action-context-${label}`;
            const response = await client.request('textDocument/codeAction', params, id,
                                                  RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 协商多范围格式化后验证逐项范围，并保留合法空结果。 */
async function testInvalidRangesFormattingParams(serverPath) {
    const uri = 'file:///invalid-ranges-formatting.zr';
    const cases = [
        ['missing params', undefined],
        ['null params', null],
        ['scalar params', 'not-an-object'],
        ['array params', []],
        ['missing text document', { ranges: [] }],
        ['missing ranges', { textDocument: { uri } }],
        ['null ranges', { textDocument: { uri }, ranges: null }],
        ['scalar ranges', { textDocument: { uri }, ranges: 'not-an-array' }],
        ['object ranges', { textDocument: { uri }, ranges: {} }],
        ['null range item', { textDocument: { uri }, ranges: [null] }],
        ['scalar range item', { textDocument: { uri }, ranges: ['not-a-range'] }],
        ['array range item', { textDocument: { uri }, ranges: [[]] }],
        ['reverse range item', {
            textDocument: { uri },
            ranges: [{
                start: { line: 1, character: 0 },
                end: { line: 0, character: 0 },
            }],
        }],
        ['invalid item after valid range', {
            textDocument: { uri },
            ranges: [{
                start: { line: 1, character: 0 },
                end: { line: 1, character: 9 },
            }, null],
        }],
    ];

    await withClient(serverPath, async (client) => {
        const payload = initializePayload('invalid-ranges-formatting-initialize');
        payload.params.capabilities = {
            textDocument: { rangeFormatting: { rangesSupport: true } },
        };
        const initializeResponse = await client.requestEnvelope(payload, RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(initializeResponse, 'invalid-ranges-formatting-initialize',
                              'ranges formatting initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'fn main(): int {\nreturn 1;\n}\n',
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics');
        for (const [label, params] of cases) {
            const id = `invalid-ranges-formatting-${label}`;
            const response = await client.request('textDocument/rangesFormatting', params, id,
                                                  RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
        const emptyId = 'valid-empty-ranges-formatting';
        const emptyResponse = await client.request('textDocument/rangesFormatting', {
            textDocument: { uri },
            ranges: [],
            options: { tabSize: 4, insertSpaces: true },
        }, emptyId, RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(emptyResponse, emptyId, 'empty ranges formatting');
        assert(Array.isArray(emptyResponse.result) && emptyResponse.result.length === 0,
               'empty ranges formatting must return an empty edit array');
    });
}

/** 验证 codeAction/resolve 不接受缺少可解析身份的数据。 */
async function testInvalidCodeActionResolveParams(serverPath) {
    const cases = [
        ['missing params', undefined],
        ['null params', null],
        ['scalar params', 'not-a-code-action'],
        ['array params', []],
        ['empty item', {}],
        ['empty data', { data: {} }],
        ['null data', { data: null }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-code-action-resolve-initialize');
        for (const [label, params] of cases) {
            const id = `invalid-code-action-resolve-${label}`;
            const response = await client.request('codeAction/resolve', params, id,
                                                  RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证 completionItem/resolve 不会把空项当成有效补全。 */
async function testInvalidCompletionResolveParams(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-completion-resolve-initialize');
        const response = await client.request('completionItem/resolve', {},
                                              'invalid-completion-resolve', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'invalid-completion-resolve', -32602,
                            'completion resolve params');
    });
}

/** 覆盖 inlineValue、moniker 与 linkedEditingRange 的参数门禁。 */
async function testInvalidAdditionalEditorParams(serverPath) {
    const cases = [
        ['inline value', 'textDocument/inlineValue'],
        ['moniker', 'textDocument/moniker'],
        ['linked editing range', 'textDocument/linkedEditingRange'],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-additional-editor-initialize');
        for (const [label, method] of cases) {
            const id = `invalid-additional-editor-${label}`;
            const response = await client.request(method, {}, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 覆盖语义 token 全量、增量和范围入口的文档参数门禁。 */
async function testInvalidSemanticTokenParams(serverPath) {
    const cases = [
        ['semantic tokens full', 'textDocument/semanticTokens/full'],
        ['semantic tokens full delta', 'textDocument/semanticTokens/full/delta'],
        ['semantic tokens range', 'textDocument/semanticTokens/range'],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-semantic-token-initialize');
        for (const [label, method] of cases) {
            const id = `invalid-semantic-token-${label}`;
            const response = await client.request(method, {}, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证 workspace/symbol 必须携带 query。 */
async function testInvalidWorkspaceSymbolParams(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-workspace-symbol-initialize');
        const response = await client.request('workspace/symbol', {},
                                              'invalid-workspace-symbol', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'invalid-workspace-symbol', -32602,
                            'workspace symbol query');
    });
}

/** 验证 workspace/diagnostic 顶层参数必须是对象。 */
async function testInvalidWorkspaceDiagnosticParams(serverPath) {
    const cases = [
        ['missing params', undefined],
        ['null params', null],
        ['scalar params', 'not-an-object'],
        ['array params', []],
    ];

    for (const [label, params] of cases) {
        await withClient(serverPath, async (client) => {
            await initialize(client, `invalid-workspace-diagnostic-${label}-initialize`);
            const id = `invalid-workspace-diagnostic-${label}`;
            const response = await client.request('workspace/diagnostic', params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        });
    }
}

/** 验证文件重命名请求的 files 列表与元素形状。 */
async function testInvalidWorkspaceWillRenameParams(serverPath) {
    const cases = [
        ['missing params', undefined],
        ['null params', null],
        ['scalar params', 'not-an-object'],
        ['array params', []],
        ['missing files', {}],
        ['null files', { files: null }],
        ['scalar files', { files: 'not-an-array' }],
        ['malformed file item', { files: [{}] }],
    ];

    for (const [label, params] of cases) {
        await withClient(serverPath, async (client) => {
            await initialize(client, `invalid-workspace-will-rename-${label}-initialize`);
            const id = `invalid-workspace-will-rename-${label}`;
            const response = await client.request('workspace/willRenameFiles', params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        });
    }
}

/** 验证诊断请求的可选 id 与标识符也执行类型检查。 */
async function testInvalidDiagnosticOptionalParams(serverPath) {
    const cases = [
        ['text document previous result id', 'textDocument/diagnostic', {
            textDocument: { uri: 'file:///invalid-diagnostic-previous-result-id.zr' },
            previousResultId: 1,
        }],
        ['text document null previous result id', 'textDocument/diagnostic', {
            textDocument: { uri: 'file:///invalid-diagnostic-null-previous-result-id.zr' },
            previousResultId: null,
        }],
        ['workspace diagnostic identifier', 'workspace/diagnostic', { identifier: 1 }],
        ['workspace diagnostic previous result ids', 'workspace/diagnostic', { previousResultIds: {} }],
        ['workspace diagnostic previous result id entry', 'workspace/diagnostic', {
            previousResultIds: [{ uri: 'file:///invalid-diagnostic-entry.zr', value: 1 }],
        }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-diagnostic-optional-initialize');
        for (const [label, method, params] of cases) {
            const id = `invalid-diagnostic-optional-${label}`;
            const response = await client.request(method, params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证语义 token 增量查询要求合法 previousResultId。 */
async function testInvalidSemanticTokenDeltaResultId(serverPath) {
    const cases = [
        ['missing result id', undefined],
        ['null result id', null],
        ['numeric result id', 1],
        ['array result id', []],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-semantic-delta-result-id-initialize');
        for (const [label, previousResultId] of cases) {
            const params = { textDocument: { uri: 'file:///invalid-semantic-delta-result-id.zr' } };
            if (previousResultId !== undefined) {
                params.previousResultId = previousResultId;
            }
            const id = `invalid-semantic-delta-result-id-${label}`;
            const response = await client.request(
                'textDocument/semanticTokens/full/delta', params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证引用查询 context 与 includeDeclaration 的布尔约束。 */
async function testInvalidReferencesContext(serverPath) {
    const cases = [
        ['missing context', undefined],
        ['null context', null],
        ['scalar context', 'not-an-object'],
        ['empty context', {}],
        ['numeric include declaration', { includeDeclaration: 1 }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-references-context-initialize');
        for (const [label, context] of cases) {
            const params = {
                textDocument: { uri: 'file:///invalid-references-context.zr' },
                position: { line: 0, character: 0 },
            };
            if (context !== undefined) {
                params.context = context;
            }
            const id = `invalid-references-context-${label}`;
            const response = await client.request('textDocument/references', params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 先协商 inlineCompletion，再检查请求参数错误，避免被能力门禁掩盖。 */
async function testInvalidInlineCompletionParams(serverPath) {
    const cases = [
        ['missing params', undefined],
        ['null params', null],
        ['scalar params', 'not-an-object'],
        ['array params', []],
        ['missing text document', {}],
    ];

    await withClient(serverPath, async (client) => {
        const payload = initializePayload('invalid-inline-completion-initialize');
        payload.params.capabilities = {
            textDocument: { inlineCompletion: {} },
        };
        const initializeResponse = await client.requestEnvelope(payload, RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(initializeResponse, 'invalid-inline-completion-initialize',
                              'inline completion initialize');
        for (const [label, params] of cases) {
            const id = `invalid-inline-completion-${label}`;
            const response = await client.request('textDocument/inlineCompletion', params, id,
                                                  RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

/** 验证未知请求映射为 Method Not Found。 */
async function testUnknownMethod(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'unknown-method-initialize');
        const response = await client.request('workspace/notReal', {}, 'unknown-method', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'unknown-method', -32601, 'unknown method');
    });
}

/** 验证未知通知不会产生 JSON-RPC 响应。 */
async function testNotificationHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'notification-initialize');
        client.notify('workspace/notReal', {});
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

/** 验证参数损坏的通知仍不得生成响应。 */
async function testMalformedNotificationHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'malformed-notification-initialize');
        client.notify('textDocument/hover', 'not-an-object');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

/** 验证完整帧内的坏 JSON 返回 null id 的 Parse Error。 */
async function testMalformedJson(serverPath) {
    await withClient(serverPath, async (client) => {
        client.sendRawFrame(Buffer.from('Content-Length: 1\r\n\r\n{', 'ascii'));
        const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, null, -32700, 'malformed JSON payload');
    });
}

/** 同 id 并发请求要求原请求成功、冲突请求失败。 */
async function testDuplicateRequestId(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'duplicate-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///duplicate-request-queue.zr',
                languageId: 'zr',
                version: 1,
                text: '// request registry queue\n'.repeat(8192),
            },
        });
        const payload = {
            jsonrpc: '2.0',
            id: 'duplicate-request',
            method: 'workspace/symbol',
            params: { query: '' },
        };
        client.sendPayload(payload);
        client.sendPayload(payload);
        const first = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const second = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const responses = [first, second];
        const errors = responses.filter((response) =>
            response && Object.prototype.hasOwnProperty.call(response, 'error'));
        const successes = responses.filter((response) =>
            response && !Object.prototype.hasOwnProperty.call(response, 'error'));
        assert(errors.length === 1 && successes.length === 1,
               `duplicate request ids require one error and one success envelope, actual=${JSON.stringify(responses)}`);
        assertErrorEnvelope(errors[0], 'duplicate-request', -32600, 'duplicate request id');
        assertSuccessEnvelope(successes[0], 'duplicate-request', 'original request id');
        assert(Array.isArray(successes[0].result) && successes[0].result.length === 0,
               `original workspace symbol request must have no symbols, actual=${JSON.stringify(successes[0])}`);
    });
}

/** 验证数字 1 与字符串 1 可作为两个独立请求 id。 */
async function testDistinctTypedRequestIds(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'typed-id-initialize');
        client.sendPayload({
            jsonrpc: '2.0',
            id: 1,
            method: 'workspace/symbol',
            params: { query: '' },
        });
        client.sendPayload({
            jsonrpc: '2.0',
            id: '1',
            method: 'workspace/symbol',
            params: { query: '' },
        });
        const first = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const second = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const responses = [first, second];
        for (const id of [1, '1']) {
            const response = responses.find((item) => item && item.id === id);
            assertSuccessEnvelope(response, id, `distinct ${typeof id} request id`);
            assert(Array.isArray(response.result) && response.result.length === 0,
                   `typed workspace symbol request must have no symbols, actual=${JSON.stringify(response)}`);
        }
    });
}

/** 用原始帧验证安全整数保真及越界 id 拒绝。 */
async function testNumericRequestIdPrecision(serverPath) {
    const safeIdText = '9007199254740991';
    const safeId = Number(safeIdText);
    const negativeSafeIdText = '-9007199254740991';
    const initializeParams = JSON.stringify(initializePayload(safeId).params);

    await withClient(serverPath, async (client) => {
        client.sendRawFrame(encodeRawJsonFrame(
            `{"jsonrpc":"2.0","id":${safeIdText},"method":"initialize","params":${initializeParams}}`));
        const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(response, safeId, 'safe numeric request id');

        client.sendRawFrame(encodeRawJsonFrame(
            `{"jsonrpc":"2.0","id":${negativeSafeIdText},"method":"workspace/symbol","params":{"query":""}}`));
        const negativeResponse = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(negativeResponse, Number(negativeSafeIdText),
                              'negative safe numeric request id');
    });

    await withClient(serverPath, async (client) => {
        for (const unsafeIdText of ['9007199254740992', '-9007199254740992']) {
            client.sendRawFrame(encodeRawJsonFrame(
                `{"jsonrpc":"2.0","id":${unsafeIdText},"method":"initialize","params":${initializeParams}}`));
            const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, null, -32600, `unsafe numeric request id ${unsafeIdText}`);
        }
    });
}

/** 未知请求的取消只是静默控制通知。 */
async function testCancelUnknownIdHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'cancel-initialize');
        client.notify('$/cancelRequest', { id: 'not-active' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

/** 在昂贵文档建立后排队已知请求和取消，验证取消错误与文档版本。 */
async function testCancelKnownRequestId(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///cancel-known-request.zr';
        const version = 1;
        const documentSetupTimeoutMs = 10000;
        const symbols = Array.from(
            { length: 2048 },
            (_, index) => `class CancellationSymbol${index} { }`).join('\n');

        await initialize(client, 'cancel-known-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version,
                text: symbols,
            },
        });

        // 文档解析先占据处理队列，使请求和取消相邻到达注册表；诊断版本证明准备阶段已完成。
        client.sendPayload({
            jsonrpc: '2.0',
            id: 'cancel-known-request',
            method: 'workspace/symbol',
            params: {
                query: 'CancellationSymbol',
                partialResultToken: 'cancel-known-progress',
            },
        });
        client.notify('$/cancelRequest', { id: 'cancel-known-request' });

        const diagnostics = await client.waitForNotification(
            'textDocument/publishDiagnostics', documentSetupTimeoutMs);
        assert(diagnostics && diagnostics.uri === uri && diagnostics.version === version,
               `cancel known request setup must publish the exact document/version, actual=${JSON.stringify(diagnostics)}`);

        // 响应期限从文档准备完成后开始；此处只检查请求取消的协议结果。
        const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'cancel-known-request', -32800, 'cancel known request id');
    });
}

/** 验证 trace 只写 stderr，不污染 stdout 的 LSP 响应帧。 */
async function testSetTraceWritesOnlyStderr(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'trace-initialize');
        client.notify('$/setTrace', { value: 'messages' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);

        const tracedResponse = await client.request('workspace/symbol', { query: '' }, 'trace-request');
        assertSuccessEnvelope(tracedResponse, 'trace-request', 'traced request');
        assert(tracedResponse && Array.isArray(tracedResponse.result),
               `traced request must retain its framed result, actual=${JSON.stringify(tracedResponse)}`);
        assert(client.stderr().includes('LSP trace inbound request workspace/symbol'),
               `messages trace must write inbound request to stderr, stderr=${client.stderr()}`);
        assert(client.stderr().includes('LSP trace outbound response workspace/symbol'),
               `messages trace must write outbound response to stderr, stderr=${client.stderr()}`);

        client.notify('$/setTrace', { value: 'verbose' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        client.notify('workspace/notReal', {});
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        assert(client.stderr().includes('LSP trace inbound notification workspace/notReal'),
               `verbose trace must write notification metadata to stderr, stderr=${client.stderr()}`);

        client.notify('$/setTrace', { value: 'off' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        const stderrBeforeOffRequest = client.stderr();
        const offResponse = await client.request('workspace/symbol', { query: '' }, 'trace-off-request');
        assertSuccessEnvelope(offResponse, 'trace-off-request', 'trace off request');
        assert(client.stderr() === stderrBeforeOffRequest,
               `off trace must not add stderr records, stderr=${client.stderr()}`);
    });
}

/** 验证生命周期外的控制通知不能改变 trace 状态。 */
async function testControlNotificationsOutsideLifecycleAreIgnored(serverPath) {
    await withClient(serverPath, async (client) => {
        client.notify('$/setTrace', { value: 'messages' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        await initialize(client, 'trace-before-initialize');

        const response = await client.request('workspace/symbol', { query: '' },
                                               'trace-before-initialize-request');
        assertSuccessEnvelope(response, 'trace-before-initialize-request',
                              'request after pre-initialize setTrace');
        assert(!client.stderr().includes('LSP trace inbound request workspace/symbol'),
               `setTrace before initialize must be ignored, stderr=${client.stderr()}`);

        const shutdown = await client.request('shutdown', undefined, 'trace-shutdown');
        assertSuccessEnvelope(shutdown, 'trace-shutdown', 'trace lifecycle shutdown');
        client.notify('$/setTrace', { value: 'verbose' });
        client.notify('workspace/notReal', {});
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        assert(!client.stderr().includes('LSP trace inbound notification workspace/notReal'),
               `setTrace after shutdown must be ignored, stderr=${client.stderr()}`);
    });
}

/** 验证 workDoneToken 类型、边界及 begin/end，同时保留常规响应。 */
async function testRequestWorkDoneProgress(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'work-done-initialize');
        const stringResponse = client.request('workspace/symbol', {
            query: '',
            workDoneToken: 'workspace-symbol-work',
        }, 'work-done-string', RESPONSE_TIMEOUT_MS);
        const stringBegin = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const stringEnd = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const stringResult = await stringResponse;
        assertSuccessEnvelope(stringResult, 'work-done-string', 'string work-done result');

        assert(stringBegin && stringBegin.token === 'workspace-symbol-work' &&
               stringBegin.value && stringBegin.value.kind === 'begin',
               `string work-done begin must preserve token, actual=${JSON.stringify(stringBegin)}`);
        assert(stringEnd && stringEnd.token === 'workspace-symbol-work' &&
               stringEnd.value && stringEnd.value.kind === 'end',
               `string work-done end must preserve token, actual=${JSON.stringify(stringEnd)}`);
        assert(stringResult && stringResult.id === 'work-done-string' && Array.isArray(stringResult.result),
               `work-done request must retain its ordinary response, actual=${JSON.stringify(stringResult)}`);

        const numberResponse = client.request('workspace/symbol', {
            query: '',
            workDoneToken: 9,
        }, 'work-done-number', RESPONSE_TIMEOUT_MS);
        const numberBegin = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const numberEnd = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const numberResult = await numberResponse;
        assertSuccessEnvelope(numberResult, 'work-done-number', 'numeric work-done result');

        assert(numberBegin && numberBegin.token === 9 && numberBegin.value && numberBegin.value.kind === 'begin',
               `numeric work-done begin must preserve token, actual=${JSON.stringify(numberBegin)}`);
        assert(numberEnd && numberEnd.token === 9 && numberEnd.value && numberEnd.value.kind === 'end',
               `numeric work-done end must preserve token, actual=${JSON.stringify(numberEnd)}`);
        assert(numberResult && numberResult.id === 'work-done-number' && Array.isArray(numberResult.result),
               `numeric work-done request must retain its ordinary response, actual=${JSON.stringify(numberResult)}`);

        for (const [label, token] of [
            ['positive-boundary', 9007199254740991],
            ['negative-boundary', -9007199254740991],
        ]) {
            const requestId = `work-done-${label}`;
            const responsePromise = client.request('workspace/symbol', {
                query: '',
                workDoneToken: token,
            }, requestId, RESPONSE_TIMEOUT_MS);
            const begin = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
            const end = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
            const response = await responsePromise;
            assertSuccessEnvelope(response, requestId, `${label} work-done result`);
            assert(begin && begin.token === token && begin.value && begin.value.kind === 'begin',
                   `${label} work-done begin must preserve token, actual=${JSON.stringify(begin)}`);
            assert(end && end.token === token && end.value && end.value.kind === 'end',
                   `${label} work-done end must preserve token, actual=${JSON.stringify(end)}`);
        }

        const invalidPartial = await client.request('workspace/symbol', {
            query: '',
            partialResultToken: false,
        }, 'work-done-invalid-partial', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(invalidPartial,
                            'work-done-invalid-partial',
                            -32602,
                            'invalid partial result token');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);

        const invalidWorkDoneBoundary = await client.request('workspace/symbol', {
            query: '',
            workDoneToken: 9007199254740992,
        }, 'work-done-invalid-work-boundary', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(invalidWorkDoneBoundary,
                            'work-done-invalid-work-boundary',
                            -32602,
                            'invalid work-done token boundary');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);

        const invalidPartialBoundary = await client.request('workspace/symbol', {
            query: '',
            partialResultToken: -9007199254740992,
        }, 'work-done-invalid-partial-boundary', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(invalidPartialBoundary,
                            'work-done-invalid-partial-boundary',
                            -32602,
                            'invalid partial-result token boundary');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

/** 用跨越 64 项批次边界的符号集验证部分结果契约。 */
async function testWorkspaceSymbolPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'partial-symbol-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///partial-progress-symbol.zr',
                languageId: 'zr',
                version: 1,
                text: Array.from(
                    { length: 65 },
                    (_, index) => `class PartialProgressSymbol${index} { }`).join('\n'),
            },
        });

        const responsePromise = client.request('workspace/symbol', {
            query: 'PartialProgressSymbol',
            partialResultToken: 17,
        }, 'workspace-symbol-partial', RESPONSE_TIMEOUT_MS);
        const firstPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const secondPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'workspace-symbol-partial', 'workspace symbol partial result');

        assert(firstPartial && firstPartial.token === 17 &&
               Array.isArray(firstPartial.value) && firstPartial.value.length === 64 &&
               firstPartial.value.some((item) => item && item.name === 'PartialProgressSymbol0'),
               `workspace symbol first partial batch must preserve 64 results, actual=${JSON.stringify(firstPartial)}`);
        assert(secondPartial && secondPartial.token === 17 &&
               Array.isArray(secondPartial.value) && secondPartial.value.length === 1 &&
               secondPartial.value[0] && secondPartial.value[0].name === 'PartialProgressSymbol64',
               `workspace symbol second partial batch must preserve the remaining result, actual=${JSON.stringify(secondPartial)}`);
        assert(response && response.id === 'workspace-symbol-partial' && response.result === null,
               `workspace symbol partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

/** 首批部分结果之后取消流式查询，验证 RequestCancelled。 */
async function testCancelDuringPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///cancel-during-partial.zr';
        const requestId = 'cancel-during-partial-request';
        const documentSetupTimeoutMs = 10000;
        const responseTimeoutMs = 20000;
        const namePadding = 'X'.repeat(64);
        const symbols = Array.from(
            { length: 1024 },
            (_, index) => `class CancellationPartialSymbol${index}${namePadding} { }`).join('\n');

        await initialize(client, 'cancel-during-partial-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: symbols,
            },
        });
        const diagnostics = await client.waitForNotification(
            'textDocument/publishDiagnostics', documentSetupTimeoutMs);
        assert(diagnostics && diagnostics.uri === uri && diagnostics.version === 1,
               `partial cancellation setup must publish the exact document/version, actual=${JSON.stringify(diagnostics)}`);

        const responsePromise = client.request('workspace/symbol', {
            query: 'CancellationPartialSymbol',
            partialResultToken: 'cancel-during-partial-progress',
        }, requestId, responseTimeoutMs);
        // TODO: 首批 progress 到达客户端不保证服务器仍在处理后续批次；复核不同调度下取消是否可能到达过晚。
        const cancellation = client.waitForNotification('$/progress', responseTimeoutMs).then((firstPartial) => {
            assert(firstPartial && firstPartial.token === 'cancel-during-partial-progress' &&
                   Array.isArray(firstPartial.value) && firstPartial.value.length === 64,
                   `partial cancellation must observe the first 64-item batch, actual=${JSON.stringify(firstPartial)}`);
            client.notify('$/cancelRequest', { id: requestId });
        });
        const [response] = await Promise.all([responsePromise, cancellation]);
        assertErrorEnvelope(response, requestId, -32800,
                            'cancellation during partial result publishing');
    });
}

/** 验证引用位置经 progress 送达，最终响应保留合法空结果。 */
async function testReferencesPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-references.zr';
        await initialize(client, 'partial-references-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: [
                    'fn partialReference(value: int): int {',
                    '    return value;',
                    '}',
                    'fn usePartialReference(): int {',
                    '    return partialReference(partialReference(1));',
                    '}',
                ].join('\n'),
            },
        });

        const responsePromise = client.request('textDocument/references', {
            textDocument: { uri },
            position: { line: 0, character: 3 },
            context: { includeDeclaration: true },
            partialResultToken: 'references-partial',
        }, 'references-partial', RESPONSE_TIMEOUT_MS);
        const partial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'references-partial', 'references partial result');

        assert(partial && partial.token === 'references-partial' && Array.isArray(partial.value) &&
               partial.value.length >= 3 && partial.value.every((location) => location && location.uri === uri),
               `references partial result must preserve all reference locations, actual=${JSON.stringify(partial)}`);
        assert(response && response.id === 'references-partial' && response.result === null,
               `references partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

/** 验证 incomingCalls 部分结果对应 prepare 得到的项。 */
async function testCallHierarchyPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-call-hierarchy.zr';
        await initialize(client, 'partial-call-hierarchy-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: [
                    'fn partialCallee(value: int): int {',
                    '    return value;',
                    '}',
                    'fn partialCaller(): int {',
                    '    return partialCallee(1);',
                    '}',
                ].join('\n'),
            },
        });

        const prepared = await client.request('textDocument/prepareCallHierarchy', {
            textDocument: { uri },
            position: { line: 0, character: 3 },
        }, 'prepare-call-hierarchy-partial', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(prepared, 'prepare-call-hierarchy-partial', 'prepare call hierarchy');
        assert(prepared && Array.isArray(prepared.result) && prepared.result.length > 0,
               `prepare call hierarchy must yield the callee item, actual=${JSON.stringify(prepared)}`);

        const responsePromise = client.request('callHierarchy/incomingCalls', {
            item: prepared.result[0],
            partialResultToken: 'call-hierarchy-partial',
        }, 'call-hierarchy-partial', RESPONSE_TIMEOUT_MS);
        const partial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'call-hierarchy-partial', 'call hierarchy partial result');

        assert(partial && partial.token === 'call-hierarchy-partial' && Array.isArray(partial.value) &&
               partial.value.some((call) => call && call.from && call.from.name === 'partialCaller'),
               `call hierarchy partial result must preserve incoming calls, actual=${JSON.stringify(partial)}`);
        assert(response && response.id === 'call-hierarchy-partial' && response.result === null,
               `call hierarchy partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

/** 分别验证 supertypes 与 subtypes 的部分结果身份。 */
async function testTypeHierarchyPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-type-hierarchy.zr';
        await initialize(client, 'partial-type-hierarchy-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: [
                    'class PartialBase {',
                    '}',
                    'class PartialDerived : PartialBase {',
                    '}',
                ].join('\n'),
            },
        });

        const derived = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri },
            position: { line: 2, character: 7 },
        }, 'prepare-type-hierarchy-derived', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(derived, 'prepare-type-hierarchy-derived', 'prepare derived type hierarchy');
        assert(derived && Array.isArray(derived.result) && derived.result.length > 0,
               `prepare type hierarchy must yield PartialDerived, actual=${JSON.stringify(derived)}`);

        const supertypesPromise = client.request('typeHierarchy/supertypes', {
            item: derived.result[0],
            partialResultToken: 'type-hierarchy-supertypes-partial',
        }, 'type-hierarchy-supertypes-partial', RESPONSE_TIMEOUT_MS);
        const supertypesPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const supertypes = await supertypesPromise;
        assertSuccessEnvelope(supertypes, 'type-hierarchy-supertypes-partial', 'supertypes partial result');
        assert(supertypesPartial && supertypesPartial.token === 'type-hierarchy-supertypes-partial' &&
               Array.isArray(supertypesPartial.value) &&
               supertypesPartial.value.some((item) => item && item.name === 'PartialBase'),
               `type hierarchy supertypes partial result must preserve PartialBase, actual=${JSON.stringify(supertypesPartial)}`);
        assert(supertypes && supertypes.id === 'type-hierarchy-supertypes-partial' && supertypes.result === null,
               `type hierarchy supertypes partial result must consume the ordinary response, actual=${JSON.stringify(supertypes)}`);

        const base = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri },
            position: { line: 0, character: 7 },
        }, 'prepare-type-hierarchy-base', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(base, 'prepare-type-hierarchy-base', 'prepare base type hierarchy');
        assert(base && Array.isArray(base.result) && base.result.length > 0,
               `prepare type hierarchy must yield PartialBase, actual=${JSON.stringify(base)}`);

        const subtypesPromise = client.request('typeHierarchy/subtypes', {
            item: base.result[0],
            partialResultToken: 'type-hierarchy-subtypes-partial',
        }, 'type-hierarchy-subtypes-partial', RESPONSE_TIMEOUT_MS);
        const subtypesPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const subtypes = await subtypesPromise;
        assertSuccessEnvelope(subtypes, 'type-hierarchy-subtypes-partial', 'subtypes partial result');
        assert(subtypesPartial && subtypesPartial.token === 'type-hierarchy-subtypes-partial' &&
               Array.isArray(subtypesPartial.value) &&
               subtypesPartial.value.some((item) => item && item.name === 'PartialDerived'),
               `type hierarchy subtypes partial result must preserve PartialDerived, actual=${JSON.stringify(subtypesPartial)}`);
        assert(subtypes && subtypes.id === 'type-hierarchy-subtypes-partial' && subtypes.result === null,
               `type hierarchy subtypes partial result must consume the ordinary response, actual=${JSON.stringify(subtypes)}`);
    });
}

/** 验证诊断报告在 progress 中承载，最终响应只含剩余集合。 */
async function testWorkspaceDiagnosticPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-workspace-diagnostic.zr';
        await initialize(client, 'partial-workspace-diagnostic-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'fn partialWorkspaceDiagnostic(value: int): int { return value; }',
            },
        });

        const responsePromise = client.request('workspace/diagnostic', {
            partialResultToken: 'workspace-diagnostic-partial',
        }, 'workspace-diagnostic-partial', RESPONSE_TIMEOUT_MS);
        const partial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'workspace-diagnostic-partial', 'workspace diagnostic partial result');

        assert(partial && partial.token === 'workspace-diagnostic-partial' &&
               partial.value && Array.isArray(partial.value.items) &&
               partial.value.items.some((report) => report && report.uri === uri),
               `workspace diagnostic partial result must preserve report items, actual=${JSON.stringify(partial)}`);
        assert(response && response.id === 'workspace-diagnostic-partial' && response.result &&
               Array.isArray(response.result.items) && response.result.items.length === 0,
               `workspace diagnostic partial result must complete with an empty report, actual=${JSON.stringify(response)}`);
    });
}

/** 以超限 Content-Length 验证帧大小门禁及失败退出。 */
async function testOversizeFrameClosesWithFailure(serverPath) {
    await withClient(serverPath, async (client) => {
        client.sendRawFrame(Buffer.from('Content-Length: 16777217\r\n\r\n', 'ascii'));
        client.endInput();
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode !== 0, `oversize frame must terminate with failure, actual=${exitCode}`);
        assert(client.stderr().includes('TOO_LARGE'),
               `oversize frame must report TOO_LARGE, stderr=${client.stderr()}`);
    });
}

/** 验证损坏帧分类为确定状态并非零退出。 */
async function testMalformedFramesCloseWithFailure(serverPath) {
    const cases = [
        ['missing content length', 'MALFORMED_HEADER', 'Content-Type: application/vscode-jsonrpc\r\n\r\n'],
        ['duplicate content length', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}'],
        ['negative content length', 'MALFORMED_HEADER', 'Content-Length: -1\r\n\r\n'],
        ['content length suffix', 'MALFORMED_HEADER', 'Content-Length: 2oops\r\n\r\n{}'],
        ['content length overflow', 'TOO_LARGE', 'Content-Length: 184467440737095516160\r\n\r\n'],
        ['NUL in content length', 'MALFORMED_HEADER', 'Content-Length: 2\0junk\r\n\r\n{}'],
        ['wrong newline', 'MALFORMED_HEADER', 'Content-Length: 2\n\n{}'],
        ['non utf8 charset', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-16\r\n\r\n{}'],
        ['charset without value', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset\r\n\r\n{}'],
        ['conflicting charset parameters', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8; charset=utf-16\r\n\r\n{}'],
        ['truncated payload', 'PAYLOAD_TRUNCATED', 'Content-Length: 4\r\n\r\n{}'],
        ['too many headers', 'TOO_LARGE',
         `${Array.from({ length: 33 }, (_, index) => `X-Test-${index}: value\r\n`).join('')}Content-Length: 2\r\n\r\n{}`],
    ];

    for (const [label, expectedStatus, frame] of cases) {
        await withClient(serverPath, async (client) => {
            client.sendRawFrame(Buffer.from(frame, 'ascii'));
            client.endInput();
            const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
            assert(exitCode !== 0, `${label}: malformed frame must exit non-zero, actual=${exitCode}`);
            assert(client.stderr().includes(expectedStatus),
                   `${label}: expected ${expectedStatus}, stderr=${client.stderr()}`);
        });
    }
}

/** 供 CTest 与信封变异测试共用用例表，避免两套预期分叉。 */
function protocolCases() {
    return [
        ['LSP 3.17 capability matrix', testCapabilityMatrix],
        ['request before initialize', testRequestBeforeInitialize],
        ['notification before initialize is ignored', testNotificationBeforeInitializeIsIgnored],
        ['repeated initialize', testRepeatedInitialize],
        ['exit before shutdown', testExitBeforeShutdown],
        ['shutdown and exit ordering', testShutdownExitOrdering],
        ['request after shutdown', testRequestAfterShutdown],
        ['missing jsonrpc', testMissingJsonRpc],
        ['wrong jsonrpc', testWrongJsonRpc],
        ['boolean request id', testInvalidBooleanId],
        ['structured request ids', testInvalidStructuredIds],
        ['fractional request id', testInvalidFractionalRequestId],
        ['invalid top-level messages', testInvalidTopLevelMessages],
        ['invalid params', testInvalidParams],
        ['invalid position and range numbers', testInvalidPositionAndRangeNumbers],
        ['invalid hierarchy params', testInvalidHierarchyParams],
        ['invalid editor feature params', testInvalidEditorFeatureParams],
        ['invalid editing params', testInvalidEditingParams],
        ['invalid code action range', testInvalidCodeActionRange],
        ['invalid code action context', testInvalidCodeActionContext],
        ['invalid ranges formatting params', testInvalidRangesFormattingParams],
        ['invalid code action resolve params', testInvalidCodeActionResolveParams],
        ['invalid completion resolve params', testInvalidCompletionResolveParams],
        ['invalid additional editor params', testInvalidAdditionalEditorParams],
        ['invalid semantic token params', testInvalidSemanticTokenParams],
        ['invalid workspace symbol params', testInvalidWorkspaceSymbolParams],
        ['invalid workspace diagnostic params', testInvalidWorkspaceDiagnosticParams],
        ['invalid workspace will rename params', testInvalidWorkspaceWillRenameParams],
        ['invalid diagnostic optional params', testInvalidDiagnosticOptionalParams],
        ['invalid semantic token delta result id', testInvalidSemanticTokenDeltaResultId],
        ['invalid references context', testInvalidReferencesContext],
        ['invalid inline completion params', testInvalidInlineCompletionParams],
        ['unknown method', testUnknownMethod],
        ['notification has no response', testNotificationHasNoResponse],
        ['malformed notification has no response', testMalformedNotificationHasNoResponse],
        ['malformed JSON payload', testMalformedJson],
        ['duplicate request id', testDuplicateRequestId],
        ['distinct typed request ids', testDistinctTypedRequestIds],
        ['numeric request id precision', testNumericRequestIdPrecision],
        ['cancel unknown id has no response', testCancelUnknownIdHasNoResponse],
        ['cancel known request id', testCancelKnownRequestId],
        ['set trace writes only stderr', testSetTraceWritesOnlyStderr],
        ['control notifications outside lifecycle are ignored', testControlNotificationsOutsideLifecycleAreIgnored],
        ['request work-done progress', testRequestWorkDoneProgress],
        ['workspace symbol partial results', testWorkspaceSymbolPartialResults],
        ['cancel during partial results', testCancelDuringPartialResults],
        ['references partial results', testReferencesPartialResults],
        ['call hierarchy partial results', testCallHierarchyPartialResults],
        ['type hierarchy partial results', testTypeHierarchyPartialResults],
        ['workspace diagnostic partial results', testWorkspaceDiagnosticPartialResults],
        ['oversize frame closes with failure', testOversizeFrameClosesWithFailure],
        ['malformed frames close with classified failure', testMalformedFramesCloseWithFailure],
    ];
}

/** 逐个执行并汇总失败，避免单个异常遮蔽后续回归信号。 */
async function main() {
    const serverPath = process.argv[2];
    const cases = protocolCases();
    let failures = 0;

    assert(serverPath, 'usage: node stdio_protocol_conformance.js <stdio-server>');
    for (const [name, run] of cases) {
        try {
            await run(serverPath);
            console.log(`Pass - ${name}`);
        } catch (error) {
            console.error(`Fail - ${name}: ${error.stack || error.message}`);
            failures += 1;
        }
    }

    if (failures !== 0) {
        throw new Error(`stdio protocol conformance has ${failures} failing cases`);
    }
}

module.exports = { protocolCases };

/** 作为 CTest 脚本时执行全表；被变异测试 require 时只暴露用例而不启动服务器。 */
if (require.main === module) {
    main().catch((error) => {
        console.error(`stdio protocol conformance failed: ${error.stack || error.message}`);
        process.exitCode = 1;
    });
}
