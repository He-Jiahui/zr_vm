//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "zr_vm_language_server.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_common_conf.h"

#include <errno.h>

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏
#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#define TEST_PASS(timer, summary) do { \
    timer.endTime = clock(); \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#define TEST_FAIL(timer, summary, reason) do { \
    timer.endTime = clock(); \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

#define TEST_MODULE_DIVIDER() do { \
    printf("==========\n"); \
    fflush(stdout); \
} while(0)

// 简单的测试分配器
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);
    
    if (newSize == 0) {
        // 释放内存
        if (pointer != ZR_NULL) {
            // 检查指针是否在合理范围内（避免释放无效指针）
            // 同时检查 originalSize 是否合理（避免释放时传入错误的 size）
            if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
                free(pointer);
            }
            // 如果指针无效，不调用free，避免崩溃
        }
        return ZR_NULL;
    }
    
    if (pointer == ZR_NULL) {
        // 分配新内存
        return malloc(newSize);
    } else {
        // 重新分配内存
        // 检查指针是否在合理范围内（避免realloc无效指针）
        if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
            return realloc(pointer, newSize);
        } else {
            // 无效指针，分配新内存
            return malloc(newSize);
        }
    }
}

static TZrBool diagnostic_array_contains_range(SZrArray *diagnostics,
                                               TZrInt32 startLine,
                                               TZrInt32 startCharacter,
                                               TZrInt32 endLine,
                                               TZrInt32 endCharacter);
static TZrBool diagnostic_array_contains_message(SZrArray *diagnostics, const TZrChar *needle);
static TZrBool build_fixture_native_path(const TZrChar *relativePath, TZrChar *buffer, TZrSize bufferSize);
static TZrChar *read_fixture_text_file(const TZrChar *path, TZrSize *outLength);
static SZrString *create_file_uri_from_native_path(SZrState *state, const TZrChar *path);

// 测试 LSP 上下文创建和释放
static void test_lsp_context_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Context Creation and Free");
    
    TEST_INFO("LSP Context Creation", "Creating and freeing LSP context");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Context Creation and Free", "Failed to create LSP context");
        return;
    }
    
    if (context->parser == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Context Creation and Free", "Parser is NULL");
        return;
    }
    
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Context Creation and Free");
}

// 测试更新文档
static void test_lsp_update_document(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Update Document");
    
    TEST_INFO("Update Document", "Updating document in LSP context");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Update Document", "Failed to create LSP context");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    TZrBool success = ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1);
    if (!success) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Update Document", "Failed to update document");
        return;
    }
    
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Update Document");
}

// 测试获取诊断
static void test_lsp_get_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Get Diagnostics");
    
    TEST_INFO("Get Diagnostics", "Getting diagnostics from LSP context");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Diagnostics", "Failed to create LSP context");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 更新文档
    ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1);
    
    // 获取诊断
    SZrArray diagnostics;
    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    (void)ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics);
    
    // 诊断可能为空（取决于代码是否有错误），只要不崩溃就算成功
    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Diagnostics");
}

static void test_lsp_get_parser_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content = "var x = ;";
    SZrArray diagnostics;

    TEST_START("LSP Get Parser Diagnostics");
    TEST_INFO("Parser Diagnostics", "Publishing syntax errors through standard LSP diagnostics");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Parser Diagnostics", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///parser_diagnostics.zr", 30);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Parser Diagnostics", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Parser Diagnostics", "Failed to update document");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Parser Diagnostics", "Diagnostics request failed");
        return;
    }

    if (diagnostics.length == 0 ||
        !diagnostic_array_contains_range(&diagnostics, 0, 8, 0, 9) ||
        !diagnostic_array_contains_message(&diagnostics, "Expected primary expression")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Parser Diagnostics", "Expected syntax diagnostics to carry the parser message and the semicolon token span");
        return;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Parser Diagnostics");
}

// 测试获取补全
static void test_lsp_get_completion(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Get Completion");
    
    TEST_INFO("Get Completion", "Getting code completion from LSP context");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Completion", "Failed to create LSP context");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10; var y = 20;";
    TZrSize contentLength = strlen(content);
    
    // 更新文档
    ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1);
    
    // 获取补全
    SZrLspPosition position;
    position.line = 0;
    position.character = 0;
    
    SZrArray completions;
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 4);
    (void)ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions);
    
    // 补全可能为空，只要不崩溃就算成功
    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Completion");
}

// 测试获取定义位置
static void test_lsp_get_definition(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Get Definition");
    
    TEST_INFO("Get Definition", "Getting definition location from LSP context");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Definition", "Failed to create LSP context");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 更新文档
    ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1);
    
    // 获取定义（在变量名位置）
    SZrLspPosition position;
    position.line = 0;
    position.character = 4; // 'x' 的位置
    
    SZrArray definitions;
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    (void)ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, &definitions);
    
    // 定义可能找不到，只要不崩溃就算成功
    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Definition");
}

// 测试查找引用
static void test_lsp_find_references(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Find References");
    
    TEST_INFO("Find References", "Finding all references to a symbol");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Find References", "Failed to create LSP context");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10; var y = x;";
    TZrSize contentLength = strlen(content);
    
    // 更新文档
    ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1);
    
    // 查找引用
    SZrLspPosition position;
    position.line = 0;
    position.character = 4; // 'x' 的位置
    
    SZrArray references;
    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 4);
    (void)ZrLanguageServer_Lsp_FindReferences(state, context, uri, position, ZR_TRUE, &references);
    
    // 引用可能找不到，只要不崩溃就算成功
    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Find References");
}

// 测试重命名
static void test_lsp_rename(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Rename");
    
    TEST_INFO("Rename Symbol", "Renaming a symbol and getting all locations");
    
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Rename", "Failed to create LSP context");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10; var y = x;";
    TZrSize contentLength = strlen(content);
    
    // 更新文档
    ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1);
    
    // 重命名
    SZrLspPosition position;
    position.line = 0;
    position.character = 4; // 'x' 的位置
    
    SZrString *newName = ZrCore_String_Create(state, "newX", 4);
    SZrArray locations;
    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), 4);
    (void)ZrLanguageServer_Lsp_Rename(state, context, uri, position, newName, &locations);
    
    // 重命名可能失败，只要不崩溃就算成功
    ZrCore_Array_Free(state, &locations);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Rename");
}

