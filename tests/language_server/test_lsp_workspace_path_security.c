#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZR_VM_TESTS_BINARY_DIR
#define ZR_VM_TESTS_BINARY_DIR "tests_generated"
#endif

#include "unity.h"
#include "zr_vm_language_server/project/lsp_workspace.h"

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static void test_workspace_path_security_rejects_dotdot_and_prefix_sibling(void) {
#if defined(_WIN32)
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(
            "C:\\workspace\\..\\outside\\secret.zr", "c:/workspace"));
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(
            "C:/workspace-copy/secret.zr", "c:/workspace"));
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(
            "c:\\WORKSPACE\\src\\main.zr", "C:/workspace/"));
#else
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(
            "/tmp/workspace/../outside/secret.zr", "/tmp/workspace"));
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(
            "/tmp/workspace-copy/secret.zr", "/tmp/workspace"));
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(
            "/tmp/workspace/src/../main.zr", "/tmp/workspace/"));
#endif
}

#if defined(_WIN32)
static void test_workspace_path_security_uses_windows_case_insensitive_paths(void) {
    char rootPath[MAX_PATH];
    char candidatePath[MAX_PATH];
    char rootVariant[MAX_PATH];
    char *separator;
    DWORD length = GetTempFileNameA(ZR_VM_TESTS_BINARY_DIR, "lsp", 0, rootPath);
    TEST_ASSERT_NOT_EQUAL(0, length);
    DeleteFileA(rootPath);
    TEST_ASSERT_NOT_EQUAL(0, CreateDirectoryA(rootPath, NULL));
    snprintf(candidatePath, sizeof(candidatePath), "%s\\child", rootPath);
    TEST_ASSERT_NOT_EQUAL(0, CreateDirectoryA(candidatePath, NULL));
    snprintf(rootVariant, sizeof(rootVariant), "%s", rootPath);
    for (separator = rootVariant; *separator != '\0'; separator++) {
        if (*separator >= 'a' && *separator <= 'z') {
            *separator = (char)(*separator - 'a' + 'A');
        }
    }
    snprintf(candidatePath, sizeof(candidatePath), "%s\\child", rootVariant);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(candidatePath, rootPath));
    snprintf(candidatePath, sizeof(candidatePath), "%s-copy\\child", rootVariant);
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(candidatePath, rootPath));
    snprintf(candidatePath, sizeof(candidatePath), "%s\\child", rootPath);
    RemoveDirectoryA(candidatePath);
    RemoveDirectoryA(rootPath);
}

static void test_workspace_path_security_handles_unicode_windows_case(void) {
    WCHAR wideBase[MAX_PATH];
    WCHAR wideRoot[MAX_PATH];
    WCHAR wideChild[MAX_PATH];
    WCHAR wideCandidate[MAX_PATH];
    char rootPath[MAX_PATH * 4];
    char candidatePath[MAX_PATH * 4];
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ZR_VM_TESTS_BINARY_DIR, -1,
                                     wideBase, MAX_PATH);
    TEST_ASSERT_TRUE(length > 0);
    TEST_ASSERT_TRUE(swprintf(wideRoot, MAX_PATH, L"%ls\\\u00dcRoot", wideBase) > 0);
    TEST_ASSERT_TRUE(swprintf(wideChild, MAX_PATH, L"%ls\\child", wideRoot) > 0);
    TEST_ASSERT_TRUE(CreateDirectoryW(wideRoot, NULL) != 0);
    TEST_ASSERT_TRUE(CreateDirectoryW(wideChild, NULL) != 0);
    TEST_ASSERT_TRUE(swprintf(wideCandidate, MAX_PATH, L"%ls\\\u00fcnicoderoot\\child", wideBase) > 0);
    length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wideRoot, -1, rootPath,
                                 (int)sizeof(rootPath), NULL, NULL);
    TEST_ASSERT_TRUE(length > 0);
    length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wideCandidate, -1, candidatePath,
                                 (int)sizeof(candidatePath), NULL, NULL);
    TEST_ASSERT_TRUE(length > 0);
    TEST_ASSERT_TRUE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(candidatePath, rootPath));
    RemoveDirectoryW(wideChild);
    RemoveDirectoryW(wideRoot);
}

