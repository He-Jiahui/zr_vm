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
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"

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

static const SZrSemanticOverloadSetRecord *find_overload_set_record(SZrSemanticContext *context,
                                                                 const char *name) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(&context->overloadSets, i);
        if (record != ZR_NULL && record->name != ZR_NULL) {
            TZrNativeString nativeName = ZrCore_String_GetNativeStringShort(record->name);
            if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
                return record;
            }
        }
    }

    return ZR_NULL;
}

static TZrBool cleanup_plan_targets_symbol(const SZrSemanticContext *context,
                                         TZrSymbolId symbolId) {
    TZrSize i;

    if (context == ZR_NULL || symbolId == 0) {
        return ZR_FALSE;
    }

    for (i = 0; i < context->cleanupPlan.length; i++) {
        const SZrDeterministicCleanupStep *step =
            (const SZrDeterministicCleanupStep *)ZrCore_Array_Get((SZrArray *)&context->cleanupPlan, i);
        if (step != ZR_NULL && step->symbolId == symbolId) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrSymbol *lookup_symbol_any_scope_by_type(SZrState *state,
                                                  SZrSymbolTable *table,
                                                  SZrString *name,
                                                  EZrSymbolType expectedType) {
    if (state == ZR_NULL || table == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr =
                (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr != ZR_NULL &&
                *symbolPtr != ZR_NULL &&
                (*symbolPtr)->type == expectedType &&
                ZrCore_String_Equal((*symbolPtr)->name, name)) {
                return *symbolPtr;
            }
        }
    }

    return ZR_NULL;
}

static TZrBool has_diagnostic_code(SZrSemanticAnalyzer *analyzer, const char *code) {
    TZrSize i;

    if (analyzer == ZR_NULL || code == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr == ZR_NULL || *diagPtr == ZR_NULL || (*diagPtr)->code == ZR_NULL) {
            continue;
        }

        if (strcmp(ZrCore_String_GetNativeStringShort((*diagPtr)->code), code) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const char *completion_detail_string(SZrCompletionItem *item) {
    if (item == ZR_NULL || item->detail == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(item->detail);
}

static const char *hover_contents_string(SZrHoverInfo *info) {
    if (info == ZR_NULL || info->contents == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(info->contents);
}

static SZrFileRange file_range_for_nth_substring(const char *content,
                                                 const char *needle,
                                                 TZrSize occurrence,
                                                 TZrBool useEnd) {
    const char *cursor = content;
    const char *found = ZR_NULL;
    TZrSize remaining = occurrence;
    TZrSize offset = 0;
    SZrFilePosition position = ZrParser_FilePosition_Create(0, 1, 1);

    while (cursor != ZR_NULL && *cursor != '\0') {
        found = strstr(cursor, needle);
        if (found == ZR_NULL) {
            break;
        }
        if (remaining == 0) {
            if (useEnd) {
                found += strlen(needle);
            }
            offset = (TZrSize)(found - content);
            break;
        }
        remaining--;
        cursor = found + 1;
    }

    for (TZrSize index = 0; index < offset && content[index] != '\0'; index++) {
        position.offset++;
        if (content[index] == '\n') {
            position.line++;
            position.column = 1;
        } else {
            position.column++;
        }
    }

    return ZrParser_FileRange_Create(position, position, ZR_NULL);
}

static TZrBool has_completion_label(SZrArray *completions, const char *label) {
    TZrSize i;

    if (completions == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < completions->length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(completions, i);
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        if (strcmp(ZrCore_String_GetNativeString((*itemPtr)->label), label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrSize count_diagnostics_with_code(SZrSemanticAnalyzer *analyzer, const char *code) {
    TZrSize i;
    TZrSize count = 0;

    if (analyzer == ZR_NULL || code == ZR_NULL) {
        return 0;
    }

    for (i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr == ZR_NULL || *diagPtr == ZR_NULL || (*diagPtr)->code == ZR_NULL) {
            continue;
        }

        if (strcmp(ZrCore_String_GetNativeStringShort((*diagPtr)->code), code) == 0) {
            count++;
        }
    }

    return count;
}

static SZrDiagnostic *find_diagnostic_by_code_and_line(SZrSemanticAnalyzer *analyzer,
                                                       const char *code,
                                                       TZrInt32 line) {
    TZrSize i;

    if (analyzer == ZR_NULL || code == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr == ZR_NULL || *diagPtr == ZR_NULL || (*diagPtr)->code == ZR_NULL) {
            continue;
        }

        if (strcmp(ZrCore_String_GetNativeStringShort((*diagPtr)->code), code) == 0 &&
            (*diagPtr)->location.start.line == line) {
            return *diagPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool diagnostic_message_contains(SZrDiagnostic *diagnostic, const char *fragment) {
    const char *messageText;

    if (diagnostic == ZR_NULL || diagnostic->message == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    messageText = ZrCore_String_GetNativeString(diagnostic->message);
    return messageText != ZR_NULL && strstr(messageText, fragment) != ZR_NULL;
}

static TZrBool has_completion_detail_fragment(SZrArray *completions,
                                              const char *label,
                                              const char *detailFragment) {
    TZrSize i;

    if (completions == ZR_NULL || label == ZR_NULL || detailFragment == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < completions->length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(completions, i);
        const char *detail;
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        if (strcmp(ZrCore_String_GetNativeString((*itemPtr)->label), label) != 0) {
            continue;
        }

        detail = completion_detail_string(*itemPtr);
        if (detail != ZR_NULL && strstr(detail, detailFragment) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const char *completion_detail_for_label(SZrArray *completions, const char *label) {
    TZrSize i;

    if (completions == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < completions->length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(completions, i);
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        if (strcmp(ZrCore_String_GetNativeString((*itemPtr)->label), label) == 0) {
            return completion_detail_string(*itemPtr);
        }
    }

    return ZR_NULL;
}

static void describe_symbol(char *buffer, size_t bufferSize, SZrSymbol *symbol) {
    const char *name = ZR_NULL;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    if (symbol == ZR_NULL) {
        snprintf(buffer, bufferSize, "symbol=<null>");
        return;
    }

    if (symbol->name != ZR_NULL) {
        name = ZrCore_String_GetNativeString(symbol->name);
    }

    snprintf(buffer,
             bufferSize,
             "symbol=%s type=%d",
             name != ZR_NULL ? name : "<unnamed>",
             (int)symbol->type);
}

static void describe_file_range(char *buffer, size_t bufferSize, SZrFileRange range) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    snprintf(buffer,
             bufferSize,
             "[%zu:%d:%d-%zu:%d:%d]",
             (size_t)range.start.offset,
             range.start.line,
             range.start.column,
             (size_t)range.end.offset,
             range.end.line,
             range.end.column);
}

static void describe_completion_labels(SZrArray *completions, char *buffer, size_t bufferSize) {
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (completions == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < completions->length && offset + 1 < bufferSize; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(completions, i);
        const char *label;
        int written;

        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        label = ZrCore_String_GetNativeString((*itemPtr)->label);
        if (label == ZR_NULL) {
            continue;
        }

        written = snprintf(buffer + offset,
                           bufferSize - offset,
                           "%s%s",
                           offset == 0 ? "" : ", ",
                           label);
        if (written < 0) {
            break;
        }
        if ((size_t)written >= bufferSize - offset) {
            offset = bufferSize - 1;
            break;
        }
        offset += (size_t)written;
    }

    if (offset == 0 && bufferSize > 0) {
        snprintf(buffer, bufferSize, "<none>");
    }
}

// 测试语义分析器创建和释放
static void test_semantic_analyzer_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Creation and Free");
    
    TEST_INFO("Semantic Analyzer Creation", "Creating and freeing semantic analyzer");
    
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Creation and Free", "Failed to create semantic analyzer");
        return;
    }
    
    if (analyzer->symbolTable == ZR_NULL || analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Creation and Free", "Symbol table or reference tracker is NULL");
        return;
    }
    
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Creation and Free");
}

// 测试语义分析
static void test_semantic_analyzer_analyze(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Analyze");
    
    TEST_INFO("Analyze AST", "Analyzing simple AST for semantic information");
    
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Failed to create semantic analyzer");
        return;
    }
    
    // 创建简单的测试代码
    const TZrChar *testCode = "var x = 10;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    
    // 解析代码
    SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (ast == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Failed to parse test code");
        return;
    }
    
    // 分析 AST
    TZrBool success = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
    if (!success) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Failed to analyze AST");
        return;
    }
    
    // 验证符号表中有符号
    SZrString *varName = ZrCore_String_Create(state, "x", 1);
    SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, varName, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Symbol not found in symbol table");
        return;
    }
    
    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Analyze");
}

// 测试 assignment + binary 表达式路径的类型检查
static void test_semantic_analyzer_type_checking_assignment_path(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Type Checking Assignment Path");

    TEST_INFO("Type Checking", "Analyzing assignment and binary expression path");

    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Type Checking Assignment Path", "Failed to create semantic analyzer");
        return;
    }

    const TZrChar *testCode = "var x = 1; x = x + 2;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (ast == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Type Checking Assignment Path", "Failed to parse test code");
        return;
    }

    if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Type Checking Assignment Path", "Failed to analyze AST");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Type Checking Assignment Path");
}

static void test_semantic_analyzer_avoids_false_binary_type_mismatch_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Avoids False Binary Type Mismatch Diagnostics");

    TEST_INFO("Binary expression diagnostics",
              "Analyzing native float arithmetic and string concatenation that the compiler accepts should not emit type_mismatch diagnostics");

    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer,
                  "Semantic Analyzer Avoids False Binary Type Mismatch Diagnostics",
                  "Failed to create semantic analyzer");
        return;
    }

    {
        const TZrChar *testCode =
            "var math = %import zr.math;"
            "pipeline(seed: float) {"
            "    var matrix = math.Matrix4x4.translation(seed, seed + 2.0, seed + 4.0);"
            "    var banner = \"PIPELINE\";"
            "    return banner + matrix.m00;"
            "}";
        SZrString *sourceName = ZrCore_String_Create(state, "native_binary_diagnostics_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Avoids False Binary Type Mismatch Diagnostics",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Avoids False Binary Type Mismatch Diagnostics",
                      "Failed to analyze AST");
            return;
        }

        if (has_diagnostic_code(analyzer, "type_mismatch")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Avoids False Binary Type Mismatch Diagnostics",
                      "Unexpected type_mismatch diagnostic for valid native arithmetic/string concatenation");
            return;
        }

        ZrParser_Ast_Free(state, ast);
    }

    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Avoids False Binary Type Mismatch Diagnostics");
}