static const char *test_string_ptr(SZrString *value) {
    if (value == ZR_NULL) {
        return "<null>";
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static void print_file_range(const char *label, SZrFileRange range) {
    printf("  %s: [%d:%d-%d:%d] offsets[%zu-%zu] uri=%s\n",
           label,
           (int)range.start.line,
           (int)range.start.column,
           (int)range.end.line,
           (int)range.end.column,
           (size_t)range.start.offset,
           (size_t)range.end.offset,
           test_string_ptr(range.source));
}

static const TZrChar *g_classes_full_fixture =
    "module \"classes_full\";\n"
    "\n"
    "class BaseHero {\n"
    "    pri var _hp: int = 0;\n"
    "\n"
    "    pub @constructor(seed: int) {\n"
    "        this._hp = seed;\n"
    "    }\n"
    "\n"
    "    pub get hp: int {\n"
    "        return this._hp;\n"
    "    }\n"
    "\n"
    "    pub set hp(v: int) {\n"
    "        this._hp = v;\n"
    "    }\n"
    "\n"
    "    pub heal(amount: int): int {\n"
    "        this.hp = this.hp + amount;\n"
    "        return this.hp;\n"
    "    }\n"
    "}\n"
    "\n"
    "class ScoreBoard {\n"
    "    pri static var _bonus: int = 5;\n"
    "\n"
    "    pub static get bonus: int {\n"
    "        return ScoreBoard._bonus;\n"
    "    }\n"
    "\n"
    "    pub static set bonus(v: int) {\n"
    "        ScoreBoard._bonus = v;\n"
    "    }\n"
    "}\n"
    "\n"
    "class BossHero: BaseHero {\n"
    "    pub static var created: int = 0;\n"
    "\n"
    "    pub @constructor(seed: int) super(seed) {\n"
    "        BossHero.created = BossHero.created + 1;\n"
    "    }\n"
    "\n"
    "    pub total(): int {\n"
    "        return this.hp + ScoreBoard.bonus + BossHero.created;\n"
    "    }\n"
    "}\n"
    "\n"
    "%test(\"classesFullProjectShape\") {\n"
    "    var boss = new BossHero(30);\n"
    "    boss.hp = boss.hp + 7;\n"
    "    ScoreBoard.bonus = boss.heal(5);\n"
    "    return boss.total() + ScoreBoard.bonus;\n"
    "}\n";

static const TZrChar *g_documentation_fixture =
    "module \"documentation\";\n"
    "\n"
    "class ScoreBoard {\n"
    "    pri static var _bonus: int = 5;\n"
    "\n"
    "    // Shared bonus exposed through get/set.\n"
    "    pub static get bonus: int {\n"
    "        return ScoreBoard._bonus;\n"
    "    }\n"
    "\n"
    "    pub static set bonus(v: int) {\n"
    "        ScoreBoard._bonus = v;\n"
    "    }\n"
    "}\n"
    "\n"
    "%test(\"documentation\") {\n"
    "    return ScoreBoard.bonus;\n"
    "}\n";

static const TZrChar *g_callbacks_fixture =
    "var math = import(\"zr.math\");\n"
    "var system = import(\"zr.system\");\n"
    "\n"
    "scaleValue(input: float) {\n"
    "    return input * 2.0;\n"
    "}\n"
    "\n"
    "summarizeCallbackImpl(value: float) {\n"
    "    return value + 7.0;\n"
    "}\n"
    "\n"
    "runCallbacksImpl(lin, signal, tensor) {\n"
    "    var vectorInfo = lin.projectVectors(2.0);\n"
    "    var signalInfo = signal.mixSignal(5.0);\n"
    "    var tensorInfo = tensor.runTensorPass();\n"
    "    var callbackValue = math.invokeCallback(scaleValue, vectorInfo.dot);\n"
    "    var exportedValue = system.vm.callModuleExport(\"callbacks\", \"summarizeCallback\", [callbackValue]);\n"
    "    var modules = system.vm.loadedModules();\n"
    "    var state = system.vm.state();\n"
    "    system.gc.stop();\n"
    "    system.gc.step();\n"
    "    system.gc.start();\n"
    "    system.gc.collect();\n"
    "    system.console.printErrorLine(\"native numeric pipeline stderr ready\");\n"
    "    return {\n"
    "        checksum: exportedValue + signalInfo.magnitude + tensorInfo.sum + tensorInfo.mean;\n"
    "        loadedModuleCount: state.loadedModuleCount;\n"
    "        moduleNames: modules;\n"
    "    };\n"
    "}\n"
    "\n"
    "pub var summarizeCallback = summarizeCallbackImpl;\n"
    "pub var runCallbacks = runCallbacksImpl;\n";

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

static TZrBool lsp_range_equals(SZrLspRange range,
                                TZrInt32 startLine,
                                TZrInt32 startCharacter,
                                TZrInt32 endLine,
                                TZrInt32 endCharacter) {
    return range.start.line == startLine &&
           range.start.character == startCharacter &&
           range.end.line == endLine &&
           range.end.character == endCharacter;
}

static TZrBool location_array_contains_uri_and_range(SZrArray *locations,
                                                     const TZrChar *uri,
                                                     TZrInt32 startLine,
                                                     TZrInt32 startCharacter,
                                                     TZrInt32 endLine,
                                                     TZrInt32 endCharacter) {
    for (TZrSize index = 0; locations != ZR_NULL && index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL &&
            strcmp(test_string_ptr((*locationPtr)->uri), uri) == 0 &&
            lsp_range_equals((*locationPtr)->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool location_array_contains_range(SZrArray *locations,
                                             TZrInt32 startLine,
                                             TZrInt32 startCharacter,
                                             TZrInt32 endLine,
                                             TZrInt32 endCharacter) {
    for (TZrSize index = 0; locations != ZR_NULL && index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL &&
            lsp_range_equals((*locationPtr)->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool location_array_contains_position(SZrArray *locations,
                                                TZrInt32 line,
                                                TZrInt32 character) {
    for (TZrSize index = 0; locations != ZR_NULL && index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            SZrLspRange range = (*locationPtr)->range;
            TZrBool startsBefore = range.start.line < line ||
                                   (range.start.line == line && range.start.character <= character);
            TZrBool endsAfter = range.end.line > line ||
                                (range.end.line == line && range.end.character >= character);
            if (startsBefore && endsAfter) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool highlight_array_contains_position(SZrArray *highlights,
                                                 TZrInt32 line,
                                                 TZrInt32 character) {
    for (TZrSize index = 0; highlights != ZR_NULL && index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            SZrLspRange range = (*highlightPtr)->range;
            TZrBool startsBefore = range.start.line < line ||
                                   (range.start.line == line && range.start.character <= character);
            TZrBool endsAfter = range.end.line > line ||
                                (range.end.line == line && range.end.character >= character);
            if (startsBefore && endsAfter) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool completion_array_contains_label(SZrArray *completions, const TZrChar *label) {
    for (TZrSize index = 0; completions != ZR_NULL && index < completions->length; index++) {
        SZrLspCompletionItem **itemPtr =
            (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            strcmp(test_string_ptr((*itemPtr)->label), label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrLspCompletionItem *find_completion_item_by_label(SZrArray *completions, const TZrChar *label) {
    for (TZrSize index = 0; completions != ZR_NULL && index < completions->length; index++) {
        SZrLspCompletionItem **itemPtr =
            (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            strcmp(test_string_ptr((*itemPtr)->label), label) == 0) {
            return *itemPtr;
        }
    }

    return ZR_NULL;
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

static const TZrChar *signature_help_first_label(SZrLspSignatureHelp *help) {
    if (help == ZR_NULL || help->signatures.length == 0) {
        return ZR_NULL;
    }

    {
        SZrLspSignatureInformation **signaturePtr =
            (SZrLspSignatureInformation **)ZrCore_Array_Get(&help->signatures, 0);
        if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL || (*signaturePtr)->label == ZR_NULL) {
            return ZR_NULL;
        }

        return test_string_ptr((*signaturePtr)->label);
    }
}

static TZrBool signature_help_contains_text(SZrLspSignatureHelp *help, const TZrChar *needle) {
    const TZrChar *label = signature_help_first_label(help);
    return label != ZR_NULL && needle != ZR_NULL && strstr(label, needle) != ZR_NULL;
}

static TZrBool diagnostic_array_contains_range(SZrArray *diagnostics,
                                               TZrInt32 startLine,
                                               TZrInt32 startCharacter,
                                               TZrInt32 endLine,
                                               TZrInt32 endCharacter) {
    for (TZrSize index = 0; diagnostics != ZR_NULL && index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL &&
            lsp_range_equals((*diagnosticPtr)->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool diagnostic_array_contains_message(SZrArray *diagnostics, const TZrChar *needle) {
    for (TZrSize index = 0; diagnostics != ZR_NULL && index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL &&
            strstr(test_string_ptr((*diagnosticPtr)->message), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

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

static SZrString *create_file_uri_from_native_path(SZrState *state, const TZrChar *path) {
    TZrChar uriBuffer[2048];
    TZrSize pathLength;
    TZrSize writeIndex = 0;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

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

static SZrLspSymbolInformation *find_symbol_information_by_name(SZrArray *symbols, const TZrChar *name) {
    for (TZrSize index = 0; symbols != ZR_NULL && index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
            strcmp(test_string_ptr((*symbolPtr)->name), name) == 0) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static SZrSemanticAnalyzer *find_test_analyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair == ZR_NULL || pair->value.type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_NULL;
    }

    return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
}

static void dump_analyzer_state(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrSemanticAnalyzer *analyzer = find_test_analyzer(state, context, uri);

    printf("  Analyzer dump for %s\n", test_string_ptr(uri));
    if (analyzer == ZR_NULL) {
        printf("  analyzer=<null>\n");
        return;
    }

    if (analyzer->symbolTable == ZR_NULL || analyzer->symbolTable->globalScope == ZR_NULL) {
        printf("  symbolTable/globalScope=<null>\n");
    } else {
        printf("  global symbols=%zu\n", (size_t)analyzer->symbolTable->globalScope->symbols.length);
        for (TZrSize i = 0; i < analyzer->symbolTable->globalScope->symbols.length; i++) {
            SZrSymbol **symbolPtr =
                (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                printf("  symbol[%zu]=%s type=%d refs=%zu\n",
                       (size_t)i,
                       test_string_ptr(symbol->name),
                       (int)symbol->type,
                       (size_t)symbol->referenceCount);
                print_file_range("symbol.location", symbol->location);
                print_file_range("symbol.selection", symbol->selectionRange);
                for (TZrSize refIndex = 0; refIndex < symbol->references.length; refIndex++) {
                    SZrFileRange *referenceRange =
                        (SZrFileRange *)ZrCore_Array_Get(&symbol->references, refIndex);
                    if (referenceRange != ZR_NULL) {
                        print_file_range("symbol.reference", *referenceRange);
                    }
                }
            }
        }
    }

    if (analyzer->referenceTracker == ZR_NULL) {
        printf("  referenceTracker=<null>\n");
        return;
    }

    printf("  tracker references=%zu diagnostics=%zu\n",
           (size_t)analyzer->referenceTracker->allReferences.length,
           (size_t)analyzer->diagnostics.length);
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr =
            (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            printf("  diagnostic[%zu]=%s code=%s severity=%d\n",
                   (size_t)i,
                   test_string_ptr((*diagPtr)->message),
                   test_string_ptr((*diagPtr)->code),
                   (int)(*diagPtr)->severity);
            print_file_range("diagnostic.location", (*diagPtr)->location);
        }
    }
    for (TZrSize i = 0; i < analyzer->referenceTracker->allReferences.length; i++) {
        SZrReference **refPtr =
            (SZrReference **)ZrCore_Array_Get(&analyzer->referenceTracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            printf("  tracker.ref[%zu]=%s type=%d\n",
                   (size_t)i,
                   test_string_ptr(ref->symbol != ZR_NULL ? ref->symbol->name : ZR_NULL),
                   (int)ref->type);
            print_file_range("tracker.ref.location", ref->location);
        }
    }
}

// 测试获取文档符号
static void test_lsp_get_document_symbols(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Get Document Symbols");

    TEST_INFO("Get Document Symbols", "Collecting symbols from a single document");

    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Document Symbols", "Failed to create LSP context");
        return;
    }

    SZrString *uri = ZrCore_String_Create(state, "file:///symbols.zr", 18);
    const TZrChar *content = "var total = 10;\nvalueOf() { return total; }\n";
    TZrSize contentLength = strlen(content);

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, contentLength, 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Document Symbols", "Failed to update document");
        return;
    }

    SZrArray symbols;
    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 4);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols)) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Document Symbols", "Failed to get document symbols");
        return;
    }

    if (symbols.length == 0) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Document Symbols", "Expected at least one document symbol");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Document Symbols");
}

// 测试获取工作区符号
static void test_lsp_get_workspace_symbols(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Get Workspace Symbols");

    TEST_INFO("Get Workspace Symbols", "Searching symbols across multiple open documents");

    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Workspace Symbols", "Failed to create LSP context");
        return;
    }

    SZrString *alphaUri = ZrCore_String_Create(state, "file:///alpha.zr", 16);
    SZrString *betaUri = ZrCore_String_Create(state, "file:///beta.zr", 15);
    SZrString *query = ZrCore_String_Create(state, "be", 2);
    const TZrChar *alphaContent = "var alpha = 1;";
    const TZrChar *betaContent = "var beta = 2;";

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, alphaUri, alphaContent, strlen(alphaContent), 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, betaUri, betaContent, strlen(betaContent), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Workspace Symbols", "Failed to update workspace documents");
        return;
    }

    SZrArray symbols;
    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 4);
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state, context, query, &symbols)) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Workspace Symbols", "Failed to search workspace symbols");
        return;
    }

    if (symbols.length == 0) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Workspace Symbols", "Expected at least one workspace symbol");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Workspace Symbols");
}

static void test_lsp_project_definition_resolves_imported_function(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *greetUri = ZR_NULL;
    SZrLspPosition definitionPosition;
    SZrLspPosition expectedDefinition;
    SZrArray definitions;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar greetPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrChar *greetContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;
    TZrSize greetLength = 0;

    TEST_START("LSP Project Definition Resolves Imported Function");
    TEST_INFO("Project Definition", "Opening a .zrp should let goto definition jump to imported source files");

    if (!build_fixture_native_path("tests/fixtures/projects/import_pub_function/import_pub_function.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_pub_function/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_pub_function/src/greet.zr",
                                   greetPath,
                                   sizeof(greetPath))) {
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Failed to build fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    greetContent = read_fixture_text_file(greetPath, &greetLength);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL || greetContent == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Failed to read project fixture content");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Failed to create LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    greetUri = create_file_uri_from_native_path(state, greetPath);
    if (projectUri == ZR_NULL || mainUri == ZR_NULL || greetUri == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Failed to create fixture URIs");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1)) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Failed to open project fixture documents");
        return;
    }

    if (!lsp_find_position_for_substring(mainContent, "greet();", 0, 0, &definitionPosition) ||
        !lsp_find_position_for_substring(greetContent, "greet()", 0, 0, &expectedDefinition)) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Failed to compute fixture positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, definitionPosition, &definitions) ||
        definitions.length == 0) {
        ZrCore_Array_Free(state, &definitions);
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Expected imported function usage to resolve to greet.zr");
        return;
    }

    {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0);
        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL ||
            strcmp(test_string_ptr((*locationPtr)->uri), test_string_ptr(greetUri)) != 0 ||
            !lsp_range_equals((*locationPtr)->range,
                              expectedDefinition.line,
                              expectedDefinition.character,
                              expectedDefinition.line,
                              expectedDefinition.character + 5)) {
            ZrCore_Array_Free(state, &definitions);
            free(projectContent);
            free(mainContent);
            free(greetContent);
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer, "LSP Project Definition Resolves Imported Function", "Imported definition should point at greet.zr:greet");
            return;
        }
    }

    ZrCore_Array_Free(state, &definitions);
    free(projectContent);
    free(mainContent);
    free(greetContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Project Definition Resolves Imported Function");
}

static void test_lsp_project_references_include_imported_function_usage(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *greetUri = ZR_NULL;
    SZrLspPosition definitionPosition;
    SZrLspPosition usagePosition;
    SZrArray references;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar greetPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrChar *greetContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;
    TZrSize greetLength = 0;

    TEST_START("LSP Project References Include Imported Function Usage");
    TEST_INFO("Project References", "Opening a .zrp should let references cross imported project source files");

    if (!build_fixture_native_path("tests/fixtures/projects/import_pub_function/import_pub_function.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_pub_function/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_pub_function/src/greet.zr",
                                   greetPath,
                                   sizeof(greetPath))) {
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Failed to build fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    greetContent = read_fixture_text_file(greetPath, &greetLength);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL || greetContent == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Failed to read project fixture content");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Failed to create LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    greetUri = create_file_uri_from_native_path(state, greetPath);
    if (projectUri == ZR_NULL || mainUri == ZR_NULL || greetUri == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Failed to create fixture URIs");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1)) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Failed to open project fixture documents");
        return;
    }

    if (!lsp_find_position_for_substring(greetContent, "greet()", 0, 0, &definitionPosition) ||
        !lsp_find_position_for_substring(mainContent, "greet();", 0, 0, &usagePosition)) {
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Failed to compute fixture positions");
        return;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, greetUri, definitionPosition, ZR_TRUE, &references) ||
        references.length < 2 ||
        !location_array_contains_uri_and_range(&references,
                                               test_string_ptr(greetUri),
                                               definitionPosition.line,
                                               definitionPosition.character,
                                               definitionPosition.line,
                                               definitionPosition.character + 5) ||
        !location_array_contains_uri_and_range(&references,
                                               test_string_ptr(mainUri),
                                               usagePosition.line,
                                               usagePosition.character,
                                               usagePosition.line,
                                               usagePosition.character + 5)) {
        ZrCore_Array_Free(state, &references);
        free(projectContent);
        free(mainContent);
        free(greetContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project References Include Imported Function Usage", "Expected project references to include both greet definition and imported usage");
        return;
    }

    ZrCore_Array_Free(state, &references);
    free(projectContent);
    free(mainContent);
    free(greetContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Project References Include Imported Function Usage");
}

static void test_lsp_project_workspace_symbols_include_imported_exports(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *query = ZR_NULL;
    SZrArray symbols;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;
    SZrLspSymbolInformation *symbolInfo;

    TEST_START("LSP Project Workspace Symbols Include Imported Exports");
    TEST_INFO("Project Workspace Symbols", "Opening a .zrp should index imported project exports for workspace/symbol");

    if (!build_fixture_native_path("tests/fixtures/projects/import_pub_function/import_pub_function.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_pub_function/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath))) {
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "Failed to build fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "Failed to read project fixture content");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "Failed to create LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    query = ZrCore_String_Create(state, "greet", 5);
    if (projectUri == ZR_NULL || mainUri == ZR_NULL || query == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "Failed to create fixture URIs");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1)) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "Failed to open project fixture documents");
        return;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 4);
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state, context, query, &symbols)) {
        ZrCore_Array_Free(state, &symbols);
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "workspace/symbol request failed");
        return;
    }

    symbolInfo = find_symbol_information_by_name(&symbols, "greet");
    if (symbolInfo == ZR_NULL) {
        ZrCore_Array_Free(state, &symbols);
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Workspace Symbols Include Imported Exports", "Expected workspace symbols to include greet from greet.zr");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    free(projectContent);
    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Project Workspace Symbols Include Imported Exports");
}

