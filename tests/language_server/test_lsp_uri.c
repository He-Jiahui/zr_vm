// Focused file-URI normalization and native-path boundary regressions.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/lsp_uri.h"

static int g_failures = 0;

static TZrPtr test_allocator(TZrPtr userData,
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
    return pointer == ZR_NULL ? malloc(newSize) : realloc(pointer, newSize);
}

static void check(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        g_failures++;
    } else {
        printf("PASS: %s\n", message);
    }
}

static SZrString *test_string(SZrState *state, const TZrChar *text) {
    return text == ZR_NULL ? ZR_NULL : ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static TZrBool string_equals(SZrString *value, const TZrChar *expected) {
    const TZrNativeString text = value == ZR_NULL
                                     ? ZR_NULL
                                     : (value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                            ? ZrCore_String_GetNativeStringShort(value)
                                            : ZrCore_String_GetNativeString(value));

    return text != ZR_NULL && expected != ZR_NULL && strcmp(text, expected) == 0;
}

static void test_file_uri_round_trip(SZrState *state) {
#ifdef ZR_VM_PLATFORM_IS_WIN
    const TZrChar *path = "C:\\Temp\\space # percent%caf\xC3\xA9.zr";
    const TZrChar *expectedUri = "file:///C:/Temp/space%20%23%20percent%25caf%C3%A9.zr";
#else
    const TZrChar *path = "/tmp/space # percent%caf\xC3\xA9.zr";
    const TZrChar *expectedUri = "file:///tmp/space%20%23%20percent%25caf%C3%A9.zr";
#endif
    TZrChar nativePath[512];
    SZrString *uri = ZrLanguageServer_LspUri_FromNativePath(state, path);

    check(string_equals(uri, expectedUri), "native path percent-encodes URI bytes");
    check(ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath)) &&
                  strcmp(nativePath, path) == 0,
          "encoded file URI round-trips to the original native path");
}

static void test_file_uri_rejections(SZrState *state) {
    TZrChar nativePath[64];
    SZrString *virtualUri = test_string(state, "vscode-test-web:/workspace/file.zr");
    SZrString *decompiledUri = test_string(state, "zr-decompiled:/module.zr");
    SZrString *badEscape = test_string(state, "file:///tmp/bad%Q0.zr");
    SZrString *encodedNul = test_string(state, "file:///tmp/bad%00.zr");
    SZrString *encodedControl = test_string(state, "file:///tmp/bad%01.zr");
    SZrString *encodedDelete = test_string(state, "file:///tmp/bad%7F.zr");
    SZrString *encodedSeparator = test_string(state, "file:///tmp/not%2Fa-path.zr");
    SZrString *rawFragment = test_string(state, "file:///tmp/not-a-uri#fragment.zr");
    SZrString *rawQuery = test_string(state, "file:///tmp/not-a-uri?query=z");
    SZrString *rawPath = test_string(state, "/tmp/not-a-uri.zr");

    check(!ZrLanguageServer_LspUri_FileToNativePath(virtualUri, nativePath, sizeof(nativePath)),
          "virtual URI is never sent to native file access");
    check(!ZrLanguageServer_LspUri_FileToNativePath(decompiledUri, nativePath, sizeof(nativePath)),
          "decompiled URI is never sent to native file access");
    check(!ZrLanguageServer_LspUri_FileToNativePath(badEscape, nativePath, sizeof(nativePath)),
          "invalid percent escape is rejected");
    check(!ZrLanguageServer_LspUri_FileToNativePath(encodedNul, nativePath, sizeof(nativePath)) &&
                  nativePath[0] == '\0',
          "percent-encoded NUL is rejected and clears the native path");
    check(!ZrLanguageServer_LspUri_FileToNativePath(encodedControl, nativePath, sizeof(nativePath)) &&
                  nativePath[0] == '\0',
          "percent-encoded control byte is rejected and clears the native path");
    check(!ZrLanguageServer_LspUri_FileToNativePath(encodedDelete, nativePath, sizeof(nativePath)) &&
                  nativePath[0] == '\0',
          "percent-encoded DEL is rejected and clears the native path");
    check(!ZrLanguageServer_LspUri_FileToNativePath(encodedSeparator, nativePath, sizeof(nativePath)),
          "encoded path separators are rejected at the native boundary");
    check(!ZrLanguageServer_LspUri_FileToNativePath(rawFragment, nativePath, sizeof(nativePath)) &&
                  !ZrLanguageServer_LspUri_FileToNativePath(rawQuery, nativePath, sizeof(nativePath)),
          "raw fragment and query syntax are rejected for native files");
    check(!ZrLanguageServer_LspUri_FileToNativePath(rawPath, nativePath, sizeof(nativePath)),
          "bare native path is rejected at the URI boundary");
}