static void test_semantic_analyzer_avoids_false_numeric_initializer_type_mismatch_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Avoids False Numeric Initializer Type Mismatch Diagnostics");

    TEST_INFO("Variable initializer diagnostics",
              "Analyzing explicit numeric initializers that the compiler accepts should not emit type_mismatch diagnostics");

    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer,
                  "Semantic Analyzer Avoids False Numeric Initializer Type Mismatch Diagnostics",
                  "Failed to create semantic analyzer");
        return;
    }

    {
        const TZrChar *testCode =
            "validateNumericAssignments(left: int, right: int) {\n"
            "    var sum: int = left + right;\n"
            "    var widenedFromZero: float = left + 0;\n"
            "    var widenedFromFloatLiteral: float = left + 0.0;\n"
            "    return widenedFromZero + widenedFromFloatLiteral + sum;\n"
            "}";
        SZrString *sourceName =
            ZrCore_String_Create(state, "numeric_initializer_diagnostics_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Avoids False Numeric Initializer Type Mismatch Diagnostics",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Avoids False Numeric Initializer Type Mismatch Diagnostics",
                      "Failed to analyze AST");
            return;
        }

        if (has_diagnostic_code(analyzer, "type_mismatch")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Avoids False Numeric Initializer Type Mismatch Diagnostics",
                      "Unexpected type_mismatch diagnostic for valid numeric initializers");
            return;
        }

        ZrParser_Ast_Free(state, ast);
    }

    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Avoids False Numeric Initializer Type Mismatch Diagnostics");
}

static void test_semantic_analyzer_populates_semantic_context(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Populates Semantic Context");
    timer.startTime = clock();

    TEST_INFO("Semantic context integration",
              "Analyzing declarations should populate semantic symbols, types, HIR module and overload sets");

    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context", "Failed to create semantic analyzer");
        return;
    }

    const TZrChar *testCode =
        "var x = 1; "
        "add(a: int): int { return a; } "
        "add(a: int, b: int): int { return a + b; }";
    SZrString *sourceName = ZrCore_String_Create(state, "semantic_context_test.zr", 24);
    SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (ast == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context", "Failed to parse test code");
        return;
    }

    if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context", "Failed to analyze AST");
        return;
    }

    if (analyzer->semanticContext == ZR_NULL || analyzer->hirModule == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context",
                  "Semantic context or HIR module was not attached");
        return;
    }

    if (analyzer->hirModule->semantic != analyzer->semanticContext ||
        analyzer->hirModule->rootAst != ast) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context",
                  "HIR module does not reference the current semantic context/AST");
        return;
    }

    if (analyzer->semanticContext->symbols.length < 3 ||
        analyzer->semanticContext->types.length == 0 ||
        analyzer->semanticContext->overloadSets.length == 0) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context",
                  "Semantic context did not record declarations");
        return;
    }

    SZrString *funcName = ZrCore_String_Create(state, "add", 3);
    SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, funcName, ZR_NULL);
    const SZrSemanticOverloadSetRecord *overloadSet =
        find_overload_set_record(analyzer->semanticContext, "add");
    if (symbol == ZR_NULL || symbol->semanticId == 0 || symbol->overloadSetId == 0 ||
        overloadSet == ZR_NULL || overloadSet->members.length < 2) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Populates Semantic Context",
                  "Function symbol was not linked into semantic overload records");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Populates Semantic Context");
}