// 测试获取文档高亮
static void test_lsp_get_document_highlights(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Get Document Highlights");

    TEST_INFO("Get Document Highlights", "Collecting highlight ranges for a symbol");

    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Get Document Highlights", "Failed to create LSP context");
        return;
    }

    SZrString *uri = ZrCore_String_Create(state, "file:///highlights.zr", 21);
    const TZrChar *content = "var x = 10; var y = x;";
    SZrLspPosition position;
    SZrArray highlights;
    position.line = 0;
    position.character = 4;

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Document Highlights", "Failed to update document");
        return;
    }

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, position, &highlights)) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Document Highlights", "Failed to get document highlights");
        return;
    }

    if (highlights.length == 0) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Get Document Highlights", "Expected at least one highlight");
        return;
    }

    ZrCore_Array_Free(state, &highlights);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Get Document Highlights");
}

// 测试预检查重命名
static void test_lsp_prepare_rename(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("LSP Prepare Rename");

    TEST_INFO("Prepare Rename", "Validating rename range and placeholder before rename");

    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Prepare Rename", "Failed to create LSP context");
        return;
    }

    SZrString *uri = ZrCore_String_Create(state, "file:///rename_check.zr", 23);
    const TZrChar *content = "var x = 10; var y = x;";
    SZrLspPosition position;
    SZrLspRange range;
    SZrString *placeholder = ZR_NULL;
    position.line = 0;
    position.character = 4;

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Prepare Rename", "Failed to update document");
        return;
    }

    if (!ZrLanguageServer_Lsp_PrepareRename(state, context, uri, position, &range, &placeholder)) {
        dump_analyzer_state(state, context, uri);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Prepare Rename", "Failed to prepare rename");
        return;
    }

    if (placeholder == ZR_NULL || range.start.character != 4) {
        dump_analyzer_state(state, context, uri);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Prepare Rename", "Rename metadata mismatch");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Prepare Rename");
}

