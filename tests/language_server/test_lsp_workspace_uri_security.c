#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZR_VM_TESTS_BINARY_DIR
#define ZR_VM_TESTS_BINARY_DIR "tests_generated"
#endif

#include "unity.h"
#include "zr_vm_language_server/lsp_uri.h"
#include "zr_vm_language_server/project/lsp_workspace.h"
#include "zr_vm_language_server_stdio_internal.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static SZrStdioServer *g_server;
static char g_basePath[1024];
static char g_rootPath[1024];
static char g_outsidePath[1024];
static char g_insideLiteralPath[1024];
static char g_insideSecretPath[1024];
static char g_outsideSecretPath[1024];
static char g_insideLiteralSecretPath[1024];

static int create_directory(const char *path) {
#if defined(_WIN32)
    return CreateDirectoryA(path, NULL) != 0;
#else
    return mkdir(path, 0700) == 0;
#endif
}

static void remove_directory(const char *path) {
#if defined(_WIN32)
    (void)RemoveDirectoryA(path);
#else
    (void)rmdir(path);
#endif
}

static void remove_file(const char *path) {
    (void)remove(path);
}

static void write_file(const char *path) {
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_UINT32(7, (unsigned)fwrite("outside", 1, 7, file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static SZrString *make_string(const char *text) {
    return ZrCore_String_Create(g_server->state, text, strlen(text));
}

static TZrBool can_process_uri(const char *uriText) {
    SZrString *uri = make_string(uriText);
    return uri != ZR_NULL && ZrLanguageServer_LspWorkspace_CanProcessFileEvent(g_server->context, uri);
}

void setUp(void) {
#if defined(_WIN32)
    DWORD length = GetTempFileNameA(ZR_VM_TESTS_BINARY_DIR, "lsp", 0, g_basePath);
    TEST_ASSERT_NOT_EQUAL(0, length);
    TEST_ASSERT_NOT_EQUAL(0, DeleteFileA(g_basePath));
    TEST_ASSERT_TRUE(create_directory(g_basePath));
#else
    snprintf(g_basePath, sizeof(g_basePath), "%s/lsp-workspace-uri-security-XXXXXX", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_NOT_NULL(mkdtemp(g_basePath));
#endif
    TEST_ASSERT_TRUE(snprintf(g_rootPath, sizeof(g_rootPath), "%s/workspace", g_basePath) < (int)sizeof(g_rootPath));
    TEST_ASSERT_TRUE(snprintf(g_outsidePath, sizeof(g_outsidePath), "%s/outside", g_basePath) < (int)sizeof(g_outsidePath));
#if defined(_WIN32)
    TEST_ASSERT_TRUE(snprintf(g_insideLiteralPath, sizeof(g_insideLiteralPath), "%s\\%%2e%%2e", g_rootPath) < (int)sizeof(g_insideLiteralPath));
    TEST_ASSERT_TRUE(snprintf(g_insideSecretPath, sizeof(g_insideSecretPath), "%s\\secret.zr", g_rootPath) < (int)sizeof(g_insideSecretPath));
    TEST_ASSERT_TRUE(snprintf(g_outsideSecretPath, sizeof(g_outsideSecretPath), "%s\\secret.zr", g_outsidePath) < (int)sizeof(g_outsideSecretPath));
    TEST_ASSERT_TRUE(snprintf(g_insideLiteralSecretPath, sizeof(g_insideLiteralSecretPath), "%s\\secret.zr", g_insideLiteralPath) < (int)sizeof(g_insideLiteralSecretPath));
#else
    TEST_ASSERT_TRUE(snprintf(g_insideLiteralPath, sizeof(g_insideLiteralPath), "%s/%%2e%%2e", g_rootPath) < (int)sizeof(g_insideLiteralPath));
    TEST_ASSERT_TRUE(snprintf(g_insideSecretPath, sizeof(g_insideSecretPath), "%s/secret.zr", g_rootPath) < (int)sizeof(g_insideSecretPath));
    TEST_ASSERT_TRUE(snprintf(g_outsideSecretPath, sizeof(g_outsideSecretPath), "%s/secret.zr", g_outsidePath) < (int)sizeof(g_outsideSecretPath));
    TEST_ASSERT_TRUE(snprintf(g_insideLiteralSecretPath, sizeof(g_insideLiteralSecretPath), "%s/secret.zr", g_insideLiteralPath) < (int)sizeof(g_insideLiteralSecretPath));
#endif
    TEST_ASSERT_TRUE(create_directory(g_rootPath));
    TEST_ASSERT_TRUE(create_directory(g_outsidePath));
    TEST_ASSERT_TRUE(create_directory(g_insideLiteralPath));
    write_file(g_insideSecretPath);
    write_file(g_outsideSecretPath);
    write_file(g_insideLiteralSecretPath);

    g_server = ZrLanguageServer_StdioServer_New(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_server);
    {
        SZrString *rootUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, g_rootPath);
        TEST_ASSERT_NOT_NULL(rootUri);
        TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_AddFolder(g_server->state, g_server->context, rootUri));
    }
}

void tearDown(void) {
    if (g_server != ZR_NULL) {
        ZrLanguageServer_StdioServer_Free(g_server);
        g_server = ZR_NULL;
    }
    remove_file(g_outsideSecretPath);
    remove_file(g_insideSecretPath);
    remove_file(g_insideLiteralSecretPath);
    remove_directory(g_insideLiteralPath);
    remove_directory(g_rootPath);
    remove_directory(g_outsidePath);
    remove_directory(g_basePath);
}

static void test_workspace_file_event_gate_decodes_uri_once_before_root_check(void) {
    char rootUriText[2048];
    char uriText[4096];
    SZrString *rootUri = ZrLanguageServer_LspUri_FromNativePath(g_server->state, g_rootPath);
    const char *rootUriNative;

    TEST_ASSERT_NOT_NULL(rootUri);
    rootUriNative = ZrCore_String_GetNativeString(rootUri);
    TEST_ASSERT_TRUE(snprintf(rootUriText, sizeof(rootUriText), "%s", rootUriNative) < (int)sizeof(rootUriText));

    TEST_ASSERT_TRUE(snprintf(uriText, sizeof(uriText), "%s/../outside/secret.zr", rootUriText) < (int)sizeof(uriText));
    TEST_ASSERT_FALSE_MESSAGE(can_process_uri(uriText), "literal dot-dot must not pass the workspace disk-event gate");

    TEST_ASSERT_TRUE(snprintf(uriText, sizeof(uriText), "%s/%%2e%%2e/outside/secret.zr", rootUriText) < (int)sizeof(uriText));
    TEST_ASSERT_FALSE_MESSAGE(can_process_uri(uriText), "percent-encoded dot-dot must be decoded before containment");

    TEST_ASSERT_TRUE(snprintf(uriText, sizeof(uriText), "%s%%2f..%%2foutside/secret.zr", rootUriText) < (int)sizeof(uriText));
    TEST_ASSERT_FALSE_MESSAGE(can_process_uri(uriText), "encoded separators must be rejected before disk-event handling");

    TEST_ASSERT_TRUE(snprintf(uriText, sizeof(uriText), "%s/%%252e%%252e/secret.zr", rootUriText) < (int)sizeof(uriText));
    TEST_ASSERT_TRUE_MESSAGE(can_process_uri(uriText), "double encoding names a literal in-root segment after one decode");

    TEST_ASSERT_TRUE(snprintf(uriText, sizeof(uriText), "%s/secret.zr", rootUriText) < (int)sizeof(uriText));
    TEST_ASSERT_TRUE_MESSAGE(can_process_uri(uriText), "a valid in-root URI remains eligible for file events");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_workspace_file_event_gate_decodes_uri_once_before_root_check);
    return UNITY_END();
}
