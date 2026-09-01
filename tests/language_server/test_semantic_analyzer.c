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
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/diagnostic_registry.h"
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

static int test_failures = 0;

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
    test_failures++; \
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

static SZrFileRange file_range_for_nth_substring_offset(const char *content,
                                                        const char *needle,
                                                        TZrSize occurrence,
                                                        TZrSize extraOffset) {
    SZrFileRange range = file_range_for_nth_substring(content, needle, occurrence, ZR_FALSE);
    range.start.offset += extraOffset;
    range.end.offset = range.start.offset;
    range.start.column += extraOffset;
    range.end.column = range.start.column;
    return range;
}

static SZrFileRange file_range_for_nth_substring_in_source(const char *content,
                                                           const char *needle,
                                                           TZrSize occurrence,
                                                           TZrBool useEnd,
                                                           SZrString *source) {
    SZrFileRange range = file_range_for_nth_substring(content, needle, occurrence, useEnd);
    range.source = source;
    return range;
}

static SZrFileRange file_range_for_nth_substring_offset_in_source(const char *content,
                                                                  const char *needle,
                                                                  TZrSize occurrence,
                                                                  TZrSize extraOffset,
                                                                  SZrString *source) {
    SZrFileRange range =
        file_range_for_nth_substring_offset(content, needle, occurrence, extraOffset);
    range.source = source;
    return range;
}

static TZrSize count_logical_facts_with_known_value(const SZrSemanticContext *context,
                                                    EZrSemanticLogicalFactKind kind,
                                                    TZrBool knownValue) {
    TZrSize count = 0;

    if (context == ZR_NULL || !context->logicalFacts.isValid) {
        return 0;
    }

    for (TZrSize i = 0; i < context->logicalFacts.length; i++) {
        const SZrSemanticLogicalFact *fact =
            (const SZrSemanticLogicalFact *)ZrCore_Array_Get((SZrArray *)&context->logicalFacts, i);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            fact->exactness == ZR_SEMANTIC_FACT_EXACT &&
            fact->hasKnownValue &&
            fact->knownValue == knownValue &&
            fact->relatedNode != ZR_NULL) {
            count++;
        }
    }

    return count;
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

#include "test_semantic_analyzer_diagnostic_golden_parity_cases.h"
#include "test_semantic_analyzer_extern_enum_decorator_cases.h"
#include "test_semantic_analyzer_extern_struct_decorator_cases.h"
#include "test_semantic_analyzer_ffi_wrapper_decorator_cases.h"
#include "test_semantic_analyzer_extern_parameter_decorator_cases.h"

static TZrBool diagnostic_string_contains(SZrString *value, const char *fragment) {
    const char *text;

    if (value == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL && strstr(text, fragment) != ZR_NULL;
}

static TZrBool diagnostic_message_contains(SZrDiagnostic *diagnostic, const char *fragment) {
    return diagnostic != ZR_NULL && diagnostic_string_contains(diagnostic->message, fragment);
}

static TZrBool diagnostic_cause_contains(SZrDiagnostic *diagnostic, const char *fragment) {
    return diagnostic != ZR_NULL && diagnostic_string_contains(diagnostic->cause, fragment);
}

static TZrBool diagnostic_suggestion_contains(SZrDiagnostic *diagnostic, const char *fragment) {
    return diagnostic != ZR_NULL && diagnostic_string_contains(diagnostic->suggestion, fragment);
}

static TZrBool ownership_fact_message_contains(const SZrSemanticOwnershipFact *fact, const char *fragment) {
    return fact != ZR_NULL && diagnostic_string_contains(fact->diagnosticMessage, fragment);
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

    if (analyzer->symbolTable->nameToSymbolsHashSet.pairPoolHead != ZR_NULL ||
        analyzer->symbolTable->nameToSymbolsHashSet.pairPoolActive != ZR_NULL ||
        analyzer->symbolTable->nameToSymbolsHashSet.pairPoolCapacity != 0 ||
        analyzer->symbolTable->nameToSymbolsHashSet.pairPoolUsed != 0) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  "Semantic Analyzer Creation and Free",
                  "Fresh symbol hash sets must start with an empty pair-pool state");
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

static void test_semantic_analyzer_projects_const_field_assignment_context(
        SZrState *state) {
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    const TZrChar *testCode =
        "class Meter {\n"
        "    pub const value: int;\n"
        "    pub @constructor(seed: int) {\n"
        "        this.value = seed;\n"
        "    }\n"
        "    pub fn update(next: int) {\n"
        "        this.value = next;\n"
        "    }\n"
        "}\n";
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrDiagnostic *constructorDiagnostic;
    SZrDiagnostic *methodDiagnostic;

    TEST_START("Semantic Analyzer Projects Const Field Assignment Context");
    TEST_INFO("Canonical const assignment",
              "Constructor initialization is accepted while a normal method assignment consumes the canonical parser diagnostic fact");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer,
                  "Semantic Analyzer Projects Const Field Assignment Context",
                  "Failed to create semantic analyzer");
        return;
    }

    sourceName = ZrCore_String_Create(
            state,
            "const_field_assignment_context_test.zr",
            strlen("const_field_assignment_context_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  "Semantic Analyzer Projects Const Field Assignment Context",
                  "Failed to parse or analyze the const field fixture");
        return;
    }

    constructorDiagnostic = find_diagnostic_by_code_and_line(
            analyzer, "const_assignment", 4);
    methodDiagnostic = find_diagnostic_by_code_and_line(
            analyzer, "const_assignment", 7);
    if (constructorDiagnostic != ZR_NULL ||
        methodDiagnostic == ZR_NULL ||
        methodDiagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        methodDiagnostic->descriptorId != 2012U ||
        methodDiagnostic->noFixReason !=
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
        !methodDiagnostic->relatedInformation.isValid ||
        methodDiagnostic->relatedInformation.length != 1U ||
        methodDiagnostic->fixes.isValid ||
        count_diagnostics_with_code(analyzer, "const_assignment") != 1U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  "Semantic Analyzer Projects Const Field Assignment Context",
                  "Expected one canonical const_assignment on the normal method and none in the constructor");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Projects Const Field Assignment Context");
}

static void test_semantic_analyzer_projects_all_const_assignment_target_kinds(
        SZrState *state) {
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    const TZrChar *testCode =
        "fn revise(const input: int) {\n"
        "    input = 2;\n"
        "    let frozen: int = 1;\n"
        "    frozen = 2;\n"
        "}\n"
        "class Limits {\n"
        "    pub static const max: int = 10;\n"
        "    pub fn update() {\n"
        "        Limits.max = 20;\n"
        "    }\n"
        "}\n";
    SZrString *sourceName;
    SZrAstNode *ast;

    TEST_START("Semantic Analyzer Projects All Const Assignment Target Kinds");
    TEST_INFO("Canonical const assignment targets",
              "Const parameters, locals, and static fields share descriptor 2012 without analyzer-owned policy");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer,
                  "Semantic Analyzer Projects All Const Assignment Target Kinds",
                  "Failed to create semantic analyzer");
        return;
    }

    sourceName = ZrCore_String_Create(
            state,
            "const_assignment_target_kinds_test.zr",
            strlen("const_assignment_target_kinds_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  "Semantic Analyzer Projects All Const Assignment Target Kinds",
                  "Failed to parse or analyze the const target fixture");
        return;
    }

    if (find_diagnostic_by_code_and_line(analyzer, "const_assignment", 2) == ZR_NULL ||
        find_diagnostic_by_code_and_line(analyzer, "const_assignment", 4) == ZR_NULL ||
        find_diagnostic_by_code_and_line(analyzer, "const_assignment", 9) == ZR_NULL ||
        count_diagnostics_with_code(analyzer, "const_assignment") != 3U) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  "Semantic Analyzer Projects All Const Assignment Target Kinds",
                  "Expected canonical diagnostics for const parameter, local, and static field assignments");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Projects All Const Assignment Target Kinds");
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
            "let math = import(\"zr.math\");"
            "fn pipeline(seed: float) {"
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
            "fn validateNumericAssignments(left: int, right: int) {\n"
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

#include "test_semantic_analyzer_exact_type_cases.h"
#include "test_semantic_analyzer_exact_type_diagnostic_cases.h"

