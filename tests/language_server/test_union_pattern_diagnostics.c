//
// Focused union pattern diagnostic regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_language_server.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static int g_testFailures = 0;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timerValue, summary) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timerValue, summary, reason) do { \
    g_testFailures++; \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while (0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while (0)

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
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

static TZrBool diagnostic_string_contains(SZrString *value, const TZrChar *fragment) {
    const TZrChar *text;

    if (value == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        text = ZrCore_String_GetNativeStringShort(value);
    }
    return text != ZR_NULL && strstr(text, fragment) != ZR_NULL;
}

static SZrDiagnostic *find_diagnostic_by_code(SZrSemanticAnalyzer *analyzer, const TZrChar *code) {
    TZrSize index;

    if (analyzer == ZR_NULL || code == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr =
            (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL || (*diagnosticPtr)->code == ZR_NULL) {
            continue;
        }
        if (strcmp(ZrCore_String_GetNativeStringShort((*diagnosticPtr)->code), code) == 0) {
            return *diagnosticPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool analyze_source(SZrState *state,
                              const TZrChar *source,
                              const TZrChar *sourceNameText,
                              SZrSemanticAnalyzer **outAnalyzer,
                              SZrAstNode **outAst) {
    SZrSemanticAnalyzer *analyzer;
    SZrString *sourceName;
    SZrAstNode *ast;

    if (state == ZR_NULL || source == ZR_NULL || outAnalyzer == ZR_NULL || outAst == ZR_NULL) {
        return ZR_FALSE;
    }

    *outAnalyzer = ZR_NULL;
    *outAst = ZR_NULL;
    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (analyzer == ZR_NULL || ast == ZR_NULL) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        return ZR_FALSE;
    }
    if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast)) {
        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        return ZR_FALSE;
    }

    *outAnalyzer = analyzer;
    *outAst = ast;
    return ZR_TRUE;
}

static void test_union_using_pattern_reports_tuple_variant_object_shape_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union Using Pattern Reports Tuple Variant Object Shape Mismatch");

    TEST_INFO("pattern_shape_mismatch",
              "Tuple variants must use tuple destructuring, not object destructuring");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: int);\n"
            "    Rect { width: int; height: int; }\n"
            "}\n"
            "var s: Shape = Shape.Circle(2);\n"
            "using (var {r: radius}: Shape.Circle = s) {\n"
            "    var hit = r;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrDiagnostic *diagnostic;

        if (!analyze_source(state, source, "union_pattern_tuple_object_shape_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Tuple Variant Object Shape Mismatch",
                      "Failed to parse or analyze test source");
            return;
        }

        diagnostic = find_diagnostic_by_code(analyzer, "pattern_shape_mismatch");
        if (diagnostic == ZR_NULL ||
            !diagnostic_string_contains(diagnostic->message, "Object destructuring") ||
            !diagnostic_string_contains(diagnostic->cause, "positional") ||
            !diagnostic_string_contains(diagnostic->suggestion, "[")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Tuple Variant Object Shape Mismatch",
                      "Expected pattern_shape_mismatch with tuple destructuring guidance");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union Using Pattern Reports Tuple Variant Object Shape Mismatch");
}

static void test_union_using_pattern_reports_struct_variant_tuple_shape_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union Using Pattern Reports Struct Variant Tuple Shape Mismatch");

    TEST_INFO("pattern_shape_mismatch",
              "Struct variants must use object destructuring, not tuple destructuring");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: int);\n"
            "    Rect { width: int; height: int; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3, height: 4 };\n"
            "using (var [w, h]: Shape.Rect = s) {\n"
            "    var hit = w;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrDiagnostic *diagnostic;

        if (!analyze_source(state, source, "union_pattern_struct_tuple_shape_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Struct Variant Tuple Shape Mismatch",
                      "Failed to parse or analyze test source");
            return;
        }

        diagnostic = find_diagnostic_by_code(analyzer, "pattern_shape_mismatch");
        if (diagnostic == ZR_NULL ||
            !diagnostic_string_contains(diagnostic->message, "object destructuring") ||
            !diagnostic_string_contains(diagnostic->cause, "named fields") ||
            !diagnostic_string_contains(diagnostic->suggestion, "{")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Struct Variant Tuple Shape Mismatch",
                      "Expected pattern_shape_mismatch with object destructuring guidance");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union Using Pattern Reports Struct Variant Tuple Shape Mismatch");
}

static void test_union_using_pattern_reports_struct_variant_unknown_field(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union Using Pattern Reports Struct Variant Unknown Field");

    TEST_INFO("pattern_unknown_field",
              "Struct variant object destructuring must name an existing payload field");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Rect { width: int; height: int; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3, height: 4 };\n"
            "using (var {r: radius}: Shape.Rect = s) {\n"
            "    var hit = r;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrDiagnostic *diagnostic;

        if (!analyze_source(state, source, "union_pattern_unknown_field_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Struct Variant Unknown Field",
                      "Failed to parse or analyze test source");
            return;
        }

        diagnostic = find_diagnostic_by_code(analyzer, "pattern_unknown_field");
        if (diagnostic == ZR_NULL ||
            !diagnostic_string_contains(diagnostic->message, "Unknown") ||
            !diagnostic_string_contains(diagnostic->cause, "radius") ||
            !diagnostic_string_contains(diagnostic->suggestion, "width")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Struct Variant Unknown Field",
                      "Expected pattern_unknown_field with available field guidance");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union Using Pattern Reports Struct Variant Unknown Field");
}

