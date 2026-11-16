#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "zr_vm_language_server_stdio_internal.h"
#include "project/lsp_project_internal.h"
#include "project/lsp_workspace.h"
#include "interface/lsp_interface_internal.h"
#include "zr_vm_language_server/lsp_semantic_snapshot.h"
#include "zr_vm_core/utf8.h"
#include "parser/parser_ownership.h"
#include "path_support.h"
#include "unity.h"

#ifdef _WIN32
#define test_dup _dup
#define test_dup2 _dup2
#define test_close _close
#define test_fileno _fileno
#else
#include <unistd.h>
#define test_dup dup
#define test_dup2 dup2
#define test_close close
#define test_fileno fileno
#endif

/* 每个 Unity 用例独占 stdio 状态与临时项目，避免 didClose/didSave 的缓存和诊断目标跨用例残留。
 * 无 text 的 didSave 与 didClose 的参数同为 textDocument.uri，两类用例共用 g_closeParams。 */
static SZrStdioServer *g_server;
static SZrString *g_mainUri;
static SZrString *g_peerUri;
static SZrString *g_rootUri;
static cJSON *g_closeParams;
static SZrArray g_uris;
static char g_mainPath[ZR_TESTS_PATH_MAX];
static char g_peerPath[ZR_TESTS_PATH_MAX];
static char g_errorPath[ZR_TESTS_PATH_MAX];
static char g_projectPath[ZR_TESTS_PATH_MAX];
static const char *g_diskSource = "module main;\npub fn diskValue(): int { return 1; }\n";
static const char *g_overlaySource = "module main;\npub fn overlayValue(): int { return 2; }\n";
static FILE *g_output;
static int g_savedStdout = -1;
#ifdef _WIN32
static int g_stdoutMode = -1;
#endif

/* 真实 push 通知经 stdout 发出；捕获 fd 并在断言跳出时由 tearDown 恢复。 */
static void restore_stdout(void) {
    if (g_savedStdout >= 0) {
        fflush(stdout);
        test_dup2(g_savedStdout, test_fileno(stdout));
        test_close(g_savedStdout);
        g_savedStdout = -1;
#ifdef _WIN32
        if (g_stdoutMode >= 0) {
            _setmode(test_fileno(stdout), g_stdoutMode);
            g_stdoutMode = -1;
        }
#endif
        clearerr(stdout);
    }
}

static void capture_stdout(void) {
    g_output = tmpfile();
    TEST_ASSERT_NOT_NULL(g_output);
    fflush(stdout);
    g_savedStdout = test_dup(test_fileno(stdout));
    TEST_ASSERT_TRUE(g_savedStdout >= 0);
#ifdef _WIN32
    g_stdoutMode = _setmode(test_fileno(stdout), _O_BINARY);
    TEST_ASSERT_TRUE(g_stdoutMode >= 0);
#endif
    TEST_ASSERT_TRUE(test_dup2(test_fileno(g_output), test_fileno(stdout)) >= 0);
}

/* 项目层关闭路径要从真实磁盘恢复版本，因此夹具写入的文件必须可被磁盘读取入口找到。 */
static void write_source_bytes(const char *path, const char *source, size_t length) {
    FILE *file = fopen(path, "wb");
    size_t written;
    int closed;

    TEST_ASSERT_NOT_NULL(file);
    written = fwrite(source, 1, length, file);
    closed = fclose(file);
    TEST_ASSERT_EQUAL_UINT64(length, written);
    TEST_ASSERT_EQUAL_INT(0, closed);
}

static void write_source(const char *path, const char *source) {
    write_source_bytes(path, source, strlen(source));
}

/* didOpen 的编辑器覆盖层以 UpdateDocument 建立；关闭测试还需确认它确实标为打开。 */
static void open_document(SZrString *uri, const char *source, TZrSize version) {
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_server->state, g_server->context, uri, source, strlen(source), version));
    TEST_ASSERT_TRUE(get_file_version_for_uri(g_server, uri)->isOpenDocument);
}

static void use_document_uri(SZrString *uri) {
    cJSON *textDocument = cJSON_GetObjectItemCaseSensitive(g_closeParams, "textDocument");
    cJSON *uriJson = cJSON_CreateString(ZrCore_String_GetNativeString(uri));

    TEST_ASSERT_NOT_NULL(uriJson);
    TEST_ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(textDocument, "uri", uriJson));
}

/* 生命周期回归必须经过真实通知处理器，不能把切换 isOpenDocument 当作 didOpen。 */
static int notify_open(const char *source, TZrSize version) {
    cJSON *params = cJSON_Duplicate(g_closeParams, 1);
    cJSON *textDocument;
    int result;

    TEST_ASSERT_NOT_NULL(params);
    textDocument = cJSON_GetObjectItemCaseSensitive(params, "textDocument");
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(textDocument, "languageId", "zr"));
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(textDocument, "text", source));
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(textDocument, "version", (double)version));
    result = handle_did_open(g_server, params);
    cJSON_Delete(params);
    return result;
}

static int notify_full_change(const char *source, TZrSize version) {
    cJSON *params = cJSON_Duplicate(g_closeParams, 1);
    cJSON *textDocument;
    cJSON *changes;
    cJSON *change;
    int result;

    TEST_ASSERT_NOT_NULL(params);
    textDocument = cJSON_GetObjectItemCaseSensitive(params, "textDocument");
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(textDocument, "version", (double)version));
    changes = cJSON_AddArrayToObject(params, "contentChanges");
    TEST_ASSERT_NOT_NULL(changes);
    change = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_TRUE(cJSON_AddItemToArray(changes, change));
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(change, "text", source));
    result = handle_did_change(g_server, params);
    cJSON_Delete(params);
    return result;
}

static int notify_comment_suffix_change(TZrSize version) {
    cJSON *params = cJSON_Duplicate(g_closeParams, 1);
    cJSON *textDocument;
    cJSON *changes;
    cJSON *change;
    cJSON *range;
    cJSON *start;
    cJSON *end;
    int column = g_server->positionEncoding == ZR_STDIO_POSITION_ENCODING_UTF8 ? 11 : 7;
    int result;

    TEST_ASSERT_NOT_NULL(params);
    textDocument = cJSON_GetObjectItemCaseSensitive(params, "textDocument");
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(textDocument, "version", (double)version));
    changes = cJSON_AddArrayToObject(params, "contentChanges");
    TEST_ASSERT_NOT_NULL(changes);
    change = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_TRUE(cJSON_AddItemToArray(changes, change));
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(change, "text", "e2"));
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(change, "rangeLength", 2));
    range = cJSON_AddObjectToObject(change, "range");
    TEST_ASSERT_NOT_NULL(range);
    start = cJSON_AddObjectToObject(range, "start");
    end = cJSON_AddObjectToObject(range, "end");
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(start, "line", 1));
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(start, "character", column));
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(end, "line", 1));
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(end, "character", column + 2));
    result = handle_did_change(g_server, params);
    cJSON_Delete(params);
    return result;
}