static void test_semantic_analyzer_unannotated_function_records_exact_return_type(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Unannotated Function Records Exact Return Type");

    TEST_INFO("Exact function return metadata",
              "Unannotated functions with provable returns should store an exact return type in symbol metadata");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn make(seed: int) {\n"
            "    return seed + 0;\n"
            "}\n"
            "fn useIt() {\n"
            "    return make(1);\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "exact_return_type_symbol_test.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrString *functionName;
        SZrSymbol *functionSymbol;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Records Exact Return Type",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Records Exact Return Type",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Records Exact Return Type",
                      "Failed to analyze AST");
            return;
        }

        if (has_diagnostic_code(analyzer, "return_type_not_provable") ||
            has_diagnostic_code(analyzer, "cannot_infer_exact_type")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Records Exact Return Type",
                      "Provable unannotated returns should not emit exact-type diagnostics");
            return;
        }

        functionName = ZrCore_String_Create(state, "make", 4);
        functionSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, functionName, ZR_NULL);
        if (functionSymbol == ZR_NULL ||
            functionSymbol->typeInfo == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_INT(functionSymbol->typeInfo->baseType)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Records Exact Return Type",
                      "Expected function symbol metadata to store an exact int return type");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Unannotated Function Records Exact Return Type");
}

static void test_semantic_analyzer_reports_initializer_requires_annotation(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Initializer Requires Annotation");

    TEST_INFO("Strong typed local declaration diagnostics",
              "An untyped declaration without an initializer must emit initializer_requires_annotation instead of inventing a weak object type");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn probe() {\n"
            "    var missing;\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "initializer_requires_annotation_test.zr", 39);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Initializer Requires Annotation",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Initializer Requires Annotation",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Initializer Requires Annotation",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "initializer_requires_annotation", 2);
        if (diagnostic == ZR_NULL ||
            diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            diagnostic->descriptorId != 2017U ||
            diagnostic->noFixReason !=
                    ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
            diagnostic->location.start.column != 9 ||
            diagnostic->location.end.column != 16 ||
            diagnostic->fixes.isValid) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Initializer Requires Annotation",
                      "Expected initializer_requires_annotation on the untyped local variable");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Initializer Requires Annotation");
}

static void test_semantic_analyzer_reports_return_type_not_provable(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Return Type Not Provable");

    TEST_INFO("Strong typed return diagnostics",
              "An unannotated function with incompatible return branches must emit return_type_not_provable and must not synthesize a weak object return type");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn probe(flag: bool) {\n"
            "    if (flag) {\n"
            "        return 1;\n"
            "    }\n"
            "    return \"text\";\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "return_type_not_provable_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        SZrString *functionName;
        SZrSymbol *functionSymbol;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Type Not Provable",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Type Not Provable",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Type Not Provable",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "return_type_not_provable", 1);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Type Not Provable",
                      "Expected return_type_not_provable on the incompatible unannotated function");
            return;
        }

        functionName = ZrCore_String_Create(state, "probe", 5);
        functionSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, functionName, ZR_NULL);
        if (functionSymbol != ZR_NULL &&
            functionSymbol->typeInfo != ZR_NULL &&
            functionSymbol->typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
            functionSymbol->typeInfo->typeName == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Type Not Provable",
                      "Inference failure must not be materialized as a weak object return type");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Return Type Not Provable");
}

static void test_semantic_analyzer_accepts_all_path_return_chains(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Accepts All Path Return Chains");

    TEST_INFO("All-path return analysis",
              "Sequential if-return chains that cover every path must not trigger return_type_not_provable when all return values share the same exact type.");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn labelFor(value: int) {\n"
            "    if (value % 2 == 0) {\n"
            "        return \"even\";\n"
            "    }\n"
            "    if (value > 2) {\n"
            "        return \"odd_hi\";\n"
            "    }\n"
            "    return \"odd_lo\";\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "all_path_return_chain_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        SZrString *functionName;
        SZrSymbol *functionSymbol;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Accepts All Path Return Chains",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Accepts All Path Return Chains",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Accepts All Path Return Chains",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "return_type_not_provable", 1);
        if (diagnostic != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Accepts All Path Return Chains",
                      "Sequential return guards should be accepted as an all-path string return");
            return;
        }

        functionName = ZrCore_String_Create(state, "labelFor", 8);
        functionSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, functionName, ZR_NULL);
        if (functionSymbol == ZR_NULL ||
            functionSymbol->typeInfo == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_STRING(functionSymbol->typeInfo->baseType)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Accepts All Path Return Chains",
                      "All-path string returns should infer an exact string return type");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Accepts All Path Return Chains");
}

static void test_semantic_analyzer_exact_type_failure_surfaces_explicit_hover(
    SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Exact Type Failure Surfaces Explicit Hover");

    TEST_INFO("Strong typed hover failure surface",
              "Symbols that fail exact inference must surface 'cannot infer exact type' in hover instead of a weak object fallback");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn probe() {\n"
            "    var missing;\n"
            "    missing;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "exact_type_failure_detail_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover",
                      "Failed to analyze AST");
            return;
        }

        if (find_diagnostic_by_code_and_line(analyzer, "initializer_requires_annotation", 2) == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover",
                      "Expected initializer_requires_annotation for the untyped local symbol");
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "missing;", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover",
                      "Failed to get hover info for the exact-type failure symbol");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "cannot infer exact type") == ZR_NULL ||
            strstr(hoverText, "object") != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Exact Type Failure Surfaces Explicit Hover");
}

static void test_semantic_analyzer_unannotated_function_surfaces_exact_return_signature_detail(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail");

    TEST_INFO("Exact return signature detail",
              "Provable unannotated functions should expose their exact return type in hover instead of 'object'");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn make(seed: int) {\n"
            "    return seed + 0;\n"
            "}\n"
            "fn use(): void {\n"
            "    make(1);\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "exact_return_signature_detail_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail",
                      "Failed to analyze AST");
            return;
        }

        if (has_diagnostic_code(analyzer, "cannot_infer_exact_type") ||
            has_diagnostic_code(analyzer, "return_type_not_provable")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail",
                      "Provable unannotated function should not emit strong-type diagnostics");
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "make(1)", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail",
                      "Failed to get hover info for the unannotated function call");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "Signature: make(seed: int): int") == ZR_NULL ||
            strstr(hoverText, "object") != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Unannotated Function Surfaces Exact Return Signature Detail");
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
        "fn add(a: int): int { return a; } "
        "fn add(a: int, b: int): int { return a + b; }";
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

static void test_semantic_analyzer_records_reference_facts_with_precise_ranges(SZrState *state) {
    SZrTestTimer timer;
    const char *summary = "Semantic Analyzer Records Reference Facts With Precise Ranges";

    TEST_START(summary);
    TEST_INFO("Reference semantic facts",
              "Analyzing an identifier use should populate shared semantic reference facts with token ranges");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const char *source =
            "var value = 1;\n"
            "fn read(): int {\n"
            "    return value;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "reference_fact_test.zr", 22);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFileRange declarationRange =
            file_range_for_nth_substring_in_source(source, "value", 0, ZR_FALSE, sourceName);
        SZrFileRange useRange =
            file_range_for_nth_substring_in_source(source, "value", 1, ZR_FALSE, sourceName);
        const SZrSemanticReferenceFact *declarationFact;
        const SZrSemanticReferenceFact *fact;

        declarationRange.end.offset = declarationRange.start.offset + strlen("value");
        declarationRange.end.line = declarationRange.start.line;
        declarationRange.end.column = declarationRange.start.column + (TZrInt32)strlen("value");
        useRange.end.offset = useRange.start.offset + strlen("value");
        useRange.end.line = useRange.start.line;
        useRange.end.column = useRange.start.column + (TZrInt32)strlen("value");

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer, summary, "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Failed to analyze AST");
            return;
        }

        if (analyzer->semanticContext == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Semantic context was not attached");
            return;
        }

        declarationFact = ZrParser_SemanticFacts_FindReferenceAtPosition(analyzer->semanticContext, declarationRange);
        if (declarationFact == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Declaration reference fact lookup returned null");
            return;
        }

        if (declarationFact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            declarationFact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            declarationFact->range.start.offset != declarationRange.start.offset ||
            declarationFact->range.end.offset != declarationRange.end.offset) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Declaration reference fact did not preserve declaration kind, symbol id, and range");
            return;
        }

        fact = ZrParser_SemanticFacts_FindReferenceAtPosition(analyzer->semanticContext, useRange);
        if (fact == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Reference fact lookup returned null");
            return;
        }

        if (fact->kind != ZR_SEMANTIC_REFERENCE_READ ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            fact->range.start.offset != useRange.start.offset ||
            fact->range.end.offset != useRange.end.offset ||
            fact->declarationRange.start.offset == useRange.start.offset) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Reference fact did not preserve read kind, symbol id, use range, and declaration range");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, summary);
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
            "using (resource) { var message = `hello ${resource}`; }";
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
            analyzer->semanticContext->cleanupPlan.length < 1 ||
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