static void test_lsp_class_member_navigation_and_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;
    SZrArray definitions;
    SZrArray references;
    SZrArray completions;
    SZrArray symbols;
    SZrLspPosition bossUsage;
    SZrLspPosition bossMemberCompletion;
    SZrLspPosition scoreBoardCompletion;
    SZrLspPosition bossHeroDefinition;
    SZrLspPosition bossHeroNewReference;

    TEST_START("LSP Class Member Navigation And Completion");
    TEST_INFO("Class Members", "Checking definition, references, document symbols, and get/set completions");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///classes_full.zr", 24);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state,
                                             context,
                                             uri,
                                             g_classes_full_fixture,
                                             strlen(g_classes_full_fixture),
                                             1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Failed to update classes fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) || diagnostics.length != 0) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Expected classes fixture diagnostics to be empty");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    if (!lsp_find_position_for_substring(g_classes_full_fixture, "BossHero(30)", 0, 0, &bossUsage) ||
        !lsp_find_position_for_substring(g_classes_full_fixture, "BossHero: BaseHero", 0, 0, &bossHeroDefinition) ||
        !lsp_find_position_for_substring(g_classes_full_fixture, "BossHero(30)", 0, 0, &bossHeroNewReference) ||
        !lsp_find_position_for_substring(g_classes_full_fixture, "boss.hp =", 0, 5, &bossMemberCompletion) ||
        !lsp_find_position_for_substring(g_classes_full_fixture, "ScoreBoard.bonus =", 0, 11, &scoreBoardCompletion)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Failed to compute fixture positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, bossUsage, &definitions) ||
        definitions.length == 0 ||
        !location_array_contains_range(&definitions,
                                       bossHeroDefinition.line,
                                       bossHeroDefinition.character,
                                       bossHeroDefinition.line,
                                       bossHeroDefinition.character + 8)) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "BossHero definition should resolve to the class identifier span");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, bossUsage, ZR_TRUE, &references) ||
        references.length < 2 ||
        !location_array_contains_range(&references,
                                       bossHeroDefinition.line,
                                       bossHeroDefinition.character,
                                       bossHeroDefinition.line,
                                       bossHeroDefinition.character + 8) ||
        !location_array_contains_range(&references,
                                       bossHeroNewReference.line,
                                       bossHeroNewReference.character,
                                       bossHeroNewReference.line,
                                       bossHeroNewReference.character + 8)) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "BossHero references should include both declaration and constructor usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, bossMemberCompletion, &completions) ||
        !completion_array_contains_label(&completions, "hp") ||
        !completion_array_contains_label(&completions, "heal") ||
        !completion_array_contains_label(&completions, "total")) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "boss. completion should include property and methods");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, scoreBoardCompletion, &completions) ||
        !completion_array_contains_label(&completions, "bonus")) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "ScoreBoard. completion should include static property bonus");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols)) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Document symbols request failed");
        return;
    }
    if (find_symbol_information_by_name(&symbols, "hp") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "bonus") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "heal") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "total") == ZR_NULL) {
        dump_analyzer_state(state, context, uri);
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Class Member Navigation And Completion", "Document symbols should include methods and get/set properties");
        return;
    }
    ZrCore_Array_Free(state, &symbols);

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Class Member Navigation And Completion");
}

