#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_language_server.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/location.h"

/* 每个矩阵场景独立计时，供失败日志定位慢查询和回归场景。 */
typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

/* 场景失败累积到进程退出码，供 language_server 测试聚合器读取。 */
static int g_failures = 0;

/* 场景使用同一报告格式；TEST_START 隐式要求调用作用域有名为 timer 的
 * SZrTestTimer 局部变量，FAIL 同时登记退出码所需的失败计数。 */
#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timer, summary) do { \
    timer.endTime = clock(); \
    printf("Pass - Cost Time:%.3fms - %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timer, summary, reason) do { \
    timer.endTime = clock(); \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary, \
           reason); \
    fflush(stdout); \
    g_failures++; \
} while (0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while (0)

/* main 将此回调交给 VM 全局状态；调用来自 VM 内存层而非各断言。 */
static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    /* TODO: 地址/旧大小阈值无法证明分配归属；合法大块重分配会退到 malloc
     * 并丢失旧内容。需核对 VM 分配回调契约再统一测试分配器。 */
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

/* 比较 LSP 返回的 VM 字符串时兼容短串和长串表示，返回值只在状态存活期借用。 */
static const TZrChar *test_string_ptr(SZrString *value) {
    if (value == ZR_NULL) {
        return "<null>";
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

/* 从 CMake 注入的源码根定位项目 fixture；失败时调用方停止场景。 */
static TZrBool build_fixture_native_path(const TZrChar *relativePath,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    int written;

    if (relativePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    written = snprintf(buffer, (size_t)bufferSize, "%s%c%s", ZR_VM_SOURCE_ROOT, ZR_SEPARATOR, relativePath);
    return written > 0 && (TZrSize)written < bufferSize;
}

/* 读取完整原始字节作为 UpdateDocument 的内容；成功缓冲区由场景 free。 */
static TZrChar *read_fixture_text_file(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    TZrChar *buffer = ZR_NULL;
    long fileSize;
    size_t readCount;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    *outLength = 0;
    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrChar *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    readCount = fread(buffer, 1, (size_t)fileSize, file);
    fclose(file);
    if (readCount != (size_t)fileSize) {
        free(buffer);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    *outLength = (TZrSize)fileSize;
    return buffer;
}

/* 项目场景把磁盘路径映射到 LSP 文档 URI，供打开文档和跨文件导航共用。 */
static SZrString *create_file_uri_from_native_path(SZrState *state, const TZrChar *path) {
    TZrChar uriBuffer[2048];
    TZrSize pathLength;
    TZrSize writeIndex = 0;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    /* TODO: 此处仅替换路径分隔符，未转义空格、# 等 URI 特殊字符；
     * 仓库移至此类路径时需改用规范 FromNativePath 并验证导航。 */
    pathLength = strlen(path);
    if (pathLength + 16 >= sizeof(uriBuffer)) {
        return ZR_NULL;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    memcpy(uriBuffer, "file:///", 8);
    writeIndex = 8;
#else
    memcpy(uriBuffer, "file://", 7);
    writeIndex = 7;
#endif

    for (TZrSize index = 0; index < pathLength && writeIndex + 2 < sizeof(uriBuffer); index++) {
        TZrChar current = path[index];
        uriBuffer[writeIndex++] = current == '\\' ? '/' : current;
    }
    uriBuffer[writeIndex] = '\0';
    return ZrCore_String_Create(state, uriBuffer, writeIndex);
}

/* 调用场景指定第几个文本命中及光标偏移，避免在 fixture 中硬编码行号。 */
static TZrBool lsp_find_position_for_substring(const TZrChar *content,
                                               const TZrChar *needle,
                                               TZrSize occurrence,
                                               TZrInt32 extraCharacterOffset,
                                               SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }

    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    /* TODO: 列按 UTF-8 字节累加，而 LSP 查询使用 UTF-16 列；现有 fixture 为
     * ASCII，若加入非 ASCII 文本，需按同版内容做坐标投影。 */
    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
    }

    outPosition->line = line;
    outPosition->character = character + extraCharacterOffset;
    return ZR_TRUE;
}

/* 跨文件定义测试同时约束 URI 与范围，避免同位置的错误目标蒙混通过。 */
static TZrBool location_array_contains_uri_and_range(SZrArray *locations,
                                                     SZrString *uri,
                                                     TZrInt32 startLine,
                                                     TZrInt32 startCharacter,
                                                     TZrInt32 endLine,
                                                     TZrInt32 endCharacter) {
    if (locations == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspLocation *location;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL) {
            continue;
        }

        location = *locationPtr;
        if (location->uri != ZR_NULL &&
            strcmp(test_string_ptr(location->uri), test_string_ptr(uri)) == 0 &&
            location->range.start.line == startLine &&
            location->range.start.character == startCharacter &&
            location->range.end.line == endLine &&
            location->range.end.character == endCharacter) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 同文档字段/枚举导航比较起点；跨文件调用还应检查 URI。 */
static TZrBool location_array_contains_position(SZrArray *locations,
                                                TZrInt32 line,
                                                TZrInt32 character) {
    if (locations == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL &&
            (*locationPtr)->range.start.line == line &&
            (*locationPtr)->range.start.character == character) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool completion_array_contains_label(SZrArray *completions, const TZrChar *label) {
    if (completions == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < completions->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            (*itemPtr)->label != ZR_NULL &&
            strcmp(test_string_ptr((*itemPtr)->label), label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool hover_contains_text(SZrLspHover *hover, const TZrChar *needle) {
    if (hover == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hover->contents.length; index++) {
        SZrString **contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, index);
        if (contentPtr != ZR_NULL && *contentPtr != ZR_NULL &&
            strstr(test_string_ptr(*contentPtr), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool diagnostic_array_contains_message(SZrArray *diagnostics, const TZrChar *needle) {
    if (diagnostics == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL &&
            (*diagnosticPtr)->message != ZR_NULL &&
            strstr(test_string_ptr((*diagnosticPtr)->message), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 报错分支汇总实际诊断，便于区分解析失败与缺少预期消息。 */
static void describe_diagnostic_messages(SZrArray *diagnostics, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (diagnostics == ZR_NULL || diagnostics->length == 0) {
        return;
    }

    for (TZrSize index = 0; index < diagnostics->length && writeIndex + 1 < bufferSize; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *message;
        int written;

        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL || (*diagnosticPtr)->message == ZR_NULL) {
            continue;
        }

        message = test_string_ptr((*diagnosticPtr)->message);
        written = snprintf(buffer + writeIndex,
                           bufferSize - writeIndex,
                           "%s%s",
                           writeIndex == 0 ? "" : " | ",
                           message != ZR_NULL ? message : "<null>");
        if (written <= 0) {
            break;
        }
        if ((TZrSize)written >= bufferSize - writeIndex) {
            writeIndex = bufferSize - 1;
            break;
        }
        writeIndex += (TZrSize)written;
    }
}

static TZrBool symbol_array_contains_name(SZrArray *symbols, const TZrChar *needle) {
    if (symbols == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
            (*symbolPtr)->name != ZR_NULL &&
            strcmp(test_string_ptr((*symbolPtr)->name), needle) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 返回当前符号结果中借用的项，调用方不得在数组释放后使用。 */
static SZrLspSymbolInformation *find_symbol_information_by_name(SZrArray *symbols, const TZrChar *needle) {
    if (symbols == ZR_NULL || needle == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
            (*symbolPtr)->name != ZR_NULL &&
            strcmp(test_string_ptr((*symbolPtr)->name), needle) == 0) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

/* 借用第一候选的 VM 字符串，用于构造器失败报告；原生 help 另须释放。 */
static const TZrChar *signature_help_first_label(SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;

    if (help == ZR_NULL || help->signatures.length == 0) {
        return ZR_NULL;
    }

    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(&help->signatures, 0);
    if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL || (*signaturePtr)->label == ZR_NULL) {
        return ZR_NULL;
    }

    return test_string_ptr((*signaturePtr)->label);
}

/* 当前场景只约束首选签名，其他候选不参与构造器断言。 */
static TZrBool signature_help_contains_text(SZrLspSignatureHelp *help, const TZrChar *needle) {
    const TZrChar *label = signature_help_first_label(help);

    if (label == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    return strstr(label, needle) != ZR_NULL;
}

/* 失败时列出实际补全标签，定位项目索引或导入绑定丢失的路径。 */
static void describe_completion_labels(SZrArray *completions, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (completions == ZR_NULL || completions->length == 0) {
        snprintf(buffer, bufferSize, "<none>");
        return;
    }

    for (TZrSize index = 0; index < completions->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        const TZrChar *label;
        int written;

        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        label = test_string_ptr((*itemPtr)->label);
        written = snprintf(buffer + writeIndex,
                           bufferSize - writeIndex,
                           "%s%s",
                           writeIndex == 0 ? "" : ", ",
                           label);
        if (written <= 0 || (TZrSize)written >= bufferSize - writeIndex) {
            break;
        }
        writeIndex += (TZrSize)written;
    }
}

/* 导航断言失败时保留首个 URI/范围，区分无结果和跳到错误声明。 */
static void describe_first_location(SZrArray *locations, TZrChar *buffer, TZrSize bufferSize) {
    SZrLspLocation **locationPtr;
    SZrLspLocation *location;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (locations == ZR_NULL || locations->length == 0) {
        snprintf(buffer, bufferSize, "<none>");
        return;
    }

    locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, 0);
    if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL) {
        snprintf(buffer, bufferSize, "<null>");
        return;
    }

    location = *locationPtr;
    snprintf(buffer,
             bufferSize,
             "%s:%d:%d-%d:%d",
             location->uri != ZR_NULL ? test_string_ptr(location->uri) : "<null>",
             (int)location->range.start.line,
             (int)location->range.start.character,
             (int)location->range.end.line,
             (int)location->range.end.character);
}

/* 用真实工程中的方法调用与声明文件，守住跨模块 goto definition 的精确 URI/范围。 */
static void test_lsp_matrix_project_definition_resolves_member_method(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *metaUri = ZR_NULL;
    SZrLspPosition methodUsePosition;
    SZrLspPosition methodDefinitionPosition;
    SZrArray definitions;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar metaPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrChar *metaContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;
    TZrSize metaLength = 0;

    TEST_START("LSP Matrix Project Definition Resolves Member Method");
    TEST_INFO("LSP matrix member-method navigation",
              "The matrix project should keep project-scoped member-method goto definition working across module boundaries");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/oop_meta.zr",
                                   metaPath,
                                   sizeof(metaPath))) {
        TEST_FAIL(timer,
                  "LSP Matrix Project Definition Resolves Member Method",
                  "Failed to build lsp_language_feature_matrix fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    metaContent = read_fixture_text_file(metaPath, &metaLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL || metaContent == ZR_NULL || context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Definition Resolves Member Method",
                  "Failed to load the matrix project fixture or create the LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    metaUri = create_file_uri_from_native_path(state, metaPath);
    if (projectUri == ZR_NULL || mainUri == ZR_NULL || metaUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, metaUri, metaContent, metaLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "meter.bump(3)", 0, 6, &methodUsePosition) ||
        !lsp_find_position_for_substring(metaContent, "bump(delta: int)", 0, 0, &methodDefinitionPosition)) {
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Definition Resolves Member Method",
                  "Failed to open the matrix project documents or compute member-method positions");
        return;
    }

    /* BUG: GetDefinition 返回的每个 Location 是原生对象；下方仅 Array_Free
     * 释放指针容器，成功及失败路径均遗留对象。需逐项释放后再清理数组。 */
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, methodUsePosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions,
                                               metaUri,
                                               methodDefinitionPosition.line,
                                               methodDefinitionPosition.character,
                                               methodDefinitionPosition.line,
                                               methodDefinitionPosition.character + 4)) {
        TZrChar locationSummary[512];
        TZrChar reason[768];
        SZrLspHover *hover = ZR_NULL;
        TZrBool hasHover = ZR_FALSE;

        describe_first_location(&definitions, locationSummary, sizeof(locationSummary));
        /* BUG: 定义断言失败时，若回退 hover 成功返回对象，此分支仅释放
         * context；hover 外壳与 contents 未按 GetHover 契约归还。 */
        if (ZrLanguageServer_Lsp_GetHover(state, context, mainUri, methodUsePosition, &hover) &&
            hover != ZR_NULL &&
            hover->contents.length > 0) {
            hasHover = ZR_TRUE;
        }
        snprintf(reason,
                 sizeof(reason),
                 "Method access in main.zr should resolve to the bump declaration in oop_meta.zr"
                 " (count=%llu first=%s hover=%s)",
                 (unsigned long long)definitions.length,
                 locationSummary,
                 hasHover ? "yes" : "no");
        ZrCore_Array_Free(state, &definitions);
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Definition Resolves Member Method",
                  reason);
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    free(projectContent);
    free(mainContent);
    free(metaContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Project Definition Resolves Member Method");
}

/* 原生 builtin import 的 hover 与网络分支补全须同时经公开 LSP API 出现。 */
static void test_lsp_matrix_builtin_import_hover_and_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *asyncUri = ZR_NULL;
    SZrLspPosition importPosition;
    SZrLspPosition networkCompletionPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    TZrChar projectPath[1024];
    TZrChar asyncPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *asyncContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize asyncLength = 0;

    TEST_START("LSP Matrix Builtin Import Hover And Completion");
    TEST_INFO("LSP matrix builtin chain",
              "The matrix fixture should surface builtin import hover metadata and chained network completions through public LSP APIs");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/async_native.zr",
                                   asyncPath,
                                   sizeof(asyncPath))) {
        TEST_FAIL(timer,
                  "LSP Matrix Builtin Import Hover And Completion",
                  "Failed to build lsp_language_feature_matrix builtin fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    asyncContent = read_fixture_text_file(asyncPath, &asyncLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (projectContent == ZR_NULL || asyncContent == ZR_NULL || context == ZR_NULL) {
        free(projectContent);
        free(asyncContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Builtin Import Hover And Completion",
                  "Failed to load the matrix builtin fixture or create the LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    asyncUri = create_file_uri_from_native_path(state, asyncPath);
    if (projectUri == ZR_NULL || asyncUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, asyncUri, asyncContent, asyncLength, 1) ||
        !lsp_find_position_for_substring(asyncContent, "\"zr.system\"", 0, 1, &importPosition) ||
        !lsp_find_position_for_substring(asyncContent, "network.tcp;", 0, 8, &networkCompletionPosition)) {
        free(projectContent);
        free(asyncContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Builtin Import Hover And Completion",
                  "Failed to open the matrix builtin module fixture or compute builtin import positions");
        return;
    }

    /* BUG: GetHover 成功时返回的外壳及 contents 数组须由调用方释放；
     * 该场景在两条退出路径都只释放 context，会遗留原生块。 */
    if (!ZrLanguageServer_Lsp_GetHover(state, context, asyncUri, importPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.system>") ||
        !hover_contains_text(hover, "Source: native builtin")) {
        free(projectContent);
        free(asyncContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Builtin Import Hover And Completion",
                  "Hover on import(\"zr.system\") should surface native builtin module metadata");
        return;
    }

    /* BUG: GetCompletion 追加原生 CompletionItem；两条退出路径只释放数组
     * 缓冲区，未逐项归还查询结果。 */
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, asyncUri, networkCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "tcp") ||
        !completion_array_contains_label(&completions, "udp")) {
        ZrCore_Array_Free(state, &completions);
        free(projectContent);
        free(asyncContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Builtin Import Hover And Completion",
                  "Completion on network. should expose builtin network module branches from the matrix fixture");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    free(projectContent);
    free(asyncContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Builtin Import Hover And Completion");
}

/* 对照限定模块名与解构导入，验证诊断、字段补全和悬停共享正确的类型绑定。 */
static void test_lsp_matrix_imported_type_bindings_surface_qualified_and_destructured_paths(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    const TZrChar *content =
            "var container = import(\"zr.container\");\n"
            "var pair1: container.Pair<int, float> = init container.Pair<int, float>(1, 2.0);\n"
            "var {Pair} = import(\"zr.container\");\n"
            "var pair2: Pair<int, float> = init Pair<int, float>(1, 2.0);\n"
            "container.Pair;\n"
            "pair2.first;\n"
            "pair2;\n";
    SZrString *uri = ZR_NULL;
    SZrLspPosition moduleCompletionPosition;
    SZrLspPosition pairCompletionPosition;
    SZrLspPosition pairHoverPosition;
    SZrArray diagnostics;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    TZrChar labels[512];
    TZrChar diagnosticsSummary[768];
    TZrBool gotDiagnostics = ZR_FALSE;
    TZrChar diagnosticsFailureReason[128];

    TEST_START("LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths");
    TEST_INFO("LSP matrix imported type bindings",
              "Qualified container.Pair and destructured Pair bindings should both stay visible to diagnostics, completion, and hover without reopening a global imported type space");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///matrix_imported_type_bindings.zr",
                               strlen("file:///matrix_imported_type_bindings.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "container.Pair;", 0, 10, &moduleCompletionPosition) ||
        !lsp_find_position_for_substring(content, "pair2.first;", 0, 6, &pairCompletionPosition) ||
        !lsp_find_position_for_substring(content, "pair2;", 0, 0, &pairHoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths",
                  "Failed to prepare the imported-type binding fixture or compute completion/hover positions");
        return;
    }

    /* BUG: 合法导入预期零诊断；若失败时返回非空诊断，本分支只
     * Array_Free 而未 FreeDiagnostics，会遗留已追加的诊断对象。 */
    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    gotDiagnostics = ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics);
    if (!gotDiagnostics || diagnostics.length != 0) {
        describe_diagnostic_messages(&diagnostics, diagnosticsSummary, sizeof(diagnosticsSummary));
        snprintf(diagnosticsFailureReason,
                 sizeof(diagnosticsFailureReason),
                 "GetDiagnostics=%s count=%llu",
                 gotDiagnostics ? "true" : "false",
                 (unsigned long long)diagnostics.length);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths",
                  diagnosticsSummary[0] != '\0'
                          ? diagnosticsSummary
                          : diagnosticsFailureReason);
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    /* BUG: 限定模块和解构实例两次补全都要求非空原生项；各自仅
     * Array_Free 指针缓冲区，成功及失败分支均未逐项释放。 */
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, moduleCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "Pair")) {
        describe_completion_labels(&completions, labels, sizeof(labels));
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths",
                  labels[0] != '\0'
                          ? labels
                          : "container. completion should expose Pair through the qualified native module path");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, pairCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "first") ||
        !completion_array_contains_label(&completions, "second")) {
        describe_completion_labels(&completions, labels, sizeof(labels));
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths",
                  labels[0] != '\0'
                          ? labels
                          : "pair2. completion should expose Pair field members after the destructured type binding");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    /* BUG: pair2 hover 正例取得原生外壳及 contents，随后只释放
     * context；两种退出路径均未按 GetHover 契约清理。 */
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, pairHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "pair2") ||
        !hover_contains_text(hover, "Pair<int, float>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths",
                  "Hover on pair2 should surface the destructured Pair<int, float> type");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Imported Type Bindings Surface Qualified And Destructured Paths");
}

/* 反例阻止裸 Pair 名称意外穿透模块边界，必须经限定名或解构导入绑定。 */
static void test_lsp_matrix_unqualified_imported_type_requires_explicit_binding_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    const TZrChar *content =
            "var container = import(\"zr.container\");\n"
            "var pair: Pair<int, float> = init Pair<int, float>(1, 2.0);\n";
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics;

    TEST_START("LSP Matrix Unqualified Imported Type Requires Explicit Binding Diagnostic");
    TEST_INFO("LSP matrix imported type diagnostics",
              "A bare imported Pair should keep emitting the explicit-binding diagnostic until it is introduced through destructuring");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///matrix_unqualified_imported_type_error.zr",
                               strlen("file:///matrix_unqualified_imported_type_error.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Unqualified Imported Type Requires Explicit Binding Diagnostic",
                  "Failed to prepare the unqualified imported-type diagnostic fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    /* BUG: 此反例需要返回带消息的诊断才会通过，但两条退出路径
     * 仅 Array_Free，未调用 FreeDiagnostics 归还原生项。 */
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) ||
        !diagnostic_array_contains_message(&diagnostics,
                                           "requires an explicit module qualifier or destructuring import")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Unqualified Imported Type Requires Explicit Binding Diagnostic",
                  "Bare Pair<int, float> should surface the explicit-binding diagnostic");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Unqualified Imported Type Requires Explicit Binding Diagnostic");
}

/* 与可用的解构 Pair 正例配对，守住同名类型声明的诊断边界。 */
static void test_lsp_matrix_destructured_imported_type_rejects_duplicate_pair_declaration(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    const TZrChar *content =
            "var {Pair} = import(\"zr.container\");\n"
            "struct Pair {\n"
            "    var left: int;\n"
            "    var right: int;\n"
            "}\n";
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics;

    TEST_START("LSP Matrix Destructured Imported Type Rejects Duplicate Pair Declaration");
    TEST_INFO("LSP matrix imported type collisions",
              "After destructuring Pair into the current context, declaring a second Pair should keep surfacing a duplicate-type diagnostic");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///matrix_duplicate_pair_declaration.zr",
                               strlen("file:///matrix_duplicate_pair_declaration.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Destructured Imported Type Rejects Duplicate Pair Declaration",
                  "Failed to prepare the duplicate Pair declaration fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    /* BUG: 重名反例通过时必有原生诊断对象；两条退出路径仅
     * Array_Free，未调用 FreeDiagnostics 释放结果。 */
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) ||
        !diagnostic_array_contains_message(&diagnostics, "Pair") ||
        !diagnostic_array_contains_message(&diagnostics, "already declared in this context")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Destructured Imported Type Rejects Duplicate Pair Declaration",
                  "Declaring struct Pair after var {Pair} = import(\"zr.container\") should surface a duplicate-type diagnostic");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Destructured Imported Type Rejects Duplicate Pair Declaration");
}

/* 打开工程后以工作区符号及模块成员补全检查跨文件索引已被接入。 */
static void test_lsp_matrix_project_workspace_symbols_and_import_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrLspPosition metaCompletionPosition;
    SZrArray completions;
    SZrArray workspaceSymbols;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;

    TEST_START("LSP Matrix Project Workspace Symbols And Import Completion");
    TEST_INFO("LSP matrix workspace symbols/completion",
              "The dedicated matrix project should surface exported symbols in workspace search and imported module members in completion");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath))) {
        TEST_FAIL(timer,
                  "LSP Matrix Project Workspace Symbols And Import Completion",
                  "Failed to build lsp_language_feature_matrix workspace-symbol fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL || context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Workspace Symbols And Import Completion",
                  "Failed to load the matrix project workspace-symbol fixture");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    if (projectUri == ZR_NULL || mainUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "meta.SignalBox(4)", 0, 5, &metaCompletionPosition)) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Workspace Symbols And Import Completion",
                  "Failed to open the matrix project entry or compute the module completion position");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    /* BUG: 工程 import 补全成功时含原生项；此场景只 Array_Free
     * 指针容器，未逐项释放 CompletionItem。 */
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, metaCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "SignalBox") ||
        !completion_array_contains_label(&completions, "combineVectors")) {
        ZrCore_Array_Free(state, &completions);
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Workspace Symbols And Import Completion",
                  "Completion on meta. should expose exported class and function members from oop_meta.zr");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &workspaceSymbols, sizeof(SZrLspSymbolInformation *), 8);
    /* BUG: WorkspaceSymbols 返回需逐项释放的 SymbolInformation；
     * 此场景仅 Array_Free 指针容器，正常查询会遗留原生对象。 */
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state,
                                                  context,
                                                  ZrCore_String_Create(state, "Score", 5),
                                                  &workspaceSymbols) ||
        !symbol_array_contains_name(&workspaceSymbols, "lambdaScore") ||
        !symbol_array_contains_name(&workspaceSymbols, "nativeScore")) {
        ZrCore_Array_Free(state, &workspaceSymbols);
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Project Workspace Symbols And Import Completion",
                  "Workspace symbols should include representative matrix exports after the project is opened");
        return;
    }

    ZrCore_Array_Free(state, &workspaceSymbols);
    free(projectContent);
    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Project Workspace Symbols And Import Completion");
}