static void test_semantic_analyzer_records_owned_field_cleanup_metadata(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Records Owned Field Cleanup Metadata");

    TEST_INFO("Owned field semantic metadata",
              "Analyzing Unique<T>/Shared<T> fields should register field symbols and distinguish struct-value cleanup from instance-field cleanup");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource { }\n"
            "struct HandleBox { var handle: Unique<Resource>; }\n"
            "class Holder { var resource: Shared<Resource>; }";
        SZrString *sourceName = ZrCore_String_Create(state, "owned_field_semantic_test.zr", 27);
        SZrAstNode *ast;
        SZrString *handleName;
        SZrString *resourceName;
        SZrSymbol *handleSymbol;
        SZrSymbol *resourceSymbol;
        const SZrDeterministicCleanupStep *firstStep;
        const SZrDeterministicCleanupStep *secondStep;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Failed to create semantic analyzer");
            return;
        }

        ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Failed to analyze AST");
            return;
        }

        if (analyzer->semanticContext == ZR_NULL ||
            analyzer->semanticContext->cleanupPlan.length != 2) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
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
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Ownership-managed fields were not registered as semantic field symbols");
            return;
        }

        if (handleSymbol->typeInfo == ZR_NULL ||
            resourceSymbol->typeInfo == ZR_NULL ||
            handleSymbol->typeInfo->typeName == ZR_NULL ||
            resourceSymbol->typeInfo->typeName == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(handleSymbol->typeInfo->typeName), "Resource") != 0 ||
            strcmp(ZrCore_String_GetNativeString(resourceSymbol->typeInfo->typeName), "Resource") != 0 ||
            handleSymbol->typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
            resourceSymbol->typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            (handleSymbol->typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
             handleSymbol->typeInfo->typeName == ZR_NULL &&
             handleSymbol->typeInfo->elementTypes.length == 0) ||
            (resourceSymbol->typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
             resourceSymbol->typeInfo->typeName == ZR_NULL &&
             resourceSymbol->typeInfo->elementTypes.length == 0)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Explicit ownership field annotations must preserve the exact declared Resource type in metadata");
            return;
        }

        firstStep = (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&analyzer->semanticContext->cleanupPlan, 0);
        secondStep = (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&analyzer->semanticContext->cleanupPlan, 1);
        if (firstStep == ZR_NULL || secondStep == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Cleanup plan entries were missing");
            return;
        }

        if (firstStep->ownerRegionId == 0 || secondStep->ownerRegionId == 0) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
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
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Cleanup plan did not distinguish struct-value fields from instance fields");
            return;
        }

        if (!cleanup_plan_targets_symbol(analyzer->semanticContext, handleSymbol->semanticId) ||
            !cleanup_plan_targets_symbol(analyzer->semanticContext, resourceSymbol->semanticId)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Records Owned Field Cleanup Metadata",
                      "Cleanup plan did not target the registered ownership-managed field symbols");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Records Owned Field Cleanup Metadata");
}

// 测试获取诊断信息
static void test_semantic_analyzer_get_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    SZrStructuredDiagnostic structured;
    SZrDiagnostic *diagnostic;
    TEST_START("Semantic Analyzer Get Diagnostics");
    
    TEST_INFO("Get Diagnostics", "Getting diagnostic information from analyzer");
    
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to create semantic analyzer");
        return;
    }
    
    SZrFileRange location = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(10, 1, 10),
        ZR_NULL
    );

    ZrParser_StructuredDiagnostic_Init(&structured);
    TZrBool success = ZrParser_DiagnosticBuilder_Build(
            state,
            &structured,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "compiler_error",
            "Test error",
            "Test structured diagnostic",
            "No automatic edit is available") &&
        ZrParser_StructuredDiagnostic_SetNoFixReason(
            &structured,
            ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT);
    diagnostic = success
                         ? ZrLanguageServer_Diagnostic_FromStructured(
                                   state,
                                   &structured)
                         : ZR_NULL;
    ZrParser_StructuredDiagnostic_Free(state, &structured);
    if (diagnostic == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  "Semantic Analyzer Get Diagnostics",
                  "Failed to project structured diagnostic");
        return;
    }

    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);

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

static void test_semantic_analyzer_get_symbol_at_resolves_local_references(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Symbol At Resolves Local References");

    TEST_INFO("Local reference resolution",
              "Resolving a symbol at a local identifier reference should return the matching parameter/local declaration instead of only global definitions");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "var seed = 0.0;\n"
            "fn helper(seed: float) {\n"
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

        position = file_range_for_nth_substring(testCode, "seed", 2, ZR_FALSE);
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
            char detail[256];
            char lookupDetail[64];
            char referenceDetail[64];
            char queryRangeDetail[64];
            describe_symbol(detail, sizeof(detail), symbol);
            describe_symbol(lookupDetail, sizeof(lookupDetail), lookupSymbol);
            describe_symbol(referenceDetail,
                            sizeof(referenceDetail),
                            reference != ZR_NULL ? reference->symbol : ZR_NULL);
            describe_file_range(queryRangeDetail, sizeof(queryRangeDetail), position);
            snprintf(detail + strlen(detail),
                     sizeof(detail) - strlen(detail),
                     " lookup=%s refHit=%s allRefs=%zu query=%s",
                     lookupDetail,
                     referenceDetail,
                     (size_t)analyzer->referenceTracker->allReferences.length,
                     queryRangeDetail);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Get Symbol At Resolves Local References",
                      detail);
            return;
        }

        position = file_range_for_nth_substring(testCode, "localValue", 1, ZR_FALSE);
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

static void test_semantic_analyzer_local_symbols_surface_rich_hover(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Local Symbols Surface Rich Hover");

    TEST_INFO("Local symbol hover detail",
              "Local variables should surface type and access detail in hover results");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn helper(seed: float) {\n"
            "    var localValue: float = seed + 1.0;\n"
            "    return localValue;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "rich_hover_local_symbols_test.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover",
                      "Failed to analyze AST");
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "localValue", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover",
                      "Failed to get hover info for local variable reference");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "localValue") == ZR_NULL ||
            strstr(hoverText, "float") == ZR_NULL ||
            strstr(hoverText, "private") == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Local Symbols Surface Rich Hover",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Local Symbols Surface Rich Hover");
}

static void test_semantic_analyzer_generic_function_symbols_surface_signature_detail(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Generic Function Symbols Surface Signature Detail");

    TEST_INFO("Generic hover detail",
              "Generic function symbols should expose explicit generic and passing-mode signature text in hover");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn swap<T>(value: ref T): T {\n"
            "    return value;\n"
            "}\n"
            "fn use(): void {\n"
            "    var slot: int = 1;\n"
            "    swap<int>(ref slot);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "generic_signature_hover_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

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

        if (count_diagnostics_with_code(analyzer, "compiler_error") != 0 ||
            analyzer->diagnostics.length != 0) {
            SZrDiagnostic **diagPtr = analyzer->diagnostics.length > 0
                                              ? (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, 0)
                                              : ZR_NULL;
            SZrDiagnostic *diag = diagPtr != ZR_NULL ? *diagPtr : ZR_NULL;
            TZrChar message[256];

            snprintf(message,
                     sizeof(message),
                     "%s: %s (line %d)",
                     diag != ZR_NULL && diag->code != ZR_NULL
                         ? ZrCore_String_GetNativeString(diag->code)
                         : "<no code>",
                     diag != ZR_NULL && diag->message != ZR_NULL
                         ? ZrCore_String_GetNativeString(diag->message)
                         : "<no message>",
                     diag != ZR_NULL ? diag->location.start.line : -1);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      message);
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "swap", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
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
            strstr(hoverText, "value: ref T") == ZR_NULL ||
            strstr(hoverText, "Access: public") == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Function Symbols Surface Signature Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Generic Function Symbols Surface Signature Detail");
}

static void test_semantic_analyzer_generic_call_infers_type_argument_without_explicit_close(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close");

    TEST_INFO("Call-site generic inference",
              "id<T>(x: T): T with id(1) should close T to int so the local binding hover exposes int");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn id<T>(x: T): T {\n"
            "    return x;\n"
            "}\n"
            "fn use(): void {\n"
            "    var inferredFromCall = id(1);\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "generic_call_inference_without_close_test.zr", 44);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close",
                      "Failed to analyze AST");
            return;
        }

        if (count_diagnostics_with_code(analyzer, "compiler_error") != 0 || analyzer->diagnostics.length != 0) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close",
                      "Generic call inference should not emit diagnostics");
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "inferredFromCall", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close",
                      "Failed to get hover info for inferred local");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "inferredFromCall") == ZR_NULL ||
            strstr(hoverText, "int") == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Generic Call Infers Type Argument Without Explicit Close");
}