static void test_semantic_analyzer_records_using_cleanup_and_template_segments(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Records Using Cleanup And Template Segments");

    TEST_INFO("Using/template semantic metadata",
              "Analyzing using statements should populate cleanup plan and template segments in semantic context");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "var resource = 1; "
            "%using resource; "
            "%using (resource) { var message = `hello ${resource}`; }";
        SZrString *sourceName = ZrCore_String_Create(state, "using_template_test.zr", 22);
        SZrAstNode *ast;
        SZrString *resourceName;
        SZrSymbol *resourceSymbol;
        TZrBool foundStatic = ZR_FALSE;
        TZrBool foundInterpolation = ZR_FALSE;
        TZrSize i;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Using Cleanup And Template Segments",
                      "Failed to create semantic analyzer");
            return;
        }

        ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Using Cleanup And Template Segments",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Using Cleanup And Template Segments",
                      "Failed to analyze AST");
            return;
        }

        if (analyzer->semanticContext == ZR_NULL ||
            analyzer->semanticContext->cleanupPlan.length < 2 ||
            analyzer->semanticContext->templateSegments.length < 3) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Using Cleanup And Template Segments",
                      "Semantic context did not record cleanup steps or template segments");
            return;
        }

        resourceName = ZrCore_String_Create(state, "resource", 8);
        resourceSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, resourceName, ZR_NULL);
        if (resourceSymbol == ZR_NULL || resourceSymbol->semanticId == 0 ||
            !cleanup_plan_targets_symbol(analyzer->semanticContext, resourceSymbol->semanticId)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Using Cleanup And Template Segments",
                      "Using statement did not bind deterministic cleanup to the resource symbol");
            return;
        }

        for (i = 0; i < analyzer->semanticContext->templateSegments.length; i++) {
            const SZrTemplateSegment *segment =
                (const SZrTemplateSegment *)ZrCore_Array_Get(&analyzer->semanticContext->templateSegments, i);
            if (segment == ZR_NULL) {
                continue;
            }
            if (segment->isInterpolation) {
                foundInterpolation = ZR_TRUE;
            } else if (segment->staticText != ZR_NULL) {
                foundStatic = ZR_TRUE;
            }
        }

        if (!foundStatic || !foundInterpolation) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Using Cleanup And Template Segments",
                      "Template string semantic segments were flattened instead of preserved");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Records Using Cleanup And Template Segments");
}

static void test_semantic_analyzer_records_field_scoped_using_cleanup_metadata(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Records Field-Scoped Using Cleanup Metadata");

    TEST_INFO("Field-scoped using semantic metadata",
              "Analyzing `%using var` fields should register field symbols and distinguish struct-value cleanup from instance-field cleanup");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "struct HandleBox { %using var handle: %unique Resource; }\n"
            "class Holder { %using var resource: %unique Resource; }";
        SZrString *sourceName = ZrCore_String_Create(state, "field_using_semantic_test.zr", 28);
        SZrAstNode *ast;
        SZrString *handleName;
        SZrString *resourceName;
        SZrSymbol *handleSymbol;
        SZrSymbol *resourceSymbol;
        const SZrDeterministicCleanupStep *firstStep;
        const SZrDeterministicCleanupStep *secondStep;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Failed to create semantic analyzer");
            return;
        }

        ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Failed to analyze AST");
            return;
        }

        if (analyzer->semanticContext == ZR_NULL ||
            analyzer->semanticContext->cleanupPlan.length != 2) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Expected one struct-value cleanup step and one instance-field cleanup step");
            return;
        }

        handleName = ZrCore_String_Create(state, "handle", 6);
        resourceName = ZrCore_String_Create(state, "resource", 8);
        handleSymbol = lookup_symbol_any_scope_by_type(state, analyzer->symbolTable, handleName, ZR_SYMBOL_FIELD);
        resourceSymbol = lookup_symbol_any_scope_by_type(state, analyzer->symbolTable, resourceName, ZR_SYMBOL_FIELD);
        if (handleSymbol == ZR_NULL || resourceSymbol == ZR_NULL ||
            handleSymbol->type != ZR_SYMBOL_FIELD ||
            resourceSymbol->type != ZR_SYMBOL_FIELD ||
            handleSymbol->semanticId == 0 || resourceSymbol->semanticId == 0) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Using-managed fields were not registered as semantic field symbols");
            return;
        }

        firstStep = (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&analyzer->semanticContext->cleanupPlan, 0);
        secondStep = (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&analyzer->semanticContext->cleanupPlan, 1);
        if (firstStep == ZR_NULL || secondStep == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Cleanup plan entries were missing");
            return;
        }

        if (firstStep->ownerRegionId == 0 || secondStep->ownerRegionId == 0) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Cleanup plan entries did not record owner lifetime regions");
            return;
        }

        if (!(firstStep->kind == ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD ||
              secondStep->kind == ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD) ||
            !(firstStep->kind == ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD ||
              secondStep->kind == ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Cleanup plan did not distinguish struct-value fields from instance fields");
            return;
        }

        if (!cleanup_plan_targets_symbol(analyzer->semanticContext, handleSymbol->semanticId) ||
            !cleanup_plan_targets_symbol(analyzer->semanticContext, resourceSymbol->semanticId)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata",
                      "Cleanup plan did not target the registered using-managed field symbols");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Records Field-Scoped Using Cleanup Metadata");
}

static void test_semantic_analyzer_rejects_static_using_field(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Rejects Static Using Field");

    TEST_INFO("Static using diagnostic",
              "Analyzing `static %using var` should emit a compile-time diagnostic instead of silently accepting lifecycle-managed static fields");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode = "class Holder { static %using var resource: %unique Resource; }";
        SZrString *sourceName = ZrCore_String_Create(state, "static_using_field_test.zr", 26);
        SZrAstNode *ast;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Rejects Static Using Field",
                      "Failed to create semantic analyzer");
            return;
        }

        ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Rejects Static Using Field",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Rejects Static Using Field",
                      "Failed to analyze AST");
            return;
        }

        if (!has_diagnostic_code(analyzer, "static_using_field")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Rejects Static Using Field",
                      "Expected a static_using_field diagnostic for lifecycle-managed static fields");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Rejects Static Using Field");
}

// 测试获取诊断信息
static void test_semantic_analyzer_get_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Diagnostics");
    
    TEST_INFO("Get Diagnostics", "Getting diagnostic information from analyzer");
    
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to create semantic analyzer");
        return;
    }
    
    // 添加一个测试诊断
    SZrFileRange location = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(10, 1, 10),
        ZR_NULL
    );
    
    TZrBool success = ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer, ZR_DIAGNOSTIC_ERROR,
                                                     location, "Test error", "test_error");
    if (!success) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to add diagnostic");
        return;
    }
    
    // 获取诊断信息
    SZrArray diagnostics;
    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrDiagnostic *), 4);
    success = ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(state, analyzer, &diagnostics);
    
    if (!success || diagnostics.length == 0) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to get diagnostics");
        return;
    }
    
    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Get Diagnostics");
}

