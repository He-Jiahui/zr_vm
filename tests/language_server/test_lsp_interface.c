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
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_library/file.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h"
#include "../../zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h"
#include "path_support.h"

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
static TZrBool lsp_find_position_for_substring(const TZrChar *content,
                                               const TZrChar *needle,
                                               TZrSize occurrence,
                                               TZrInt32 extraCharacterOffset,
                                               SZrLspPosition *outPosition);
static TZrBool hover_contains_text(SZrLspHover *hover, const TZrChar *needle);
static const TZrChar *hover_first_text(SZrLspHover *hover);
static TZrBool rich_hover_section_contains_text(SZrLspRichHover *hover,
                                                const TZrChar *role,
                                                const TZrChar *needle);
static TZrBool inlay_hint_array_contains_label(SZrArray *hints, const TZrChar *label);
static SZrLspSymbolInformation *find_symbol_information_by_name(SZrArray *symbols, const TZrChar *name);
static TZrBool build_fixture_native_path(const TZrChar *relativePath, TZrChar *buffer, TZrSize bufferSize);
static TZrChar *read_fixture_text_file(const TZrChar *path, TZrSize *outLength);
static SZrString *create_file_uri_from_native_path(SZrState *state, const TZrChar *path);
static SZrString *create_percent_encoded_file_uri_from_native_path(SZrState *state, const TZrChar *path);

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

    if (context->uriToAnalyzerMap.pairPoolHead != ZR_NULL ||
        context->uriToAnalyzerMap.pairPoolActive != ZR_NULL ||
        context->uriToAnalyzerMap.pairPoolCapacity != 0 ||
        context->uriToAnalyzerMap.pairPoolUsed != 0 ||
        context->parser->uriToFileMap.pairPoolHead != ZR_NULL ||
        context->parser->uriToFileMap.pairPoolActive != ZR_NULL ||
        context->parser->uriToFileMap.pairPoolCapacity != 0 ||
        context->parser->uriToFileMap.pairPoolUsed != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Context Creation and Free",
                  "Fresh hash sets must start with an empty pair-pool state");
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

