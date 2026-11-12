#include "zr_vm_language_server_stdio_internal.h"
#include "project/lsp_project_internal.h"
#include "project/lsp_workspace.h"
#include "path_support.h"
#include "unity.h"

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
static char g_projectPath[ZR_TESTS_PATH_MAX];
static const char *g_diskSource = "module main;\npub fn diskValue(): int { return 1; }\n";
static const char *g_overlaySource = "module main;\npub fn overlayValue(): int { return 2; }\n";

/* 项目层关闭路径要从真实磁盘恢复版本，因此夹具写入的文件必须可被磁盘读取入口找到。 */
static void write_source(const char *path, const char *source) {
    FILE *file = fopen(path, "wb");
    size_t length = strlen(source);
    size_t written;
    int closed;

    TEST_ASSERT_NOT_NULL(file);
    written = fwrite(source, 1, length, file);
    closed = fclose(file);
    TEST_ASSERT_EQUAL_UINT64(length, written);
    TEST_ASSERT_EQUAL_INT(0, closed);
}

/* didOpen 的编辑器覆盖层以 UpdateDocument 建立；关闭测试还需确认它确实标为打开。 */
static void open_document(SZrString *uri, const char *source, TZrSize version) {
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_server->state, g_server->context, uri, source, strlen(source), version));
    TEST_ASSERT_TRUE(get_file_version_for_uri(g_server, uri)->isOpenDocument);
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
    cJSON_Delete(g_closeParams);
    g_closeParams = ZR_NULL;
    if (g_uris.isValid) {
        ZrCore_Array_Free(g_server->state, &g_uris);
    }
    ZrLanguageServer_StdioServer_Free(g_server);
    g_server = ZR_NULL;
    remove(g_mainPath);
    remove(g_peerPath);
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

    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    firstVersion = fileVersion->version;
    assert_main_content(g_diskSource);

    write_source(g_mainPath, savedSource);
    TEST_ASSERT_EQUAL_INT(1, handle_did_save(g_server, g_closeParams));
    fileVersion = get_file_version_for_uri(g_server, g_mainUri);
    TEST_ASSERT_NOT_NULL(fileVersion);
    TEST_ASSERT_FALSE(fileVersion->isOpenDocument);
    TEST_ASSERT_EQUAL_UINT(firstVersion + 1, fileVersion->version);
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

/* CMake 为 document_close 建立单独目标，该进程内逐一运行四种 didClose 场景和四种 didSave 场景。 */
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
    return UNITY_END();
}