static void test_semantic_analyzer_function_signatures_preserve_ownership_qualifiers(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Function Signatures Preserve Ownership Qualifiers");

    TEST_INFO("Ownership-aware hover detail",
              "Function hover should preserve AST ownership qualifiers on parameters and return types");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Hero {\n"
            "    pub @constructor() {\n"
            "    }\n"
            "}\n"
            "fn take(seed: Shared<Hero>): Unique<Hero> {\n"
            "    return own Hero();\n"
            "}\n"
            "fn use(sharedSeed: Shared<Hero>): void {\n"
            "    var hero = take(sharedSeed);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_signature_hover_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;
        SZrString *takeName;
        SZrString *seedName;
        SZrSymbol *takeSymbol;
        SZrSymbol *seedSymbol;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers",
                      "Failed to analyze AST");
            return;
        }

        takeName = ZrCore_String_Create(state, "take", 4);
        seedName = ZrCore_String_Create(state, "seed", 4);
        takeSymbol = lookup_symbol_any_scope_by_type(state, analyzer->symbolTable, takeName, ZR_SYMBOL_FUNCTION);
        seedSymbol = lookup_symbol_any_scope_by_type(state, analyzer->symbolTable, seedName, ZR_SYMBOL_PARAMETER);
        if (takeSymbol == ZR_NULL ||
            seedSymbol == ZR_NULL ||
            takeSymbol->typeInfo == ZR_NULL ||
            takeSymbol->typeInfo->typeName == ZR_NULL ||
            seedSymbol->typeInfo == ZR_NULL ||
            seedSymbol->typeInfo->typeName == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(takeSymbol->typeInfo->typeName), "Hero") != 0 ||
            strcmp(ZrCore_String_GetNativeString(seedSymbol->typeInfo->typeName), "Hero") != 0 ||
            takeSymbol->typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
            seedSymbol->typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_SHARED) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers",
                      "Function return and parameter metadata must preserve the exact declared Hero type and ownership qualifiers");
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "take(sharedSeed)", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers",
                      "Failed to get hover info for ownership-qualified function call");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "Signature: take(") == ZR_NULL ||
            strstr(hoverText, "seed: Shared<Hero>") == ZR_NULL ||
            strstr(hoverText, "): Unique<Hero>") == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Function Signatures Preserve Ownership Qualifiers");
}

static void test_semantic_analyzer_signature_type_display_fails_closed_without_canonical_fact(
        SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact");

    TEST_INFO("Canonical declaration signature display",
              "Hover signatures must not render an unresolved AST type as an exact type");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
                "fn redact(value: MissingType): MissingType {\n"
                "    return value;\n"
                "}\n"
                "fn use(): void {\n"
                "    redact(null);\n"
                "}\n";
        SZrString *sourceName = ZrCore_String_Create(
                state,
                "signature_type_display_fail_closed_test.zr",
                strlen("signature_type_display_fail_closed_test.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const TZrChar *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact",
                      "Failed to create semantic analyzer");
            return;
        }
        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact",
                      "Failed to parse test code");
            return;
        }
        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact",
                      "Failed to analyze unresolved declaration type fixture");
            return;
        }
        hoverPosition = file_range_for_nth_substring(testCode, "redact(null)", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact",
                      "Failed to get hover information for unresolved declaration type");
            return;
        }
        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL ||
            strstr(hoverText, "value: cannot infer exact type") == ZR_NULL ||
            strstr(hoverText, "): cannot infer exact type") == ZR_NULL ||
            strstr(hoverText, "MissingType") != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Signature Type Display Fails Closed Without Canonical Fact");
}

static void test_semantic_analyzer_generic_type_symbols_surface_signature_detail(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Generic Type Symbols Surface Signature Detail");

    TEST_INFO("Generic type hover detail",
              "Generic class and interface symbols should surface inheritance, const generics, variance, and where clauses in hover");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "interface Producer<out T> {\n"
            "    fn next(): T;\n"
            "}\n"
            "class Item {\n"
            "    pub @constructor() { }\n"
            "}\n"
            "class Derived<T, const N: int> : Producer<T>\n"
            "where T: class, new() {\n"
            "}\n"
            "fn use(): void {\n"
            "    var value: Derived<Item, 4> = null;\n"
            "    value;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "generic_type_signature_hover_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange derivedHoverPosition;
        SZrFileRange producerHoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
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

        if (count_diagnostics_with_code(analyzer, "compiler_error") != 0 ||
            analyzer->diagnostics.length != 0) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      "Generic type signature analysis should not emit diagnostics");
            return;
        }

        derivedHoverPosition = file_range_for_nth_substring(testCode, "Derived", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, derivedHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
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
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Generic Type Symbols Surface Signature Detail",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Generic Type Symbols Surface Signature Detail");
}

static void test_semantic_analyzer_closed_generic_receiver_calls_stay_local_to_type_metadata(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata");

    TEST_INFO("Closed generic receiver calls",
              "Calling members on a closed generic local such as Box<int> should resolve through the local type prototype graph instead of falling back to import metadata loading");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "class Matrix<T, const N: int> { }\n"
            "class Box<T> {\n"
            "    fn shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
            "}\n"
            "fn use(): void {\n"
            "    var box = new Box<int>();\n"
            "    var m = new Matrix<int, 2 + 2>();\n"
            "    var shaped = box.shape(m);\n"
            "    shaped;\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "closed_generic_receiver_metadata_test.zr", 40);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange hoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const TZrChar *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata",
                      "Failed to analyze AST");
            return;
        }

        if (count_diagnostics_with_code(analyzer, "compiler_error") != 0 ||
            analyzer->diagnostics.length != 0) {
            SZrDiagnostic **diagPtr = analyzer->diagnostics.length > 0
                                              ? (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, 0)
                                              : ZR_NULL;
            SZrDiagnostic *diag = diagPtr != ZR_NULL ? *diagPtr : ZR_NULL;
            TZrChar message[256];

            snprintf(message,
                     sizeof(message),
                     "%s: %s (line %d)",
                     diag != ZR_NULL && diag->code != ZR_NULL
                         ? ZrCore_String_GetNativeString(diag->code)
                         : "<no code>",
                     diag != ZR_NULL && diag->message != ZR_NULL
                         ? ZrCore_String_GetNativeString(diag->message)
                         : "<no message>",
                     diag != ZR_NULL ? diag->location.start.line : -1);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata",
                      message);
            return;
        }

        hoverPosition = file_range_for_nth_substring(testCode, "shaped;", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata",
                      "Failed to collect hover info for shaped local");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "Resolved Type: Matrix<int, 4>") == ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Closed Generic Receiver Calls Stay Local To Type Metadata");
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
            "    fn accept(value: T): void;\n"
            "}\n"
            "interface Consumer<in T> {\n"
            "    fn next(): T;\n"
            "}\n"
            "interface Store<out T> {\n"
            "    pub var value: T;\n"
            "}\n"
            "interface OutputProperty<out T> {\n"
            "    pub property item: T { set; }\n"
            "}\n"
            "interface InputProperty<in T> {\n"
            "    pub property item: T { get; }\n"
            "}\n"
            "interface Sink<in T> {\n"
            "    fn accept(value: T): void;\n"
            "}\n"
            "interface Nested<out T> {\n"
            "    fn next(): Sink<T>;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "invalid_variance_positions_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagAccept;
        SZrDiagnostic *diagReturn;
        SZrDiagnostic *diagField;
        SZrDiagnostic *diagSetter;
        SZrDiagnostic *diagGetter;
        SZrDiagnostic *diagNested;
        SZrDiagnostic *varianceDiagnostics[6];
        TZrBool hasCanonicalProjection = ZR_TRUE;

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
        varianceDiagnostics[0] = diagAccept;
        varianceDiagnostics[1] = diagReturn;
        varianceDiagnostics[2] = diagField;
        varianceDiagnostics[3] = diagSetter;
        varianceDiagnostics[4] = diagGetter;
        varianceDiagnostics[5] = diagNested;
        for (TZrSize index = 0U; index < 6U; index++) {
            SZrDiagnostic *diagnostic = varianceDiagnostics[index];
            if (diagnostic == ZR_NULL || diagnostic->descriptorId != 2013U ||
                diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
                diagnostic->noFixReason !=
                        ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
                !diagnostic->relatedInformation.isValid ||
                diagnostic->relatedInformation.length != 1U ||
                diagnostic->fixes.isValid) {
                hasCanonicalProjection = ZR_FALSE;
                break;
            }
        }
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
            !diagnostic_message_contains(diagNested, "nested") ||
            !hasCanonicalProjection) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid Interface Variance Positions",
                      "Expected six canonical invalid_variance diagnostics with related declarations and no-fix disposition");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Invalid Interface Variance Positions");
}