static void test_lsp_incomplete_edit_preserves_prior_semantic_snapshot(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics;
    SZrArray symbols;
    SZrLspHover *hover = ZR_NULL;
    SZrLspPosition laterUsage;
    const TZrChar *validContent =
        "var total = 10;\n"
        "valueOf() {\n"
        "    return total;\n"
        "}\n"
        "var later: int = 12;\n";
    const TZrChar *brokenContent =
        "var total = 10;\n"
        "valueOf() {\n"
        "    return total;\n"
        "\n"
        "var later: int = 12;\n";

    TEST_START("LSP Incomplete Edit Preserves Prior Semantic Snapshot");
    TEST_INFO("Incremental syntax fallback",
              "A missing closing brace in the middle of the file should still surface parser diagnostics without dropping semantic information for unchanged declarations below it");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Incomplete Edit Preserves Prior Semantic Snapshot",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///incomplete_edit_snapshot.zr", 36);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, validContent, strlen(validContent), 1) ||
        !lsp_find_position_for_substring(validContent, "var later: int = 12;", 0, 4, &laterUsage)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Incomplete Edit Preserves Prior Semantic Snapshot",
                  "Failed to prepare the valid baseline document");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, brokenContent, strlen(brokenContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Incomplete Edit Preserves Prior Semantic Snapshot",
                  "Failed to update the document to the incomplete-edit version");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) ||
        diagnostics.length == 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Incomplete Edit Preserves Prior Semantic Snapshot",
                  "Parser diagnostics should still report the missing function terminator");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, laterUsage, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "later") ||
        !hover_contains_text(hover, "int")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Incomplete Edit Preserves Prior Semantic Snapshot",
                  "Hover for the unchanged declaration below the syntax error should still resolve through the last good semantic snapshot");
        return;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 4);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols) ||
        find_symbol_information_by_name(&symbols, "total") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "valueOf") == ZR_NULL ||
        find_symbol_information_by_name(&symbols, "later") == ZR_NULL) {
        ZrCore_Array_Free(state, &symbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Incomplete Edit Preserves Prior Semantic Snapshot",
                  "Document symbols for unchanged declarations should survive a mid-file missing-brace edit");
        return;
    }

    ZrCore_Array_Free(state, &symbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Incomplete Edit Preserves Prior Semantic Snapshot");
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
    "    system.gc.disable();\n"
    "    system.gc.collect(\"minor\");\n"
    "    system.gc.enable();\n"
    "    system.gc.set_heap_limit(8192);\n"
    "    system.gc.set_budget(2000);\n"
    "    var gcStats = system.gc.get_stats();\n"
    "    system.gc.collect(\"full\");\n"
    "    system.console.printErrorLine(\"native numeric pipeline stderr ready\");\n"
    "    return {\n"
    "        checksum: exportedValue + signalInfo.magnitude + tensorInfo.sum + tensorInfo.mean;\n"
    "        loadedModuleCount: state.loadedModuleCount;\n"
    "        gcPhase: gcStats.collectionPhase;\n"
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

static TZrBool location_array_contains_uri_text(SZrArray *locations, const TZrChar *uriText) {
    if (locations == ZR_NULL || uriText == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        const TZrChar *text;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL) {
            continue;
        }

        text = test_string_ptr((*locationPtr)->uri);
        if (text != ZR_NULL && strcmp(text, uriText) == 0) {
            return ZR_TRUE;
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

static TZrInt32 semantic_token_type_index(const TZrChar *typeName) {
    if (typeName == ZR_NULL) {
        return -1;
    }

    for (TZrSize index = 0; index < ZrLanguageServer_Lsp_SemanticTokenTypeCount(); index++) {
        const TZrChar *candidate = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (candidate != ZR_NULL && strcmp(candidate, typeName) == 0) {
            return (TZrInt32)index;
        }
    }

    return -1;
}

static TZrBool semantic_tokens_contain(SZrArray *data,
                                       TZrInt32 line,
                                       TZrInt32 character,
                                       TZrInt32 length,
                                       const TZrChar *typeName) {
    TZrUInt32 currentLine = 0;
    TZrUInt32 currentCharacter = 0;
    TZrInt32 typeIndex = semantic_token_type_index(typeName);

    if (data == ZR_NULL || typeIndex < 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index + 4 < data->length; index += 5) {
        TZrUInt32 *deltaLinePtr = (TZrUInt32 *)ZrCore_Array_Get(data, index);
        TZrUInt32 *deltaStartPtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 1);
        TZrUInt32 *lengthPtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 2);
        TZrUInt32 *typePtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 3);

        if (deltaLinePtr == ZR_NULL || deltaStartPtr == ZR_NULL || lengthPtr == ZR_NULL || typePtr == ZR_NULL) {
            continue;
        }

        currentLine += *deltaLinePtr;
        if (*deltaLinePtr == 0) {
            currentCharacter += *deltaStartPtr;
        } else {
            currentCharacter = *deltaStartPtr;
        }

        if ((TZrInt32)currentLine == line &&
            (TZrInt32)currentCharacter == character &&
            (TZrInt32)(*lengthPtr) == length &&
            (TZrInt32)(*typePtr) == typeIndex) {
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

static const TZrChar *hover_first_text(SZrLspHover *hover) {
    if (hover == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < hover->contents.length; index++) {
        SZrString **contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, index);
        if (contentPtr != ZR_NULL && *contentPtr != ZR_NULL) {
            return test_string_ptr(*contentPtr);
        }
    }

    return ZR_NULL;
}

static TZrBool rich_hover_section_contains_text(SZrLspRichHover *hover,
                                                const TZrChar *role,
                                                const TZrChar *needle) {
    if (hover == ZR_NULL || role == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hover->sections.length; index++) {
        SZrLspRichHoverSection **sectionPtr =
            (SZrLspRichHoverSection **)ZrCore_Array_Get(&hover->sections, index);
        if (sectionPtr == ZR_NULL || *sectionPtr == ZR_NULL || (*sectionPtr)->role == ZR_NULL ||
            (*sectionPtr)->value == ZR_NULL) {
            continue;
        }

        if (strcmp(test_string_ptr((*sectionPtr)->role), role) == 0 &&
            strstr(test_string_ptr((*sectionPtr)->value), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool inlay_hint_array_contains_label(SZrArray *hints, const TZrChar *label) {
    for (TZrSize index = 0; hints != ZR_NULL && label != ZR_NULL && index < hints->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL &&
            strcmp(test_string_ptr((*hintPtr)->label), label) == 0) {
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

typedef struct SZrGeneratedBinaryMetadataFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryPath[ZR_TESTS_PATH_MAX];
} SZrGeneratedBinaryMetadataFixture;

static TZrChar *find_last_path_separator(TZrChar *path) {
    TZrChar *forwardSlash;
    TZrChar *backSlash;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    forwardSlash = strrchr(path, '/');
    backSlash = strrchr(path, '\\');
    if (forwardSlash == ZR_NULL) {
        return backSlash;
    }
    if (backSlash == ZR_NULL) {
        return forwardSlash;
    }

    return forwardSlash > backSlash ? forwardSlash : backSlash;
}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *content, TZrSize length) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(content, 1, (size_t)length, file);
    fclose(file);
    return written == (size_t)length;
}

static TZrBool copy_fixture_file(const TZrChar *sourcePath, const TZrChar *targetPath) {
    TZrSize contentLength = 0;
    TZrChar *content = read_fixture_text_file(sourcePath, &contentLength);
    TZrBool success;

    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    success = write_text_file(targetPath, content, contentLength);
    free(content);
    return success;
}

static TZrBool regenerate_binary_metadata_fixture_artifacts(SZrState *state,
                                                            const TZrChar *binaryPath,
                                                            const TZrChar *moduleSource) {
    SZrString *sourceName;
    SZrFunction *function;
    SZrBinaryWriterOptions options;
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *fileName;
    const TZrChar *extension;
    TZrSize moduleNameLength;
    TZrBool success;

    if (state == ZR_NULL || binaryPath == ZR_NULL || moduleSource == ZR_NULL || binaryPath[0] == '\0') {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_EnsureParentDirectory(binaryPath)) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    fileName = strrchr(binaryPath, '/');
    if (fileName == ZR_NULL) {
        fileName = strrchr(binaryPath, '\\');
    }
    fileName = fileName != ZR_NULL ? fileName + 1 : binaryPath;
    extension = strrchr(fileName, '.');
    moduleNameLength = extension != ZR_NULL ? (TZrSize)(extension - fileName) : strlen(fileName);
    if (moduleNameLength == 0 || moduleNameLength >= sizeof(moduleNameBuffer)) {
        return ZR_FALSE;
    }

    memcpy(moduleNameBuffer, fileName, moduleNameLength);
    moduleNameBuffer[moduleNameLength] = '\0';
    options.moduleName = moduleNameBuffer;

    sourceName = ZrCore_String_Create(state, (TZrNativeString)binaryPath, strlen(binaryPath));
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }

    function = ZrParser_Source_Compile(state, (TZrNativeString)moduleSource, strlen(moduleSource), sourceName);
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    success = ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &options);
    ZrCore_Function_Free(state, function);
    return success;
}

static TZrBool prepare_generated_binary_metadata_fixture(SZrState *state,
                                                         const TZrChar *artifactName,
                                                         SZrGeneratedBinaryMetadataFixture *fixture) {
    TZrChar generatedProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureProjectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureMainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureStageAPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureStageBPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureBinarySourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryRootPath[ZR_TESTS_PATH_MAX];
    TZrChar targetStageAPath[ZR_TESTS_PATH_MAX];
    TZrChar targetStageBPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    TZrSize binarySourceLength = 0;
    TZrChar *binarySourceContent = ZR_NULL;

    if (state == ZR_NULL || artifactName == ZR_NULL || fixture == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(fixture, 0, sizeof(*fixture));
    if (!ZrTests_Path_GetGeneratedArtifact("language_server",
                                           artifactName,
                                           "aot_module_graph_pipeline",
                                           ".zrp",
                                           generatedProjectPath,
                                           sizeof(generatedProjectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/aot_module_graph_pipeline.zrp",
                                   fixtureProjectPath,
                                   sizeof(fixtureProjectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/main.zr",
                                   fixtureMainPath,
                                   sizeof(fixtureMainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/graph_stage_a.zr",
                                   fixtureStageAPath,
                                   sizeof(fixtureStageAPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/graph_stage_b.zr",
                                   fixtureStageBPath,
                                   sizeof(fixtureStageBPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/fixtures/graph_binary_stage_source.zr",
                                   fixtureBinarySourcePath,
                                   sizeof(fixtureBinarySourcePath))) {
        return ZR_FALSE;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", generatedProjectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    snprintf(fixture->projectPath, sizeof(fixture->projectPath), "%s", generatedProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "src", sourceRootPath);
    ZrLibrary_File_PathJoin(rootPath, "bin", binaryRootPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "graph_stage_a.zr", targetStageAPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "graph_stage_b.zr", targetStageBPath);
    ZrLibrary_File_PathJoin(binaryRootPath, "graph_binary_stage.zro", fixture->binaryPath);

    binarySourceContent = read_fixture_text_file(fixtureBinarySourcePath, &binarySourceLength);
    if (binarySourceContent == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!copy_fixture_file(fixtureProjectPath, fixture->projectPath) ||
        !copy_fixture_file(fixtureMainPath, fixture->mainPath) ||
        !copy_fixture_file(fixtureStageAPath, targetStageAPath) ||
        !copy_fixture_file(fixtureStageBPath, targetStageBPath) ||
        !regenerate_binary_metadata_fixture_artifacts(state, fixture->binaryPath, binarySourceContent)) {
        free(binarySourceContent);
        return ZR_FALSE;
    }

    free(binarySourceContent);
    return ZR_TRUE;
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

static SZrString *create_percent_encoded_file_uri_from_native_path(SZrState *state, const TZrChar *path) {
    TZrChar uriBuffer[2048];
    TZrSize writeIndex = 0;
    TZrSize pathLength;

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
    if (pathLength >= 2 &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        TZrChar drive = path[0];
        if (drive >= 'A' && drive <= 'Z') {
            drive = (TZrChar)(drive - 'A' + 'a');
        }
        uriBuffer[writeIndex++] = drive;
        memcpy(uriBuffer + writeIndex, "%3A", 3);
        writeIndex += 3;
        path += 2;
        pathLength -= 2;
    }
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

static void test_lsp_project_encoded_uri_builtin_import_refresh_does_not_crash(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrLspPosition importLiteralPosition;
    SZrArray diagnostics;
    SZrLspHover *hover = ZR_NULL;
    TZrChar mainPath[1024];
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "return 1;\n";

    TEST_START("LSP Project Encoded URI Builtin Import Refresh Does Not Crash");
    TEST_INFO("Project refresh encoded URI",
              "Opening a project source file through a percent-encoded file URI should keep project refresh and native builtin metadata loading stable for zr.system imports");

    if (!build_fixture_native_path("tests/fixtures/projects/import_pub_function/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath))) {
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Failed to build project fixture path");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Failed to create LSP context");
        return;
    }

    mainUri = create_percent_encoded_file_uri_from_native_path(state, mainPath);
    if (mainUri == ZR_NULL ||
        !lsp_find_position_for_substring(content, "\"zr.system\"", 0, 1, &importLiteralPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Failed to prepare percent-encoded URI fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Failed to update the encoded project source document");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, mainUri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Failed to retrieve diagnostics after project refresh");
        return;
    }
    if (diagnostics.length != 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Expected the encoded-uri builtin import fixture to analyze without diagnostics");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, importLiteralPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.system>") ||
        !hover_contains_text(hover, "Source: native builtin")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Encoded URI Builtin Import Refresh Does Not Crash",
                  "Hover on the encoded-uri zr.system import should still resolve through native builtin metadata");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Project Encoded URI Builtin Import Refresh Does Not Crash");
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

static void test_lsp_callable_assignment_surfaces_exact_signature_and_parameter_scope(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class BaseHero {\n"
        "    pri var _hp: int = 0;\n"
        "    pub @constructor(seed: int) {\n"
        "        this._hp = seed;\n"
        "    }\n"
        "    pub get hp: int {\n"
        "        return this._hp;\n"
        "    }\n"
        "    pub set hp(v: int) {\n"
        "        this._hp = v;\n"
        "    }\n"
        "    pub heal(amount: int): int {\n"
        "        this.hp = this.hp + amount;\n"
        "        return this.hp;\n"
        "    }\n"
        "}\n"
        "class BossHero: BaseHero {\n"
        "    pub @constructor(seed: int) super(seed) { }\n"
        "    pub prepare(amount: int): int {\n"
        "        this.hp = this.hp + amount;\n"
        "        return this.hp;\n"
        "    }\n"
        "    pub afterBattle(amount: int): int {\n"
        "        return this.heal(amount);\n"
        "    }\n"
        "}\n"
        "runBossScenarioImpl(seed: int, prepareAmount: int, battleAmount: int) {\n"
        "    var boss = new BossHero(seed);\n"
        "    boss.prepare(prepareAmount);\n"
        "    return boss.afterBattle(battleAmount) + boss.heal(0);\n"
        "}\n"
        "pub var runBossScenario = runBossScenarioImpl;\n"
        "useScenario(): int {\n"
        "    return runBossScenario(30, 7, 5);\n"
        "}\n";
    SZrLspPosition callTargetPosition;
    SZrLspPosition signaturePosition;
    SZrLspPosition parameterUsePosition;
    SZrLspPosition bossUsePosition;
    SZrLspPosition parameterDeclarationPosition;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrString *placeholder = ZR_NULL;
    SZrLspRange range;

    TEST_START("LSP Callable Assignment Surfaces Exact Signature And Parameter Scope");
    TEST_INFO("Callable value inference / scope",
              "Function values assigned through pub var should stay callable, and body-local parameter/receiver hover must resolve through the method scope without drifting.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state,
                               "file:///callable_assignment_scope.zr",
                               strlen("file:///callable_assignment_scope.zr"));
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "runBossScenario(30, 7, 5)", 0, 0, &callTargetPosition) ||
        !lsp_find_position_for_substring(content, "runBossScenario(30, 7, 5)", 0, 16, &signaturePosition) ||
        !lsp_find_position_for_substring(content, "boss.prepare(prepareAmount);", 0, 13, &parameterUsePosition) ||
        !lsp_find_position_for_substring(content, "boss.prepare(prepareAmount);", 0, 0, &bossUsePosition) ||
        !lsp_find_position_for_substring(content, "prepareAmount: int", 0, 0, &parameterDeclarationPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Failed to prepare callable-assignment fixture positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, signaturePosition, &help) ||
        help == ZR_NULL ||
        !signature_help_contains_text(help, "seed: int") ||
        !signature_help_contains_text(help, "prepareAmount: int") ||
        !signature_help_contains_text(help, "battleAmount: int") ||
        !signature_help_contains_text(help, ": int")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Calling a pub var bound to a function should expose the callable signature instead of degrading to a non-callable/int value");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, callTargetPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "prepareAmount: int") ||
        !hover_contains_text(hover, "battleAmount: int") ||
        hover_contains_text(hover, "cannot infer exact type")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Hover on the assigned callable should preserve the exact function value signature");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, parameterUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "prepareAmount") ||
        !hover_contains_text(hover, "int")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Hover inside the function body should resolve the parameter binding to int");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, bossUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "boss") ||
        !hover_contains_text(hover, "BossHero")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Hover inside the function body should resolve the local receiver variable to BossHero");
        return;
    }

    if (!ZrLanguageServer_Lsp_PrepareRename(state, context, uri, parameterDeclarationPosition, &range, &placeholder) ||
        placeholder == ZR_NULL ||
        strcmp(test_string_ptr(placeholder), "prepareAmount") != 0 ||
        !lsp_range_equals(range,
                          parameterDeclarationPosition.line,
                          parameterDeclarationPosition.character,
                          parameterDeclarationPosition.line,
                          parameterDeclarationPosition.character + 13)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope",
                  "Parameter prepare-rename should use the parameter identifier span instead of the owner fallback range");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Callable Assignment Surfaces Exact Signature And Parameter Scope");
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
        TZrChar reason[256];
        const TZrChar *label = signature_help_first_label(help);

        snprintf(reason,
                 sizeof(reason),
                 "Signature help should show the closed generic method signature with normalized const generics (label=%s, activeParameter=%zu)",
                 label != ZR_NULL ? label : "<null>",
                 help != ZR_NULL ? (TZrSize)help->activeParameter : (TZrSize)0);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Signature Help Displays Closed Generic Instantiation", reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Signature Help Displays Closed Generic Instantiation");
}

static void test_lsp_inlay_hints_surface_exact_inferred_types(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class Matrix<T, const N: int> { }\n"
        "class Box<T> {\n"
        "    func shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
        "}\n"
        "func inferNumber() {\n"
        "    return 42;\n"
        "}\n"
        "func use(): void {\n"
        "    var box = new Box<int>();\n"
        "    var m = new Matrix<int, 2 + 2>();\n"
        "    var shaped = box.shape(m);\n"
        "}\n";
    SZrLspRange range;
    SZrArray hints;

    TEST_START("LSP Inlay Hints Surface Exact Inferred Types");
    TEST_INFO("Inlay Hints",
              "Unannotated locals and callable returns should surface exact strong types from the shared semantic truth without weak object fallback");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Inlay Hints Surface Exact Inferred Types", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///inlay_exact_inferred_types.zr", 38);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Inlay Hints Surface Exact Inferred Types",
                  "Failed to prepare the inlay hint fixture");
        return;
    }

    range.start.line = 0;
    range.start.character = 0;
    range.end.line = 32;
    range.end.character = 0;

    ZrCore_Array_Init(state, &hints, sizeof(SZrLspInlayHint *), 8);
    if (!ZrLanguageServer_Lsp_GetInlayHints(state, context, uri, range, &hints) ||
        !inlay_hint_array_contains_label(&hints, ": Box<int>") ||
        !inlay_hint_array_contains_label(&hints, ": Matrix<int, 4>") ||
        !inlay_hint_array_contains_label(&hints, ": int")) {
        ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Inlay Hints Surface Exact Inferred Types",
                  "Inlay hints should expose exact inferred closed-generic local types and inferred callable return types");
        return;
    }

    ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Inlay Hints Surface Exact Inferred Types");
}

static void test_lsp_hover_and_completion_surface_explicit_exact_type_failures(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "probe() {\n"
        "    var missing;\n"
        "    missing;\n"
        "}\n";
    SZrLspPosition usagePosition;
    SZrArray diagnostics;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    SZrLspCompletionItem *missingItem;
    const TZrChar *detailText = ZR_NULL;

    TEST_START("LSP Hover And Completion Surface Explicit Exact Type Failures");
    TEST_INFO("Strong typed hover/completion failure surface",
              "LSP hover and completion detail must say 'cannot infer exact type' for unresolved strong-typed symbols instead of falling back to object.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Hover And Completion Surface Explicit Exact Type Failures",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///exact_type_failure_surface.zr", 37);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "missing;", 1, 0, &usagePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover And Completion Surface Explicit Exact Type Failures",
                  "Failed to prepare the exact-type failure fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) ||
        !diagnostic_array_contains_message(&diagnostics, "initializer requires annotation")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover And Completion Surface Explicit Exact Type Failures",
                  "Expected initializer requires annotation diagnostic for the untyped local");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, usagePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "cannot infer exact type") ||
        hover_contains_text(hover, "object")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover And Completion Surface Explicit Exact Type Failures",
                  "Hover should surface explicit exact-type failure text without a weak object fallback");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, usagePosition, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover And Completion Surface Explicit Exact Type Failures",
                  "Completion request failed");
        return;
    }

    missingItem = find_completion_item_by_label(&completions, "missing");
    detailText = missingItem != ZR_NULL && missingItem->detail != ZR_NULL
                     ? test_string_ptr(missingItem->detail)
                     : ZR_NULL;
    if (detailText == ZR_NULL ||
        strstr(detailText, "cannot infer exact type") == ZR_NULL ||
        strstr(detailText, "object") != ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover And Completion Surface Explicit Exact Type Failures",
                  detailText != ZR_NULL ? detailText : "<null completion detail>");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Hover And Completion Surface Explicit Exact Type Failures");
}

static void test_lsp_signature_help_and_completion_surface_exact_unannotated_return_type(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "make(seed: int) {\n"
        "    return seed + 0;\n"
        "}\n"
        "use(): void {\n"
        "    make(1);\n"
        "}\n";
    SZrLspPosition callPosition;
    SZrLspSignatureHelp *help = ZR_NULL;
    SZrLspHover *hover = ZR_NULL;
    SZrArray completions;
    SZrLspCompletionItem *makeItem;
    const TZrChar *detailText = ZR_NULL;

    TEST_START("LSP Signature Help And Completion Surface Exact Unannotated Return Type");
    TEST_INFO("Exact return signature surface",
              "Provable unannotated functions must expose their exact return type in hover, completion, and signature help instead of object.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Signature Help And Completion Surface Exact Unannotated Return Type",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///exact_unannotated_return_signature.zr", 44);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "make(1)", 0, 5, &callPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help And Completion Surface Exact Unannotated Return Type",
                  "Failed to prepare the unannotated return fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, callPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Signature: make(seed: int): int") ||
        hover_contains_text(hover, "object")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help And Completion Surface Exact Unannotated Return Type",
                  "Hover should show the exact inferred return type for the unannotated function");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, callPosition, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help And Completion Surface Exact Unannotated Return Type",
                  "Completion request failed");
        return;
    }

    makeItem = find_completion_item_by_label(&completions, "make");
    detailText = makeItem != ZR_NULL && makeItem->detail != ZR_NULL
                     ? test_string_ptr(makeItem->detail)
                     : ZR_NULL;
    if (detailText == ZR_NULL ||
        strstr(detailText, "make(seed: int): int") == ZR_NULL ||
        strstr(detailText, "object") != ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help And Completion Surface Exact Unannotated Return Type",
                  detailText != ZR_NULL ? detailText : "<null completion detail>");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, callPosition, &help) ||
        help == ZR_NULL ||
        !signature_help_contains_text(help, "make(seed: int): int") ||
        signature_help_contains_text(help, "object")) {
        const TZrChar *label = signature_help_first_label(help);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Signature Help And Completion Surface Exact Unannotated Return Type",
                  label != ZR_NULL ? label : "<null signature>");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Signature Help And Completion Surface Exact Unannotated Return Type");
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