static int notify_watch(SZrString *uri, int type) {
    cJSON *params = cJSON_CreateObject();
    cJSON *changes;
    cJSON *change;
    int result;

    TEST_ASSERT_NOT_NULL(params);
    changes = cJSON_AddArrayToObject(params, "changes");
    TEST_ASSERT_NOT_NULL(changes);
    change = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_TRUE(cJSON_AddItemToArray(changes, change));
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(change, "uri", ZrCore_String_GetNativeString(uri)));
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(change, "type", type));
    result = handle_did_change_watched_files(g_server, params);
    cJSON_Delete(params);
    return result;
}

/* 每次断言重新读取项目诊断目标，防止沿用关闭前的 URI 列表。 */
static void collect_uris(void) {
    if (g_uris.isValid) {
        ZrCore_Array_Free(g_server->state, &g_uris);
    }
    ZrCore_Array_Construct(&g_uris);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspProject_CollectDiagnosticDocumentUris(
            g_server->state, g_server->context, &g_uris));
}

/* 项目可能规范化 file URI；断言目标身份时与生产层使用同一等价比较。 */
static TZrBool has_uri(SZrString *uri) {
    for (TZrSize index = 0; index < g_uris.length; index++) {
        SZrString **item = (SZrString **)ZrCore_Array_Get(&g_uris, index);
        if (item != ZR_NULL && ZrLanguageServer_LspUri_Equivalent(*item, uri)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 关闭失败清理既要释放文件版本，也要从 workspace diagnostic 集合撤销目标。 */
static void assert_main_released(void) {
    TEST_ASSERT_NULL(get_file_version_for_uri(g_server, g_mainUri));
    collect_uris();
    TEST_ASSERT_FALSE_MESSAGE(has_uri(g_mainUri),
                             "A released document must not remain a workspace diagnostic target");
}

/* 以内容快照比较 main 的已提交文本，区分磁盘版本与编辑器 overlay。 */
static void assert_main_content(const char *expected) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    SZrFileVersionContentSnapshot snapshot;
    TZrBool matches;

    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_TRUE(ZrLanguageServer_FileVersionContentSnapshot_Acquire(g_server->state, fileVersion, &snapshot));
    matches = snapshot.contentLength == strlen(expected) &&
              memcmp(snapshot.content, expected, strlen(expected)) == 0;
    ZrLanguageServer_FileVersionContentSnapshot_Free(g_server->state, &snapshot);
    TEST_ASSERT_TRUE(matches);
}

/* 各用例共享可写的项目/源文件布局，但 server 与 JSON 请求在用例间重建。 */
void setUp(void) {
    char rootPath[ZR_TESTS_PATH_MAX];
    char *mainUriText;
    cJSON *textDocument;

    g_server = ZrLanguageServer_StdioServer_New(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_server);
    ZrCore_Array_Construct(&g_uris);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "stdio_document_close", "project", "close", ".zrp", g_projectPath, sizeof(g_projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "stdio_document_close", "project/src", "main", ".zr", g_mainPath, sizeof(g_mainPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "stdio_document_close", "project/src", "peer", ".zr", g_peerPath, sizeof(g_peerPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "stdio_document_close", "project/src", "other_error", ".zr", g_errorPath, sizeof(g_errorPath)));
    write_source(g_projectPath, "{\"name\":\"close\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}");
    write_source(g_mainPath, g_diskSource);
    write_source(g_peerPath, "module peer;\npub fn peerValue(): int { return 3; }\n");
    strcpy(rootPath, g_projectPath);
    *strrchr(rootPath, '/') = '\0';
    g_rootUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, rootPath);
    g_mainUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, g_mainPath);
    g_peerUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, g_peerPath);
    TEST_ASSERT_NOT_NULL(g_rootUri);
    TEST_ASSERT_NOT_NULL(g_mainUri);
    TEST_ASSERT_NOT_NULL(g_peerUri);
    mainUriText = ZrCore_String_GetNativeString(g_mainUri);
    g_closeParams = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(g_closeParams);
    textDocument = cJSON_AddObjectToObject(g_closeParams, "textDocument");
    TEST_ASSERT_NOT_NULL(textDocument);
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(textDocument, "uri", mainUriText));
}

/* 先释放引用 URI 的协议与项目状态，再删除磁盘夹具，避免下一个用例读取旧版本。 */
void tearDown(void) {
    restore_stdout();
    if (g_output != ZR_NULL) {
        fclose(g_output);
        g_output = ZR_NULL;
    }
    cJSON_Delete(g_closeParams);
    g_closeParams = ZR_NULL;
    if (g_uris.isValid) {
        ZrCore_Array_Free(g_server->state, &g_uris);
    }
    ZrLanguageServer_StdioServer_Free(g_server);
    g_server = ZR_NULL;
    remove(g_mainPath);
    remove(g_peerPath);
    remove(g_errorPath);
    remove(g_projectPath);
}

/* 无工作区根时，关闭必须撤销独立打开文档；重复关闭和随后重开都应保持可用。 */
static void test_close_outside_workspace_releases_project_diagnostic_target(void) {
    open_document(g_mainUri, g_overlaySource, 7);
    collect_uris();
    TEST_ASSERT_TRUE(has_uri(g_mainUri));
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    assert_main_released();
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    assert_main_released();
    open_document(g_mainUri, g_overlaySource, 0);
    collect_uris();
    TEST_ASSERT_TRUE(has_uri(g_mainUri));
}

/* 关闭同一夹具目录中的 main，不得清除仍打开的 peer 覆盖层或更改其版本。 */
static void test_close_preserves_other_open_project_document(void) {
    SZrFileVersion *peerVersion;
    open_document(g_mainUri, g_overlaySource, 7);
    open_document(g_peerUri, "module peer;\npub fn peerOverlay(): int { return 4; }\n", 9);
    peerVersion = get_file_version_for_uri(g_server, g_peerUri);
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    assert_main_released();
    TEST_ASSERT_TRUE(has_uri(g_peerUri));
    TEST_ASSERT_EQUAL_PTR(peerVersion, get_file_version_for_uri(g_server, g_peerUri));
    TEST_ASSERT_TRUE(peerVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT(9, peerVersion->version);
}

/* 工作区内磁盘文件仍在时，关闭应以磁盘内容替换 overlay 并继续作为诊断目标。 */
static void test_close_inside_workspace_restores_disk_diagnostic_target(void) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    TZrBool matchesDisk;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    open_document(g_mainUri, g_overlaySource, 7);
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_TRUE(ZrLanguageServer_FileVersionContentSnapshot_Acquire(g_server->state, fileVersion, &snapshot));
    matchesDisk = snapshot.contentLength == strlen(g_diskSource) &&
                  memcmp(snapshot.content, g_diskSource, strlen(g_diskSource)) == 0;
    ZrLanguageServer_FileVersionContentSnapshot_Free(g_server->state, &snapshot);
    TEST_ASSERT_TRUE(matchesDisk);
    collect_uris();
    TEST_ASSERT_TRUE(has_uri(g_mainUri));
}