static void test_semantic_analyzer_projects_interface_const_field_contract(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Projects Interface Const Field Contract");

    TEST_INFO("Interface const field diagnostics",
              "Non-const and missing implementations should project parser-owned diagnostics with exact ranges and no-fix disposition");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "interface Versioned {\n"
            "    pub const version: int;\n"
            "}\n"
            "class MutableVersion: Versioned {\n"
            "    pub var version: int;\n"
            "}\n"
            "class MissingVersion: Versioned {\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(
            state,
            "interface_const_field_contract_test.zr",
            strlen("interface_const_field_contract_test.zr"));
        SZrAstNode *ast = ZrParser_Parse(
            state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *mutableDiagnostic;
        SZrDiagnostic *missingDiagnostic;

        if (analyzer == ZR_NULL || ast == ZR_NULL ||
            !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            if (ast != ZR_NULL) {
                ZrParser_Ast_Free(state, ast);
            }
            if (analyzer != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            }
            TEST_FAIL(timer,
                      "Semantic Analyzer Projects Interface Const Field Contract",
                      "Failed to analyze interface const field fixture");
            return;
        }

        mutableDiagnostic = find_diagnostic_by_code_and_line(
            analyzer, "const_interface_mismatch", 5);
        missingDiagnostic = find_diagnostic_by_code_and_line(
            analyzer, "const_interface_mismatch", 7);
        if (count_diagnostics_with_code(analyzer, "const_interface_mismatch") != 2 ||
            mutableDiagnostic == ZR_NULL || missingDiagnostic == ZR_NULL ||
            mutableDiagnostic->descriptorId != 2014U ||
            missingDiagnostic->descriptorId != 2014U ||
            mutableDiagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            missingDiagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            mutableDiagnostic->location.start.column != 13 ||
            missingDiagnostic->location.start.column != 7 ||
            !mutableDiagnostic->relatedInformation.isValid ||
            mutableDiagnostic->relatedInformation.length != 1U ||
            !missingDiagnostic->relatedInformation.isValid ||
            missingDiagnostic->relatedInformation.length != 1U ||
            mutableDiagnostic->noFixReason !=
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
            missingDiagnostic->noFixReason !=
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
            mutableDiagnostic->fixes.isValid || missingDiagnostic->fixes.isValid) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Projects Interface Const Field Contract",
                      "Expected two canonical descriptor-2014 diagnostics with exact primary/related ranges");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Projects Interface Const Field Contract");
}

static void test_semantic_analyzer_preserves_owner_generic_context_in_member_signatures(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Preserves Owner Generic Context In Member Signatures");

    TEST_INFO("Owner generic member signatures",
              "Class member signatures should resolve owner and method generic parameters without emitting compiler diagnostics");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "class Matrix<T, const N: int> { }\n"
            "class Box<T> {\n"
            "    fn shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> {\n"
            "        return value;\n"
            "    }\n"
            "}\n"
            "fn use(): void {\n"
            "    var box = new Box<int>();\n"
            "    var value = new Matrix<int, 4>();\n"
            "    box.shape(value);\n"
            "}\n";
        SZrString *sourceName =
            ZrCore_String_Create(state, "owner_generic_member_signature_test.zr", 38);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Failed to analyze AST");
            return;
        }

        if (count_diagnostics_with_code(analyzer, "compiler_error") != 0 ||
            analyzer->diagnostics.length != 0) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Preserves Owner Generic Context In Member Signatures",
                      "Owner/member generic signature analysis should not emit diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Preserves Owner Generic Context In Member Signatures");
}

static void test_semantic_analyzer_records_reachability_facts_for_unreachable_statements(SZrState *state) {
    SZrTestTimer timer;
    const char *summary = "Semantic Analyzer Records Reachability Facts For Unreachable Statements";

    TEST_START(summary);
    TEST_INFO("Reachability semantic facts",
              "Analyzing statements after return/throw should record shared reachability facts with precise causes");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn returnFlow() {\n"
            "    return 1;\n"
            "    var deadAfterReturn = 2;\n"
            "}\n"
            "fn throwFlow() {\n"
            "    throw \"boom\";\n"
            "    var deadAfterThrow = 3;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "reachability_fact_test.zr", 25);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange deadAfterReturnRange =
            file_range_for_nth_substring_in_source(
                testCode, "deadAfterReturn", 0, ZR_FALSE, sourceName);
        SZrFileRange deadAfterThrowRange =
            file_range_for_nth_substring_in_source(
                testCode, "deadAfterThrow", 0, ZR_FALSE, sourceName);
        const SZrSemanticReachabilityFact *returnFact;
        const SZrSemanticReachabilityFact *throwFact;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer, summary, "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Failed to analyze AST");
            return;
        }

        if (analyzer->semanticContext == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Semantic context was not attached");
            return;
        }

        returnFact =
            ZrParser_SemanticFacts_FindReachabilityAtPosition(analyzer->semanticContext, deadAfterReturnRange);
        throwFact =
            ZrParser_SemanticFacts_FindReachabilityAtPosition(analyzer->semanticContext, deadAfterThrowRange);
        if (returnFact == ZR_NULL || throwFact == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Reachability fact lookup returned null for unreachable statements");
            return;
        }

        if (returnFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
            returnFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_RETURN ||
            returnFact->causeNode == ZR_NULL ||
            throwFact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
            throwFact->cause != ZR_SEMANTIC_REACHABILITY_AFTER_THROW ||
            throwFact->causeNode == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Reachability facts did not preserve unreachable state, cause, and cause node");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_records_short_circuit_logical_facts(SZrState *state) {
    SZrTestTimer timer;
    const char *summary = "Semantic Analyzer Records Short Circuit Logical Facts";

    TEST_START(summary);
    TEST_INFO("Logical semantic facts",
              "Analyzing deterministic boolean short-circuit expressions should record the known result and skipped branch");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn shorts() {\n"
            "    var skippedOr = true || false;\n"
            "    var skippedAnd = false && true;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "logical_fact_test.zr", 20);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange skippedOrRange =
            file_range_for_nth_substring_in_source(
                testCode, "false;", 0, ZR_FALSE, sourceName);
        SZrFileRange skippedAndRange =
            file_range_for_nth_substring_in_source(
                testCode, "true;", 0, ZR_FALSE, sourceName);
        const SZrSemanticReachabilityFact *orReachability;
        const SZrSemanticReachabilityFact *andReachability;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer, summary, "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Failed to analyze AST");
            return;
        }

        if (analyzer->semanticContext == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Semantic context was not attached");
            return;
        }

        if (count_logical_facts_with_known_value(analyzer->semanticContext,
                                                 ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT,
                                                 ZR_TRUE) < 1 ||
            count_logical_facts_with_known_value(analyzer->semanticContext,
                                                 ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT,
                                                 ZR_FALSE) < 1) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Short-circuit logical facts did not record both known true and known false results");
            return;
        }

        orReachability =
            ZrParser_SemanticFacts_FindReachabilityAtPosition(analyzer->semanticContext, skippedOrRange);
        andReachability =
            ZrParser_SemanticFacts_FindReachabilityAtPosition(analyzer->semanticContext, skippedAndRange);
        if (orReachability == ZR_NULL ||
            andReachability == ZR_NULL ||
            orReachability->cause != ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT ||
            andReachability->cause != ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer, summary, "Short-circuit reachability facts did not preserve skipped right-hand branches");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_warns_on_unreachable_statements_after_return_or_throw(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Warns On Unreachable Statements After Return Or Throw");

    TEST_INFO("Deterministic unreachable statements",
              "Analyzing statements after return/throw should emit warning diagnostics instead of silently treating them as reachable");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn returnFlow() {\n"
            "    return 1;\n"
            "    var deadAfterReturn = 2;\n"
            "}\n"
            "fn throwFlow() {\n"
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
            "fn branching() {\n"
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

        deadElseDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_code", 4);
        deadThenDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_code", 7);
        if (count_diagnostics_with_code(analyzer, "unreachable_code") < 2 ||
            count_diagnostics_with_code(analyzer, "unreachable_branch") != 0 ||
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
            "fn shorts() {\n"
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

        orDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_code", 2);
        andDiag = find_diagnostic_by_code_and_line(analyzer, "unreachable_code", 3);
        if (count_diagnostics_with_code(analyzer, "unreachable_code") < 2 ||
            count_diagnostics_with_code(analyzer, "short_circuit_unreachable") != 0 ||
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
              "Analyzing an explicit Unique<T> declaration initialized from Shared<T> should emit a specific ownership_mismatch diagnostic and fact");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "var borrowed: Shared<Resource>;\n"
            "var owned: Unique<Resource> = borrowed;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_decl_mismatch_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrSemanticOwnershipFact *fact;

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

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "ownership_mismatch", 4);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Expected ownership_mismatch diagnostic on the explicit unique<Resource> initializer");
            return;
        }
        if (!diagnostic_message_contains(diagnostic, "Ownership") ||
            !diagnostic_cause_contains(diagnostic, "Shared<Resource>") ||
            !diagnostic_suggestion_contains(diagnostic, "Unique<Resource>")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Expected ownership diagnostic to include message, cause, and suggestion");
            return;
        }
        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_in_source(
                    testCode, "borrowed", 1, ZR_FALSE, sourceName));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            !fact->isViolation ||
            !ownership_fact_message_contains(fact, "Ownership")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Declared Ownership Initializer Mismatch",
                      "Expected ownership mismatch fact at the initializer expression");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Declared Ownership Initializer Mismatch");
}