static void test_lsp_hover_and_completion_include_comments(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition bonusUsage;
    SZrLspHover *hover = ZR_NULL;
    SZrArray completions;
    SZrLspCompletionItem *bonusItem;

    TEST_START("LSP Hover And Completion Include Comments");
    TEST_INFO("Documentation Comments", "Checking hover and completion documentation pick up leading comments");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///documentation.zr", 24);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state,
                                             context,
                                             uri,
                                             g_documentation_fixture,
                                             strlen(g_documentation_fixture),
                                             1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Failed to update documentation fixture");
        return;
    }

    if (!lsp_find_position_for_substring(g_documentation_fixture, "ScoreBoard.bonus;", 0, 11, &bonusUsage)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Failed to compute documentation fixture positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, bonusUsage, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Shared bonus exposed through get/set.")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Hover should include the leading property comment");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, bonusUsage, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Completion request failed");
        return;
    }

    bonusItem = find_completion_item_by_label(&completions, "bonus");
    if (bonusItem == ZR_NULL ||
        bonusItem->documentation == ZR_NULL ||
        strstr(test_string_ptr(bonusItem->documentation), "Shared bonus exposed through get/set.") == ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover And Completion Include Comments", "Completion documentation should include the leading property comment");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Hover And Completion Include Comments");
}

static void test_lsp_function_symbol_range_trimmed_to_body(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray symbols;
    SZrLspPosition functionStart;
    SZrLspPosition functionEnd;
    SZrLspSymbolInformation *symbol;

    TEST_START("LSP Function Symbol Range Trimmed To Body");
    TEST_INFO("Function Symbol Range", "Checking function range end does not drift into the next declaration");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Function Symbol Range Trimmed To Body", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///callbacks.zr", 20);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Symbol Range Trimmed To Body", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state,
                                             context,
                                             uri,
                                             g_callbacks_fixture,
                                             strlen(g_callbacks_fixture),
                                             1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Symbol Range Trimmed To Body", "Failed to update callbacks fixture");
        return;
    }

    if (!lsp_find_position_for_substring(g_callbacks_fixture, "runCallbacksImpl(lin, signal, tensor)", 0, 0, &functionStart) ||
        !lsp_find_position_for_substring(g_callbacks_fixture, "}\n\npub var summarizeCallback", 0, 1, &functionEnd)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Symbol Range Trimmed To Body", "Failed to compute callbacks fixture positions");
        return;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols)) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Symbol Range Trimmed To Body", "Failed to get document symbols");
        return;
    }

    symbol = find_symbol_information_by_name(&symbols, "runCallbacksImpl");
    if (symbol == ZR_NULL ||
        symbol->location.range.start.line != functionStart.line ||
        symbol->location.range.start.character != functionStart.character ||
        symbol->location.range.end.line != functionEnd.line ||
        symbol->location.range.end.character != functionEnd.character) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Symbol Range Trimmed To Body", "Function range should stop at the closing brace instead of drifting to the next declaration");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Function Symbol Range Trimmed To Body");
}

static void test_lsp_function_prepare_rename_uses_name_range(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition functionName;
    SZrLspRange range;
    SZrString *placeholder = ZR_NULL;

    TEST_START("LSP Function Prepare Rename Uses Name Range");
    TEST_INFO("Function Rename Range", "Checking function declarations map to the identifier span instead of the whole declaration");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Function Prepare Rename Uses Name Range", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///callbacks.zr", 20);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Prepare Rename Uses Name Range", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state,
                                             context,
                                             uri,
                                             g_callbacks_fixture,
                                             strlen(g_callbacks_fixture),
                                             1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Prepare Rename Uses Name Range", "Failed to update callbacks fixture");
        return;
    }

    if (!lsp_find_position_for_substring(g_callbacks_fixture,
                                         "runCallbacksImpl(lin, signal, tensor)",
                                         0,
                                         0,
                                         &functionName)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Prepare Rename Uses Name Range", "Failed to compute function name position");
        return;
    }

    if (!ZrLanguageServer_Lsp_PrepareRename(state, context, uri, functionName, &range, &placeholder)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Prepare Rename Uses Name Range", "Prepare rename request failed");
        return;
    }

    if (placeholder == ZR_NULL ||
        strcmp(test_string_ptr(placeholder), "runCallbacksImpl") != 0 ||
        !lsp_range_equals(range,
                          functionName.line,
                          functionName.character,
                          functionName.line,
                          functionName.character + 16)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Function Prepare Rename Uses Name Range", "Function rename range should match the identifier span exactly");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Function Prepare Rename Uses Name Range");
}

static void test_lsp_closed_generic_type_display_and_definition(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class Item {\n"
        "    pub @constructor() { }\n"
        "}\n"
        "class Derived<T, const N: int> {\n"
        "}\n"
        "func use(): void {\n"
        "    var value: Derived<Item, 2 + 2> = null;\n"
        "    value;\n"
        "}\n";
    SZrLspPosition typeUsePosition;
    SZrLspPosition derivedDefinition;
    SZrLspHover *hover = ZR_NULL;
    SZrArray definitions;
    SZrArray completions;
    SZrLspCompletionItem *derivedItem;

    TEST_START("LSP Closed Generic Type Display And Definition");
    TEST_INFO("Closed Generic Types", "Checking hover, completion detail, and goto definition stay consistent for closed generic type uses");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///closed_generic_type_display.zr", 39);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "Derived<Item, 2 + 2>", 0, 0, &typeUsePosition) ||
        !lsp_find_position_for_substring(content, "class Derived<T, const N: int>", 0, 6, &derivedDefinition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Failed to compute generic type positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, typeUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Resolved Type: Derived<Item, 4>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Hover should include the normalized closed generic type");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, typeUsePosition, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Completion request failed");
        return;
    }

    derivedItem = find_completion_item_by_label(&completions, "Derived");
    if (derivedItem == ZR_NULL ||
        derivedItem->detail == ZR_NULL ||
        strstr(test_string_ptr(derivedItem->detail), "Resolved Type: Derived<Item, 4>") == ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Completion detail should include the normalized closed generic type");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, typeUsePosition, &definitions) ||
        !location_array_contains_range(&definitions,
                                       derivedDefinition.line,
                                       derivedDefinition.character,
                                       derivedDefinition.line,
                                       derivedDefinition.character + 7)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Closed Generic Type Display And Definition", "Goto definition should resolve from the closed generic type use to the open generic declaration");
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Closed Generic Type Display And Definition");
}

static void test_lsp_signature_help_displays_closed_generic_instantiation(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class Matrix<T, const N: int> { }\n"
        "class Box<T> {\n"
        "    func shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
        "}\n"
        "func use(): void {\n"
        "    var box = new Box<int>();\n"
        "    var m = new Matrix<int, 2 + 2>();\n"
        "    box.shape(m);\n"
        "}\n";
    SZrLspPosition callArgumentPosition;
    SZrLspPosition boxHoverPosition;
    SZrLspPosition matrixHoverPosition;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Signature Help Displays Closed Generic Instantiation");
    TEST_INFO("Signature Help", "Checking signature help closes receiver and const generic types at the call site");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///closed_generic_signature_help.zr", 40);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "box.shape(m);", 0, 10, &callArgumentPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", "Failed to compute call argument position");
        return;
    }

    if (!lsp_find_position_for_substring(content, "var box = new Box<int>();", 0, 4, &boxHoverPosition) ||
        !lsp_find_position_for_substring(content, "var m = new Matrix<int, 2 + 2>();", 0, 4, &matrixHoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", "Failed to compute local variable hover positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, boxHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Resolved Type: Box<int>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help Displays Closed Generic Instantiation",
                  "Hover for the constructed box local should show the closed generic source type");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, matrixHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Resolved Type: Matrix<int, 4>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help Displays Closed Generic Instantiation",
                  "Hover for the constructed matrix local should show the normalized closed const-generic source type");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, callArgumentPosition, &help) ||
        help == ZR_NULL ||
        help->signatures.length == 0 ||
        !signature_help_contains_text(help, "shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>") ||
        help->activeParameter != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", "Signature help should show the closed generic method signature with normalized const generics");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Signature Help Displays Closed Generic Instantiation");
}