static void test_workspace_path_security_rejects_existing_junction_escape(void) {
    char rootPath[MAX_PATH];
    char outsidePath[MAX_PATH];
    char nestedPath[MAX_PATH];
    char linkPath[MAX_PATH];
    char secretPath[MAX_PATH];
    char candidatePath[MAX_PATH];
    char *root = rootPath;
    char *outside = outsidePath;
    HANDLE file;
    DWORD length = GetTempFileNameA(ZR_VM_TESTS_BINARY_DIR, "lsp", 0, rootPath);
    TEST_ASSERT_NOT_EQUAL(0, length);
    DeleteFileA(rootPath);
    TEST_ASSERT_NOT_EQUAL(0, CreateDirectoryA(rootPath, NULL));
    length = GetTempFileNameA(ZR_VM_TESTS_BINARY_DIR, "out", 0, outsidePath);
    TEST_ASSERT_NOT_EQUAL(0, length);
    DeleteFileA(outsidePath);
    TEST_ASSERT_NOT_EQUAL(0, CreateDirectoryA(outsidePath, NULL));
    snprintf(nestedPath, sizeof(nestedPath), "%s\\nested", outside);
    TEST_ASSERT_NOT_EQUAL(0, CreateDirectoryA(nestedPath, NULL));
    snprintf(secretPath, sizeof(secretPath), "%s\\secret.zr", outside);
    file = CreateFileA(secretPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    TEST_ASSERT_NOT_EQUAL(INVALID_HANDLE_VALUE, file);
    CloseHandle(file);
    snprintf(linkPath, sizeof(linkPath), "%s\\link", root);
#if defined(SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
    if (!CreateSymbolicLinkA(linkPath, nestedPath,
                             SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        DeleteFileA(secretPath);
        RemoveDirectoryA(nestedPath);
        RemoveDirectoryA(outsidePath);
        RemoveDirectoryA(rootPath);
        TEST_IGNORE_MESSAGE("Windows symbolic-link creation is unavailable for this account");
    }
#else
    if (!CreateSymbolicLinkA(linkPath, nestedPath, SYMBOLIC_LINK_FLAG_DIRECTORY)) {
        DeleteFileA(secretPath);
        RemoveDirectoryA(nestedPath);
        RemoveDirectoryA(outsidePath);
        RemoveDirectoryA(rootPath);
        TEST_IGNORE_MESSAGE("Windows symbolic-link creation is unavailable for this account");
    }
#endif
    snprintf(candidatePath, sizeof(candidatePath), "%s\\link\\..\\secret.zr", root);
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(candidatePath, root));
    RemoveDirectoryA(linkPath);
    DeleteFileA(secretPath);
    RemoveDirectoryA(nestedPath);
    RemoveDirectoryA(outsidePath);
    RemoveDirectoryA(rootPath);
}
#else
static void test_workspace_path_security_rejects_existing_symlink_escape(void) {
    char rootTemplate[512];
    char outsideTemplate[512];
    char *root;
    char *outside;
    char linkPath[512];
    char nestedPath[512];
    char secretPath[512];
    char linkedSecretPath[512];
    char linkParentSecretPath[512];
    FILE *secretFile;

    snprintf(rootTemplate, sizeof(rootTemplate), "%s/zr-lsp-path-security-XXXXXX", ZR_VM_TESTS_BINARY_DIR);
    snprintf(outsideTemplate, sizeof(outsideTemplate), "%s/zr-lsp-path-outside-XXXXXX", ZR_VM_TESTS_BINARY_DIR);
    root = mkdtemp(rootTemplate);
    outside = mkdtemp(outsideTemplate);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(outside);
    snprintf(linkPath, sizeof(linkPath), "%s/link", root);
    snprintf(nestedPath, sizeof(nestedPath), "%s/nested", outside);
    snprintf(secretPath, sizeof(secretPath), "%s/secret.zr", outside);
    snprintf(linkedSecretPath, sizeof(linkedSecretPath), "%s/secret.zr", linkPath);
    snprintf(linkParentSecretPath, sizeof(linkParentSecretPath), "%s/../secret.zr", linkPath);
    TEST_ASSERT_EQUAL_INT(0, mkdir(nestedPath, 0700));
    secretFile = fopen(secretPath, "wb");
    TEST_ASSERT_NOT_NULL(secretFile);
    fclose(secretFile);
    TEST_ASSERT_EQUAL_INT(0, symlink(nestedPath, linkPath));
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(linkedSecretPath, root));
    /* The filesystem follows link before applying ..; lexical collapse would accept this. */
    TEST_ASSERT_FALSE(ZrLanguageServer_LspWorkspace_IsPathWithinRoot(linkParentSecretPath, root));
    unlink(linkPath);
    unlink(secretPath);
    rmdir(nestedPath);
    rmdir(root);
    rmdir(outside);
}
#endif

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_workspace_path_security_rejects_dotdot_and_prefix_sibling);
#if defined(_WIN32)
    RUN_TEST(test_workspace_path_security_uses_windows_case_insensitive_paths);
    RUN_TEST(test_workspace_path_security_handles_unicode_windows_case);
    RUN_TEST(test_workspace_path_security_rejects_existing_junction_escape);
#else
    RUN_TEST(test_workspace_path_security_rejects_existing_symlink_escape);
#endif
    return UNITY_END();
}