static void test_preencoded_file_uri(SZrState *state) {
    TZrChar nativePath[256];
#ifdef ZR_VM_PLATFORM_IS_WIN
    SZrString *uri = test_string(state, "FILE://localhost/C:/Temp/already%25encoded%20%E2%82%AC.zr");
    const TZrChar *expected = "C:\\Temp\\already%encoded \xE2\x82\xAC.zr";
    const TZrChar *canonicalUri = "file:///c:/Temp/already%25encoded%20%E2%82%AC.zr";
#else
    SZrString *uri = test_string(state, "FILE://localhost/tmp/already%25encoded%20%E2%82%AC.zr");
    const TZrChar *expected = "/tmp/already%encoded \xE2\x82\xAC.zr";
    const TZrChar *canonicalUri = "file:///tmp/already%25encoded%20%E2%82%AC.zr";
#endif

    check(ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath)) &&
                  strcmp(nativePath, expected) == 0,
          "case-insensitive file localhost URI decodes pre-encoded UTF-8 bytes");
    check(ZrLanguageServer_LspUri_Equivalent(uri, test_string(state, canonicalUri)),
          "localhost and canonical file URI normalize to the same native path");
}

static void test_uri_equivalence(SZrState *state) {
#ifdef ZR_VM_PLATFORM_IS_WIN
    SZrString *left = test_string(state, "file:///C:/Temp/dir/../File%20Name.zr");
    SZrString *right = test_string(state, "file:///c:/Temp/File%20Name.zr");
    SZrString *unc = test_string(state, "file://server/share/folder/file.zr");
    TZrChar nativePath[256];
#else
    SZrString *left = test_string(state, "file:///tmp/dir/../File%20Name.zr");
    SZrString *right = test_string(state, "file:///tmp/File%20Name.zr");
#endif
    SZrString *virtualA = test_string(state, "vscode-test-web:/workspace/file.zr");
    SZrString *virtualB = test_string(state, "vscode-test-web:/workspace/file.zr");
    SZrString *virtualOther = test_string(state, "zr-decompiled:/workspace/file.zr");

    check(ZrLanguageServer_LspUri_Equivalent(left, right),
          "equivalent file URIs normalize separators and dot segments");
    check(ZrLanguageServer_LspUri_Equivalent(virtualA, virtualB),
          "identical virtual URIs remain equivalent without native conversion");
    check(!ZrLanguageServer_LspUri_Equivalent(virtualA, virtualOther),
          "different virtual URI schemes do not alias");
#ifdef ZR_VM_PLATFORM_IS_WIN
    check(ZrLanguageServer_LspUri_FileToNativePath(unc, nativePath, sizeof(nativePath)) &&
                  strcmp(nativePath, "\\\\server\\share\\folder\\file.zr") == 0,
          "Windows UNC file URI maps to a native UNC path");
    check(string_equals(ZrLanguageServer_LspUri_FromNativePath(
                            state, "\\\\server\\share\\folder\\file.zr"),
                        "file://server/share/folder/file.zr"),
          "Windows native UNC path encodes as a canonical file URI");
#endif
}

static void test_native_path_overflow(SZrState *state) {
    SZrString *uri = test_string(state, "file:///tmp/too-long.zr");
    TZrChar tiny[4];

    check(!ZrLanguageServer_LspUri_FileToNativePath(uri, tiny, sizeof(tiny)) && tiny[0] == '\0',
          "native path conversion rejects a too-small destination buffer");
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    SZrState *state;

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        fprintf(stderr, "unable to create test runtime\n");
        return 1;
    }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    test_file_uri_round_trip(state);
    test_file_uri_rejections(state);
    test_preencoded_file_uri(state);
    test_uri_equivalence(state);
    test_native_path_overflow(state);

    ZrCore_GlobalState_Free(global);
    printf("LSP URI: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