/* 根内文件在关闭前消失时，磁盘恢复失败应转入完整清理路径。 */
static void test_close_missing_disk_file_releases_project_diagnostic_target(void) {
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    open_document(g_mainUri, g_overlaySource, 7);
    TEST_ASSERT_EQUAL_INT(0, remove(g_mainPath));
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    assert_main_released();
}

/* 无工作区根时，未打开文档的 didSave 与 didClose 走同一门禁：不读盘，也不登记文件版本或诊断目标。 */
static void test_save_closed_outside_workspace_does_not_reload_disk(void) {
    TEST_ASSERT_EQUAL_INT(0, handle_did_save(g_server, g_closeParams));
    assert_main_released();
}

/* 根已登记时，借 `..` 离开根的未打开 URI 同样被拒绝：文件按该 URI 读得到，但 didSave 不读盘、不登记版本。 */
static void test_save_closed_dotdot_escape_does_not_reload_disk(void) {
    char outsidePath[ZR_TESTS_PATH_MAX];
    char escapeUriText[ZR_TESTS_PATH_MAX * 4];
    cJSON *params;
    cJSON *textDocument;
    SZrString *escapeUri;
    char *reachableText;
    size_t reachableLength = 0;
    int saved;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "stdio_document_close", "outside", "secret", ".zr", outsidePath, sizeof(outsidePath)));
    write_source(outsidePath, "module secret;\npub fn outsideValue(): int { return 6; }\n");
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_TRUE(snprintf(escapeUriText, sizeof(escapeUriText), "%s/../outside/secret.zr",
                              ZrCore_String_GetNativeString(g_rootUri)) < (int)sizeof(escapeUriText));
    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    textDocument = cJSON_AddObjectToObject(params, "textDocument");
    TEST_ASSERT_NOT_NULL(textDocument);
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(textDocument, "uri", escapeUriText));

    saved = handle_did_save(g_server, params);
    escapeUri = server_get_cached_uri(g_server, escapeUriText);
    /* 先确认夹具有效：若该 URI 本就读不到文件，下面的"未读盘"断言就不说明门禁起了作用。 */
    reachableText = escapeUri != ZR_NULL ? read_document_text_from_uri(escapeUri, &reachableLength) : NULL;
    cJSON_Delete(params);
    remove(outsidePath);
    free(reachableText);
    TEST_ASSERT_NOT_NULL(escapeUri);
    TEST_ASSERT_NOT_NULL_MESSAGE(reachableText, "The escape URI must name a readable file outside the root");
    TEST_ASSERT_EQUAL_INT(0, saved);
    TEST_ASSERT_NULL(get_file_version_for_uri(g_server, escapeUri));
}

/* 根内未打开的文档由 didSave 重新读盘；磁盘内容变化后再次保存取得下一代版本。 */
static void test_save_closed_inside_workspace_reloads_disk(void) {
    const char *savedSource = "module main;\npub fn savedValue(): int { return 5; }\n";
    SZrFileVersion *fileVersion;
    TZrSize firstVersion;
    TZrSize firstGeneration;
    TZrUInt64 providerGeneration;
    SZrSemanticAnalyzer *analyzer;

    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    firstVersion = fileVersion->version;
    firstGeneration = fileVersion->textBlock->contentGeneration;
    assert_main_content(g_diskSource);
    providerGeneration = g_server->context->semanticSnapshotProviderGeneration;
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(analyzer);
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(analyzer, ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_UINT64(firstGeneration, fileVersion->textBlock->contentGeneration);
    TEST_ASSERT_EQUAL_UINT64(providerGeneration, g_server->context->semanticSnapshotProviderGeneration);

    write_source(g_mainPath, savedSource);
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(firstVersion, fileVersion->version);
    TEST_ASSERT_TRUE(fileVersion->textBlock->contentGeneration > firstGeneration);
    assert_main_content(savedSource);
}

/* 已打开的根外文档收到 didSave 只刷新诊断：保留 overlay 与客户端版本，不以磁盘内容替换。 */
static void test_save_open_outside_workspace_keeps_overlay(void) {
    SZrFileVersion *fileVersion;

    open_document(g_mainUri, g_overlaySource, 7);
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_TRUE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT(7, fileVersion->version);
    assert_main_content(g_overlaySource);
}

/* 高版本 overlay 真正关闭、watch 后重新 didOpen(1)；版本门禁只在新的编辑 epoch 重启。 */
static void assert_close_watch_reopen_lifecycle(EZrStdioPositionEncoding encoding) {
    const char *watchedDisk = "module main;\r\n// \xe4\xb8\xad\xf0\x9f\x98\x80 disk\r\npub fn watchedDisk(): int { return 8; }\r\n";
    const char *reopened = "module main;\r\n// \xe4\xb8\xad\xf0\x9f\x98\x80 e1\r\npub fn unsavedEpoch(): int { return 9; }\r\n";
    const char *changed = "module main;\r\n// \xe4\xb8\xad\xf0\x9f\x98\x80 e2\r\npub fn unsavedEpoch(): int { return 9; }\r\n";
    SZrFileVersion *fileVersion;
    TZrUInt64 closedOwnerKey;
    TZrUInt64 closedSourceOrigin;
    SZrFileVersionContentSnapshot oldContent = {0};
    SZrLspSemanticSnapshot *oldSemantic;
    SZrLspHistoricalSemanticSnapshot history;
    TZrSize highGeneration;
    TZrSize closedGeneration;
    SZrFileVersionContentBlock *changedBlock;

    g_server->deferDiagnostics = ZR_TRUE;
    g_server->positionEncoding = encoding;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, notify_open(g_overlaySource, 900));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    highGeneration = fileVersion->textBlock->contentGeneration;
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(900, fileVersion->version);
    TEST_ASSERT_TRUE(fileVersion->textBlock->contentGeneration > highGeneration);
    assert_main_content(g_diskSource);

    write_source(g_mainPath, watchedDisk);
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 2));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(900, fileVersion->version);
    assert_main_content(watchedDisk);
    TEST_ASSERT_TRUE(ZrLanguageServer_FileVersionContentSnapshot_Acquire(
            g_server->state, fileVersion, &oldContent));
    oldSemantic = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(oldSemantic);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, oldSemantic));
    TEST_ASSERT_NOT_NULL(fileVersion->ast);
    closedOwnerKey = ZrParser_Ownership_Key(fileVersion->ast->ownership);
    closedSourceOrigin = ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership);
    closedGeneration = fileVersion->textBlock->contentGeneration;
    TEST_ASSERT_TRUE(closedOwnerKey != 0 && closedSourceOrigin != 0);

    TEST_ASSERT_EQUAL_INT(1, notify_open(reopened, 1));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_TRUE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(1, fileVersion->version);
    TEST_ASSERT_TRUE(fileVersion->textBlock != oldContent.contentBlock);
    TEST_ASSERT_TRUE(fileVersion->textBlock->contentGeneration > closedGeneration);
    TEST_ASSERT_NOT_NULL(fileVersion->ast);
    TEST_ASSERT_TRUE(ZrParser_Ownership_Key(fileVersion->ast->ownership) != 0 &&
                     ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership) != 0);
    TEST_ASSERT_TRUE(ZrParser_Ownership_Key(fileVersion->ast->ownership) != closedOwnerKey);
    TEST_ASSERT_TRUE(ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership) != closedSourceOrigin);
    TEST_ASSERT_EQUAL_UINT64(0, fileVersion->parserDiagnostics.length);
    TEST_ASSERT_NOT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(fileVersion->ast,
                          ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri)->ast);
    TEST_ASSERT_EQUAL_INT(ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE, fileVersion->lastParseMode);
    TEST_ASSERT_FALSE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, oldSemantic));
    TEST_ASSERT_FALSE(ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(g_server->context, g_mainUri, 0, &history));
    TEST_ASSERT_EQUAL_UINT64(strlen(watchedDisk), oldContent.contentLength);
    TEST_ASSERT_EQUAL_MEMORY(watchedDisk, oldContent.content, oldContent.contentLength);
    assert_main_content(reopened);
    ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, oldSemantic);
    ZrLanguageServer_FileVersionContentSnapshot_Free(g_server->state, &oldContent);

    /* 同一行中跨过中/补充码点后按协商编码应用范围，不以全量替换掩盖位置转换。 */
    TEST_ASSERT_EQUAL_INT(1, notify_comment_suffix_change(2));
    TEST_ASSERT_EQUAL_UINT64(2, fileVersion->version);
    TEST_ASSERT_EQUAL_UINT64(0, fileVersion->parserDiagnostics.length);
    TEST_ASSERT_NOT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(fileVersion->ast,
                          ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri)->ast);
    TEST_ASSERT_FALSE(document_is_desynchronized(g_server, g_mainUri));
    assert_main_content(changed);
    changedBlock = fileVersion->textBlock;
    TEST_ASSERT_EQUAL_INT(0, notify_full_change(g_diskSource, 1));
    TEST_ASSERT_EQUAL_INT(0, notify_full_change(g_diskSource, 2));
    TEST_ASSERT_EQUAL_PTR(changedBlock, fileVersion->textBlock);
    TEST_ASSERT_EQUAL_UINT64(2, fileVersion->version);
    TEST_ASSERT_TRUE(document_is_desynchronized(g_server, g_mainUri));
    assert_main_content(changed);
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(changed, 3));
    TEST_ASSERT_FALSE(document_is_desynchronized(g_server, g_mainUri));
}