// 测试代码补全
static void test_semantic_analyzer_get_completions(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Completions");
    
    TEST_INFO("Get Completions", "Getting code completion suggestions");
    
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Get Completions", "Failed to create semantic analyzer");
        return;
    }
    
    // 创建测试代码并分析
    const TZrChar *testCode = "var x = 10; var y = 20;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    
    if (ast != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
    }
    
    // 获取补全
    SZrFileRange position = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(0, 1, 0),
        ZR_NULL
    );
    
    SZrArray completions;
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 4);
    ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, position, &completions);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    // 补全可能为空（取决于实现），只要不崩溃就算成功
    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Get Completions");
}

static void test_semantic_analyzer_get_completions_includes_local_scope_symbols(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Completions Includes Local Scope Symbols");

    TEST_INFO("Local scope completions",
              "Getting code completion suggestions inside a function body should include parameters and local variables");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "helper(seed: float) {\n"
            "    var localValue = seed + 1.0;\n"
            "    return localValue;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "local_completion_test.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange position;
        SZrArray completions;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Local Scope Symbols",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Local Scope Symbols",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Local Scope Symbols",
                      "Failed to analyze AST");
            return;
        }

        position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 3, 5),
            ZrParser_FilePosition_Create(0, 3, 5),
            ZR_NULL
        );
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, position, &completions);

        if (!has_completion_label(&completions, "seed") ||
            !has_completion_label(&completions, "localValue")) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Local Scope Symbols",
                      "Expected parameter/local symbols in function-body completions");
            return;
        }

        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Get Completions Includes Local Scope Symbols");
}

static void test_semantic_analyzer_get_symbol_at_resolves_local_references(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Symbol At Resolves Local References");

    TEST_INFO("Local reference resolution",
              "Resolving a symbol at a local identifier reference should return the matching parameter/local declaration instead of only global definitions");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "var seed = 0.0;\n"
            "helper(seed: float) {\n"
            "    var localValue = seed + 1.0;\n"
            "    return localValue;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "local_reference_resolution_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange position;
        SZrSymbol *symbol;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Symbol At Resolves Local References",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Symbol At Resolves Local References",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Symbol At Resolves Local References",
                      "Failed to analyze AST");
            return;
        }

        position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(59, 3, 22),
            ZrParser_FilePosition_Create(59, 3, 22),
            sourceName
        );
        symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
        if (symbol == ZR_NULL ||
            symbol->name == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(symbol->name), "seed") != 0 ||
            symbol->type != ZR_SYMBOL_PARAMETER) {
            SZrString *lookupName = ZrCore_String_Create(state, "seed", 4);
            SZrSymbol *lookupSymbol =
                ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable,
                                                              lookupName,
                                                              position);
            SZrReference *reference =
                ZrLanguageServer_ReferenceTracker_FindReferenceAt(analyzer->referenceTracker, position);
            TZrSize referenceCount =
                lookupSymbol != ZR_NULL
                ? ZrLanguageServer_ReferenceTracker_GetReferenceCount(analyzer->referenceTracker, lookupSymbol)
                : 0;
            SZrArray references;
            char detail[256];
            char lookupDetail[64];
            char referenceDetail[64];
            char referenceRangeDetail[64];
            char queryRangeDetail[64];
            ZrCore_Array_Construct(&references);
            ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 4);
            describe_symbol(detail, sizeof(detail), symbol);
            describe_symbol(lookupDetail, sizeof(lookupDetail), lookupSymbol);
            describe_symbol(referenceDetail,
                            sizeof(referenceDetail),
                            reference != ZR_NULL ? reference->symbol : ZR_NULL);
            describe_file_range(queryRangeDetail, sizeof(queryRangeDetail), position);
            if (lookupSymbol != ZR_NULL &&
                ZrLanguageServer_ReferenceTracker_FindReferences(state,
                                                                 analyzer->referenceTracker,
                                                                 lookupSymbol,
                                                                 &references) &&
                references.length > 0) {
                SZrReference **referencePtr =
                    (SZrReference **)ZrCore_Array_Get(&references, 0);
                if (referencePtr != ZR_NULL && *referencePtr != ZR_NULL) {
                    describe_file_range(referenceRangeDetail,
                                        sizeof(referenceRangeDetail),
                                        (*referencePtr)->location);
                } else {
                    snprintf(referenceRangeDetail, sizeof(referenceRangeDetail), "<null>");
                }
            } else {
                snprintf(referenceRangeDetail, sizeof(referenceRangeDetail), "<none>");
            }
            snprintf(detail + strlen(detail),
                     sizeof(detail) - strlen(detail),
                     " lookup=%s refHit=%s refCount=%zu allRefs=%zu query=%s refRange=%s",
                     lookupDetail,
                     referenceDetail,
                     (size_t)referenceCount,
                     (size_t)analyzer->referenceTracker->allReferences.length,
                     queryRangeDetail,
                     referenceRangeDetail);
            ZrCore_Array_Free(state, &references);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Symbol At Resolves Local References",
                      detail);
            return;
        }

        position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(82, 4, 12),
            ZrParser_FilePosition_Create(82, 4, 12),
            sourceName
        );
        symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
        if (symbol == ZR_NULL ||
            symbol->name == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(symbol->name), "localValue") != 0 ||
            symbol->type != ZR_SYMBOL_VARIABLE) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Symbol At Resolves Local References",
                      "Expected local variable reference to resolve to the local declaration");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Get Symbol At Resolves Local References");
}

static void test_semantic_analyzer_get_completions_includes_native_hint_entries(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Completions Includes Native Hint Entries");

    TEST_INFO("Native hint completions",
              "Getting code completion suggestions should surface lib_system/lib_math registered hint entries with detail text");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "var system = %import zr.system;\n"
            "var math = %import zr.math;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "native_hint_completion_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange position;
        SZrArray completions;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Native Hint Entries",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Native Hint Entries",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Native Hint Entries",
                      "Failed to analyze AST");
            return;
        }

        position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(62, 2, 29),
            ZrParser_FilePosition_Create(62, 2, 29),
            ZR_NULL
        );
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 16);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, position, &completions);

        if (!has_completion_label(&completions, "readText") ||
            !has_completion_detail_fragment(&completions, "readText", "readText(") ||
            !has_completion_label(&completions, "Matrix4x4") ||
            !has_completion_detail_fragment(&completions, "Matrix4x4", "struct Matrix4x4") ||
            !has_completion_label(&completions, "translation")) {
            char labels[512];
            describe_completion_labels(&completions, labels, sizeof(labels));
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Completions Includes Native Hint Entries",
                      labels);
            return;
        }

        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Get Completions Includes Native Hint Entries");
}