static void test_lsp_signature_help_resolves_super_constructor(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class BaseHero {\n"
        "    pub @constructor(origin: int) {\n"
        "    }\n"
        "}\n"
        "class BossHero: BaseHero {\n"
        "    pub @constructor(seed: int) super(seed) {\n"
        "    }\n"
        "}\n";
    SZrLspPosition superArgumentPosition;
    SZrLspSignatureHelp *help = ZR_NULL;

    TEST_START("LSP Signature Help Resolves Super Constructor");
    TEST_INFO("Super constructor signature help",
              "The derived constructor should surface the base constructor signature at super(...) call sites");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Signature Help Resolves Super Constructor", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///super_constructor_signature_help.zr", 42);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Resolves Super Constructor", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Resolves Super Constructor", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "super(seed)", 0, 7, &superArgumentPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Resolves Super Constructor", "Failed to compute super call position");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, superArgumentPosition, &help) ||
        help == ZR_NULL ||
        !signature_help_contains_text(help, "origin: int") ||
        signature_help_contains_text(help, "seed: int")) {
        TZrChar reason[256];
        const TZrChar *label = signature_help_first_label(help);

        snprintf(reason,
                 sizeof(reason),
                 "Signature help at super(...) should come from the base constructor, not the derived constructor (label=%s)",
                 label != ZR_NULL ? label : "<null>");
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help Resolves Super Constructor",
                  reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Signature Help Resolves Super Constructor");
}

static void test_lsp_definition_resolves_super_constructor(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class BaseHero {\n"
        "    pub @constructor(origin: int) {\n"
        "    }\n"
        "}\n"
        "class BossHero: BaseHero {\n"
        "    pub @constructor(seed: int) super(seed) {\n"
        "    }\n"
        "}\n";
    SZrLspPosition superCallPosition;
    SZrLspPosition baseConstructorPosition;
    SZrArray definitions;

    TEST_START("LSP Definition Resolves Super Constructor");
    TEST_INFO("Super constructor definition",
              "Goto definition on super(...) should jump to the base constructor declaration");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Definition Resolves Super Constructor", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///super_constructor_definition.zr", 38);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Super Constructor", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Super Constructor", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "super(seed)", 0, 0, &superCallPosition) ||
        !lsp_find_position_for_substring(content, "@constructor(origin: int)", 0, 1, &baseConstructorPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Super Constructor", "Failed to compute constructor navigation positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, superCallPosition, &definitions) ||
        definitions.length == 0 ||
        !location_array_contains_position(&definitions,
                                          baseConstructorPosition.line,
                                          baseConstructorPosition.character)) {
        TZrChar reason[256];
        SZrLspLocation **firstLocationPtr =
            definitions.length > 0 ? (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0) : ZR_NULL;
        SZrLspLocation *firstLocation = firstLocationPtr != ZR_NULL ? *firstLocationPtr : ZR_NULL;
        snprintf(reason,
                 sizeof(reason),
                 "Definition on super(...) should land on the base constructor declaration (count=%zu first=%d:%d-%d:%d)",
                 (size_t)definitions.length,
                 firstLocation != ZR_NULL ? firstLocation->range.start.line : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.start.character : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.end.line : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.end.character : -1);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Definition Resolves Super Constructor",
                  reason);
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Definition Resolves Super Constructor");
}

static void test_lsp_references_resolve_super_constructor(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class BaseHero {\n"
        "    pub @constructor(origin: int) {\n"
        "    }\n"
        "}\n"
        "class BossHero: BaseHero {\n"
        "    pub @constructor(seed: int) super(seed) {\n"
        "    }\n"
        "}\n";
    SZrLspPosition superCallPosition;
    SZrLspPosition baseConstructorPosition;
    SZrArray references;

    TEST_START("LSP References Resolve Super Constructor");
    TEST_INFO("Super constructor references",
              "Find references on super(...) should include the base constructor declaration and super call");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP References Resolve Super Constructor", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///super_constructor_references.zr", 38);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP References Resolve Super Constructor", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP References Resolve Super Constructor", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "super(seed)", 0, 0, &superCallPosition) ||
        !lsp_find_position_for_substring(content, "@constructor(origin: int)", 0, 1, &baseConstructorPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP References Resolve Super Constructor", "Failed to compute reference positions");
        return;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, superCallPosition, ZR_TRUE, &references) ||
        references.length < 2 ||
        !location_array_contains_position(&references, baseConstructorPosition.line, baseConstructorPosition.character) ||
        !location_array_contains_position(&references, superCallPosition.line, superCallPosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP References Resolve Super Constructor",
                  "References on super(...) should include both the base constructor declaration and the super call");
        return;
    }

    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP References Resolve Super Constructor");
}

static void test_lsp_document_highlights_resolve_super_constructor(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class BaseHero {\n"
        "    pub @constructor(origin: int) {\n"
        "    }\n"
        "}\n"
        "class BossHero: BaseHero {\n"
        "    pub @constructor(seed: int) super(seed) {\n"
        "    }\n"
        "}\n";
    SZrLspPosition superCallPosition;
    SZrLspPosition baseConstructorPosition;
    SZrArray highlights;

    TEST_START("LSP Document Highlights Resolve Super Constructor");
    TEST_INFO("Super constructor highlights",
              "Document highlights on super(...) should include the base constructor declaration and super call");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Document Highlights Resolve Super Constructor", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///super_constructor_highlights.zr", 38);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Document Highlights Resolve Super Constructor", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Document Highlights Resolve Super Constructor", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "super(seed)", 0, 0, &superCallPosition) ||
        !lsp_find_position_for_substring(content, "@constructor(origin: int)", 0, 1, &baseConstructorPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Document Highlights Resolve Super Constructor", "Failed to compute highlight positions");
        return;
    }

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, superCallPosition, &highlights) ||
        highlights.length < 2 ||
        !highlight_array_contains_position(&highlights, baseConstructorPosition.line, baseConstructorPosition.character) ||
        !highlight_array_contains_position(&highlights, superCallPosition.line, superCallPosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Document Highlights Resolve Super Constructor",
                  "Document highlights on super(...) should include both the base constructor declaration and the super call");
        return;
    }

    ZrCore_Array_Free(state, &highlights);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Document Highlights Resolve Super Constructor");
}