static void test_close_watch_reopen_version_one_utf16(void) {
    assert_close_watch_reopen_lifecycle(ZR_STDIO_POSITION_ENCODING_UTF16);
}

static void test_close_watch_reopen_version_one_utf8(void) {
    assert_close_watch_reopen_lifecycle(ZR_STDIO_POSITION_ENCODING_UTF8);
}

/* 同文本、同版本也属于新的 editor epoch，不能保留旧文本块和 AST/source borrow。 */
static void test_reopen_equal_version_same_text_gets_new_generation(void) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot closedContent = {0};
    SZrLspSemanticSnapshot *closedSemantic;
    TZrUInt64 closedOwnerKey;
    TZrUInt64 closedSourceOrigin;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, notify_open(g_diskSource, 7));
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_EQUAL_UINT64(7, fileVersion->version);
    TEST_ASSERT_TRUE(ZrLanguageServer_FileVersionContentSnapshot_Acquire(
            g_server->state, fileVersion, &closedContent));
    closedSemantic = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(closedSemantic);
    TEST_ASSERT_NOT_NULL(fileVersion->ast);
    closedOwnerKey = ZrParser_Ownership_Key(fileVersion->ast->ownership);
    closedSourceOrigin = ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership);
    TEST_ASSERT_TRUE(closedOwnerKey != 0 && closedSourceOrigin != 0);
    TEST_ASSERT_EQUAL_INT(1, notify_open(g_diskSource, 7));
    TEST_ASSERT_TRUE(fileVersion->textBlock != closedContent.contentBlock);
    TEST_ASSERT_TRUE(fileVersion->textBlock->contentGeneration > closedContent.contentGeneration);
    TEST_ASSERT_NOT_NULL(fileVersion->ast);
    TEST_ASSERT_TRUE(ZrParser_Ownership_Key(fileVersion->ast->ownership) != 0 &&
                     ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership) != 0);
    TEST_ASSERT_TRUE(ZrParser_Ownership_Key(fileVersion->ast->ownership) != closedOwnerKey);
    TEST_ASSERT_TRUE(ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership) != closedSourceOrigin);
    TEST_ASSERT_FALSE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, closedSemantic));
    TEST_ASSERT_EQUAL_INT(0, notify_open(g_overlaySource, 8));
    assert_main_content(g_diskSource);
    ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, closedSemantic);
    ZrLanguageServer_FileVersionContentSnapshot_Free(g_server->state, &closedContent);
}

/* watcher/change/delete 以及直接磁盘刷新都不能擦除仍打开的未保存 buffer。 */
static void test_open_overlay_survives_disk_change_and_delete(void) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *block;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;
    cJSON *params;
    cJSON *files;
    cJSON *file;
    int deleted;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, notify_open(g_overlaySource, 7));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    block = fileVersion->textBlock;
    ast = fileVersion->ast;
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(analyzer);
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 2));
    TEST_ASSERT_EQUAL_INT(0, update_document_contents_from_disk(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_INT(0, remove(g_mainPath));
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 3));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    files = cJSON_AddArrayToObject(params, "files");
    TEST_ASSERT_NOT_NULL(files);
    file = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(cJSON_AddItemToArray(files, file));
    TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(file, "uri", ZrCore_String_GetNativeString(g_mainUri)));
    deleted = handle_did_delete_files(g_server, params);
    cJSON_Delete(params);
    TEST_ASSERT_EQUAL_INT(1, deleted);
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(block, fileVersion->textBlock);
    TEST_ASSERT_EQUAL_PTR(ast, fileVersion->ast);
    TEST_ASSERT_EQUAL_PTR(analyzer, ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_TRUE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(7, fileVersion->version);
    assert_main_content(g_overlaySource);
}

/* 删除磁盘工程清单也不能连带回收其仍打开的源文件覆盖层。 */
static void test_project_disk_delete_preserves_open_source_overlay(void) {
    SZrString *projectUri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *block;
    SZrSemanticAnalyzer *analyzer;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, notify_open(g_overlaySource, 7));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    block = fileVersion->textBlock;
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(analyzer);
    projectUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, g_projectPath);
    TEST_ASSERT_NOT_NULL(projectUri);
    TEST_ASSERT_EQUAL_INT(0, remove(g_projectPath));
    TEST_ASSERT_EQUAL_INT(1, notify_watch(projectUri, 3));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(block, fileVersion->textBlock);
    TEST_ASSERT_EQUAL_PTR(analyzer, ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_TRUE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(7, fileVersion->version);
    assert_main_content(g_overlaySource);
}