/* 项目内类构造、实例成员、文档大纲与跨文件定义须共同指向 meta 声明。 */
static void test_lsp_matrix_meta_surface_constructor_completion_and_symbols(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *metaUri = ZR_NULL;
    SZrLspPosition classUsePosition;
    SZrLspPosition classDefinitionPosition;
    SZrLspPosition constructorSignaturePosition;
    SZrLspPosition instanceCompletionPosition;
    SZrArray definitions;
    SZrArray completions;
    SZrArray symbols;
    SZrLspSignatureHelp *signatureHelp = ZR_NULL;
    const TZrChar *signatureLabel = ZR_NULL;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar metaPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrChar *metaContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;
    TZrSize metaLength = 0;

    TEST_START("LSP Matrix Meta Surface Constructor Completion And Symbols");
    TEST_INFO("LSP matrix oop/meta coverage",
              "The matrix project should keep class constructor signature help, instance completion, and document symbols alive for getter/setter and meta-call surfaces");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/oop_meta.zr",
                                   metaPath,
                                   sizeof(metaPath))) {
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  "Failed to build lsp_language_feature_matrix meta-surface fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    metaContent = read_fixture_text_file(metaPath, &metaLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL || metaContent == ZR_NULL || context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  "Failed to load the matrix oop/meta fixture or create the LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    metaUri = create_file_uri_from_native_path(state, metaPath);
    if (projectUri == ZR_NULL || mainUri == ZR_NULL || metaUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, metaUri, metaContent, metaLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "meta.SignalBox(4)", 0, 5, &classUsePosition) ||
        !lsp_find_position_for_substring(mainContent, "meta.SignalBox(4)", 0, 15, &constructorSignaturePosition) ||
        !lsp_find_position_for_substring(mainContent, "meter.value = getterValue + 1;", 0, 6, &instanceCompletionPosition) ||
        !lsp_find_position_for_substring(metaContent, "pub class SignalBox", 0, 10, &classDefinitionPosition)) {
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  "Failed to open the matrix oop/meta documents or compute constructor/member positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    /* TODO: 此处只比较定义起点，错误 URI 若恰好同坐标也会通过；
     * 应与上方方法导航一样校验 metaUri 与范围。 */
    /* BUG: 此跨文件定义正例返回原生 Location；失败与成功路径仅
     * Array_Free 指针缓冲区，未逐项归还位置对象。 */
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, classUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, classDefinitionPosition.line, classDefinitionPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  "Goto definition on meta.SignalBox should jump to the SignalBox class declaration in oop_meta.zr");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    /* BUG: GetSignatureHelp 成功时交出需 SignatureHelp_Free 的原生结果；
     * 两条退出路径都未归还，构造器签名查询会泄漏。 */
    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, mainUri, constructorSignaturePosition, &signatureHelp) ||
        signatureHelp == ZR_NULL ||
        !signature_help_contains_text(signatureHelp, "start: int")) {
        /* TODO: 失败日志借用签名中的 VM 字符串；context 先释放后才输出 label。
         * 需核实 GC 根和 context 生命周期，必要时在释放前复制诊断文本。 */
        signatureLabel = signature_help_first_label(signatureHelp);
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  signatureLabel != ZR_NULL
                          ? signatureLabel
                          : "Constructor signature help for meta.SignalBox(4) should surface the imported start parameter");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    /* BUG: 实例成员补全要求 value/bump 原生项；两条退出路径
     * 仅 Array_Free，未逐项释放 CompletionItem。 */
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, instanceCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "value") ||
        !completion_array_contains_label(&completions, "bump")) {
        TZrChar labels[512];

        describe_completion_labels(&completions, labels, sizeof(labels));
        ZrCore_Array_Free(state, &completions);
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  labels);
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 8);
    /* BUG: 文档大纲要求多个原生 SymbolInformation；两条退出路径
     * 仅 Array_Free，未逐项归还这些符号对象。 */
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, metaUri, &symbols) ||
        find_symbol_information_by_name(&symbols, "SignalBox") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "value") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "bump") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "identity") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "Vector2") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "combineVectors") == ZR_NULL) {
        ZrCore_Array_Free(state, &symbols);
        free(projectContent);
        free(mainContent);
        free(metaContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Meta Surface Constructor Completion And Symbols",
                  "Document symbols for oop_meta.zr should include the class, property, member/static methods, struct, and exported helper");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    free(projectContent);
    free(mainContent);
    free(metaContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Meta Surface Constructor Completion And Symbols");
}

/* 同一工程文件内串接字段/枚举导航、成员补全和符号大纲，守住核心语义表面。 */
static void test_lsp_matrix_core_semantics_field_enum_and_symbols(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *coreUri = ZR_NULL;
    SZrLspPosition fieldUsePosition;
    SZrLspPosition fieldDeclPosition;
    SZrLspPosition enumMemberUsePosition;
    SZrLspPosition enumMemberDeclPosition;
    SZrLspPosition pairCompletionPosition;
    SZrArray definitions;
    SZrArray completions;
    SZrArray symbols;
    TZrChar projectPath[1024];
    TZrChar corePath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *coreContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize coreLength = 0;

    TEST_START("LSP Matrix Core Semantics Field Enum And Symbols");
    TEST_INFO("LSP matrix core semantics coverage",
              "The matrix project should keep struct-field navigation, enum-member navigation, member completion, and document symbols stable inside core_semantics.zr");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_language_feature_matrix/src/core_semantics.zr",
                                   corePath,
                                   sizeof(corePath))) {
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  "Failed to build lsp_language_feature_matrix core-semantics fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    coreContent = read_fixture_text_file(corePath, &coreLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (projectContent == ZR_NULL || coreContent == ZR_NULL || context == ZR_NULL) {
        free(projectContent);
        free(coreContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  "Failed to load the matrix core-semantics fixture or create the LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    coreUri = create_file_uri_from_native_path(state, corePath);
    if (projectUri == ZR_NULL || coreUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, coreUri, coreContent, coreLength, 1) ||
        !lsp_find_position_for_substring(coreContent, "pub var left: int;", 0, 4, &fieldDeclPosition) ||
        !lsp_find_position_for_substring(coreContent, "pair.left)", 0, 5, &fieldUsePosition) ||
        !lsp_find_position_for_substring(coreContent, "Hot = 2;", 0, 0, &enumMemberDeclPosition) ||
        !lsp_find_position_for_substring(coreContent, "Mode.Hot", 0, 5, &enumMemberUsePosition) ||
        !lsp_find_position_for_substring(coreContent, "pair.left)", 0, 5, &pairCompletionPosition)) {
        free(projectContent);
        free(coreContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  "Failed to open the matrix core-semantics document or compute field/enum positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    /* BUG: 字段与枚举两次定义查询各要求非空 Location；各自只
     * Array_Free 指针缓冲区，未逐项归还原生位置对象。 */
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, coreUri, fieldUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, fieldDeclPosition.line, fieldDeclPosition.character)) {
        TZrChar locationSummary[512];

        describe_first_location(&definitions, locationSummary, sizeof(locationSummary));
        ZrCore_Array_Free(state, &definitions);
        free(projectContent);
        free(coreContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  locationSummary);
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, coreUri, enumMemberUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, enumMemberDeclPosition.line, enumMemberDeclPosition.character)) {
        TZrChar locationSummary[512];

        describe_first_location(&definitions, locationSummary, sizeof(locationSummary));
        ZrCore_Array_Free(state, &definitions);
        free(projectContent);
        free(coreContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  locationSummary);
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    /* BUG: Pair 成员补全正例取得原生项后只 Array_Free
     * 指针缓冲区，失败及成功路径均缺逐项清理。 */
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, coreUri, pairCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "left") ||
        !completion_array_contains_label(&completions, "right")) {
        TZrChar labels[512];

        describe_completion_labels(&completions, labels, sizeof(labels));
        ZrCore_Array_Free(state, &completions);
        free(projectContent);
        free(coreContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  labels);
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 8);
    /* BUG: core 大纲正例取得多个原生符号项后只 Array_Free
     * 指针缓冲区，失败及成功路径均缺逐项清理。 */
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, coreUri, &symbols) ||
        find_symbol_information_by_name(&symbols, "ScoreReadable") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "Counter") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "Pair") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "Mode") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "makeEscaping") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "lambdaScore") == ZR_NULL) {
        ZrCore_Array_Free(state, &symbols);
        free(projectContent);
        free(coreContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Matrix Core Semantics Field Enum And Symbols",
                  "Document symbols for core_semantics.zr should include the interface, class, struct, enum, closure factory, and exported scoring function");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    free(projectContent);
    free(coreContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Matrix Core Semantics Field Enum And Symbols");
}

/* CMake 的 language_server 套件执行此矩阵；各场景继续运行并汇总失败。 */
int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - LSP Language Feature Matrix Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Fail - Failed to create global state\n");
        return 1;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Fail - Failed to get main thread state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(state, global);

    test_lsp_matrix_project_definition_resolves_member_method(state);
    TEST_DIVIDER();

    test_lsp_matrix_builtin_import_hover_and_completion(state);
    TEST_DIVIDER();

    test_lsp_matrix_imported_type_bindings_surface_qualified_and_destructured_paths(state);
    TEST_DIVIDER();

    test_lsp_matrix_unqualified_imported_type_requires_explicit_binding_diagnostic(state);
    TEST_DIVIDER();

    test_lsp_matrix_destructured_imported_type_rejects_duplicate_pair_declaration(state);
    TEST_DIVIDER();

    test_lsp_matrix_project_workspace_symbols_and_import_completion(state);
    TEST_DIVIDER();

    test_lsp_matrix_meta_surface_constructor_completion_and_symbols(state);
    TEST_DIVIDER();

    test_lsp_matrix_core_semantics_field_enum_and_symbols(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);
    printf("LSP language feature matrix: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
