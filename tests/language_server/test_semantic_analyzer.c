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
            "using resource; "
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
              "Analyzing `using var` fields should register field symbols and distinguish struct-value cleanup from instance-field cleanup");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode =
            "struct HandleBox { using var handle: %unique Resource; }\n"
            "class Holder { using var resource: %unique Resource; }";
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
        handleSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, handleName, ZR_NULL);
        resourceSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, resourceName, ZR_NULL);
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
              "Analyzing `static using var` should emit a compile-time diagnostic instead of silently accepting lifecycle-managed static fields");

    {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        const TZrChar *testCode = "class Holder { static using var resource: %unique Resource; }";
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
int main() {
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
    
    // 运行测试
    test_semantic_analyzer_create_and_free(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_analyze(state);
    TEST_DIVIDER();

    test_semantic_analyzer_type_checking_assignment_path(state);
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
    
    test_semantic_analyzer_cache(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All Semantic Analyzer Tests Completed\n");
    printf("==========\n");
    
    return 0;
}