/* 错误 overlay 的 parser 诊断与已推送结果不能被 disk delete 的空诊断覆盖。 */
static void test_open_error_overlay_watch_delete_does_not_clear_diagnostic_push(void) {
    const char *badEditor = "\"unfinished editor\r\n";
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *block;
    TZrSize diagnosticCount;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];
    SZrStdioFrameReaderLimits limits;
    char *payload = ZR_NULL;
    TZrSize payloadLength = 0;
    EZrStdioFrameReadStatus frameStatus;
    cJSON *message;
    const cJSON *params;
    const cJSON *diagnostics;
    TZrBool isExpectedDiagnostic;
    int frames = 0;

    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    capture_stdout();
    TEST_ASSERT_EQUAL_INT(1, notify_open(badEditor, 900));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    block = fileVersion->textBlock;
    diagnosticCount = fileVersion->parserDiagnostics.length;
    TEST_ASSERT_TRUE(diagnosticCount > 0);
    TEST_ASSERT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_UINT64(1, g_server->diagnosticPushCache.count);
    strcpy(resultId, g_server->diagnosticPushCache.items[0].resultId);
    TEST_ASSERT_TRUE(resultId[0] != '\0');
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 2));
    TEST_ASSERT_EQUAL_INT(0, remove(g_mainPath));
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 3));
    publish_diagnostics(g_server, g_mainUri);
    restore_stdout();

    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(block, fileVersion->textBlock);
    TEST_ASSERT_TRUE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(900, fileVersion->version);
    TEST_ASSERT_EQUAL_UINT64(diagnosticCount, fileVersion->parserDiagnostics.length);
    TEST_ASSERT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_STRING(resultId, g_server->diagnosticPushCache.items[0].resultId);
    TEST_ASSERT_TRUE(g_server->diagnosticPushCache.items[0].hasDocumentVersion);
    TEST_ASSERT_EQUAL_UINT64(900, g_server->diagnosticPushCache.items[0].documentVersion);
    assert_main_content(badEditor);

    rewind(g_output);
    ZrLanguageServer_StdioFrameReader_DefaultLimits(&limits);
    while ((frameStatus = ZrLanguageServer_StdioFrameReader_Read(
                    g_output, &limits, &payload, &payloadLength)) == ZR_STDIO_FRAME_READ_OK) {
        message = cJSON_ParseWithLength(payload, payloadLength);
        free(payload);
        payload = ZR_NULL;
        params = get_object_item(message, "params");
        diagnostics = get_object_item(params, "diagnostics");
        isExpectedDiagnostic = cJSON_IsString(get_object_item(message, "method")) &&
                               strcmp(cJSON_GetStringValue(get_object_item(message, "method")),
                                      ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS) == 0 &&
                               cJSON_IsNumber(get_object_item(params, "version")) &&
                               get_object_item(params, "version")->valuedouble == 900 &&
                               cJSON_IsArray(diagnostics) && cJSON_GetArraySize(diagnostics) > 0;
        cJSON_Delete(message);
        TEST_ASSERT_TRUE_MESSAGE(isExpectedDiagnostic, "Open errors must never receive an empty diagnostic push from disk events");
        frames++;
    }
    free(payload);
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_FRAME_READ_EOF, frameStatus);
    TEST_ASSERT_EQUAL_INT(1, frames);
}

/* 关闭缓存的读取失败不得先释放 parser/analyzer/历史状态，也不能把失败当成一次成功更新。 */
static void test_closed_disk_read_failure_preserves_existing_state(void) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *block;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;
    SZrLspSemanticSnapshot *snapshot;
    TZrSize version;
    TZrSize generation;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    block = fileVersion->textBlock;
    ast = fileVersion->ast;
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(analyzer);
    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(snapshot);
    version = fileVersion->version;
    generation = block->contentGeneration;
    TEST_ASSERT_EQUAL_INT(0, remove(g_mainPath));
    TEST_ASSERT_EQUAL_INT(0, update_document_contents_from_disk(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_INT(0, notify_watch(g_mainUri, 2));
    TEST_ASSERT_EQUAL_INT(0, handle_did_save(g_server, g_closeParams));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(block, fileVersion->textBlock);
    TEST_ASSERT_EQUAL_PTR(ast, fileVersion->ast);
    TEST_ASSERT_EQUAL_PTR(analyzer, ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_UINT64(version, fileVersion->version);
    TEST_ASSERT_EQUAL_UINT64(generation, block->contentGeneration);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, snapshot));
    assert_main_content(g_diskSource);
    ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, snapshot);
}

/* rb 保原始字节；NUL+坏 UTF-8 与 NUL+全合法 UTF-8 都须在提交前拒绝，源码转义则合法。 */
static void test_disk_raw_bytes_and_invalid_utf8_preserve_cached_state(void) {
    static const char rawSource[] = "module main;\r\n// raw\0\r\n\xff";
    static const char nulSource[] = "module main;\r\npub fn prefix(): int { return 1; }\r\n\0pub fn validTail(): int { return 2; }\r\n";
    static const char escapedSource[] = "module main;\r\npub fn escapedNul(): string { return \"\\0\"; }\r\n";
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *block;
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *ast;
    SZrLspSemanticSnapshot *snapshot;
    TZrSize version;
    char *readSource;
    size_t readLength = 0;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    block = fileVersion->textBlock;
    ast = fileVersion->ast;
    version = fileVersion->version;
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(analyzer);
    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(snapshot);
    write_source_bytes(g_mainPath, rawSource, sizeof(rawSource) - 1);
    readSource = read_document_text_from_uri(g_mainUri, &readLength);
    TEST_ASSERT_NOT_NULL(readSource);
    TEST_ASSERT_EQUAL_UINT64(sizeof(rawSource) - 1, readLength);
    TEST_ASSERT_EQUAL_MEMORY(rawSource, readSource, readLength);
    free(readSource);
    TEST_ASSERT_EQUAL_INT(0, update_document_contents_from_disk(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_INT(0, handle_did_save(g_server, g_closeParams));
    TEST_ASSERT_EQUAL_PTR(block, fileVersion->textBlock);

    /* 证明不是依赖坏 UTF-8 拒绝，也不是只验证 NUL 前缀。 */
    TEST_ASSERT_TRUE(ZrCore_Utf8_IsValid((TZrNativeString)nulSource, sizeof(nulSource) - 1));
    write_source_bytes(g_mainPath, nulSource, sizeof(nulSource) - 1);
    readSource = read_document_text_from_uri(g_mainUri, &readLength);
    TEST_ASSERT_NOT_NULL(readSource);
    TEST_ASSERT_EQUAL_UINT64(sizeof(nulSource) - 1, readLength);
    TEST_ASSERT_EQUAL_MEMORY(nulSource, readSource, readLength);
    free(readSource);
    TEST_ASSERT_EQUAL_INT(0, update_document_contents_from_disk(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_INT(0, notify_watch(g_mainUri, 2));
    TEST_ASSERT_EQUAL_INT(0, handle_did_save(g_server, g_closeParams));
    TEST_ASSERT_FALSE(ZrLanguageServer_Lsp_UpdateClosedDocument(
            g_server->state, g_server->context, g_mainUri, nulSource, sizeof(nulSource) - 1));
    TEST_ASSERT_FALSE(ZrLanguageServer_IncrementalParser_UpdateClosedDocument(
            g_server->state, g_server->context->parser, g_mainUri, nulSource, sizeof(nulSource) - 1));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(block, fileVersion->textBlock);
    TEST_ASSERT_EQUAL_PTR(ast, fileVersion->ast);
    TEST_ASSERT_EQUAL_PTR(analyzer, ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_EQUAL_UINT64(version, fileVersion->version);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, snapshot));
    assert_main_content(g_diskSource);

    /* 字符串字面量中的反斜线+0 不是物理 NUL，不能被 memchr gate 误拒。 */
    write_source(g_mainPath, escapedSource);
    TEST_ASSERT_EQUAL_INT(1, update_document_contents_from_disk(g_server, g_mainUri));
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(version, fileVersion->version);
    TEST_ASSERT_EQUAL_UINT64(0, fileVersion->parserDiagnostics.length);
    TEST_ASSERT_NOT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_FALSE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, snapshot));
    assert_main_content(escapedSource);
    ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, snapshot);
}