static void test_semantic_analyzer_local_symbols_surface_rich_hover_and_completion_detail(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail");

    TEST_INFO("Local symbol hover/completion detail",
              "Local parameters and variables should surface type and access detail in completion and hover results");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "helper(seed: float) {\n"
            "    var localValue: float = seed + 1.0;\n"
            "    return localValue;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "rich_hover_local_symbols_test.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange completionPosition;
        SZrFileRange hoverPosition;
        SZrArray completions;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail",
                      "Failed to analyze AST");
            return;
        }

        completionPosition = file_range_for_nth_substring(testCode, "return", 0, ZR_FALSE);
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, completionPosition, &completions);

        if (!has_completion_detail_fragment(&completions, "seed", "float") ||
            !has_completion_detail_fragment(&completions, "localValue", "float") ||
            !has_completion_detail_fragment(&completions, "localValue", "private")) {
            char details[512];
            snprintf(details,
                     sizeof(details),
                     "seed=%s | localValue=%s",
                     completion_detail_for_label(&completions, "seed") != ZR_NULL
                        ? completion_detail_for_label(&completions, "seed")
                        : "<null>",
                     completion_detail_for_label(&completions, "localValue") != ZR_NULL
                        ? completion_detail_for_label(&completions, "localValue")
                        : "<null>");
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail",
                      details);
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "localValue", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail",
                      "Failed to get hover info for local variable reference");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "localValue") == ZR_NULL ||
            strstr(hoverText, "float") == ZR_NULL ||
            strstr(hoverText, "private") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Local Symbols Surface Rich Hover And Completion Detail");
}

static void test_semantic_analyzer_generic_function_symbols_surface_signature_detail(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Generic Function Symbols Surface Signature Detail");

    TEST_INFO("Generic hover/completion detail",
              "Generic function symbols should expose explicit generic and passing-mode signature text in completion and hover");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "func swap<T>(%ref value: T): T {\n"
            "    return value;\n"
            "}\n"
            "func use(): void {\n"
            "    var slot: int = 1;\n"
            "    swap<int>(slot);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "generic_signature_hover_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange completionPosition;
        SZrFileRange hoverPosition;
        SZrArray completions;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;
        const char *detailText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      "Failed to analyze AST");
            return;
        }

        completionPosition = file_range_for_nth_substring(testCode, "swap<int>", 0, ZR_FALSE);
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, completionPosition, &completions);

        detailText = completion_detail_for_label(&completions, "swap");
        if (detailText == ZR_NULL ||
            strstr(detailText, "swap<T>(") == ZR_NULL ||
            strstr(detailText, "%ref value: T") == ZR_NULL ||
            strstr(detailText, "): T") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      detailText != ZR_NULL ? detailText : "<null detail>");
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "swap", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      "Failed to get hover info for generic function call");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "swap<T>(") == ZR_NULL ||
            strstr(hoverText, "%ref value: T") == ZR_NULL ||
            strstr(hoverText, "Access: public") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Generic Function Symbols Surface Signature Detail");
}

static void test_semantic_analyzer_generic_type_symbols_surface_signature_detail(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Generic Type Symbols Surface Signature Detail");

    TEST_INFO("Generic type hover/completion detail",
              "Generic class and interface symbols should surface inheritance, const generics, variance, and where clauses in completion and hover");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "interface Producer<out T> {\n"
            "    next(): T;\n"
            "}\n"
            "class Item {\n"
            "    pub @constructor() { }\n"
            "}\n"
            "class Derived<T, const N: int> : Producer<T>\n"
            "where T: class, new() {\n"
            "}\n"
            "func use(): void {\n"
            "    var value: Derived<Item, 4> = null;\n"
            "    value;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "generic_type_signature_hover_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange completionPosition;
        SZrFileRange derivedHoverPosition;
        SZrFileRange producerHoverPosition;
        SZrArray completions;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *detailText;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      "Failed to analyze AST");
            return;
        }

        completionPosition = file_range_for_nth_substring(testCode, "value;", 0, ZR_FALSE);
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, completionPosition, &completions);

        detailText = completion_detail_for_label(&completions, "Derived");
        if (detailText == ZR_NULL ||
            strstr(detailText, "class Derived<T, const N: int>") == ZR_NULL ||
            strstr(detailText, ": Producer<T>") == ZR_NULL ||
            strstr(detailText, "where T: class, new()") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      detailText != ZR_NULL ? detailText : "<null detail>");
            return;
        }

        detailText = completion_detail_for_label(&completions, "Producer");
        if (detailText == ZR_NULL ||
            strstr(detailText, "interface Producer<out T>") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      detailText != ZR_NULL ? detailText : "<null detail>");
            return;
        }

        derivedHoverPosition = file_range_for_nth_substring(testCode, "Derived", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, derivedHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      "Failed to get hover info for generic class reference");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "class Derived<T, const N: int> : Producer<T>") == ZR_NULL ||
            strstr(hoverText, "Resolved Type: Derived<Item, 4>") == ZR_NULL ||
            strstr(hoverText, "where T: class, new()") == ZR_NULL ||
            strstr(hoverText, "Access: private") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        producerHoverPosition = file_range_for_nth_substring(testCode, "Producer", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, producerHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      "Failed to get hover info for generic interface reference");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "interface Producer<out T>") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Generic Type Symbols Surface Signature Detail");
}

static void test_semantic_analyzer_reports_invalid_interface_variance_positions(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Invalid Interface Variance Positions");

    TEST_INFO("Variance diagnostics",
              "Illegal interface variance positions should emit dedicated diagnostics for method, field, property, and nested generic usage");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "interface Producer<out T> {\n"
            "    accept(value: T): void;\n"
            "}\n"
            "interface Consumer<in T> {\n"
            "    next(): T;\n"
            "}\n"
            "interface Store<out T> {\n"
            "    pub var value: T;\n"
            "}\n"
            "interface OutputProperty<out T> {\n"
            "    pub set item: T;\n"
            "}\n"
            "interface InputProperty<in T> {\n"
            "    pub get item: T;\n"
            "}\n"
            "interface Sink<in T> {\n"
            "    accept(value: T): void;\n"
            "}\n"
            "interface Nested<out T> {\n"
            "    next(): Sink<T>;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "invalid_variance_positions_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagAccept;
        SZrDiagnostic *diagReturn;
        SZrDiagnostic *diagField;
        SZrDiagnostic *diagSetter;
        SZrDiagnostic *diagGetter;
        SZrDiagnostic *diagNested;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid Interface Variance Positions",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid Interface Variance Positions",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid Interface Variance Positions",
                      "Failed to analyze AST");
            return;
        }

        diagAccept = find_diagnostic_by_code_and_line(analyzer, "invalid_variance", 2);
        diagReturn = find_diagnostic_by_code_and_line(analyzer, "invalid_variance", 5);
        diagField = find_diagnostic_by_code_and_line(analyzer, "invalid_variance", 8);
        diagSetter = find_diagnostic_by_code_and_line(analyzer, "invalid_variance", 11);
        diagGetter = find_diagnostic_by_code_and_line(analyzer, "invalid_variance", 14);
        diagNested = find_diagnostic_by_code_and_line(analyzer, "invalid_variance", 20);
        if (count_diagnostics_with_code(analyzer, "invalid_variance") != 6 ||
            diagAccept == ZR_NULL ||
            diagReturn == ZR_NULL ||
            diagField == ZR_NULL ||
            diagSetter == ZR_NULL ||
            diagGetter == ZR_NULL ||
            diagNested == ZR_NULL ||
            !diagnostic_message_contains(diagAccept, "covariant") ||
            !diagnostic_message_contains(diagReturn, "contravariant") ||
            !diagnostic_message_contains(diagField, "field") ||
            !diagnostic_message_contains(diagSetter, "setter") ||
            !diagnostic_message_contains(diagGetter, "getter") ||
            !diagnostic_message_contains(diagNested, "nested")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid Interface Variance Positions",
                      "Expected six invalid_variance diagnostics covering parameter, return, field, property, and nested generic positions");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Invalid Interface Variance Positions");
}