static void test_semantic_analyzer_reports_assignment_ownership_mismatch(SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Reports Assignment Ownership Mismatch";
    const TZrChar *testCode =
            "resource class Resource {}\n"
            "fn assign(target: Unique<Resource>, source: Shared<Resource>) {\n"
            "    target = source;\n"
            "}\n";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange sourceRange;
    SZrDiagnostic *diagnostic;
    const SZrSemanticOwnershipFact *fact;

    TEST_START(summary);
    TEST_INFO("Ownership compatibility in assignment expressions",
              "Assigning Shared<T> to Unique<T> should project the parser diagnostic and exact ownership fact");

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "ownership_assignment_mismatch_test.zr",
            strlen("ownership_assignment_mismatch_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze assignment fixture");
        return;
    }

    sourceRange = file_range_for_nth_substring_in_source(
            testCode, "source;", 0U, ZR_FALSE, sourceName);
    diagnostic = find_diagnostic_by_code_and_line(
            analyzer, "ownership_mismatch", sourceRange.start.line);
    fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext, sourceRange);
    if (diagnostic == ZR_NULL ||
        diagnostic->descriptorId != 2008U ||
        diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        diagnostic->location.start.offset != sourceRange.start.offset ||
        diagnostic->location.end.offset !=
                sourceRange.start.offset + strlen("source") ||
        diagnostic->noFixReason !=
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
        (diagnostic->fixes.isValid && diagnostic->fixes.length != 0U) ||
        fact == ZR_NULL ||
        fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
        fact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
        !fact->isViolation ||
        fact->range.start.offset != sourceRange.start.offset ||
        fact->range.end.offset !=
                sourceRange.start.offset + strlen("source")) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected descriptor 2008 and an exact Shared ownership fact at the assignment source");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_reports_owner_to_plain_initializer_escape(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Owner To Plain Initializer Escape");

    TEST_INFO("Ownership compatibility in variable declarations",
              "Analyzing Unique<T>/Shared<T> values assigned to plain GC declarations should emit owner_to_plain_escape diagnostics and facts");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "var owner: Unique<Resource>;\n"
            "var plainFromUnique: Resource = owner;\n"
            "var sharedOwner: Shared<Resource>;\n"
            "var plainFromShared: Resource = sharedOwner;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_plain_escape_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *uniqueDiagnostic;
        SZrDiagnostic *sharedDiagnostic;
        const SZrSemanticOwnershipFact *uniqueFact;
        const SZrSemanticOwnershipFact *sharedFact;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Owner To Plain Initializer Escape",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Owner To Plain Initializer Escape",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Owner To Plain Initializer Escape",
                      "Failed to analyze AST");
            return;
        }

        uniqueDiagnostic = find_diagnostic_by_code_and_line(analyzer, "owner_to_plain_escape", 4);
        sharedDiagnostic = find_diagnostic_by_code_and_line(analyzer, "owner_to_plain_escape", 6);
        if (uniqueDiagnostic == ZR_NULL || sharedDiagnostic == ZR_NULL ||
            uniqueDiagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            sharedDiagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Owner To Plain Initializer Escape",
                      "Expected owner_to_plain_escape diagnostics for implicit owner-to-plain assignments");
            return;
        }
        if (!diagnostic_suggestion_contains(uniqueDiagnostic, "ownership wrapper") ||
            !diagnostic_suggestion_contains(sharedDiagnostic, "ownership wrapper")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Owner To Plain Initializer Escape",
                      "Expected owner-to-plain diagnostics to suggest explicit detach");
            return;
        }
        uniqueFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_in_source(
                    testCode, "owner;", 0, ZR_FALSE, sourceName));
        sharedFact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_in_source(
                    testCode, "sharedOwner;", 0, ZR_FALSE, sourceName));
        if (uniqueFact == ZR_NULL || sharedFact == ZR_NULL ||
            uniqueFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            sharedFact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            uniqueFact->qualifier != ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
            sharedFact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            !uniqueFact->isViolation ||
            !sharedFact->isViolation) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Owner To Plain Initializer Escape",
                      "Expected owner-to-plain ownership facts at both initializer expressions");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Owner To Plain Initializer Escape");
}

static void test_semantic_analyzer_reports_return_ownership_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Return Ownership Mismatch");

    TEST_INFO("Ownership compatibility in return statements",
              "Analyzing a function that promises Unique<T> but returns Shared<T> should emit a specific ownership_mismatch diagnostic and fact");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "fn upgrade(resource: Shared<Resource>): Unique<Resource> {\n"
            "    return resource;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_return_mismatch_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrSemanticOwnershipFact *fact;

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

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "ownership_mismatch", 4);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Expected ownership_mismatch diagnostic on the incompatible return ownership");
            return;
        }
        if (!diagnostic_cause_contains(diagnostic, "Shared<Resource>") ||
            !diagnostic_suggestion_contains(diagnostic, "Unique<Resource>")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Expected return ownership diagnostic to include cause and suggestion");
            return;
        }
        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_in_source(
                    testCode, "resource", 2, ZR_FALSE, sourceName));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            !fact->isViolation) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Return Ownership Mismatch",
                      "Expected ownership mismatch fact at the return expression");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Return Ownership Mismatch");
}

static void test_semantic_analyzer_reports_borrowed_return_escape(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Borrowed Return Escape");

    TEST_INFO("Ownership borrow escape in return statements",
              "Analyzing a function that returns ref owner should emit a borrow_escape diagnostic and ownership fact");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "fn leak(resource: Unique<Resource>): ref readonly Resource {\n"
            "    return ref resource;\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_borrowed_return_escape_test.zr", 39);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrSemanticOwnershipFact *fact;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "borrow_escape", 4);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Expected borrow_escape diagnostic for borrowed return escape");
            return;
        }
        if (!diagnostic_message_contains(diagnostic, "Borrowed value cannot escape") ||
            !diagnostic_cause_contains(diagnostic, "ref") ||
            !diagnostic_suggestion_contains(diagnostic, "Return")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Expected borrow escape diagnostic to include message, cause, and suggestion");
            return;
        }
        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_in_source(
                    testCode, "resource;", 0, ZR_FALSE, sourceName));
        if (fact == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Expected borrow escape ownership fact to cover the borrowed source");
            return;
        }
        if (fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_BORROWED ||
            !fact->isViolation) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Expected borrow escape ownership fact to be a borrowed violation");
            return;
        }
        if (!ownership_fact_message_contains(fact, "Borrowed")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Borrowed Return Escape",
                      "Expected borrow escape ownership fact to keep the diagnostic message");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Borrowed Return Escape");
}