/* 坏 dependency 的来源替换先撤销 importer typed；保其 overlay，下一次真实 editor 修复可恢复导航。 */
static void test_bad_dependency_invalidates_importer_and_editor_repair_restores_navigation(void) {
    const char *mainSource = "module main;\nlet peerModule = import(\"peer\");\npub fn importedValue(): int { return peerModule.peerValue(); }\n";
    const char *badPeer = "module peer;\r\npub fn peerValue( {\r\n";
    const char *repairedPeer = "module peer;\r\npub fn peerValue(): int { return 19; }\r\n";
    const char *mainSourceUpdated = "module main;\nlet peerModule = import(\"peer\");\npub fn importedValue(): int { var updated = 1; return peerModule.peerValue() + updated; }\n";
    const char *sameContractBadPeer = "module peer;\r\npub fn peerValue(): int { return 19 }\r\n";
    SZrLspProjectIndex *projectIndex;
    TZrSize unavailableContractCount;
    const char *otherSource = "module other_error;\npub fn independent(): int { return 1; }\n";
    const char *otherError = "\"unfinished other editor\r\n";
    const char *callLine = strstr(mainSource, "pub fn importedValue");
    const char *callToken = strstr(callLine, "peerValue");
    SZrFileVersion *mainVersion;
    SZrFileVersion *peerVersion;
    SZrFileVersion *otherVersion;
    SZrString *otherUri;
    SZrFileVersionContentBlock *mainBlock;
    SZrFileVersionContentBlock *otherBlock;
    TZrSize otherDiagnosticCount;
    SZrLspSemanticSnapshot *snapshot;
    SZrLspSemanticSnapshot *freshSnapshot;
    SZrLspPosition position = {2, (TZrInt32)(callToken - callLine)};
    SZrArray locations = {0};
    SZrArray diagnostics = {0};
    SZrLspLocation **location;

    g_server->deferDiagnostics = ZR_TRUE;
    write_source(g_mainPath, mainSource);
    write_source(g_errorPath, otherSource);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, notify_open(mainSource, 10));
    otherUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, g_errorPath);
    TEST_ASSERT_NOT_NULL(otherUri);
    use_document_uri(otherUri);
    TEST_ASSERT_EQUAL_INT(1, notify_open(otherSource, 42));
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(otherError, 43));
    otherVersion = get_file_version_for_uri(g_server, otherUri);
    TEST_ASSERT_NOT_NULL(otherVersion);
    otherBlock = otherVersion->textBlock;
    otherDiagnosticCount = otherVersion->parserDiagnostics.length;
    TEST_ASSERT_TRUE(otherDiagnosticCount > 0);
    use_document_uri(g_mainUri);
    mainVersion = get_file_version_for_uri(g_server, g_mainUri);
    peerVersion = get_file_version_for_uri(g_server, g_peerUri);
    TEST_ASSERT_NOT_NULL(mainVersion);
    TEST_ASSERT_NOT_NULL(peerVersion);
    mainBlock = mainVersion->textBlock;
    TEST_ASSERT_NOT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(g_server->state, g_server->context, g_mainUri, position, &locations));
    TEST_ASSERT_TRUE(locations.length > 0);
    free_locations_array(g_server->state, &locations);
    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(snapshot);

    write_source(g_peerPath, badPeer);
    TEST_ASSERT_EQUAL_INT(1, update_document_contents_from_disk(g_server, g_peerUri));
    TEST_ASSERT_EQUAL_PTR(mainVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(mainBlock, mainVersion->textBlock);
    TEST_ASSERT_TRUE(mainVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(10, mainVersion->version);
    TEST_ASSERT_NULL(peerVersion->ast);
    TEST_ASSERT_TRUE(peerVersion->parserDiagnostics.length > 0);
    TEST_ASSERT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_FALSE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, snapshot));
    freshSnapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    if (freshSnapshot != ZR_NULL) {
        /* 允许主线的带真实 import 错误的 fresh partial 分析，但不能借回旧 peer typed authority。 */
        TEST_ASSERT_TRUE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, freshSnapshot));
        ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, freshSnapshot);
    }
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(g_server->state, g_server->context, g_mainUri, &diagnostics));
    TEST_ASSERT_TRUE(diagnostics.length > 0);
    free_diagnostics_array(g_server->state, &diagnostics);
    (void)ZrLanguageServer_Lsp_GetDefinition(g_server->state, g_server->context, g_mainUri, position, &locations);
    TEST_ASSERT_EQUAL_UINT64(0, locations.length);
    free_locations_array(g_server->state, &locations);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(g_server->state, g_server->context, g_peerUri, &diagnostics));
    TEST_ASSERT_TRUE(diagnostics.length > 0);
    free_diagnostics_array(g_server->state, &diagnostics);
    TEST_ASSERT_EQUAL_PTR(otherBlock, otherVersion->textBlock);
    TEST_ASSERT_TRUE(otherVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(43, otherVersion->version);
    TEST_ASSERT_EQUAL_UINT64(otherDiagnosticCount, otherVersion->parserDiagnostics.length);
    TEST_ASSERT_EQUAL_UINT64(strlen(otherError), otherBlock->contentLength);
    TEST_ASSERT_EQUAL_MEMORY(otherError, otherBlock->content, otherBlock->contentLength);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(g_server->state, g_server->context, otherUri, &diagnostics));
    TEST_ASSERT_TRUE(diagnostics.length > 0);
    free_diagnostics_array(g_server->state, &diagnostics);
    assert_main_content(mainSource);

    use_document_uri(g_peerUri);
    TEST_ASSERT_EQUAL_INT(1, notify_open(badPeer, 1));
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(repairedPeer, 2));
    TEST_ASSERT_TRUE(peerVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(2, peerVersion->version);
    TEST_ASSERT_EQUAL_UINT64(0, peerVersion->parserDiagnostics.length);
    TEST_ASSERT_NOT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(g_server->state, g_server->context, g_mainUri, position, &locations));
    TEST_ASSERT_TRUE(locations.length > 0);
    location = (SZrLspLocation **)ZrCore_Array_Get(&locations, 0);
    TEST_ASSERT_NOT_NULL(location);
    TEST_ASSERT_NOT_NULL(*location);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspUri_Equivalent((*location)->uri, g_peerUri));
    free_locations_array(g_server->state, &locations);
    TEST_ASSERT_EQUAL_PTR(otherBlock, otherVersion->textBlock);
    TEST_ASSERT_EQUAL_UINT64(otherDiagnosticCount, otherVersion->parserDiagnostics.length);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(g_server->state, g_server->context, otherUri, &diagnostics));
    TEST_ASSERT_TRUE(diagnostics.length > 0);
    free_diagnostics_array(g_server->state, &diagnostics);
    TEST_ASSERT_EQUAL_PTR(mainBlock, mainVersion->textBlock);
    assert_main_content(mainSource);

    /* 物理 peer 删除后强制 main 新 AST 分析，不能靠先前健康 analyzer 或旧磁盘蒙混。 */
    TEST_ASSERT_EQUAL_INT(0, remove(g_peerPath));
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_peerUri, 3));
    use_document_uri(g_mainUri);
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(mainSourceUpdated, 11));
    TEST_ASSERT_EQUAL_UINT64(11, mainVersion->version);
    TEST_ASSERT_EQUAL_UINT64(2, peerVersion->version);
    TEST_ASSERT_TRUE(peerVersion->isOpenDocument);
    callLine = strstr(mainSourceUpdated, "pub fn importedValue");
    callToken = strstr(callLine, "peerValue");
    position.character = (TZrInt32)(callToken - callLine);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(g_server->state, g_server->context, g_mainUri, position, &locations));
    TEST_ASSERT_TRUE(locations.length > 0);
    location = (SZrLspLocation **)ZrCore_Array_Get(&locations, 0);
    TEST_ASSERT_NOT_NULL(location);
    TEST_ASSERT_NOT_NULL(*location);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspUri_Equivalent((*location)->uri, g_peerUri));
    TEST_ASSERT_EQUAL_INT(1, (*location)->range.start.line);
    free_locations_array(g_server->state, &locations);

    /* ordinary open 的 last-good 政策保留；修复成同一公开签名也必须重析真实反向 importer。 */
    use_document_uri(g_peerUri);
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(sameContractBadPeer, 3));
    TEST_ASSERT_TRUE(peerVersion->parserDiagnostics.length > 0);
    projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(g_server->context, g_peerUri);
    TEST_ASSERT_NOT_NULL(projectIndex);
    unavailableContractCount = projectIndex->publicContractHashUnavailableCount;
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(repairedPeer, 4));
    TEST_ASSERT_EQUAL_UINT64(4, peerVersion->version);
    TEST_ASSERT_EQUAL_UINT64(0, peerVersion->parserDiagnostics.length);
    TEST_ASSERT_TRUE(projectIndex->publicContractHashUnavailableCount > unavailableContractCount);
    TEST_ASSERT_TRUE(projectIndex->lastReverseDependencyReanalysisCount > 0);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(g_server->state, g_server->context, g_mainUri, position, &locations));
    TEST_ASSERT_TRUE(locations.length > 0);
    location = (SZrLspLocation **)ZrCore_Array_Get(&locations, 0);
    TEST_ASSERT_NOT_NULL(location);
    TEST_ASSERT_NOT_NULL(*location);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspUri_Equivalent((*location)->uri, g_peerUri));
    free_locations_array(g_server->state, &locations);
    assert_main_content(mainSourceUpdated);
    TEST_ASSERT_EQUAL_PTR(otherBlock, otherVersion->textBlock);
    TEST_ASSERT_EQUAL_UINT64(otherDiagnosticCount, otherVersion->parserDiagnostics.length);
    ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, snapshot);
}