static void test_semantic_analyzer_warns_on_unreachable_statements_after_return_or_throw(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Warns On Unreachable Statements After Return Or Throw");

    TEST_INFO("Deterministic unreachable statements",
              "Analyzing statements after return/throw should emit warning diagnostics instead of silently treating them as reachable");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "returnFlow() {\n"
            "    return 1;\n"
            "    var deadAfterReturn = 2;\n"
            "}\n"
            "throwFlow() {\n"
            "    throw \"boom\";\n"
            "    var deadAfterThrow = 3;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "unreachable_after_exit_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *returnDiag;
        SZrDiagnostic *throwDiag;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable Statements After Return Or Throw",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable Statements After Return Or Throw",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable Statements After Return Or Throw",
                      "Failed to analyze AST");
            return;
        }

        returnDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_code", 3);
        throwDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_code", 7);
        if (count_diagnostics_with_code(analyzer, "unreachable_code") < 2 ||
            returnDiag == ZR_NULL ||
            throwDiag == ZR_NULL ||
            returnDiag->severity != ZR_DIAGNOSTIC_WARNING ||
            throwDiag->severity != ZR_DIAGNOSTIC_WARNING) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable Statements After Return Or Throw",
                      "Expected warning diagnostics for statements after return/throw");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Warns On Unreachable Statements After Return Or Throw");
}

static void test_semantic_analyzer_warns_on_unreachable_if_branches(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Warns On Unreachable If Branches");

    TEST_INFO("Deterministic branch reachability",
              "Analyzing if(true/false) should emit warnings for the statically unreachable branch");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "branching() {\n"
            "    if (true) {\n"
            "        var liveThen = 1;\n"
            "    } else {\n"
            "        var deadElse = 2;\n"
            "    }\n"
            "    if (false) {\n"
            "        var deadThen = 3;\n"
            "    } else {\n"
            "        var liveElse = 4;\n"
            "    }\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "unreachable_if_branch_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *deadElseDiag;
        SZrDiagnostic *deadThenDiag;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable If Branches",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable If Branches",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable If Branches",
                      "Failed to analyze AST");
            return;
        }

        deadElseDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_branch", 4);
        deadThenDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_branch", 7);
        if (count_diagnostics_with_code(analyzer, "unreachable_branch") < 2 ||
            deadElseDiag == ZR_NULL ||
            deadThenDiag == ZR_NULL ||
            deadElseDiag->severity != ZR_DIAGNOSTIC_WARNING ||
            deadThenDiag->severity != ZR_DIAGNOSTIC_WARNING) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Unreachable If Branches",
                      "Expected warning diagnostics for statically unreachable if branches");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Warns On Unreachable If Branches");
}

static void test_semantic_analyzer_warns_on_deterministic_short_circuit_branches(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Warns On Deterministic Short Circuit Branches");

    TEST_INFO("Deterministic short-circuit reachability",
              "Analyzing true || ... and false && ... should warn that the right-hand branch is unreachable");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "shorts() {\n"
            "    var skippedOr = true || false;\n"
            "    var skippedAnd = false && true;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "short_circuit_test.zr", 21);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *orDiag;
        SZrDiagnostic *andDiag;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Deterministic Short Circuit Branches",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Deterministic Short Circuit Branches",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Deterministic Short Circuit Branches",
                      "Failed to analyze AST");
            return;
        }

        orDiag = find_diagnostic_by_code_and_line(analyzer, "short_circuit_unreachable", 2);
        andDiag = find_diagnostic_by_code_and_line(analyzer, "short_circuit_unreachable", 3);
        if (count_diagnostics_with_code(analyzer, "short_circuit_unreachable") < 2 ||
            orDiag == ZR_NULL ||
            andDiag == ZR_NULL ||
            orDiag->severity != ZR_DIAGNOSTIC_WARNING ||
            andDiag->severity != ZR_DIAGNOSTIC_WARNING) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Warns On Deterministic Short Circuit Branches",
                      "Expected warning diagnostics for deterministic short-circuit right-hand branches");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Warns On Deterministic Short Circuit Branches");
}

static void test_semantic_analyzer_reports_declared_ownership_initializer_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Declared Ownership Initializer Mismatch");

    TEST_INFO("Ownership compatibility in variable declarations",
              "Analyzing an explicit %unique T declaration initialized from %shared T should emit a type_mismatch diagnostic");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "var borrowed: %shared Resource;\n"
            "var owned: %unique Resource = borrowed;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_decl_mismatch_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "type_mismatch", 2);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Expected type_mismatch diagnostic on the explicit unique<Resource> initializer");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Declared Ownership Initializer Mismatch");
}

static void test_semantic_analyzer_reports_return_ownership_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Return Ownership Mismatch");

    TEST_INFO("Ownership compatibility in return statements",
              "Analyzing a function that promises %unique T but returns %shared T should emit a type_mismatch diagnostic");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "upgrade(resource: %shared Resource): %unique Resource {\n"
            "    return resource;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_return_mismatch_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "type_mismatch", 2);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Expected type_mismatch diagnostic on the incompatible return ownership");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Return Ownership Mismatch");
}

static void test_semantic_analyzer_reports_function_argument_ownership_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Function Argument Ownership Mismatch");

    TEST_INFO("Ownership compatibility in function calls",
              "Analyzing a direct function call that passes %shared T into %unique T should emit a type_mismatch diagnostic");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "consume(resource: %unique Resource) {\n"
            "}\n"
            "run(resource: %shared Resource) {\n"
            "    consume(resource);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_call_mismatch_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Function Argument Ownership Mismatch",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Function Argument Ownership Mismatch",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Function Argument Ownership Mismatch",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "type_mismatch", 4);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Function Argument Ownership Mismatch",
                      "Expected type_mismatch diagnostic on the incompatible function argument ownership");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Function Argument Ownership Mismatch");
}