static void test_lsp_rich_hover_structures_meta_sections(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "class Foo {\n"
        "    pub @constructor(seed: int) {\n"
        "    }\n"
        "}\n";
    SZrLspPosition metaMethodPosition;
    SZrLspRichHover *richHover = ZR_NULL;

    TEST_START("LSP Rich Hover Structures Meta Sections");
    TEST_INFO("Rich hover payload",
              "Structured rich hover sections should preserve semantic roles for meta methods so the extension can color them without reparsing markdown.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Rich Hover Structures Meta Sections", "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///rich_hover_sections.zr", 31);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "@constructor", 0, 1, &metaMethodPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Rich Hover Structures Meta Sections",
                  "Failed to prepare rich-hover fixture positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetRichHover(state, context, uri, metaMethodPosition, &richHover) ||
        richHover == ZR_NULL ||
        !rich_hover_section_contains_text(richHover, "name", "@constructor") ||
        !rich_hover_section_contains_text(richHover, "category", "lifecycle") ||
        !rich_hover_section_contains_text(richHover, "applicableTo", "class/struct meta function") ||
        !rich_hover_section_contains_text(richHover, "detail", "lifecycle meta method")) {
        ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Rich Hover Structures Meta Sections",
                  "Meta method rich hover should expose name/category/applicableTo/detail sections for the extension panel");
        return;
    }

    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Rich Hover Structures Meta Sections");
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
    SZrArray diagnostics;
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

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) ||
        diagnostic_array_contains_message(&diagnostics, "Ambiguous overload for function 'NativeAdd'")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Function Navigation And Signature Help",
                  "Extern function analysis should not emit a duplicate overload diagnostic for NativeAdd");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

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

static void test_lsp_extern_layout_hover_surfaces_ffi_metadata(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "%extern(\"fixture\") {\n"
        "    #zr.ffi.pack(8)#\n"
        "    #zr.ffi.align(16)#\n"
        "    struct NativePoint {\n"
        "        var x: i32;\n"
        "        #zr.ffi.offset(8)#\n"
        "        var y: i32;\n"
        "    }\n"
        "    #zr.ffi.underlying(\"i32\")#\n"
        "    enum Mode {\n"
        "        #zr.ffi.value(1)# On\n"
        "    }\n"
        "}\n"
        "func read(point: NativePoint): i32 {\n"
        "    return point.y;\n"
        "}\n"
        "func current(): Mode {\n"
        "    return Mode.On;\n"
        "}\n";
    SZrLspPosition pointUsePosition;
    SZrLspPosition fieldDeclPosition;
    SZrLspPosition fieldUsePosition;
    SZrLspPosition enumUsePosition;
    SZrLspPosition enumMemberUsePosition;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Extern Layout Hover Surfaces FFI Metadata");
    TEST_INFO("Extern layout metadata hover",
              "Extern struct/enum hover should surface ffi layout decorators like pack/align/offset/underlying/value through the same source-symbol path");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///extern_layout_hover.zr", 30);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "point: NativePoint", 0, 7, &pointUsePosition) ||
        !lsp_find_position_for_substring(content, "var y: i32;", 0, 4, &fieldDeclPosition) ||
        !lsp_find_position_for_substring(content, "point.y", 0, 6, &fieldUsePosition) ||
        !lsp_find_position_for_substring(content, "current(): Mode", 0, 11, &enumUsePosition) ||
        !lsp_find_position_for_substring(content, "Mode.On", 0, 5, &enumMemberUsePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Failed to prepare extern layout hover positions");
        return;
    }
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, fieldUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, fieldDeclPosition.line, fieldDeclPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Goto definition on an extern struct field usage should jump to the field declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);
    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, fieldUsePosition, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, fieldDeclPosition.line, fieldDeclPosition.character) ||
        !location_array_contains_position(&references, fieldUsePosition.line, fieldUsePosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Extern struct field references should include the field declaration and usage");
        return;
    }
    ZrCore_Array_Free(state, &references);
    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, fieldUsePosition, &highlights) ||
        !highlight_array_contains_position(&highlights, fieldDeclPosition.line, fieldDeclPosition.character) ||
        !highlight_array_contains_position(&highlights, fieldUsePosition.line, fieldUsePosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Extern struct field document highlights should include the field declaration and usage");
        return;
    }
    ZrCore_Array_Free(state, &highlights);
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, pointUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "NativePoint") ||
        !hover_contains_text(hover, "Source: ffi extern") ||
        !hover_contains_text(hover, "Pack: 8") ||
        !hover_contains_text(hover, "Align: 16")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Hover on an extern struct type usage should surface pack/align metadata");
        return;
    }
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, fieldUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "y") ||
        !hover_contains_text(hover, "Source: ffi extern") ||
        !hover_contains_text(hover, "Offset: 8")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Hover on an extern struct field usage should surface its ffi offset metadata");
        return;
    }
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, enumUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Mode") ||
        !hover_contains_text(hover, "Source: ffi extern") ||
        !hover_contains_text(hover, "Underlying: i32")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Hover on an extern enum usage should surface its ffi underlying metadata");
        return;
    }
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, enumMemberUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "On") ||
        !hover_contains_text(hover, "Source: ffi extern") ||
        !hover_contains_text(hover, "Value: 1")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Extern Layout Hover Surfaces FFI Metadata",
                  "Hover on an extern enum member usage should surface its ffi value metadata");
        return;
    }
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Extern Layout Hover Surfaces FFI Metadata");
}