/* 同一个 FileVersion 经等价 URI 别名进入时，历史只登记 canonical key，close/reopen 要清到它。 */
static void test_equivalent_uri_epoch_clears_canonical_semantic_history(void) {
    const char *changed = "module main;\npub fn aliasOverlay(): int { return 12; }\n";
    const char *uriText = ZrCore_String_GetNativeString(g_mainUri);
    const char *lastSlash = strrchr(uriText, '/');
    char aliasText[ZR_TESTS_PATH_MAX * 4];
    SZrString *aliasUri;
    SZrFileVersion *fileVersion;
    SZrLspHistoricalSemanticSnapshot history;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(lastSlash);
    TEST_ASSERT_TRUE(snprintf(aliasText, sizeof(aliasText), "%.*s/./%s",
                              (int)(lastSlash - uriText), uriText, lastSlash + 1) < (int)sizeof(aliasText));
    aliasUri = server_get_cached_uri(g_server, aliasText);
    TEST_ASSERT_NOT_NULL(aliasUri);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspUri_Equivalent(g_mainUri, aliasUri));
    TEST_ASSERT_FALSE(ZrLanguageServer_Lsp_StringsEqual(g_mainUri, aliasUri));
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, notify_open(g_overlaySource, 7));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    use_document_uri(aliasUri);
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(changed, 8));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, aliasUri));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(g_server->context, fileVersion->uri, 0, &history));
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    TEST_ASSERT_FALSE(ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(g_server->context, fileVersion->uri, 0, &history));
    TEST_ASSERT_EQUAL_INT(1, notify_open(changed, 1));
    TEST_ASSERT_EQUAL_PTR(fileVersion, get_file_version_for_uri(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_UINT64(1, fileVersion->version);
    TEST_ASSERT_FALSE(ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(g_server->context, g_mainUri, 0, &history));
    TEST_ASSERT_FALSE(ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(g_server->context, aliasUri, 0, &history));
    assert_main_content(changed);
}

/* 真正的语法错误仍提交磁盘/编辑器全文与诊断，不能把旧 typed 状态伪装成新文本的健康分析。 */
static void test_malformed_disk_and_editor_keep_current_text_without_old_analyzer(void) {
    const char *badDisk = "\"unfinished disk\r\n";
    const char *badEditor = "module main;\r\n// \xe4\xb8\xad\xf0\x9f\x98\x80\r\npub fn editorValue(): int { return 9; }\r\npub fn beforeError(): int { return editorValue(); }\r\npub fn editorUse(): int {\r\n    var editorLocal = 1 + 2;\r\n    editorLoc\r\n    return editorLocal;\r\n}\r\n";
    const char *callLine = strstr(badEditor, "pub fn beforeError");
    const char *callToken = strstr(callLine, "editorValue");
    SZrLspPosition position = {3, (TZrInt32)(callToken - callLine)};
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *badBlock;
    TZrSize badDiagnosticCount;
    TZrSize diskGeneration;
    TZrUInt64 editorOwnerKey;
    TZrUInt64 editorSourceOrigin;
    SZrSemanticAnalyzer *analyzer;
    SZrLspSemanticSnapshot *partialSnapshot;
    SZrArray diagnostics = {0};
    SZrArray locations = {0};
    SZrLspLocation **location;

    g_server->deferDiagnostics = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    write_source(g_mainPath, badDisk);
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 2));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_TRUE(fileVersion->parserDiagnostics.length > 0);
    TEST_ASSERT_NULL(fileVersion->ast);
    TEST_ASSERT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    assert_main_content(badDisk);
    /* completed syntax error 的 unchanged 快路径只确认字节，不能把诊断清成 healthy。 */
    badBlock = fileVersion->textBlock;
    badDiagnosticCount = fileVersion->parserDiagnostics.length;
    TEST_ASSERT_FALSE(fileVersion->isDirty);
    TEST_ASSERT_EQUAL_INT(1, update_document_contents_from_disk(g_server, g_mainUri));
    TEST_ASSERT_EQUAL_PTR(badBlock, fileVersion->textBlock);
    TEST_ASSERT_EQUAL_UINT64(badDiagnosticCount, fileVersion->parserDiagnostics.length);
    TEST_ASSERT_NULL(fileVersion->ast);
    TEST_ASSERT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    diskGeneration = fileVersion->textBlock->contentGeneration;

    TEST_ASSERT_EQUAL_INT(1, notify_open(badEditor, 1));
    TEST_ASSERT_TRUE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT64(1, fileVersion->version);
    TEST_ASSERT_TRUE(fileVersion->textBlock != badBlock);
    TEST_ASSERT_TRUE(fileVersion->textBlock->contentGeneration > diskGeneration);
    TEST_ASSERT_TRUE(fileVersion->parserDiagnostics.length > 0);
    TEST_ASSERT_NOT_NULL(fileVersion->ast);
    TEST_ASSERT_FALSE(fileVersion->usesFallbackAst);
    editorOwnerKey = ZrParser_Ownership_Key(fileVersion->ast->ownership);
    editorSourceOrigin = ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership);
    TEST_ASSERT_TRUE(editorOwnerKey != 0 && editorSourceOrigin != 0);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(analyzer);
    TEST_ASSERT_EQUAL_PTR(fileVersion->ast, analyzer->ast);
    partialSnapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(g_server->state, g_server->context, g_mainUri);
    TEST_ASSERT_NOT_NULL(partialSnapshot);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspSemanticSnapshot_Validate(g_server->state, g_server->context, partialSnapshot));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(g_server->state, g_server->context, g_mainUri, &diagnostics));
    TEST_ASSERT_TRUE(diagnostics.length > 0);
    free_diagnostics_array(g_server->state, &diagnostics);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDefinition(g_server->state, g_server->context, g_mainUri, position, &locations));
    TEST_ASSERT_TRUE(locations.length > 0);
    location = (SZrLspLocation **)ZrCore_Array_Get(&locations, 0);
    TEST_ASSERT_NOT_NULL(location);
    TEST_ASSERT_NOT_NULL(*location);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspUri_Equivalent((*location)->uri, g_mainUri));
    TEST_ASSERT_EQUAL_INT(2, (*location)->range.start.line);
    free_locations_array(g_server->state, &locations);
    ZrLanguageServer_LspSemanticSnapshot_Release(g_server->state, partialSnapshot);
    assert_main_content(badEditor);
    write_source(g_mainPath, g_diskSource);
    TEST_ASSERT_EQUAL_INT(1, notify_watch(g_mainUri, 2));
    assert_main_content(badEditor);
    TEST_ASSERT_TRUE(fileVersion->parserDiagnostics.length > 0);
    TEST_ASSERT_FALSE(fileVersion->usesFallbackAst);
    TEST_ASSERT_EQUAL_UINT64(editorOwnerKey, ZrParser_Ownership_Key(fileVersion->ast->ownership));
    TEST_ASSERT_EQUAL_UINT64(editorSourceOrigin, ZrParser_Ownership_SourceOrigin(fileVersion->ast->ownership));
    TEST_ASSERT_EQUAL_INT(1, notify_full_change(g_overlaySource, 2));
    TEST_ASSERT_EQUAL_UINT64(0, fileVersion->parserDiagnostics.length);
    TEST_ASSERT_NOT_NULL(ZrLanguageServer_Lsp_FindAnalyzer(g_server->state, g_server->context, g_mainUri));
    assert_main_content(g_overlaySource);
}