static void test_lsp_definition_resolves_decorator_target(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "#singleton#\n"
        "class SingletonClass {\n"
        "    #trace#\n"
        "    pub run(): int {\n"
        "        return 1;\n"
        "    }\n"
        "}\n";
    SZrLspPosition classDecoratorPosition;
    SZrLspPosition methodDecoratorPosition;
    SZrLspPosition classNamePosition;
    SZrLspPosition methodNamePosition;
    SZrArray definitions;

    TEST_START("LSP Definition Resolves Decorator Target");
    TEST_INFO("Decorator definition",
              "Goto definition on a decorator token should jump to the decorated class or method");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Definition Resolves Decorator Target", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///decorator_definition.zr", 32);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Decorator Target", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Decorator Target", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "#singleton#", 0, 1, &classDecoratorPosition) ||
        !lsp_find_position_for_substring(content, "#trace#", 0, 1, &methodDecoratorPosition) ||
        !lsp_find_position_for_substring(content, "SingletonClass", 0, 0, &classNamePosition) ||
        !lsp_find_position_for_substring(content, "run(): int", 0, 0, &methodNamePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Decorator Target", "Failed to compute decorator target positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, classDecoratorPosition, &definitions) ||
        !location_array_contains_position(&definitions, classNamePosition.line, classNamePosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Definition Resolves Decorator Target",
                  "Definition on #singleton# should jump to the decorated class");
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, methodDecoratorPosition, &definitions) ||
        !location_array_contains_position(&definitions, methodNamePosition.line, methodNamePosition.character)) {
        TZrChar reason[256];
        SZrLspLocation **firstLocationPtr =
            definitions.length > 0 ? (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0) : ZR_NULL;
        SZrLspLocation *firstLocation = firstLocationPtr != ZR_NULL ? *firstLocationPtr : ZR_NULL;
        snprintf(reason,
                 sizeof(reason),
                 "Definition on #trace# should jump to the decorated method (count=%zu first=%d:%d-%d:%d)",
                 (size_t)definitions.length,
                 firstLocation != ZR_NULL ? firstLocation->range.start.line : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.start.character : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.end.line : -1,
                 firstLocation != ZR_NULL ? firstLocation->range.end.character : -1);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Definition Resolves Decorator Target", reason);
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Definition Resolves Decorator Target");
}

static void test_lsp_hover_describes_decorator_target(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "#singleton#\n"
        "class SingletonClass {\n"
        "    pub var value: int = 0;\n"
        "}\n";
    SZrLspPosition decoratorPosition;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Hover Describes Decorator Target");
    TEST_INFO("Decorator hover",
              "Hover on a decorator token should describe the decorator category and decorated declaration");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Hover Describes Decorator Target", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///decorator_hover.zr", 27);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Describes Decorator Target", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Describes Decorator Target", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "#singleton#", 0, 1, &decoratorPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Describes Decorator Target", "Failed to compute decorator position");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, decoratorPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Decorator: singleton") ||
        !hover_contains_text(hover, "Category: decorator") ||
        !hover_contains_text(hover, "Target: class SingletonClass")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover Describes Decorator Target",
                  "Hover on #singleton# should describe the decorator name, category, and decorated class");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Hover Describes Decorator Target");
}

static void test_lsp_hover_describes_meta_method_category(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class Foo {\n"
        "    pub @constructor(seed: int) {\n"
        "    }\n"
        "}\n";
    SZrLspPosition metaMethodPosition;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Hover Describes Meta Method Category");
    TEST_INFO("Meta method hover",
              "Hover on an @meta-method token should describe its category and applicable declaration shape");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Hover Describes Meta Method Category", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///meta_method_hover.zr", 29);
    if (uri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Describes Meta Method Category", "Failed to allocate URI");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Describes Meta Method Category", "Failed to update document");
        return;
    }

    if (!lsp_find_position_for_substring(content, "@constructor", 0, 1, &metaMethodPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Describes Meta Method Category", "Failed to compute meta method position");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, metaMethodPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Meta Method: @constructor") ||
        !hover_contains_text(hover, "Category: lifecycle") ||
        !hover_contains_text(hover, "Applicable To: class/struct meta function")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover Describes Meta Method Category",
                  "Hover on @constructor should describe its lifecycle category and applicable targets");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Hover Describes Meta Method Category");
}

static void test_lsp_extern_function_navigation_and_signature_help(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "%extern(\"fixture\") {\n"
        "    NativeAdd(lhs: i32, rhs: i32): i32;\n"
        "}\n"
        "func use(): i32 {\n"
        "    var sum = NativeAdd(1, 2);\n"
        "    return NativeAdd(sum, 3);\n"
        "}\n";
    SZrLspPosition definitionPosition;
    SZrLspPosition firstCallPosition;
    SZrLspPosition secondCallPosition;
    SZrLspPosition signaturePosition;
    SZrLspPosition completionPosition;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;
    SZrArray completions;
    SZrLspSignatureHelp *help = ZR_NULL;

    TEST_START("LSP Extern Function Navigation And Signature Help");
    TEST_INFO("Extern function semantic integration",
              "Extern function declarations should participate in completion, hover-independent definition/reference tracking, document highlights, and signature help");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///extern_function_navigation.zr", 38);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "NativeAdd(lhs: i32, rhs: i32): i32;", 0, 0, &definitionPosition) ||
        !lsp_find_position_for_substring(content, "NativeAdd(1, 2)", 0, 0, &firstCallPosition) ||
        !lsp_find_position_for_substring(content, "NativeAdd(sum, 3)", 0, 0, &secondCallPosition) ||
        !lsp_find_position_for_substring(content, "NativeAdd(1, 2)", 0, 10, &signaturePosition) ||
        !lsp_find_position_for_substring(content, "return NativeAdd(sum, 3);", 0, 7, &completionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Failed to prepare extern function test fixture positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, firstCallPosition, &definitions) ||
        !location_array_contains_position(&definitions, definitionPosition.line, definitionPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Goto definition on an extern function call should jump to the source extern declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, firstCallPosition, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, definitionPosition.line, definitionPosition.character) ||
        !location_array_contains_position(&references, firstCallPosition.line, firstCallPosition.character) ||
        !location_array_contains_position(&references, secondCallPosition.line, secondCallPosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Extern function references should include the declaration and every call usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, firstCallPosition, &highlights) ||
        !highlight_array_contains_position(&highlights, definitionPosition.line, definitionPosition.character) ||
        !highlight_array_contains_position(&highlights, firstCallPosition.line, firstCallPosition.character) ||
        !highlight_array_contains_position(&highlights, secondCallPosition.line, secondCallPosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Document highlights for an extern function should share the same declaration and usage target set");
        return;
    }
    ZrCore_Array_Free(state, &highlights);

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, signaturePosition, &help) ||
        help == ZR_NULL ||
        !signature_help_contains_text(help, "NativeAdd(lhs: i32, rhs: i32): i32") ||
        help->activeParameter != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Signature help for an extern function call should come from the registered extern signature");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "NativeAdd")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Completion inside source code should expose the extern function symbol");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Extern Function Navigation And Signature Help");
}