static void test_lsp_ffi_pointer_helpers_surface_extern_wrapper_types(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "%extern(\"fixture\") {\n"
        "    struct NativePoint {\n"
        "        var x: i32;\n"
        "        #zr.ffi.offset(4)#\n"
        "        var y: i32;\n"
        "    }\n"
        "}\n"
        "var ffi = %import(\"zr.ffi\");\n"
        "func run(buffer: ffi.BufferHandle): i32 {\n"
        "    var bytePtr = buffer.pin();\n"
        "    var pointPtr = bytePtr.as({ kind: \"pointer\", to: NativePoint, direction: \"inout\" });\n"
        "    var pointValue = pointPtr.read(NativePoint);\n"
        "    return pointValue.y;\n"
        "}\n";
    SZrLspPosition pointFieldDeclPosition;
    SZrLspPosition pointPtrHoverPosition;
    SZrLspPosition pointValueHoverPosition;
    SZrLspPosition pointFieldUsePosition;
    SZrLspPosition pointValueCompletionPosition;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP FFI Pointer Helpers Surface Extern Wrapper Types");
    TEST_INFO("FFI pointer helper extern wrapper flow",
              "pin/as/read should keep extern wrapper types on locals, completions, field navigation, and hover metadata");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///ffi_pointer_extern_wrapper.zr", 38);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "var y: i32;", 0, 4, &pointFieldDeclPosition) ||
        !lsp_find_position_for_substring(content, "var pointPtr = ", 0, 4, &pointPtrHoverPosition) ||
        !lsp_find_position_for_substring(content, "var pointValue = ", 0, 4, &pointValueHoverPosition) ||
        !lsp_find_position_for_substring(content, "pointValue.y", 0, 11, &pointFieldUsePosition) ||
        !lsp_find_position_for_substring(content, "pointValue.y", 0, 10, &pointValueCompletionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Failed to prepare ffi pointer extern wrapper positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, pointPtrHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "pointPtr") ||
        !hover_contains_text(hover, "Ptr<NativePoint>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Hover on pointPtr should expose the refined Ptr<NativePoint> type");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, pointValueHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "pointValue") ||
        !hover_contains_text(hover, "NativePoint")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Hover on pointValue should expose the extern wrapper result type inferred from read(NativePoint)");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, pointValueCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "x") ||
        !completion_array_contains_label(&completions, "y")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Completion on pointValue should list extern struct fields propagated through the pointer helpers");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, pointFieldUsePosition, &definitions) ||
        !location_array_contains_position(&definitions, pointFieldDeclPosition.line, pointFieldDeclPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Goto definition on pointValue.y should jump to the extern struct field declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, pointFieldUsePosition, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, pointFieldDeclPosition.line, pointFieldDeclPosition.character) ||
        !location_array_contains_position(&references, pointFieldUsePosition.line, pointFieldUsePosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "References on pointValue.y should include the extern field declaration and local usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, pointFieldUsePosition, &highlights) ||
        !highlight_array_contains_position(&highlights, pointFieldDeclPosition.line, pointFieldDeclPosition.character) ||
        !highlight_array_contains_position(&highlights, pointFieldUsePosition.line, pointFieldUsePosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Document highlights on pointValue.y should stay attached to the same extern field symbol");
        return;
    }
    ZrCore_Array_Free(state, &highlights);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, pointFieldUsePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "y") ||
        !hover_contains_text(hover, "Source: ffi extern") ||
        !hover_contains_text(hover, "Offset: 4")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Helpers Surface Extern Wrapper Types",
                  "Hover on pointValue.y should still surface the extern field metadata after pointer helper inference");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP FFI Pointer Helpers Surface Extern Wrapper Types");
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

static void test_lsp_semantic_query_unifies_local_symbol_navigation_and_hover(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "func run(seed: int): int {\n"
        "    var result = seed + 1;\n"
        "    return result;\n"
        "}\n";
    SZrLspPosition resultDeclPosition;
    SZrLspPosition resultUsePosition;
    SZrLspSemanticQuery query;
    SZrLspHover *hover = ZR_NULL;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;

    TEST_START("LSP Semantic Query Unifies Local Symbol Navigation And Hover");
    TEST_INFO("Structured semantic query",
              "The shared semantic query should resolve one local symbol target and drive hover/definition/references/highlights from the same structured result");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_local_symbol.zr", 38);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "var result = seed + 1;", 0, 4, &resultDeclPosition) ||
        !lsp_find_position_for_substring(content, "return result;", 0, 7, &resultUsePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Failed to prepare semantic query local symbol fixture");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, resultUsePosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query.symbol == ZR_NULL ||
        query.resolvedTypeInfo.resolvedTypeText == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(query.resolvedTypeInfo.resolvedTypeText), "int") != 0) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Structured semantic query should resolve the local symbol and its inferred int type");
        return;
    }

    if (!ZrLanguageServer_LspSemanticQuery_BuildHover(state, context, &query, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "result") ||
        !hover_contains_text(hover, "int")) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Semantic query hover should be produced from the same structured symbol result");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_LspSemanticQuery_AppendDefinitions(state, context, &query, &definitions) ||
        !location_array_contains_position(&definitions, resultDeclPosition.line, resultDeclPosition.character)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Semantic query definitions should include the local declaration");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_AppendReferences(state, context, &query, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, resultDeclPosition.line, resultDeclPosition.character) ||
        !location_array_contains_position(&references, resultUsePosition.line, resultUsePosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Semantic query references should include both the declaration and the usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(state, context, &query, &highlights) ||
        !highlight_array_contains_position(&highlights, resultDeclPosition.line, resultDeclPosition.character) ||
        !highlight_array_contains_position(&highlights, resultUsePosition.line, resultUsePosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Local Symbol Navigation And Hover",
                  "Semantic query highlights should include both the declaration and the usage");
        return;
    }
    ZrCore_Array_Free(state, &highlights);

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Unifies Local Symbol Navigation And Hover");
}

static void test_lsp_web_uri_local_symbol_navigation(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content = "var x = 10; var y = x;\n";
    SZrLspPosition definitionPosition;
    SZrLspPosition declarationPosition;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;

    TEST_START("LSP Web URI Local Symbol Navigation");
    TEST_INFO("Web URI support",
              "Non-file document URIs such as vscode-test-web://... should still support same-document definition, references, and highlights through the shared semantic query path");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Web URI Local Symbol Navigation",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state,
                               "vscode-test-web://mount/src/lsp_smoke.zr",
                               strlen("vscode-test-web://mount/src/lsp_smoke.zr"));
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "x = 10", 0, 0, &declarationPosition) ||
        !lsp_find_position_for_substring(content, "= x;", 0, 2, &definitionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Web URI Local Symbol Navigation",
                  "Failed to prepare the web-uri local symbol fixture");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, definitionPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions,
                                               "vscode-test-web://mount/src/lsp_smoke.zr",
                                               declarationPosition.line,
                                               declarationPosition.character,
                                               declarationPosition.line,
                                               declarationPosition.character + 1)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Web URI Local Symbol Navigation",
                  "Definition should resolve the local declaration for vscode-test-web URIs");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, definitionPosition, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, declarationPosition.line, declarationPosition.character) ||
        !location_array_contains_position(&references, definitionPosition.line, definitionPosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Web URI Local Symbol Navigation",
                  "References should include both declaration and usage for vscode-test-web URIs");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, definitionPosition, &highlights) ||
        !highlight_array_contains_position(&highlights, declarationPosition.line, declarationPosition.character) ||
        !highlight_array_contains_position(&highlights, definitionPosition.line, definitionPosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Web URI Local Symbol Navigation",
                  "Document highlights should include both declaration and usage for vscode-test-web URIs");
        return;
    }
    ZrCore_Array_Free(state, &highlights);

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Web URI Local Symbol Navigation");
}

static void test_lsp_file_uri_to_native_path_rejects_non_file_web_uri(SZrState *state) {
    SZrTestTimer timer;
    SZrString *webUri;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    TEST_START("LSP FileUriToNativePath Rejects Non-File Web Uri");
    TEST_INFO("Web URI native-path rejection",
              "Non-file URIs such as vscode-test-web://... must not be treated as native file-system paths for project refresh or metadata loading");

    webUri = ZrCore_String_Create(state,
                                  "vscode-test-web://mount/src/lsp_smoke.zr",
                                  strlen("vscode-test-web://mount/src/lsp_smoke.zr"));
    if (webUri == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP FileUriToNativePath Rejects Non-File Web Uri",
                  "Failed to create the test web URI");
        return;
    }

    if (ZrLanguageServer_Lsp_FileUriToNativePath(webUri, nativePath, sizeof(nativePath))) {
        TEST_FAIL(timer,
                  "LSP FileUriToNativePath Rejects Non-File Web Uri",
                  "Non-file web URIs should not be converted into native paths");
        return;
    }

    TEST_PASS(timer, "LSP FileUriToNativePath Rejects Non-File Web Uri");
}

static void test_lsp_semantic_query_resolves_native_import_member_source_kind(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var ffi = %import(\"zr.ffi\");\n"
        "var buffer = ffi.BufferHandle.allocate(8);\n";
    SZrLspPosition memberPosition;
    SZrLspSemanticQuery query;

    TEST_START("LSP Semantic Query Resolves Native Import Member Source Kind");
    TEST_INFO("Structured metadata provider",
              "The shared semantic query should resolve imported members through the metadata provider and preserve the native builtin source kind");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Native Import Member Source Kind",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_native_import.zr", 38);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "ffi.BufferHandle.allocate(8)", 0, 5, &memberPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Native Import Member Source Kind",
                  "Failed to prepare semantic query native import fixture");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN ||
        query.moduleName == ZR_NULL ||
        query.memberName == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Native Import Member Source Kind",
                  "Structured semantic query should resolve imported members through the native builtin metadata path");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Resolves Native Import Member Source Kind");
}

