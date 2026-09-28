#include "zr_vm_language_server_stdio_internal.h"

/* 把 VM 的分配回调接到宿主堆；全局状态销毁时仍须使用同一回调。 */
static TZrPtr stdio_server_allocator(TZrPtr userData,
                                     TZrPtr pointer,
                                     TZrSize originalSize,
                                     TZrSize newSize,
                                     TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }
    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    return realloc(pointer, newSize);
}

/* 缓存中的 URI 文本归会话所有；VM 字符串由 GC 管理，此处不直接释放。 */
void free_uri_cache(SZrUriCache *cache) {
    size_t index;

    if (cache == ZR_NULL) {
        return;
    }
    for (index = 0; index < cache->count; index++) {
        free(cache->items[index].text);
        cache->items[index].text = ZR_NULL;
        cache->items[index].value = ZR_NULL;
    }
    free(cache->items);
    memset(cache, 0, sizeof(*cache));
}

/* 失同步集合只持有 VM 字符串引用，不能在此单独释放元素。 */
void free_desynchronized_document_set(SZrDesynchronizedDocumentSet *set) {
    if (set == ZR_NULL) {
        return;
    }
    free(set->items);
    memset(set, 0, sizeof(*set));
}

/* 历史 token 数据与 URI 副本独立于 VM 对象，须在会话销毁前回收。 */
static void stdio_server_free_semantic_token_cache(SZrSemanticTokenCache *cache) {
    size_t index;

    if (cache == ZR_NULL) {
        return;
    }
    for (index = 0; index < cache->count; index++) {
        free(cache->items[index].uriText);
        free(cache->items[index].data);
    }
    free(cache->items);
    memset(cache, 0, sizeof(*cache));
}

/* 推送诊断的去重快照归会话所有，释放后不再允许发布诊断。 */
static void stdio_server_free_diagnostic_push_cache(SZrDiagnosticPushCache *cache) {
    size_t index;

    if (cache == ZR_NULL) {
        return;
    }
    for (index = 0U; index < cache->count; index++) {
        free(cache->items[index].uriText);
        cache->items[index].uriText = ZR_NULL;
        cache->items[index].resultId[0] = '\0';
    }
    free(cache->items);
    memset(cache, 0, sizeof(*cache));
}

/* 一次性建立主 VM、LSP 上下文、请求去重表和读队列。
 * 各故障注入点复用 Free，故半初始化状态也必须可被安全析构。 */
SZrStdioServer *ZrLanguageServer_StdioServer_New(const SZrStdioServerOptions *options) {
    SZrStdioServer *server;
    SZrCallbackGlobal callbacks = {0};
    EZrStdioServerFaultPoint faultPoint = ZR_STDIO_SERVER_FAULT_NONE;

    if (options != ZR_NULL) {
        faultPoint = options->faultPoint;
    }
    server = (SZrStdioServer *)calloc(1, sizeof(SZrStdioServer));
    if (server == ZR_NULL) {
        return ZR_NULL;
    }
    server->requestInput.input = options != ZR_NULL && options->input != ZR_NULL
                                         ? options->input
                                         : stdin;
    server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF16;
    server->traceLevel = ZR_STDIO_TRACE_OFF;
    server->faultPoint = faultPoint;
    ZrLanguageServer_StdioLifecycle_Init(&server->lifecycle);

    server->global = ZrCore_GlobalState_New(stdio_server_allocator, ZR_NULL, 0, &callbacks);
    if (server->global == ZR_NULL || faultPoint == ZR_STDIO_SERVER_FAULT_AFTER_GLOBAL) {
        ZrLanguageServer_StdioServer_Free(server);
        return ZR_NULL;
    }
    server->state = server->global->mainThreadState;
    if (server->state == ZR_NULL) {
        ZrLanguageServer_StdioServer_Free(server);
        return ZR_NULL;
    }
    ZrCore_GlobalState_InitRegistry(server->state, server->global);
    server->context = ZrLanguageServer_LspContext_New(server->state);
    if (server->context == ZR_NULL || faultPoint == ZR_STDIO_SERVER_FAULT_AFTER_CONTEXT) {
        ZrLanguageServer_StdioServer_Free(server);
        return ZR_NULL;
    }
    server->requestRegistry = ZrLanguageServer_StdioRequestRegistry_New();
    if (server->requestRegistry == ZR_NULL || !ZrLanguageServer_StdioRequestInput_Init(server) ||
        faultPoint == ZR_STDIO_SERVER_FAULT_AFTER_INPUT_INIT) {
        ZrLanguageServer_StdioServer_Free(server);
        return ZR_NULL;
    }
    return server;
}

/* 仅启动输入线程；请求分发仍由调用方在主线程驱动。 */
TZrBool ZrLanguageServer_StdioServer_Start(SZrStdioServer *server) {
    if (server == ZR_NULL || !ZrLanguageServer_StdioRequestInput_Start(server)) {
        return ZR_FALSE;
    }
    if (server->requestInput.stopRequested ||
        server->requestInput.readerStarted == ZR_FALSE) {
        return ZR_FALSE;
    }
    if (server->faultPoint == ZR_STDIO_SERVER_FAULT_AFTER_READER_START) {
        ZrLanguageServer_StdioRequestInput_Stop(server);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 通知读队列停止并唤醒 Take；调用方仍须保证阻塞中的 FILE 读取能返回。 */
void ZrLanguageServer_StdioServer_Shutdown(SZrStdioServer *server) {
    ZrLanguageServer_StdioRequestInput_Stop(server);
}

/* 先停读并 Join，随后释放队列/注册表、缓存和依赖它们的 LSP/VM 状态。 */
void ZrLanguageServer_StdioServer_Free(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }

    ZrLanguageServer_StdioRequestInput_Stop(server);
    ZrLanguageServer_StdioRequestInput_Join(server);
    ZrLanguageServer_StdioRequestInput_Free(server);
    ZrLanguageServer_StdioRequestRegistry_Free(server->requestRegistry);
    server->requestRegistry = ZR_NULL;
    free_desynchronized_document_set(&server->desynchronizedDocuments);
    free_uri_cache(&server->uriCache);
    stdio_server_free_semantic_token_cache(&server->semanticTokenCache);
    stdio_server_free_diagnostic_push_cache(&server->diagnosticPushCache);
    if (server->context != ZR_NULL && server->state != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(server->state, server->context);
    }
    server->context = ZR_NULL;
    server->state = ZR_NULL;
    if (server->global != ZR_NULL) {
        ZrCore_GlobalState_Free(server->global);
    }
    server->global = ZR_NULL;
    free(server);
}