static void test_union_using_pattern_reports_tuple_variant_arity_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union Using Pattern Reports Tuple Variant Arity Mismatch");

    TEST_INFO("pattern_arity_mismatch",
              "Tuple variant destructuring must bind exactly the declared payload arity");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Circle(radius: int);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2);\n"
            "using (var [r, extra]: Shape.Circle = s) {\n"
            "    var hit = r;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrDiagnostic *diagnostic;

        if (!analyze_source(state, source, "union_pattern_arity_mismatch_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Tuple Variant Arity Mismatch",
                      "Failed to parse or analyze test source");
            return;
        }

        diagnostic = find_diagnostic_by_code(analyzer, "pattern_arity_mismatch");
        if (diagnostic == ZR_NULL ||
            !diagnostic_string_contains(diagnostic->message, "expects 1") ||
            !diagnostic_string_contains(diagnostic->cause, "got 2") ||
            !diagnostic_string_contains(diagnostic->suggestion, "tuple")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Tuple Variant Arity Mismatch",
                      "Expected pattern_arity_mismatch with tuple binding count guidance");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union Using Pattern Reports Tuple Variant Arity Mismatch");
}

static void test_union_using_pattern_reports_struct_variant_arity_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union Using Pattern Reports Struct Variant Arity Mismatch");

    TEST_INFO("pattern_arity_mismatch",
              "Struct variant destructuring must bind exactly the declared payload fields");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Rect { width: int; height: int; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3, height: 4 };\n"
            "using (var {width}: Shape.Rect = s) {\n"
            "    var hit = width;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrDiagnostic *diagnostic;

        if (!analyze_source(state, source, "union_pattern_struct_arity_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Struct Variant Arity Mismatch",
                      "Failed to parse or analyze test source");
            return;
        }

        diagnostic = find_diagnostic_by_code(analyzer, "pattern_arity_mismatch");
        if (diagnostic == ZR_NULL ||
            !diagnostic_string_contains(diagnostic->message, "expects 2") ||
            !diagnostic_string_contains(diagnostic->cause, "got 1") ||
            !diagnostic_string_contains(diagnostic->suggestion, "width") ||
            !diagnostic_string_contains(diagnostic->suggestion, "height")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Struct Variant Arity Mismatch",
                      "Expected pattern_arity_mismatch with struct field count guidance");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union Using Pattern Reports Struct Variant Arity Mismatch");
}

static void test_union_using_pattern_reports_variant_type_mismatch(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union Using Pattern Reports Variant Type Mismatch");

    TEST_INFO("pattern_variant_mismatch",
              "A using pattern annotation must name a variant from the resource union type");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Circle(radius: int);\n"
            "}\n"
            "union Other {\n"
            "    Some(value: int);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2);\n"
            "using (var [v]: Other.Some = s) {\n"
            "    var hit = v;\n"
            "} else {\n"
            "    var miss = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrDiagnostic *diagnostic;

        if (!analyze_source(state, source, "union_pattern_variant_mismatch_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Variant Type Mismatch",
                      "Failed to parse or analyze test source");
            return;
        }

        diagnostic = find_diagnostic_by_code(analyzer, "pattern_variant_mismatch");
        if (diagnostic == ZR_NULL ||
            !diagnostic_string_contains(diagnostic->message, "Other.Some") ||
            !diagnostic_string_contains(diagnostic->cause, "Shape") ||
            !diagnostic_string_contains(diagnostic->suggestion, "Shape")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union Using Pattern Reports Variant Type Mismatch",
                      "Expected pattern_variant_mismatch with resource union guidance");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union Using Pattern Reports Variant Type Mismatch");
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    printf("ZR VM Language Server Union Pattern Diagnostics Tests\n");
    printf("====================================================\n\n");

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Failed to create global state\n");
        return 1;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Failed to get main state\n");
        return 1;
    }
    ZrCore_GlobalState_InitRegistry(state, global);
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);
    ZrVmLibContainer_Register(global);

    test_union_using_pattern_reports_tuple_variant_object_shape_mismatch(state);
    TEST_DIVIDER();
    test_union_using_pattern_reports_struct_variant_tuple_shape_mismatch(state);
    TEST_DIVIDER();
    test_union_using_pattern_reports_struct_variant_unknown_field(state);
    TEST_DIVIDER();
    test_union_using_pattern_reports_tuple_variant_arity_mismatch(state);
    TEST_DIVIDER();
    test_union_using_pattern_reports_struct_variant_arity_mismatch(state);
    TEST_DIVIDER();
    test_union_using_pattern_reports_variant_type_mismatch(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All Union Pattern Diagnostics Tests Completed\n");
    printf("==========\n");
    return g_testFailures == 0 ? 0 : 1;
}
