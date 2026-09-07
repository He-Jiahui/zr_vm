#include "zr_vm_language_server_stdio_internal.h"
#include "project/lsp_project_internal.h"
#include "project/lsp_workspace.h"
#include "path_support.h"
#include "unity.h"

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

static void open_document(SZrString *uri, const char *source, TZrSize version) {
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_server->state, g_server->context, uri, source, strlen(source), version));
    TEST_ASSERT_TRUE(get_file_version_for_uri(g_server, uri)->isOpenDocument);
}

static void collect_uris(void) {
    if (g_uris.isValid) {
        ZrCore_Array_Free(g_server->state, &g_uris);
    }
    ZrCore_Array_Construct(&g_uris);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspProject_CollectDiagnosticDocumentUris(
            g_server->state, g_server->context, &g_uris));
}

static TZrBool has_uri(SZrString *uri) {
    for (TZrSize index = 0; index < g_uris.length; index++) {
        SZrString **item = (SZrString **)ZrCore_Array_Get(&g_uris, index);
        if (item != ZR_NULL && ZrLanguageServer_LspUri_Equivalent(*item, uri)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void assert_main_released(void) {
    TEST_ASSERT_NULL(get_file_version_for_uri(g_server, g_mainUri));
    collect_uris();
    TEST_ASSERT_FALSE_MESSAGE(has_uri(g_mainUri),
                             "A released document must not remain a workspace diagnostic target");
}

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

static void test_close_missing_disk_file_releases_project_diagnostic_target(void) {
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, g_rootUri));
    open_document(g_mainUri, g_overlaySource, 7);
    TEST_ASSERT_EQUAL_INT(0, remove(g_mainPath));
    TEST_ASSERT_EQUAL_INT(1, handle_did_close(g_server, g_closeParams));
    assert_main_released();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_close_outside_workspace_releases_project_diagnostic_target);
    RUN_TEST(test_close_preserves_other_open_project_document);
    RUN_TEST(test_close_inside_workspace_restores_disk_diagnostic_target);
    RUN_TEST(test_close_missing_disk_file_releases_project_diagnostic_target);
    return UNITY_END();
}