static void test_lsp_extern_type_symbols_surface_hover_and_definition(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "%extern(\"fixture\") {\n"
        "    delegate Callback(value: i32): void;\n"
        "    struct NativePoint {\n"
        "        var x: i32;\n"
        "    }\n"
        "    #zr.ffi.underlying(\"i32\")#\n"
        "    enum Mode {\n"
        "        #zr.ffi.value(0)# Off,\n"
        "        #zr.ffi.value(1)# On\n"
        "    }\n"
        "}\n"
        "func apply(cb: Callback, point: NativePoint): Mode {\n"
        "    return Mode.On;\n"
        "}\n";
    SZrLspPosition callbackDeclPosition;
    SZrLspPosition callbackUsePosition;
    SZrLspPosition pointDeclPosition;
    SZrLspPosition pointUsePosition;
    SZrLspPosition modeDeclPosition;
    SZrLspPosition modeUsePosition;
    SZrLspPosition completionPosition;
    SZrArray definitions;
    SZrArray references;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Extern Type Symbols Surface Hover And Definition");
    TEST_INFO("Extern type semantic integration",
              "Extern delegate/struct/enum declarations should become navigable source symbols with stable hover and completion");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///extern_type_symbols.zr", 31);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "delegate Callback(value: i32): void;", 0, 9, &callbackDeclPosition) ||
        !lsp_find_position_for_substring(content, "cb: Callback", 0, 4, &callbackUsePosition) ||
        !lsp_find_position_for_substring(content, "struct NativePoint", 0, 7, &pointDeclPosition) ||
        !lsp_find_position_for_substring(content, "point: NativePoint", 0, 7, &pointUsePosition) ||
        !lsp_find_position_for_substring(content, "enum Mode", 0, 5, &modeDeclPosition) ||
        !lsp_find_position_for_substring(content, "): Mode", 0, 3, &modeUsePosition) ||
        !lsp_find_position_for_substring(content, "return Mode.On;", 0, 7, &completionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Failed to prepare extern type test fixture positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, callbackUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Callback") ||
        !hover_contains_text(hover, "extern") ||
        !hover_contains_text(hover, "Source: ffi extern")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Hover on an extern delegate type usage should describe the delegate and surface the ffi source origin");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, pointUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "NativePoint") ||
        !hover_contains_text(hover, "struct") ||
        !hover_contains_text(hover, "Source: ffi extern")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Hover on an extern struct type usage should resolve through the same ffi source-symbol path");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, modeUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Mode") ||
        !hover_contains_text(hover, "enum") ||
        !hover_contains_text(hover, "Source: ffi extern")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Hover on an extern enum type usage should surface the enum symbol and ffi source origin");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, callbackUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, callbackDeclPosition.line, callbackDeclPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Goto definition on an extern delegate type usage should jump to the source extern declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, pointUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, pointDeclPosition.line, pointDeclPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Goto definition on an extern struct type usage should jump to the source struct declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, modeUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, modeDeclPosition.line, modeDeclPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Goto definition on an extern enum type usage should jump to the source enum declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, pointUsePosition, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, pointDeclPosition.line, pointDeclPosition.character) ||
        !location_array_contains_position(&references, pointUsePosition.line, pointUsePosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Extern struct type references should include both declaration and annotation usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "Callback") ||
        !completion_array_contains_label(&completions, "NativePoint") ||
        !completion_array_contains_label(&completions, "Mode")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Type Symbols Surface Hover And Definition",
                  "Completion should list extern delegate/struct/enum symbols from the unified source-symbol path");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Extern Type Symbols Surface Hover And Definition");
}

static void test_lsp_completion_lists_directives_and_meta_methods(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "%compileTime var sentinel = 1;\n"
        "class Foo {\n"
        "    pub @constructor() { }\n"
        "}\n";
    SZrString *uri;
    SZrLspPosition directivePosition;
    SZrLspPosition metaPosition;
    SZrArray completions;

    TEST_START("LSP Completion Lists Directives And Meta Methods");
    TEST_INFO("Directive/meta completion",
              "Typing % or @ should offer reserved directives and meta method categories");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///directive_meta_completion.zr",
                               strlen("file:///directive_meta_completion.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Completion Lists Directives And Meta Methods", "Failed to prepare completion source");
        return;
    }

    if (!lsp_find_position_for_substring(content, "%compileTime", 0, 1, &directivePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Completion Lists Directives And Meta Methods", "Failed to compute directive completion position");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, directivePosition, &completions) ||
        !completion_array_contains_label(&completions, "%import") ||
        !completion_array_contains_label(&completions, "%compileTime") ||
        !completion_array_contains_label(&completions, "%test") ||
        !completion_array_contains_label(&completions, "%extern") ||
        !completion_array_contains_label(&completions, "%type")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Completion Lists Directives And Meta Methods",
                  "Expected % completion to list language directives");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!lsp_find_position_for_substring(content, "@constructor", 0, 1, &metaPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Completion Lists Directives And Meta Methods", "Failed to compute meta completion position");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, metaPosition, &completions) ||
        !completion_array_contains_label(&completions, "@constructor") ||
        !completion_array_contains_label(&completions, "@destructor") ||
        !completion_array_contains_label(&completions, "@close") ||
        !completion_array_contains_label(&completions, "@add") ||
        !completion_array_contains_label(&completions, "@toString")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Completion Lists Directives And Meta Methods",
                  "Expected @ completion to list lifecycle and operator meta methods");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Completion Lists Directives And Meta Methods");
}

// 主测试函数
int main(void) {
    printf("==========\n");
    printf("Language Server - LSP Interface Tests\n");
    printf("==========\n\n");
    
    // 创建全局状态
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Fail - Failed to create global state\n");
        return 1;
    }
    
    // 获取主线程状态
    SZrState *state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Fail - Failed to get main thread state\n");
        return 1;
    }
    
    // 初始化注册表
    ZrCore_GlobalState_InitRegistry(state, global);
    
    // 运行测试
    test_lsp_context_create_and_free(state);
    TEST_DIVIDER();
    
    test_lsp_update_document(state);
    TEST_DIVIDER();
    
    test_lsp_get_diagnostics(state);
    TEST_DIVIDER();

    test_lsp_get_parser_diagnostics(state);
    TEST_DIVIDER();
    
    test_lsp_get_completion(state);
    TEST_DIVIDER();
    
    test_lsp_get_definition(state);
    TEST_DIVIDER();
    
    test_lsp_find_references(state);
    TEST_DIVIDER();
    
    test_lsp_rename(state);
    TEST_DIVIDER();

    test_lsp_get_document_symbols(state);
    TEST_DIVIDER();

    test_lsp_get_workspace_symbols(state);
    TEST_DIVIDER();

    test_lsp_project_definition_resolves_imported_function(state);
    TEST_DIVIDER();

    test_lsp_project_references_include_imported_function_usage(state);
    TEST_DIVIDER();

    test_lsp_project_workspace_symbols_include_imported_exports(state);
    TEST_DIVIDER();

    test_lsp_get_document_highlights(state);
    TEST_DIVIDER();

    test_lsp_prepare_rename(state);
    TEST_DIVIDER();

    test_lsp_class_member_navigation_and_completion(state);
    TEST_DIVIDER();

    test_lsp_hover_and_completion_include_comments(state);
    TEST_DIVIDER();

    test_lsp_function_symbol_range_trimmed_to_body(state);
    TEST_DIVIDER();

    test_lsp_function_prepare_rename_uses_name_range(state);
    TEST_DIVIDER();

    test_lsp_closed_generic_type_display_and_definition(state);
    TEST_DIVIDER();

    test_lsp_signature_help_displays_closed_generic_instantiation(state);
    TEST_DIVIDER();

    test_lsp_signature_help_resolves_super_constructor(state);
    TEST_DIVIDER();

    test_lsp_definition_resolves_super_constructor(state);
    TEST_DIVIDER();

    test_lsp_references_resolve_super_constructor(state);
    TEST_DIVIDER();

    test_lsp_document_highlights_resolve_super_constructor(state);
    TEST_DIVIDER();

    test_lsp_definition_resolves_decorator_target(state);
    TEST_DIVIDER();

    test_lsp_hover_describes_decorator_target(state);
    TEST_DIVIDER();

    test_lsp_hover_describes_meta_method_category(state);
    TEST_DIVIDER();

    test_lsp_extern_function_navigation_and_signature_help(state);
    TEST_DIVIDER();

    test_lsp_extern_type_symbols_surface_hover_and_definition(state);
    TEST_DIVIDER();

    test_lsp_completion_lists_directives_and_meta_methods(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All LSP Interface Tests Completed\n");
    printf("==========\n");
    
    return 0;
}

