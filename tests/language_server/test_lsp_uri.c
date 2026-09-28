// Focused file-URI normalization and native-path boundary regressions.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/lsp_uri.h"

/* 单进程累计所有断言失败，由 main 转成 CTest 可观察的退出码。 */
static int g_failures = 0;

/* 为本文件的 VM 夹具提供原生堆分配；释放仍由全局状态销毁路径驱动。 */
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

/* 汇总所有 URI 边界断言，使一个失败不会遮蔽后续独立场景。 */
static void check(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        g_failures++;
    } else {
        printf("PASS: %s\n", message);
    }
}

/* 测试字面量以 NUL 终止；这里的 strlen 不构造带原始内嵌 NUL 的 VM 字符串。 */
static SZrString *test_string(SZrState *state, const TZrChar *text) {
    return text == ZR_NULL ? ZR_NULL : ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

/* 借用短串或长串的字节视图，仅在当前比较期间使用，不接管 VM 字符串所有权。 */
static TZrBool string_equals(SZrString *value, const TZrChar *expected) {
    const TZrNativeString text = value == ZR_NULL
                                     ? ZR_NULL
                                     : (value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                            ? ZrCore_String_GetNativeStringShort(value)
                                            : ZrCore_String_GetNativeString(value));

    return text != ZR_NULL && expected != ZR_NULL && strcmp(text, expected) == 0;
}

/* 本机绝对路径经字节级百分号编码后仍能回到原路径，覆盖空格、保留字节和 UTF-8。 */
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

/* 磁盘访问边界拒绝虚拟方案、歧义转义和会改变路径分段的 URI 语法。 */
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
    /* 以下清零断言只锁定当前实现的失败路径；公开 API 不承诺失败后可读取 buffer。
     * BUG: 显式长度构造的 file URI 含原始 NUL 时，FileToNativePath 的 strlen 会忽略后缀，
     * 让合法前缀作为文件路径通过；本组仅覆盖 %00，需补原始 NUL 的直调回归。 */
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

/* 已编码 URI 的大小写方案与 localhost authority 被接受，解码一次后与规范 URI 同身份。 */
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

/* 本组 NUL 终止用例检查文件 URI 的词法身份，以及虚拟 URI 的原样比较。 */
static void test_uri_equivalence(SZrState *state) {
#ifdef ZR_VM_PLATFORM_IS_WIN
    /* TODO: 当前只测 ASCII 盘符大小写；需用非 ASCII 文件名核对字节级 tolower 与宿主文件系统的等价关系。 */
    /* BUG: 当前只测目录内的 ..；file:///C:/../x 与 file:///C:/x 在盘符根同址，
     * 现有归一化会误删驱动器冒号并把两者判为不同，需补根级上行回归。 */
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
    /* BUG: Equivalent 用 strcmp 比较 VM 字符串；显式长度构造的虚拟 URI 若 NUL 前缀相同、
     * 后缀不同，仍会被误判为同一文档；本组 NUL 终止字面量尚未覆盖该缺陷。 */

    check(ZrLanguageServer_LspUri_Equivalent(left, right),
          "equivalent file URIs normalize separators and dot segments");
    check(ZrLanguageServer_LspUri_Equivalent(virtualA, virtualB),
          "identical virtual URIs remain equivalent without native conversion");
    check(!ZrLanguageServer_LspUri_Equivalent(virtualA, virtualOther),
          "different virtual URI schemes do not alias");
#ifdef ZR_VM_PLATFORM_IS_WIN
    /* UNC 往返另测主机和共享路径，避免普通盘符路径通过而网络文件身份退化。 */
    check(ZrLanguageServer_LspUri_FileToNativePath(unc, nativePath, sizeof(nativePath)) &&
                  strcmp(nativePath, "\\\\server\\share\\folder\\file.zr") == 0,
          "Windows UNC file URI maps to a native UNC path");
    check(string_equals(ZrLanguageServer_LspUri_FromNativePath(
                            state, "\\\\server\\share\\folder\\file.zr"),
                        "file://server/share/folder/file.zr"),
          "Windows native UNC path encodes as a canonical file URI");
#endif
}

/* 目标缓冲区不足时转换须失败；清零检查只验证当前实现，不扩大公开 API 的失败契约。 */
static void test_native_path_overflow(SZrState *state) {
    SZrString *uri = test_string(state, "file:///tmp/too-long.zr");
    TZrChar tiny[4];

    check(!ZrLanguageServer_LspUri_FileToNativePath(uri, tiny, sizeof(tiny)) && tiny[0] == '\0',
          "native path conversion rejects a too-small destination buffer");
}

/* 根 CMake 将本目标纳入 language_server CTest 套件；一次性 VM 夹具承载全部 URI 场景。 */
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