static void test_lsp_semantic_query_resolves_module_link_chain_member_navigation(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrString *consoleModuleUri = ZR_NULL;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "run() {\n"
        "    system.console.printLine(\"one\");\n"
        "    return system.console.printLine(\"two\");\n"
        "}\n";
    SZrLspPosition firstPrintLinePosition;
    SZrLspPosition secondPrintLinePosition;
    SZrLspSemanticQuery query;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;

    TEST_START("LSP Semantic Query Resolves Module-Link Chain Member Navigation");
    TEST_INFO("Structured import chain query",
              "Secondary chain segments such as system.console.printLine should resolve through the same structured metadata query used by definition, references, and document highlight");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_module_link_chain.zr", 43);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "console.printLine", 0, 8, &firstPrintLinePosition) ||
        !lsp_find_position_for_substring(content, "console.printLine", 1, 8, &secondPrintLinePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "Failed to prepare module-link chain fixture");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, firstPrintLinePosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        query.moduleName == ZR_NULL ||
        query.memberName == ZR_NULL ||
        strcmp(test_string_ptr(query.moduleName), "zr.system.console") != 0 ||
        strcmp(test_string_ptr(query.memberName), "printLine") != 0) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "Structured semantic query should resolve system.console.printLine to the linked console module member");
        return;
    }
    consoleModuleUri = query.resolvedMember.declarationUri;
    if (!query.resolvedMember.hasDeclaration || consoleModuleUri == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "Structured semantic query should expose the linked module declaration uri for printLine");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_LspSemanticQuery_AppendDefinitions(state, context, &query, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, test_string_ptr(consoleModuleUri), 0, 0, 0, 0)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "Definition on system.console.printLine should resolve to the linked native module metadata entry");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_AppendReferences(state, context, &query, ZR_TRUE, &references) ||
        !location_array_contains_uri_and_range(&references, test_string_ptr(consoleModuleUri), 0, 0, 0, 0) ||
        !location_array_contains_uri_and_range(&references,
                                               test_string_ptr(uri),
                                               firstPrintLinePosition.line,
                                               firstPrintLinePosition.character,
                                               firstPrintLinePosition.line,
                                               firstPrintLinePosition.character + 9) ||
        !location_array_contains_uri_and_range(&references,
                                               test_string_ptr(uri),
                                               secondPrintLinePosition.line,
                                               secondPrintLinePosition.character,
                                               secondPrintLinePosition.line,
                                               secondPrintLinePosition.character + 9)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "References on system.console.printLine should include the linked module entry and each secondary member usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(state, context, &query, &highlights) ||
        !highlight_array_contains_position(&highlights, firstPrintLinePosition.line, firstPrintLinePosition.character) ||
        !highlight_array_contains_position(&highlights, secondPrintLinePosition.line, secondPrintLinePosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves Module-Link Chain Member Navigation",
                  "Document highlights on system.console.printLine should include each same-document secondary member usage");
        return;
    }
    ZrCore_Array_Free(state, &highlights);

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Resolves Module-Link Chain Member Navigation");
}

static void test_lsp_semantic_query_unifies_import_target_navigation(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "run() {\n"
        "    return system.clockTicks();\n"
        "}\n";
    SZrLspPosition importLiteralPosition;
    SZrLspPosition importBindingPosition;
    SZrLspPosition importUsePosition;
    SZrLspSemanticQuery query;
    SZrArray definitions;
    SZrArray references;
    SZrArray highlights;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Semantic Query Unifies Import Target Navigation");
    TEST_INFO("Structured import target query",
              "Import literals and aliases should resolve through the shared semantic query path for hover/definition/references/highlights instead of interface-level string navigation");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_import_target.zr", 39);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "\"zr.system\"", 0, 1, &importLiteralPosition) ||
        !lsp_find_position_for_substring(content, "var system = ", 0, 4, &importBindingPosition) ||
        !lsp_find_position_for_substring(content, "system.clockTicks", 0, 0, &importUsePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "Failed to prepare semantic query import target fixture");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, importLiteralPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION ||
        query.moduleName == ZR_NULL ||
        strcmp(test_string_ptr(query.moduleName), "zr.system") != 0 ||
        query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN ||
        query.resolvedTypeInfo.valueKind != ZR_LSP_RESOLVED_VALUE_KIND_MODULE) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "Structured semantic query should resolve %import literals as imported module targets with the native builtin source kind");
        return;
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, importLiteralPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.system>") ||
        !hover_contains_text(hover, "Source: native builtin")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "Hover on an import literal should be produced through the shared imported-module metadata path");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, importLiteralPosition, &definitions) ||
        definitions.length == 0) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "Goto definition on an import literal should resolve through the shared imported-module entry target");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, importLiteralPosition, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, importLiteralPosition.line, importLiteralPosition.character) ||
        !location_array_contains_position(&references, importBindingPosition.line, importBindingPosition.character) ||
        !location_array_contains_position(&references, importUsePosition.line, importUsePosition.character)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "References on an import literal should include the literal, alias declaration, and alias receiver use through the shared query path");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, importLiteralPosition, &highlights) ||
        !highlight_array_contains_position(&highlights, importLiteralPosition.line, importLiteralPosition.character) ||
        !highlight_array_contains_position(&highlights, importBindingPosition.line, importBindingPosition.character) ||
        !highlight_array_contains_position(&highlights, importUsePosition.line, importUsePosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Unifies Import Target Navigation",
                  "Document highlights on an import literal should stay on the same imported module target graph");
        return;
    }

    ZrCore_Array_Free(state, &highlights);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Unifies Import Target Navigation");
}

static void test_lsp_native_declaration_document_renders_virtual_zr_source(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrString *documentText = ZR_NULL;
    const TZrChar *renderedText;

    TEST_START("LSP Native Declaration Document Renders Virtual ZR Source");
    TEST_INFO("Native declaration virtual document",
              "The language server should render native module descriptors into zr-decompiled virtual ZR declarations.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Native Declaration Document Renders Virtual ZR Source",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state,
                               "zr-decompiled:/zr.container.zr",
                               strlen("zr-decompiled:/zr.container.zr"));
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &documentText) ||
        documentText == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Declaration Document Renders Virtual ZR Source",
                  "Failed to resolve the zr.container decompiled declaration document");
        return;
    }

    renderedText = test_string_ptr(documentText);
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "%extern(\"zr.container\")") == ZR_NULL ||
        strstr(renderedText, "pub class Array<T>") == ZR_NULL ||
        strstr(renderedText, "pub var length: int;") == ZR_NULL ||
        strstr(renderedText, "pub @constructor") == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Declaration Document Renders Virtual ZR Source",
                  "Expected zr.container to render extern-style declarations for Array<T> including fields and constructor metadata");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Declaration Document Renders Virtual ZR Source");
}

static void test_lsp_native_console_virtual_document_prefers_descriptor_signatures(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrString *documentText = ZR_NULL;
    const TZrChar *renderedText;

    TEST_START("LSP Native Console Virtual Document Prefers Descriptor Signatures");
    TEST_INFO("Native declaration signature rendering",
              "Console virtual declarations should render descriptor/native-hint signatures instead of falling back to arity comments.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Native Console Virtual Document Prefers Descriptor Signatures",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state,
                               "zr-decompiled:/zr.system.console.zr",
                               strlen("zr-decompiled:/zr.system.console.zr"));
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &documentText) ||
        documentText == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Console Virtual Document Prefers Descriptor Signatures",
                  "Failed to resolve the zr.system.console decompiled declaration document");
        return;
    }

    renderedText = test_string_ptr(documentText);
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "%extern(\"zr.system.console\")") == ZR_NULL ||
        strstr(renderedText, "pub print(value: any): null;") == ZR_NULL ||
        strstr(renderedText, "pub printLine(value: any): null;") == ZR_NULL ||
        strstr(renderedText, "pub readLine(): string?;") == ZR_NULL ||
        strstr(renderedText, "/* arity 1 */") != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Console Virtual Document Prefers Descriptor Signatures",
                  renderedText != ZR_NULL ? renderedText : "<null virtual document>");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Console Virtual Document Prefers Descriptor Signatures");
}

static void test_lsp_native_import_definition_uses_virtual_declaration_uri(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var container = %import(\"zr.container\");\n"
        "run() {\n"
        "    return container;\n"
        "}\n";
    SZrLspPosition importLiteralPosition;
    SZrArray definitions;

    TEST_START("LSP Native Import Definition Uses Virtual Declaration URI");
    TEST_INFO("Native import goto definition",
              "Goto definition on a native import should land on the zr-decompiled virtual declaration document instead of a C source file.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Native Import Definition Uses Virtual Declaration URI",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///native_import_virtual_definition.zr", 42);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "\"zr.container\"", 0, 1, &importLiteralPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Import Definition Uses Virtual Declaration URI",
                  "Failed to prepare native import definition fixture");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, importLiteralPosition, &definitions) ||
        !location_array_contains_uri_text(&definitions, "zr-decompiled:/zr.container.zr")) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Import Definition Uses Virtual Declaration URI",
                  "Expected native import definition results to include zr-decompiled:/zr.container.zr");
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Import Definition Uses Virtual Declaration URI");
}

static void test_lsp_native_network_tcp_leaf_virtual_declaration_renders_and_import_targets_uri(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *tcpVirtualUri;
    SZrString *sourceUri;
    SZrString *documentText = ZR_NULL;
    const TZrChar *renderedText;
    const TZrChar *importContent =
        "var tcpLeaf = %import(\"zr.network.tcp\");\n"
        "run() {\n"
        "    return tcpLeaf;\n"
        "}\n";
    SZrLspPosition tcpImportLiteralPosition;
    SZrArray definitions;

    TEST_START("LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI");
    TEST_INFO("zr.network.tcp leaf module",
              "Leaf module zr.network.tcp should render a zr-decompiled virtual document and %import should navigate to the same URI (moduleLinks / dotted native name).");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI",
                  "Failed to create LSP context");
        return;
    }

    tcpVirtualUri = ZrCore_String_Create(state,
                                         "zr-decompiled:/zr.network.tcp.zr",
                                         strlen("zr-decompiled:/zr.network.tcp.zr"));
    if (tcpVirtualUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, tcpVirtualUri, &documentText) ||
        documentText == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI",
                  "Failed to resolve the zr.network.tcp decompiled declaration document");
        return;
    }

    renderedText = test_string_ptr(documentText);
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "%extern(\"zr.network.tcp\")") == ZR_NULL ||
        strstr(renderedText, "listen") == ZR_NULL ||
        strstr(renderedText, "TcpListener") == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI",
                  "Expected zr.network.tcp virtual text to include extern header, listen, and TcpListener metadata");
        return;
    }

    sourceUri = ZrCore_String_Create(state, "file:///native_import_zr_network_tcp_leaf.zr", 44);
    if (sourceUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, sourceUri, importContent, strlen(importContent), 1) ||
        !lsp_find_position_for_substring(importContent, "\"zr.network.tcp\"", 0, 1, &tcpImportLiteralPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI",
                  "Failed to prepare zr.network.tcp import definition fixture");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, sourceUri, tcpImportLiteralPosition, &definitions) ||
        !location_array_contains_uri_text(&definitions, "zr-decompiled:/zr.network.tcp.zr")) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI",
                  "Expected goto definition on %import(\"zr.network.tcp\") to include zr-decompiled:/zr.network.tcp.zr");
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Network TCP Leaf Virtual Declaration Renders And Import Targets URI");
}