/* CMake 的现有 document_close 目标直接链接完整生产 stdio 源集，所有新用例都在此注册执行。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_close_outside_workspace_releases_project_diagnostic_target);
    RUN_TEST(test_close_preserves_other_open_project_document);
    RUN_TEST(test_close_inside_workspace_restores_disk_diagnostic_target);
    RUN_TEST(test_close_missing_disk_file_releases_project_diagnostic_target);
    RUN_TEST(test_save_closed_outside_workspace_does_not_reload_disk);
    RUN_TEST(test_save_closed_dotdot_escape_does_not_reload_disk);
    RUN_TEST(test_save_closed_inside_workspace_reloads_disk);
    RUN_TEST(test_save_open_outside_workspace_keeps_overlay);
    RUN_TEST(test_close_watch_reopen_version_one_utf16);
    RUN_TEST(test_close_watch_reopen_version_one_utf8);
    RUN_TEST(test_reopen_equal_version_same_text_gets_new_generation);
    RUN_TEST(test_open_overlay_survives_disk_change_and_delete);
    RUN_TEST(test_project_disk_delete_preserves_open_source_overlay);
    RUN_TEST(test_open_error_overlay_watch_delete_does_not_clear_diagnostic_push);
    RUN_TEST(test_closed_disk_read_failure_preserves_existing_state);
    RUN_TEST(test_disk_raw_bytes_and_invalid_utf8_preserve_cached_state);
    RUN_TEST(test_bad_dependency_invalidates_importer_and_editor_repair_restores_navigation);
    RUN_TEST(test_equivalent_uri_epoch_clears_canonical_semantic_history);
    RUN_TEST(test_malformed_disk_and_editor_keep_current_text_without_old_analyzer);
    return UNITY_END();
}