static void test_semantic_analyzer_reports_method_argument_ownership_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Method Argument Ownership Mismatch");

    TEST_INFO("Ownership compatibility in method calls",
              "Analyzing an instance method call that passes %shared T into %unique T should emit a type_mismatch diagnostic");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "class ResourceBox {\n"
            "    consume(resource: %unique Resource): int {\n"
            "        return 0;\n"
            "    }\n"
            "}\n"
            "run(box: ResourceBox, resource: %shared Resource) {\n"
            "    box.consume(resource);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_method_call_mismatch_test.zr", 38);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Method Argument Ownership Mismatch",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Method Argument Ownership Mismatch",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Method Argument Ownership Mismatch",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "type_mismatch", 7);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Method Argument Ownership Mismatch",
                      "Expected type_mismatch diagnostic on the incompatible method argument ownership");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Method Argument Ownership Mismatch");
}

static void test_semantic_analyzer_resolves_overloads_for_call_compatibility(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Resolves Overloads For Call Compatibility");

    TEST_INFO("Overload-aware call compatibility",
              "Analyzing overload sets should keep the matching call green and still diagnose the unmatched call");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "mix(value: int): int {\n"
            "    return value;\n"
            "}\n"
            "mix(value: string): string {\n"
            "    return value;\n"
            "}\n"
            "run(flag: bool) {\n"
            "    mix(\"ok\");\n"
            "    mix(flag);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "overload_call_compatibility_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Resolves Overloads For Call Compatibility",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Resolves Overloads For Call Compatibility",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Resolves Overloads For Call Compatibility",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "type_mismatch", 9);
        if (diagnostic == ZR_NULL ||
            diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            count_diagnostics_with_code(analyzer, "type_mismatch") != 1) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Resolves Overloads For Call Compatibility",
                      "Expected exactly one type_mismatch diagnostic for the overload call with no compatible candidate");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Resolves Overloads For Call Compatibility");
}

static void test_semantic_analyzer_reports_invalid_ffi_decorators(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Invalid FFI Decorators");

    TEST_INFO("Decorator semantic validation",
              "Analyzing invalid zr.ffi decorator usage should emit decorator diagnostics without stopping later analysis");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "%extern(\"fixture\") {\n"
            "    #zr.ffi.unknown(\"native_bad\")#\n"
            "    BadUnknown(lhs:i32): i32;\n"
            "    #zr.ffi.entry(\"native_struct\")#\n"
            "    struct InvalidTarget {\n"
            "        var value:i32;\n"
            "    }\n"
            "    delegate MutPtr(\n"
            "        #zr.ffi.out#\n"
            "        #zr.ffi.inout#\n"
            "        value:pointer<i32>\n"
            "    ): void;\n"
            "    #zr.ffi.callconv(123)#\n"
            "    BadCallconv(): void;\n"
            "}\n"
            "#zr.ffi.lowering(\"bad\")#\n"
            "#zr.ffi.underlying(123)#\n"
            "class InvalidWrapper {\n"
            "    var handleId:i32;\n"
            "}\n"
            "#zr.ffi.lowering(\"handle_id\")#\n"
            "#zr.ffi.underlying(\"string\")#\n"
            "class InvalidUnderlyingWrapper {\n"
            "    var handleId:i32;\n"
            "}\n"
            "struct PlainView {\n"
            "    var raw:i32;\n"
            "}\n"
            "#zr.ffi.lowering(\"value\")#\n"
            "#zr.ffi.viewType(\"PlainView\")#\n"
            "class InvalidViewWrapper {\n"
            "    var raw:i32;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "invalid_ffi_decorator_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *invalidUnderlyingDiagnostic;
        SZrDiagnostic *invalidViewTypeDiagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid FFI Decorators",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid FFI Decorators",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid FFI Decorators",
                      "Failed to analyze AST");
            return;
        }

        invalidUnderlyingDiagnostic = find_diagnostic_by_code_and_line(analyzer, "invalid_decorator", 22);
        invalidViewTypeDiagnostic = find_diagnostic_by_code_and_line(analyzer, "invalid_decorator", 30);
        if (count_diagnostics_with_code(analyzer, "invalid_decorator") != 8 ||
            invalidUnderlyingDiagnostic == ZR_NULL ||
            !diagnostic_message_contains(invalidUnderlyingDiagnostic, "supported integer type") ||
            invalidViewTypeDiagnostic == ZR_NULL ||
            !diagnostic_message_contains(invalidViewTypeDiagnostic, "source extern struct")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid FFI Decorators",
                      "Expected eight invalid_decorator diagnostics including invalid wrapper lowering, invalid wrapper underlying arguments, invalid handle_id underlying type names, and non-extern viewType references");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Invalid FFI Decorators");
}

static void test_semantic_analyzer_class_method_scope_surfaces_this_super_and_locals(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Class Method Scope Surfaces This Super And Locals");

    TEST_INFO("Class method scopes",
              "Instance methods should expose this/super receivers, parameters, and locals to completion and hover");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "class Base {\n"
            "    pub var baseValue: int = 10;\n"
            "    pub @constructor(seed: int) {\n"
            "        this.baseValue = seed;\n"
            "    }\n"
            "}\n"
            "class Derived: Base {\n"
            "    pub var derivedValue: int = 20;\n"
            "    pub total(extra: int): int {\n"
            "        var localResult = extra + 1;\n"
            "        return this.derivedValue + localResult;\n"
            "    }\n"
            "    pub @constructor(seed: int) super(seed) {\n"
            "        this.derivedValue = this.derivedValue + 1;\n"
            "    }\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "class_scope_receivers_test.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange completionPosition;
        SZrFileRange thisHoverPosition;
        SZrFileRange localHoverPosition;
        SZrArray completions;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      "Failed to parse class scope test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      "Failed to analyze class scope test code");
            return;
        }

        completionPosition = file_range_for_nth_substring(testCode, "localResult;", 0, ZR_FALSE);
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, completionPosition, &completions);

        if (!has_completion_label(&completions, "this") ||
            !has_completion_label(&completions, "super") ||
            !has_completion_label(&completions, "extra") ||
            !has_completion_label(&completions, "localResult")) {
            char labels[512];
            char rangeDetail[128];
            char reason[768];
            describe_completion_labels(&completions, labels, sizeof(labels));
            describe_file_range(rangeDetail, sizeof(rangeDetail), completionPosition);
            snprintf(reason, sizeof(reason), "position=%s labels=%s", rangeDetail, labels);
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      reason);
            return;
        }

        thisHoverPosition = file_range_for_nth_substring(testCode, "this.derivedValue", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, thisHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      "Failed to get hover info for this");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "this") == ZR_NULL || strstr(hoverText, "Derived") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }
        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        hoverInfo = ZR_NULL;

        localHoverPosition = file_range_for_nth_substring(testCode, "localResult", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, localHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      "Failed to get hover info for localResult");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "localResult") == ZR_NULL || strstr(hoverText, "int") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces This Super And Locals",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Class Method Scope Surfaces This Super And Locals");
}