static void test_lsp_auto_registers_linked_native_libraries_for_import_metadata(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var task = %import(\"zr.task\");\n"
        "var thread = %import(\"zr.thread\");\n"
        "run() {\n"
        "    var pending = task.spawn;\n"
        "    return thread.spawnThread;\n"
        "}\n";
    SZrLspPosition taskImportPosition;
    SZrLspPosition threadImportPosition;
    SZrLspHover *hover = ZR_NULL;
    SZrArray definitions;
    SZrString *taskDeclarationUri = ZR_NULL;
    SZrString *threadDeclarationUri = ZR_NULL;
    SZrString *documentText = ZR_NULL;
    const TZrChar *renderedText;

    TEST_START("LSP Auto Registers Linked Native Libraries For Import Metadata");
    TEST_INFO("Linked native library imports",
              "The LSP context should auto-register every linked builtin library so zr.task/zr.thread imports immediately surface native metadata and completions.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///native_import_all_libraries.zr", 38);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "\"zr.task\"", 0, 1, &taskImportPosition) ||
        !lsp_find_position_for_substring(content, "\"zr.thread\"", 0, 1, &threadImportPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Failed to prepare linked native-library import metadata fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, taskImportPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.task>") ||
        !hover_contains_text(hover, "Source: native builtin")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Hover on %import(\"zr.task\") should resolve through auto-registered native builtin metadata");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, threadImportPosition, &definitions) ||
        !location_array_contains_uri_text(&definitions, "zr-decompiled:/zr.thread.zr")) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Goto definition on %import(\"zr.thread\") should resolve through the auto-registered zr-decompiled virtual document");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    taskDeclarationUri = ZrCore_String_Create(state,
                                              "zr-decompiled:/zr.task.zr",
                                              strlen("zr-decompiled:/zr.task.zr"));
    threadDeclarationUri = ZrCore_String_Create(state,
                                                "zr-decompiled:/zr.thread.zr",
                                                strlen("zr-decompiled:/zr.thread.zr"));
    if (taskDeclarationUri == ZR_NULL || threadDeclarationUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Failed to allocate the zr-decompiled declaration URIs for linked native modules");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, taskDeclarationUri, &documentText) ||
        documentText == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Failed to render the auto-registered zr.task declaration document");
        return;
    }
    renderedText = test_string_ptr(documentText);
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "%extern(\"zr.task\")") == ZR_NULL ||
        strstr(renderedText, "pub __createTaskRunner(") == ZR_NULL ||
        strstr(renderedText, "pub interface IScheduler") == ZR_NULL ||
        strstr(renderedText, "pub class TaskRunner<T>") == ZR_NULL ||
        strstr(renderedText, "pub class Task<T>") == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "The auto-registered zr.task decompiled document should expose the built-in scheduler/task metadata registered by the runtime descriptor");
        return;
    }

    documentText = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, threadDeclarationUri, &documentText) ||
        documentText == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "Failed to render the auto-registered zr.thread declaration document");
        return;
    }
    renderedText = test_string_ptr(documentText);
    if (renderedText == ZR_NULL ||
        strstr(renderedText, "%extern(\"zr.thread\")") == ZR_NULL ||
        strstr(renderedText, "pub spawnThread(") == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Registers Linked Native Libraries For Import Metadata",
                  "The auto-registered zr.thread decompiled document should expose spawnThread declarations");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Auto Registers Linked Native Libraries For Import Metadata");
}

static void test_lsp_project_modules_summarize_project_native_and_binary_modules(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *projectUri = ZR_NULL;
    SZrArray modules;
    TZrBool foundProjectModule = ZR_FALSE;
    TZrBool foundBinaryModule = ZR_FALSE;
    TZrBool foundNativeModule = ZR_FALSE;

    TEST_START("LSP Project Modules Summarize Project Native And Binary Modules");
    TEST_INFO("Project module summary",
              "The language server should summarize selected-project source, binary and native modules through a single request.");

    if (!build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/aot_module_graph_pipeline.zrp",
                                   projectPath,
                                   sizeof(projectPath))) {
        TEST_FAIL(timer,
                  "LSP Project Modules Summarize Project Native And Binary Modules",
                  "Failed to build the project fixture path");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    projectUri = create_file_uri_from_native_path(state, projectPath);
    if (context == ZR_NULL || projectUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Modules Summarize Project Native And Binary Modules",
                  "Failed to create the LSP context or project URI");
        return;
    }

    ZrCore_Array_Init(state, &modules, sizeof(SZrLspProjectModuleSummary *), 8);
    if (!ZrLanguageServer_Lsp_GetProjectModules(state, context, projectUri, &modules)) {
        ZrCore_Array_Free(state, &modules);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Modules Summarize Project Native And Binary Modules",
                  "The project modules request failed for the fixture project");
        return;
    }

    for (TZrSize index = 0; index < modules.length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(&modules, index);
        const TZrChar *moduleNameText;
        const TZrChar *navigationUriText;

        if (summaryPtr == ZR_NULL || *summaryPtr == ZR_NULL) {
            continue;
        }

        moduleNameText = test_string_ptr((*summaryPtr)->moduleName);
        navigationUriText = test_string_ptr((*summaryPtr)->navigationUri);
        if (moduleNameText != ZR_NULL &&
            strcmp(moduleNameText, "main") == 0 &&
            (*summaryPtr)->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE) {
            foundProjectModule = ZR_TRUE;
        } else if (moduleNameText != ZR_NULL &&
                   strcmp(moduleNameText, "graph_binary_stage") == 0 &&
                   (*summaryPtr)->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
                   navigationUriText != ZR_NULL &&
                   strstr(navigationUriText, "graph_binary_stage.zro") != ZR_NULL) {
            foundBinaryModule = ZR_TRUE;
        } else if (moduleNameText != ZR_NULL &&
                   strcmp(moduleNameText, "zr.system") == 0 &&
                   (*summaryPtr)->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN &&
                   navigationUriText != ZR_NULL &&
                   strcmp(navigationUriText, "zr-decompiled:/zr.system.zr") == 0) {
            foundNativeModule = ZR_TRUE;
        }
    }

    ZrLanguageServer_Lsp_FreeProjectModules(state, &modules);
    ZrLanguageServer_LspContext_Free(state, context);

    if (!foundProjectModule || !foundBinaryModule || !foundNativeModule) {
        TEST_FAIL(timer,
                  "LSP Project Modules Summarize Project Native And Binary Modules",
                  "Expected projectModules to include at least one project source module, one binary metadata module, and the zr.system native module");
        return;
    }

    TEST_PASS(timer, "LSP Project Modules Summarize Project Native And Binary Modules");
}

static void test_lsp_project_source_bootstrap_indexes_open_file_symbols_and_modules(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *sourceContent = ZR_NULL;
    TZrSize sourceLength = 0;
    SZrString *sourceUri = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *query = ZR_NULL;
    SZrArray symbols;
    SZrArray modules;
    SZrLspSymbolInformation *symbolInfo = ZR_NULL;
    TZrBool foundOpenSourceModule = ZR_FALSE;

    TEST_START("LSP Project Source Bootstrap Indexes Open File Symbols And Modules");
    TEST_INFO("Project source bootstrap",
              "Opening a project-owned .zr file through a VS Code style encoded URI should upgrade the project to semantic load, index the file into workspace symbols, and expose it through projectModules.");

    if (!build_fixture_native_path("tests/fixtures/projects/import_basic/import_basic.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_basic/src/greet.zr",
                                   sourcePath,
                                   sizeof(sourcePath))) {
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to build the project or source fixture paths");
        return;
    }

    sourceContent = read_fixture_text_file(sourcePath, &sourceLength);
    context = ZrLanguageServer_LspContext_New(state);
    sourceUri = create_percent_encoded_file_uri_from_native_path(state, sourcePath);
    projectUri = create_percent_encoded_file_uri_from_native_path(state, projectPath);
    query = ZrCore_String_Create(state, "greet", 5);
    if (sourceContent == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to read the encoded bootstrap source fixture");
        return;
    }
    if (context == ZR_NULL) {
        free(sourceContent);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to create the LSP context for the encoded bootstrap fixture");
        return;
    }
    if (sourceUri == ZR_NULL) {
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to create the percent-encoded source URI for the bootstrap fixture");
        return;
    }
    if (projectUri == ZR_NULL) {
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to create the percent-encoded project URI for the bootstrap fixture");
        return;
    }
    if (query == ZR_NULL) {
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to allocate the workspace-symbol query string for the bootstrap fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, sourceUri, sourceContent, sourceLength, 1)) {
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Failed to open the project-owned source document");
        return;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), 8);
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state, context, query, &symbols)) {
        ZrCore_Array_Free(state, &symbols);
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "workspace/symbol failed for the bootstrapped source file");
        return;
    }

    symbolInfo = find_symbol_information_by_name(&symbols, "greet");
    if (symbolInfo == ZR_NULL || symbolInfo->location.uri == ZR_NULL ||
        !ZrCore_String_Equal(symbolInfo->location.uri, sourceUri)) {
        ZrCore_Array_Free(state, &symbols);
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Expected workspace symbols to include greet from the encoded open project source file");
        return;
    }
    ZrCore_Array_Free(state, &symbols);

    ZrCore_Array_Init(state, &modules, sizeof(SZrLspProjectModuleSummary *), 8);
    if (!ZrLanguageServer_Lsp_GetProjectModules(state, context, projectUri, &modules)) {
        ZrLanguageServer_Lsp_FreeProjectModules(state, &modules);
        free(sourceContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "projectModules failed for the encoded selected project URI");
        return;
    }

    for (TZrSize index = 0; index < modules.length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(&modules, index);
        const TZrChar *moduleNameText;
        const TZrChar *navigationUriText;

        if (summaryPtr == ZR_NULL || *summaryPtr == ZR_NULL) {
            continue;
        }

        moduleNameText = test_string_ptr((*summaryPtr)->moduleName);
        navigationUriText = test_string_ptr((*summaryPtr)->navigationUri);
        if (moduleNameText != ZR_NULL &&
            strcmp(moduleNameText, "greet") == 0 &&
            (*summaryPtr)->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE &&
            navigationUriText != ZR_NULL &&
            strstr(navigationUriText, "greet.zr") != ZR_NULL) {
            foundOpenSourceModule = ZR_TRUE;
            break;
        }
    }

    ZrLanguageServer_Lsp_FreeProjectModules(state, &modules);
    free(sourceContent);
    ZrLanguageServer_LspContext_Free(state, context);

    if (!foundOpenSourceModule) {
        TEST_FAIL(timer,
                  "LSP Project Source Bootstrap Indexes Open File Symbols And Modules",
                  "Expected projectModules to reuse the semantic project and include the opened greet source file");
        return;
    }

    TEST_PASS(timer, "LSP Project Source Bootstrap Indexes Open File Symbols And Modules");
}