static void test_semantic_analyzer_reports_function_argument_ownership_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Function Argument Ownership Mismatch");

    TEST_INFO("Ownership compatibility in function calls",
              "Analyzing a direct function call that passes Shared<T> into Unique<T> should emit an ownership_mismatch diagnostic and fact");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "fn consume(resource: Unique<Resource>) {\n"
            "}\n"
            "fn run(resource: Shared<Resource>) {\n"
            "    consume(resource);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_call_mismatch_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrSemanticOwnershipFact *fact;

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

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "ownership_mismatch", 6);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Function Argument Ownership Mismatch",
                      "Expected ownership_mismatch diagnostic on the incompatible function argument ownership");
            return;
        }
        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_offset_in_source(
                    testCode,
                    "consume(resource);",
                    0,
                    strlen("consume("),
                    sourceName));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            !fact->isViolation) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Function Argument Ownership Mismatch",
                      "Expected ownership mismatch fact at the function argument");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Function Argument Ownership Mismatch");
}

static void test_semantic_analyzer_reports_weak_argument_requires_wake(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Weak Argument Requires Wake");

    TEST_INFO("Ownership compatibility in function calls",
              "Passing Weak<T> through an explicit ref call must emit a weak_value_requires_wake diagnostic and fact");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "fn observe(resource: ref readonly Resource): int { return 0; }\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    observe(ref watcher);\n"
            "    return 0;\n"
            "}\n";
        TZrChar sourceNameText[] = "ownership_weak_wake_required_test.zr";
        SZrString *sourceName = ZrCore_String_Create(
                state, sourceNameText, strlen(sourceNameText));
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrSemanticOwnershipFact *fact;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Weak Argument Requires Wake",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Weak Argument Requires Wake",
                      "Failed to parse test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Weak Argument Requires Wake",
                      "Failed to analyze AST");
            return;
        }

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "weak_value_requires_wake", 6);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Weak Argument Requires Wake",
                      "Expected weak_value_requires_wake diagnostic for an explicit weak ref argument");
            return;
        }
        if (!diagnostic_message_contains(diagnostic, "Weak value must be woken") ||
            !diagnostic_cause_contains(diagnostic, "Weak<T>") ||
            !diagnostic_suggestion_contains(diagnostic, "wake(")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Weak Argument Requires Wake",
                      "Expected weak wake diagnostic to include message, cause, and suggestion");
            return;
        }
        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_offset_in_source(
                    testCode,
                    "observe(ref watcher);",
                    0,
                    strlen("observe(ref "),
                    sourceName));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_WEAK ||
            !fact->isViolation ||
            !ownership_fact_message_contains(fact, "Weak")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Weak Argument Requires Wake",
                      "Expected weak wake ownership fact at the function argument");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Weak Argument Requires Wake");
}

static void test_semantic_analyzer_reports_method_argument_ownership_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Reports Method Argument Ownership Mismatch");

    TEST_INFO("Ownership compatibility in method calls",
              "Analyzing an instance method call that passes Shared<T> into Unique<T> should emit an ownership_mismatch diagnostic and fact");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "resource class Resource {\n"
            "}\n"
            "class ResourceBox {\n"
            "    fn consume(resource: Unique<Resource>): int {\n"
            "        return 0;\n"
            "    }\n"
            "}\n"
            "fn run(box: ResourceBox, resource: Shared<Resource>) {\n"
            "    box.consume(resource);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_method_call_mismatch_test.zr", 38);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrDiagnostic *diagnostic;
        const SZrSemanticOwnershipFact *fact;

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

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "ownership_mismatch", 9);
        if (diagnostic == ZR_NULL || diagnostic->severity != ZR_DIAGNOSTIC_ERROR) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Method Argument Ownership Mismatch",
                      "Expected ownership_mismatch diagnostic on the incompatible method argument ownership");
            return;
        }
        fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
                analyzer->semanticContext,
                file_range_for_nth_substring_offset_in_source(
                    testCode,
                    "box.consume(resource);",
                    0,
                    strlen("box.consume("),
                    sourceName));
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_OWNERSHIP_FACT_ERROR ||
            fact->qualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            !fact->isViolation) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Method Argument Ownership Mismatch",
                      "Expected ownership mismatch fact at the method argument");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Method Argument Ownership Mismatch");
}

static void test_semantic_analyzer_projects_unresolved_member_query_diagnostic(
        SZrState *state) {
    const TZrChar *summary =
            "Semantic Analyzer Projects Unresolved Member Query Diagnostic";
    const TZrChar *testCode =
            "class Meter {\n"
            "    var value: int;\n"
            "}\n"
            "fn read(meter: Meter): int {\n"
            "    return meter.missingField;\n"
            "}\n";
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange memberRange;
    const SZrSemanticReferenceFact *fact;
    SZrDiagnostic *diagnostic;
    SZrTestTimer timer;

    TEST_START(summary);
    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(
            state,
            "unresolved_member_query_diagnostic_test.zr",
            strlen("unresolved_member_query_diagnostic_test.zr"));
    ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer, summary, "Failed to analyze unresolved member fixture");
        return;
    }

    memberRange = file_range_for_nth_substring_in_source(
            testCode, "missingField", 0U, ZR_FALSE, sourceName);
    fact = ZrParser_SemanticFacts_FindReferenceAtPosition(
            analyzer->semanticContext, memberRange);
    diagnostic = find_diagnostic_by_code_and_line(
            analyzer, "member_not_found", memberRange.start.line);
    if (fact == ZR_NULL ||
        fact->kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
        fact->isResolved ||
        fact->name == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeStringShort(fact->name), "missingField") != 0 ||
        diagnostic == ZR_NULL ||
        diagnostic->descriptorId != 2016U ||
        diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
        fact->range.start.offset != memberRange.start.offset ||
        fact->range.end.offset !=
                memberRange.start.offset + strlen("missingField") ||
        diagnostic->location.start.offset != fact->range.start.offset ||
        diagnostic->location.end.offset != fact->range.end.offset ||
        diagnostic->noFixReason !=
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION ||
        (diagnostic->fixes.isValid && diagnostic->fixes.length != 0U)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Expected query-projected member_not_found at the unresolved fact range");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
}

static void test_semantic_analyzer_resolves_overloads_for_call_compatibility(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Resolves Overloads For Call Compatibility");

    TEST_INFO("Overload-aware call compatibility",
              "Analyzing overload sets should keep the matching call green and still diagnose the unmatched call");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "fn mix(value: int): int {\n"
            "    return value;\n"
            "}\n"
            "fn mix(value: string): string {\n"
            "    return value;\n"
            "}\n"
            "fn run(flag: bool) {\n"
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

        diagnostic = find_diagnostic_by_code_and_line(analyzer, "compiler_error", 9);
        if (diagnostic == ZR_NULL ||
            diagnostic->severity != ZR_DIAGNOSTIC_ERROR ||
            !diagnostic_message_contains(diagnostic, "No matching overload") ||
            count_diagnostics_with_code(analyzer, "compiler_error") != 1 ||
            analyzer->diagnostics.length != 1U) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Resolves Overloads For Call Compatibility",
                      "Expected exactly one canonical compiler_error for the overload call with no compatible candidate");
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
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.unknown(\"native_bad\")#\n"
            "    fn BadUnknown(lhs:i32): i32;\n"
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
            "    fn BadCallconv(): void;\n"
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
        if (count_diagnostics_with_code(analyzer, "invalid_decorator") != 7 ||
            invalidUnderlyingDiagnostic == ZR_NULL ||
            !diagnostic_message_contains(invalidUnderlyingDiagnostic, "supported integer type") ||
            invalidViewTypeDiagnostic == ZR_NULL ||
            !diagnostic_message_contains(invalidViewTypeDiagnostic, "source extern struct")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Reports Invalid FFI Decorators",
                      "Expected seven canonical declaration diagnostics, including one fail-fast wrapper error per invalid class declaration");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Reports Invalid FFI Decorators");
}

static void test_semantic_analyzer_class_method_scope_surfaces_receiver_and_local_hover(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover");

    TEST_INFO("Class method scopes",
              "Instance methods should expose receiver and local symbols to hover");

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
            "    pub fn total(extra: int): int {\n"
            "        var localResult = extra + 1;\n"
            "        return this.derivedValue + localResult;\n"
            "    }\n"
            "    pub @constructor(seed: int) super(seed) {\n"
            "        this.derivedValue = this.derivedValue + 1;\n"
            "    }\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "class_scope_receivers_test.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange thisHoverPosition;
        SZrFileRange localHoverPosition;
        SZrHoverInfo *hoverInfo = ZR_NULL;
        const char *hoverText;

        if (analyzer == ZR_NULL) {
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      "Failed to create semantic analyzer");
            return;
        }

        if (ast == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      "Failed to parse class scope test code");
            return;
        }

        if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      "Failed to analyze class scope test code");
            return;
        }

        thisHoverPosition = file_range_for_nth_substring(testCode, "this.derivedValue", 0, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, thisHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      "Failed to get hover info for this");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "this") == ZR_NULL || strstr(hoverText, "Derived") == ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }
        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        hoverInfo = ZR_NULL;

        localHoverPosition = file_range_for_nth_substring(testCode, "localResult", 1, ZR_FALSE);
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, localHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      "Failed to get hover info for localResult");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "localResult") == ZR_NULL || strstr(hoverText, "int") == ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Semantic Analyzer Class Method Scope Surfaces Receiver And Local Hover");
}