static void test_semantic_analyzer_compile_time_test_and_lambda_scopes_surface_symbols(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols");

    TEST_INFO("Compile-time/test/lambda scopes",
              "Compile-time declarations, test bodies, and typed lambdas should register symbols and inferred types");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "%compileTime var MAX_SIZE = 100;\n"
            "%compileTime addBias(seed: int): int {\n"
            "    return seed + MAX_SIZE;\n"
            "}\n"
            "%test(\"scope\") {\n"
            "    var result = addBias(1);\n"
            "    var typed = (value: int) => {\n"
            "        return value + result;\n"
            "    };\n"
            "    return typed(2);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "compile_time_test_scope_symbols.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange testCompletionPosition;
        SZrFileRange lambdaCompletionPosition;
        SZrFileRange compileTimeHoverPosition;
        SZrFileRange lambdaHoverPosition;
        SZrArray completions;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      "Failed to parse compile-time/lambda test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      "Failed to analyze compile-time/lambda test code");
            return;
        }

        testCompletionPosition = file_range_for_nth_substring(testCode, "return typed", 0, ZR_FALSE);
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, testCompletionPosition, &completions);

        if (!has_completion_label(&completions, "result") ||
            !has_completion_label(&completions, "typed") ||
            !has_completion_label(&completions, "addBias")) {
            char labels[512];
            describe_completion_labels(&completions, labels, sizeof(labels));
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      labels);
            return;
        }
        ZrCore_Array_Free(state, &completions);

        lambdaCompletionPosition = file_range_for_nth_substring(testCode, "return value + result", 0, ZR_FALSE);
        ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
        ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, lambdaCompletionPosition, &completions);
        if (!has_completion_label(&completions, "value") ||
            !has_completion_label(&completions, "result")) {
            char labels[512];
            describe_completion_labels(&completions, labels, sizeof(labels));
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      labels);
            return;
        }

        compileTimeHoverPosition = file_range_for_nth_substring(testCode, "seed + MAX_SIZE", 0, ZR_FALSE);
        compileTimeHoverPosition.start.offset += 7;
        compileTimeHoverPosition.end.offset += 7;
        compileTimeHoverPosition.start.column += 7;
        compileTimeHoverPosition.end.column += 7;
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, compileTimeHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      "Failed to get hover info for compile-time var");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "MAX_SIZE") == ZR_NULL || strstr(hoverText, "int") == ZR_NULL) {
            char positionDetail[128];
            char symbolDetail[128];
            char reason[512];
            describe_file_range(positionDetail, sizeof(positionDetail), compileTimeHoverPosition);
            describe_symbol(symbolDetail,
                            sizeof(symbolDetail),
                            ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, compileTimeHoverPosition));
            snprintf(reason,
                     sizeof(reason),
                     "%s | position=%s %s",
                     hoverText != ZR_NULL ? hoverText : "<null hover>",
                     positionDetail,
                     symbolDetail);
            ZrCore_Array_Free(state, &completions);
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      reason);
            return;
        }
        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        hoverInfo = ZR_NULL;

        lambdaHoverPosition = file_range_for_nth_substring(testCode, "result", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, lambdaHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      "Failed to get hover info for lambda capture");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "result") == ZR_NULL || strstr(hoverText, "int") == ZR_NULL) {
            ZrCore_Array_Free(state, &completions);
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        ZrCore_Array_Free(state, &completions);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols");
}

// 测试缓存功能
static void test_semantic_analyzer_cache(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Cache");
    
    TEST_INFO("Cache Functionality", "Testing cache enable/disable and clear");
    
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Failed to create semantic analyzer");
        return;
    }
    
    // 测试启用/禁用缓存
    ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(analyzer, ZR_TRUE);
    if (!analyzer->enableCache) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Failed to enable cache");
        return;
    }
    
    ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(analyzer, ZR_FALSE);
    if (analyzer->enableCache) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Failed to disable cache");
        return;
    }
    
    // 测试清除缓存
    ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(analyzer, ZR_TRUE);
    ZrLanguageServer_SemanticAnalyzer_ClearCache(state, analyzer);
    
    if (analyzer->cache != ZR_NULL && analyzer->cache->isValid) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Cache still valid after clear");
        return;
    }
    
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Cache");
}

// 主测试函数
int main(void) {
    printf("==========\n");
    printf("Language Server - Semantic Analyzer Tests\n");
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
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);
    ZrVmLibContainer_Register(global);
    
    // 运行测试
    test_semantic_analyzer_create_and_free(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_analyze(state);
    TEST_DIVIDER();

    test_semantic_analyzer_type_checking_assignment_path(state);
    TEST_DIVIDER();

    test_semantic_analyzer_avoids_false_binary_type_mismatch_diagnostics(state);
    TEST_DIVIDER();

    test_semantic_analyzer_avoids_false_numeric_initializer_type_mismatch_diagnostics(state);
    TEST_DIVIDER();

    test_semantic_analyzer_populates_semantic_context(state);
    TEST_DIVIDER();

    test_semantic_analyzer_records_using_cleanup_and_template_segments(state);
    TEST_DIVIDER();
    test_semantic_analyzer_records_field_scoped_using_cleanup_metadata(state);
    TEST_DIVIDER();
    test_semantic_analyzer_rejects_static_using_field(state);
    TEST_DIVIDER();

    test_semantic_analyzer_get_diagnostics(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_get_completions(state);
    TEST_DIVIDER();

    test_semantic_analyzer_get_completions_includes_local_scope_symbols(state);
    TEST_DIVIDER();

    test_semantic_analyzer_get_symbol_at_resolves_local_references(state);
    TEST_DIVIDER();

    test_semantic_analyzer_get_completions_includes_native_hint_entries(state);
    TEST_DIVIDER();

    test_semantic_analyzer_local_symbols_surface_rich_hover_and_completion_detail(state);
    TEST_DIVIDER();

    test_semantic_analyzer_class_method_scope_surfaces_this_super_and_locals(state);
    TEST_DIVIDER();

    test_semantic_analyzer_compile_time_test_and_lambda_scopes_surface_symbols(state);
    TEST_DIVIDER();

    test_semantic_analyzer_generic_function_symbols_surface_signature_detail(state);
    TEST_DIVIDER();

    test_semantic_analyzer_generic_type_symbols_surface_signature_detail(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_invalid_interface_variance_positions(state);
    TEST_DIVIDER();

    test_semantic_analyzer_warns_on_unreachable_statements_after_return_or_throw(state);
    TEST_DIVIDER();

    test_semantic_analyzer_warns_on_unreachable_if_branches(state);
    TEST_DIVIDER();

    test_semantic_analyzer_warns_on_deterministic_short_circuit_branches(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_declared_ownership_initializer_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_return_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_function_argument_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_method_argument_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_resolves_overloads_for_call_compatibility(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_invalid_ffi_decorators(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_cache(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All Semantic Analyzer Tests Completed\n");
    printf("==========\n");
    
    return 0;
}