static void test_lsp_semantic_query_collects_receiver_completion_items(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var math = %import(\"zr.math\");\n"
        "run() {\n"
        "    var vector = $math.Vector3(4.0, 5.0, 6.0);\n"
        "    return vector.;\n"
        "}\n";
    SZrLspPosition completionPosition;
    SZrArray completions;

    TEST_START("LSP Semantic Query Collects Receiver Completion Items");
    TEST_INFO("Structured completion query",
              "Completion should be collectable through the shared semantic query path for local receivers such as value-constructor-backed structs");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Receiver Completion Items",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_completion_receiver.zr", 45);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "return vector.", 0, 14, &completionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Receiver Completion Items",
                  "Failed to prepare semantic query completion fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(state,
                                                                  context,
                                                                  uri,
                                                                  completionPosition,
                                                                  &completions) ||
        !completion_array_contains_label(&completions, "x") ||
        !completion_array_contains_label(&completions, "y") ||
        !completion_array_contains_label(&completions, "z")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Receiver Completion Items",
                  "Structured completion query should surface Vector3 field completions through the unified semantic query path");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Collects Receiver Completion Items");
}

static void test_lsp_semantic_query_collects_import_module_completion_items(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "run() {\n"
        "    return system.console;\n"
        "}\n";
    SZrLspPosition completionPosition;
    SZrArray completions;

    TEST_START("LSP Semantic Query Collects Import Module Completion Items");
    TEST_INFO("Structured import completion query",
              "Completion on imported module members should come from the shared semantic query/module metadata path instead of the legacy string-scanning import completion helper");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Import Module Completion Items",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_completion_import_module.zr", 50);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "console", 0, 0, &completionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Import Module Completion Items",
                  "Failed to prepare semantic query import completion fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(state,
                                                                  context,
                                                                  uri,
                                                                  completionPosition,
                                                                  &completions) ||
        !completion_array_contains_label(&completions, "console")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Import Module Completion Items",
                  "Structured semantic query completion should surface imported module members without the legacy import string scanner");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Collects Import Module Completion Items");
}

static void test_lsp_semantic_query_collects_import_chain_completion_items(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "run() {\n"
        "    return system.console.;\n"
        "}\n";
    SZrLspPosition completionPosition;
    SZrArray completions;

    TEST_START("LSP Semantic Query Collects Import Chain Completion Items");
    TEST_INFO("Structured import-chain completion query",
              "Completion after system.console. should come from the same import-chain metadata resolver instead of falling back to generic receiver heuristics");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Import Chain Completion Items",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_completion_import_chain.zr", 49);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "console.", 0, 8, &completionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Import Chain Completion Items",
                  "Failed to prepare semantic query import-chain completion fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (!ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(state,
                                                                  context,
                                                                  uri,
                                                                  completionPosition,
                                                                  &completions) ||
        !completion_array_contains_label(&completions, "printLine")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Collects Import Chain Completion Items",
                  "Structured semantic query completion should surface linked module members after system.console.");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Collects Import Chain Completion Items");
}

static void test_lsp_semantic_tokens_cover_import_chain_members(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "run() {\n"
        "    system.console.printLine(\"x\");\n"
        "}\n";
    SZrLspPosition consolePosition;
    SZrLspPosition printLinePosition;
    SZrArray tokens;

    TEST_START("LSP Semantic Tokens Cover Import Chain Members");
    TEST_INFO("Structured import-chain semantic tokens",
              "Semantic tokens should classify linked submodules and their terminal members through the shared import-chain metadata resolver");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Tokens Cover Import Chain Members",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_tokens_import_chain.zr", 39);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "console.printLine", 0, 0, &consolePosition) ||
        !lsp_find_position_for_substring(content, "console.printLine", 0, 8, &printLinePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Tokens Cover Import Chain Members",
                  "Failed to prepare semantic token import-chain fixture");
        return;
    }

    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 32);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &tokens) ||
        !semantic_tokens_contain(&tokens, consolePosition.line, consolePosition.character, 7, "namespace") ||
        !semantic_tokens_contain(&tokens, printLinePosition.line, printLinePosition.character, 9, "method")) {
        ZrCore_Array_Free(state, &tokens);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Tokens Cover Import Chain Members",
                  "Semantic tokens should classify console as namespace and printLine as method through the shared chain resolver");
        return;
    }

    ZrCore_Array_Free(state, &tokens);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Tokens Cover Import Chain Members");
}

static void test_lsp_semantic_query_builds_native_receiver_member_hover(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    const TZrChar *content =
        "var math = %import(\"zr.math\");\n"
        "runImpl() {\n"
        "    return $math.Vector3(4.0, 5.0, 6.0).y;\n"
        "}\n";
    SZrLspPosition hoverPosition;
    SZrLspSemanticQuery query;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Semantic Query Builds Native Receiver Member Hover");
    TEST_INFO("Structured native receiver hover",
              "Hover on $module.Type(...).member should resolve through the shared semantic query instead of interface markdown fallback");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Builds Native Receiver Member Hover",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///semantic_query_native_receiver_hover.zr", 46);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, ").y", 0, 2, &hoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Builds Native Receiver Member Hover",
                  "Failed to prepare native receiver hover fixture");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, hoverPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        !ZrLanguageServer_LspSemanticQuery_BuildHover(state, context, &query, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "field") ||
        !hover_contains_text(hover, "y") ||
        !hover_contains_text(hover, "float") ||
        !hover_contains_text(hover, "Vector3")) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Builds Native Receiver Member Hover",
                  "Structured semantic query should resolve native receiver fields without interface-only markdown fallback");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Builds Native Receiver Member Hover");
}

static void test_lsp_semantic_query_resolves_external_metadata_declaration_targets(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedBinaryMetadataFixture fixture;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *binaryUri = ZR_NULL;
    SZrLspPosition binaryEntryPosition = {0, 0};
    SZrLspSemanticQuery query;

    TEST_START("LSP Semantic Query Resolves External Metadata Declaration Targets");
    TEST_INFO("Structured external metadata query",
              "The shared semantic query should recognize .zro declaration entries directly instead of relying on interface-level project fallback code");

    if (!prepare_generated_binary_metadata_fixture(state,
                                                   "interface_external_metadata_declaration_target",
                                                   &fixture)) {
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves External Metadata Declaration Targets",
                  "Failed to prepare generated binary metadata fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves External Metadata Declaration Targets",
                  "Failed to load binary metadata fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    binaryUri = create_file_uri_from_native_path(state, fixture.binaryPath);
    if (mainUri == ZR_NULL || binaryUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves External Metadata Declaration Targets",
                  "Failed to open binary metadata fixture source");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, binaryUri, binaryEntryPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION ||
        query.moduleName == ZR_NULL ||
        strcmp(test_string_ptr(query.moduleName), "graph_binary_stage") != 0 ||
        strcmp(test_string_ptr(query.uri), test_string_ptr(binaryUri)) != 0 ||
        query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query Resolves External Metadata Declaration Targets",
                  "Structured semantic query should resolve .zro module entries as first-class external metadata declarations");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query Resolves External Metadata Declaration Targets");
}

static void test_lsp_semantic_query_external_metadata_references_and_highlights(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedBinaryMetadataFixture fixture;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *binaryUri = ZR_NULL;
    SZrLspPosition binaryEntryPosition = {0, 0};
    SZrLspPosition importLiteralPosition = {0, 0};
    SZrLspPosition importBindingPosition = {0, 0};
    SZrLspSemanticQuery query;
    SZrArray references;
    SZrArray highlights;

    TEST_START("LSP Semantic Query External Metadata References And Highlights");
    TEST_INFO("Structured external metadata references",
              "External metadata semantic queries should drive references and same-document highlights without routing back through legacy project position fallback");

    if (!prepare_generated_binary_metadata_fixture(state,
                                                   "interface_external_metadata_references",
                                                   &fixture)) {
        TEST_FAIL(timer,
                  "LSP Semantic Query External Metadata References And Highlights",
                  "Failed to prepare generated binary metadata fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query External Metadata References And Highlights",
                  "Failed to load binary metadata fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    binaryUri = create_file_uri_from_native_path(state, fixture.binaryPath);
    if (mainUri == ZR_NULL || binaryUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "\"graph_binary_stage\"", 0, 1, &importLiteralPosition) ||
        !lsp_find_position_for_substring(mainContent, "binaryStage", 0, 0, &importBindingPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query External Metadata References And Highlights",
                  "Failed to prepare semantic-query external metadata fixture");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, binaryUri, binaryEntryPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION ||
        !ZrLanguageServer_LspSemanticQuery_AppendReferences(state, context, &query, ZR_TRUE, &references) ||
        !location_array_contains_position(&references, importLiteralPosition.line, importLiteralPosition.character) ||
        !location_array_contains_position(&references, importBindingPosition.line, importBindingPosition.character) ||
        !ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(state, context, &query, &highlights) ||
        !highlight_array_contains_position(&highlights, binaryEntryPosition.line, binaryEntryPosition.character)) {
        ZrCore_Array_Free(state, &highlights);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Query External Metadata References And Highlights",
                  "Structured external metadata queries should surface import references and same-document highlights through the shared query path");
        return;
    }

    ZrCore_Array_Free(state, &highlights);
    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Query External Metadata References And Highlights");
}