static void test_semantic_analyzer_compile_time_test_and_lambda_scopes_surface_symbols(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols");

    TEST_INFO("Compile-time/test/lambda scopes",
              "Compile-time declarations, test bodies, and typed lambdas should register symbols and inferred types");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "comptime fn addBias(seed: int): int {\n"
            "    let MAX_SIZE = 100;\n"
            "    return seed + MAX_SIZE;\n"
            "}\n"
            "fn verifyScope(): int {\n"
            "    var result = addBias(1);\n"
            "    var typed = fn(value: int): int => {\n"
            "        return value + result;\n"
            "    };\n"
            "    return typed(2);\n"
            "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "compile_time_test_scope_symbols.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, testCode, strlen(testCode), sourceName);
        SZrFileRange compileTimeHoverPosition;
        SZrFileRange lambdaHoverPosition;
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

        compileTimeHoverPosition = file_range_for_nth_substring(testCode, "seed + MAX_SIZE", 0, ZR_FALSE);
        compileTimeHoverPosition.start.offset += 7;
        compileTimeHoverPosition.end.offset += 7;
        compileTimeHoverPosition.start.column += 7;
        compileTimeHoverPosition.end.column += 7;
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, compileTimeHoverPosition, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
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
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      "Failed to get hover info for lambda capture");
            return;
        }

        hoverText = hover_contents_string(hoverInfo);
        if (hoverText == ZR_NULL || strstr(hoverText, "result") == ZR_NULL || strstr(hoverText, "int") == ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Semantic Analyzer Compile Time Test And Lambda Scopes Surface Symbols",
                      hoverText != ZR_NULL ? hoverText : "<null hover>");
            return;
        }

        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
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

static void test_semantic_analyzer_releases_cache_storage(SZrState *state) {
    const TZrChar *summary = "Semantic Analyzer Releases Cache Storage";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrAstNode *ast;
    SZrString *sourceName;
    const TZrChar *source = "var cached = 1;";
    TZrSize primaryCacheBytes;
    TZrSize totalCacheBytes;

    TEST_START(summary);
    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, summary, "Failed to create semantic analyzer");
        return;
    }

    primaryCacheBytes = ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(analyzer);
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            analyzer);
    totalCacheBytes = ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(analyzer);
    if (primaryCacheBytes == 0 || scopedAnalyzer == ZR_NULL ||
        totalCacheBytes <= primaryCacheBytes) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Cache storage accounting did not include primary and scoped analyzers");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage(state, analyzer);
    if (analyzer->cache != ZR_NULL || analyzer->scopedQueryAnalyzer != scopedAnalyzer ||
        scopedAnalyzer->cache != ZR_NULL ||
        ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(analyzer) != 0) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Releasing cache storage retained a cache allocation or lost scoped state");
        return;
    }

    sourceName = ZrCore_String_Create(state, "cache_release.zr", 16);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) ||
        analyzer->cache == ZR_NULL ||
        ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(analyzer) == 0) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        TEST_FAIL(timer,
                  summary,
                  "Analyzing after release did not rehydrate cache storage");
        return;
    }

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    TEST_PASS(timer, summary);
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

    test_semantic_analyzer_projects_const_field_assignment_context(state);
    TEST_DIVIDER();

    test_semantic_analyzer_projects_all_const_assignment_target_kinds(state);
    TEST_DIVIDER();

    test_semantic_analyzer_avoids_false_binary_type_mismatch_diagnostics(state);
    TEST_DIVIDER();

    test_semantic_analyzer_avoids_false_numeric_initializer_type_mismatch_diagnostics(state);
    TEST_DIVIDER();

    test_semantic_analyzer_expression_metadata_records_exact_types(state);
    TEST_DIVIDER();

    test_semantic_analyzer_unannotated_function_records_exact_return_type(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_initializer_requires_annotation(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_return_type_not_provable(state);
    TEST_DIVIDER();

    test_semantic_analyzer_accepts_all_path_return_chains(state);
    TEST_DIVIDER();

    test_semantic_analyzer_exact_type_failure_surfaces_explicit_hover(state);
    TEST_DIVIDER();

    test_semantic_analyzer_unannotated_function_surfaces_exact_return_signature_detail(state);
    TEST_DIVIDER();

    test_semantic_analyzer_populates_semantic_context(state);
    TEST_DIVIDER();

    test_semantic_analyzer_records_reference_facts_with_precise_ranges(state);
    TEST_DIVIDER();

    test_semantic_analyzer_records_using_cleanup_and_template_segments(state);
    TEST_DIVIDER();
    test_semantic_analyzer_records_owned_field_cleanup_metadata(state);
    TEST_DIVIDER();

    test_semantic_analyzer_get_diagnostics(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_get_symbol_at_resolves_local_references(state);
    TEST_DIVIDER();

    test_semantic_analyzer_local_symbols_surface_rich_hover(state);
    TEST_DIVIDER();

    test_semantic_analyzer_class_method_scope_surfaces_receiver_and_local_hover(state);
    TEST_DIVIDER();

    test_semantic_analyzer_compile_time_test_and_lambda_scopes_surface_symbols(state);
    TEST_DIVIDER();

    test_semantic_analyzer_generic_function_symbols_surface_signature_detail(state);
    TEST_DIVIDER();

    test_semantic_analyzer_generic_call_infers_type_argument_without_explicit_close(state);
    TEST_DIVIDER();

    test_semantic_analyzer_function_signatures_preserve_ownership_qualifiers(state);
    TEST_DIVIDER();

    test_semantic_analyzer_signature_type_display_fails_closed_without_canonical_fact(state);
    TEST_DIVIDER();

    test_semantic_analyzer_generic_type_symbols_surface_signature_detail(state);
    TEST_DIVIDER();

    test_semantic_analyzer_closed_generic_receiver_calls_stay_local_to_type_metadata(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_invalid_interface_variance_positions(state);
    TEST_DIVIDER();

    test_semantic_analyzer_projects_interface_const_field_contract(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_owner_generic_context_in_member_signatures(state);
    TEST_DIVIDER();

    test_semantic_analyzer_records_reachability_facts_for_unreachable_statements(state);
    TEST_DIVIDER();

    test_semantic_analyzer_records_short_circuit_logical_facts(state);
    TEST_DIVIDER();

    test_semantic_analyzer_warns_on_unreachable_statements_after_return_or_throw(state);
    TEST_DIVIDER();

    test_semantic_analyzer_warns_on_unreachable_if_branches(state);
    TEST_DIVIDER();

    test_semantic_analyzer_warns_on_deterministic_short_circuit_branches(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_declared_ownership_initializer_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_assignment_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_query_diagnostic_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_method_call_mismatch_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_duplicate_type_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_initializer_annotation_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_return_type_not_provable_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_cannot_infer_exact_type_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_callable_decorator_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_extern_struct_decorator_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_extern_field_decorator_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_extern_enum_decorator_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_extern_enum_member_decorator_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_ffi_wrapper_lowering_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_ffi_wrapper_view_type_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_accepts_valid_extern_parameter_charset(state);
    TEST_DIVIDER();

    test_semantic_analyzer_ignores_non_extern_parameter_decorators(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_invalid_extern_parameter_charset_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_preserves_conflicting_extern_parameter_direction_golden_parity(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_owner_to_plain_initializer_escape(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_return_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_borrowed_return_escape(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_function_argument_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_weak_argument_requires_wake(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_method_argument_ownership_mismatch(state);
    TEST_DIVIDER();

    test_semantic_analyzer_projects_unresolved_member_query_diagnostic(state);
    TEST_DIVIDER();

    test_semantic_analyzer_resolves_overloads_for_call_compatibility(state);
    TEST_DIVIDER();

    test_semantic_analyzer_reports_invalid_ffi_decorators(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_cache(state);
    TEST_DIVIDER();

    test_semantic_analyzer_releases_cache_storage(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All Semantic Analyzer Tests Completed\n");
    printf("==========\n");
    
    return test_failures == 0 ? 0 : 1;
}