static void test_lsp_top_level_script_return_semantics_match_compiler(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *implicitUri = ZR_NULL;
    SZrString *explicitUri = ZR_NULL;
    const TZrChar *implicitContent =
        "var checksum = 1;\n"
        "checksum = checksum + 1;\n";
    const TZrChar *explicitContent =
        "var checksum = 1;\n"
        "checksum = checksum + 1;\n"
        "return checksum;\n";
    SZrArray diagnostics;
    SZrLspPosition returnValuePosition;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Top Level Script Return Semantics Match Compiler");
    TEST_INFO("Top-level script returns",
              "Module top-level code should allow both implicit null returns and explicit return expressions without function-return provability diagnostics.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Top Level Script Return Semantics Match Compiler",
                  "Failed to create LSP context");
        return;
    }

    implicitUri = ZrCore_String_Create(state,
                                       "file:///top_level_implicit_return.zr",
                                       strlen("file:///top_level_implicit_return.zr"));
    explicitUri = ZrCore_String_Create(state,
                                       "file:///top_level_explicit_return.zr",
                                       strlen("file:///top_level_explicit_return.zr"));
    if (implicitUri == ZR_NULL ||
        explicitUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state,
                                             context,
                                             implicitUri,
                                             implicitContent,
                                             strlen(implicitContent),
                                             1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state,
                                             context,
                                             explicitUri,
                                             explicitContent,
                                             strlen(explicitContent),
                                             1) ||
        !lsp_find_position_for_substring(explicitContent, "return checksum;", 0, 7, &returnValuePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Top Level Script Return Semantics Match Compiler",
                  "Failed to prepare top-level script return fixtures");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, implicitUri, &diagnostics) ||
        diagnostic_array_contains_message(&diagnostics, "return type not provable")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Top Level Script Return Semantics Match Compiler",
                  "Top-level scripts without an explicit return should not be forced through function-return provability checks");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, explicitUri, &diagnostics) ||
        diagnostic_array_contains_message(&diagnostics, "return type not provable")) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Top Level Script Return Semantics Match Compiler",
                  "Explicit top-level return expressions should be accepted as script entry returns");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, explicitUri, returnValuePosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "checksum") ||
        !hover_contains_text(hover, "int")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Top Level Script Return Semantics Match Compiler",
                  "Top-level return expressions should still surface the resolved expression type in hover");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Top Level Script Return Semantics Match Compiler");
}

static void test_lsp_hover_prefers_nearest_shadowed_foreach_binding(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    const TZrChar *content =
        "var container = %import(\"zr.container\");\n"
        "probe(): void {\n"
        "    var ints = new container.Array<int>();\n"
        "    var texts = new container.Array<string>();\n"
        "    for (var item in ints) {\n"
        "        item;\n"
        "    }\n"
        "    for (var item in texts) {\n"
        "        item;\n"
        "    }\n"
        "}\n";
    SZrLspPosition firstItemPosition;
    SZrLspPosition secondItemPosition;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *hoverText = ZR_NULL;
    TZrChar reason[512];

    TEST_START("LSP Hover Prefers Nearest Shadowed Foreach Binding");
    TEST_INFO("Shadowed foreach locals",
              "Repeated foreach item names should resolve to the nearest in-scope binding instead of drifting to an earlier loop variable.");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Hover Prefers Nearest Shadowed Foreach Binding",
                  "Failed to create LSP context");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///shadowed_foreach_hover.zr", 34);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "        item;", 0, 8, &firstItemPosition) ||
        !lsp_find_position_for_substring(content, "        item;", 1, 8, &secondItemPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Hover Prefers Nearest Shadowed Foreach Binding",
                  "Failed to prepare the shadowed foreach hover fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, firstItemPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "item") ||
        !hover_contains_text(hover, "int") ||
        hover_contains_text(hover, "string")) {
        hoverText = hover_first_text(hover);
        snprintf(reason,
                 sizeof(reason),
                 "First foreach item hover should resolve to int, actual=%s",
                 hoverText != ZR_NULL ? hoverText : "<null>");
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Prefers Nearest Shadowed Foreach Binding", reason);
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, secondItemPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "item") ||
        !hover_contains_text(hover, "string") ||
        hover_contains_text(hover, "Resolved Type: int") ||
        hover_contains_text(hover, "cannot infer exact type") ||
        hover_contains_text(hover, "object")) {
        hoverText = hover_first_text(hover);
        snprintf(reason,
                 sizeof(reason),
                 "Second foreach item hover should resolve to string, actual=%s",
                 hoverText != ZR_NULL ? hoverText : "<null>");
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Hover Prefers Nearest Shadowed Foreach Binding", reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Hover Prefers Nearest Shadowed Foreach Binding");
}

static void test_lsp_container_matrix_project_infers_bucket_and_foreach_types(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *projectUri = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrLspPosition bucketHoverPosition;
    SZrLspPosition oddLoHoverPosition;
    SZrLspPosition itemHoverPosition;
    SZrArray diagnostics;
    SZrLspHover *hover = ZR_NULL;
    TZrChar projectPath[1024];
    TZrChar mainPath[1024];
    TZrChar *projectContent = ZR_NULL;
    TZrChar *mainContent = ZR_NULL;
    TZrSize projectLength = 0;
    TZrSize mainLength = 0;

    TEST_START("LSP Container Matrix Project Infers Bucket And Foreach Types");
    TEST_INFO("Project generics / foreach inference",
              "Computed map access, nullable bucket locals, foreach item bindings, and all-path returns should resolve cleanly in the container_matrix fixture.");

    if (!build_fixture_native_path("tests/fixtures/projects/container_matrix/container_matrix.zrp",
                                   projectPath,
                                   sizeof(projectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/container_matrix/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath))) {
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Failed to build container_matrix fixture paths");
        return;
    }

    projectContent = read_fixture_text_file(projectPath, &projectLength);
    mainContent = read_fixture_text_file(mainPath, &mainLength);
    if (projectContent == ZR_NULL || mainContent == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Failed to read container_matrix fixture content");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        free(projectContent);
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Failed to create LSP context");
        return;
    }

    projectUri = create_file_uri_from_native_path(state, projectPath);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    if (projectUri == ZR_NULL ||
        mainUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, projectUri, projectContent, projectLength, 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "var bucket = buckets[label];", 0, 4, &bucketHoverPosition) ||
        !lsp_find_position_for_substring(mainContent, "var oddLo: Array<int> = buckets[\"odd_lo\"];", 0, 4, &oddLoHoverPosition) ||
        !lsp_find_position_for_substring(mainContent, "<int> item", 1, 6, &itemHoverPosition)) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Failed to open container_matrix fixture or compute hover positions");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 8);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, mainUri, &diagnostics) ||
        diagnostic_array_contains_message(&diagnostics, "return type not provable") ||
        diagnostic_array_contains_message(&diagnostics, "cannot infer exact type")) {
        ZrCore_Array_Free(state, &diagnostics);
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "container_matrix should not report return-provability or exact-type failures for labelFor, bucket access, or foreach item bindings");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, bucketHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "bucket") ||
        !hover_contains_text(hover, "Array<int>") ||
        hover_contains_text(hover, "object") ||
        hover_contains_text(hover, "cannot infer exact type")) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Hover on the computed bucket local should preserve the closed Array<int> type");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, oddLoHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "oddLo") ||
        !hover_contains_text(hover, "Array<int>")) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Hover on bucket extraction into oddLo should resolve the specialized Array<int> type");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, itemHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "item") ||
        !hover_contains_text(hover, "int") ||
        hover_contains_text(hover, "object") ||
        hover_contains_text(hover, "cannot infer exact type")) {
        free(projectContent);
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Matrix Project Infers Bucket And Foreach Types",
                  "Hover inside the oddLo foreach loop should bind item to the concrete int element type");
        return;
    }

    free(projectContent);
    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Container Matrix Project Infers Bucket And Foreach Types");
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

    test_lsp_incomplete_edit_preserves_prior_semantic_snapshot(state);
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

    test_lsp_native_network_tcp_leaf_virtual_declaration_renders_and_import_targets_uri(state);
    TEST_DIVIDER();

    test_lsp_project_workspace_symbols_include_imported_exports(state);
    TEST_DIVIDER();

    test_lsp_project_encoded_uri_builtin_import_refresh_does_not_crash(state);
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

    test_lsp_callable_assignment_surfaces_exact_signature_and_parameter_scope(state);
    TEST_DIVIDER();

    test_lsp_closed_generic_type_display_and_definition(state);
    TEST_DIVIDER();

    test_lsp_signature_help_displays_closed_generic_instantiation(state);
    TEST_DIVIDER();

    test_lsp_inlay_hints_surface_exact_inferred_types(state);
    TEST_DIVIDER();

    test_lsp_hover_and_completion_surface_explicit_exact_type_failures(state);
    TEST_DIVIDER();

    test_lsp_signature_help_and_completion_surface_exact_unannotated_return_type(state);
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

    test_lsp_rich_hover_structures_meta_sections(state);
    TEST_DIVIDER();

    test_lsp_extern_function_navigation_and_signature_help(state);
    TEST_DIVIDER();

    test_lsp_extern_type_symbols_surface_hover_and_definition(state);
    TEST_DIVIDER();

    test_lsp_extern_layout_hover_surfaces_ffi_metadata(state);
    TEST_DIVIDER();

    test_lsp_ffi_pointer_helpers_surface_extern_wrapper_types(state);
    TEST_DIVIDER();

    test_lsp_completion_lists_directives_and_meta_methods(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_unifies_local_symbol_navigation_and_hover(state);
    TEST_DIVIDER();

    test_lsp_web_uri_local_symbol_navigation(state);
    TEST_DIVIDER();

    test_lsp_file_uri_to_native_path_rejects_non_file_web_uri(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_resolves_native_import_member_source_kind(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_resolves_module_link_chain_member_navigation(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_unifies_import_target_navigation(state);
    TEST_DIVIDER();

    test_lsp_native_declaration_document_renders_virtual_zr_source(state);
    TEST_DIVIDER();

    test_lsp_native_console_virtual_document_prefers_descriptor_signatures(state);
    TEST_DIVIDER();

    test_lsp_native_import_definition_uses_virtual_declaration_uri(state);
    TEST_DIVIDER();

    test_lsp_auto_registers_linked_native_libraries_for_import_metadata(state);
    TEST_DIVIDER();

    test_lsp_project_modules_summarize_project_native_and_binary_modules(state);
    TEST_DIVIDER();

    test_lsp_project_source_bootstrap_indexes_open_file_symbols_and_modules(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_collects_receiver_completion_items(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_collects_import_module_completion_items(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_collects_import_chain_completion_items(state);
    TEST_DIVIDER();

    test_lsp_semantic_tokens_cover_import_chain_members(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_builds_native_receiver_member_hover(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_resolves_external_metadata_declaration_targets(state);
    TEST_DIVIDER();

    test_lsp_semantic_query_external_metadata_references_and_highlights(state);
    TEST_DIVIDER();

    test_lsp_top_level_script_return_semantics_match_compiler(state);
    TEST_DIVIDER();

    test_lsp_hover_prefers_nearest_shadowed_foreach_binding(state);
    TEST_DIVIDER();

    test_lsp_container_matrix_project_infers_bucket_and_foreach_types(state);
    TEST_DIVIDER();

    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All LSP Interface Tests Completed\n");
    printf("==========\n");
    
    return 0;
}
