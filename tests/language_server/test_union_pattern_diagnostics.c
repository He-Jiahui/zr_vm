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

static const TZrChar *test_string_ptr(SZrString *value) {
    const TZrChar *text;

    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        text = ZrCore_String_GetNativeStringShort(value);
    }
    return text;
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

static TZrBool analyzer_has_diagnostic_code(SZrSemanticAnalyzer *analyzer, const TZrChar *code) {
    return find_diagnostic_by_code(analyzer, code) != ZR_NULL;
}

static SZrSymbol *find_symbol_any_scope(SZrSemanticAnalyzer *analyzer, const TZrChar *name) {
    TZrSize scopeIndex;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (scopeIndex = 0; scopeIndex < analyzer->symbolTable->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&analyzer->symbolTable->allScopes, scopeIndex);
        TZrSize symbolIndex;

        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr =
                (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr != ZR_NULL &&
                *symbolPtr != ZR_NULL &&
                (*symbolPtr)->name != ZR_NULL &&
                strcmp(test_string_ptr((*symbolPtr)->name), name) == 0) {
                return *symbolPtr;
            }
        }
    }

    return ZR_NULL;
}

static TZrBool symbol_has_int_type(SZrSymbol *symbol) {
    return symbol != ZR_NULL &&
           symbol->typeInfo != ZR_NULL &&
           ZR_VALUE_IS_TYPE_INT(symbol->typeInfo->baseType);
}

static TZrBool symbol_has_type_name(SZrSymbol *symbol, const TZrChar *expectedTypeName) {
    return symbol != ZR_NULL &&
           symbol->typeInfo != ZR_NULL &&
           symbol->typeInfo->typeName != ZR_NULL &&
           strcmp(test_string_ptr(symbol->typeInfo->typeName), expectedTypeName) == 0;
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

static void test_union_lsp_infers_generic_variant_constructor_symbol_type(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union LSP Infers Generic Variant Constructor Symbol Type");

    TEST_INFO("union constructor semantic type",
              "A variable initialized from Option<int>.Some should keep the generic union instance type");

    {
        const TZrChar *source =
            "union Option<T> {\n"
            "    None;\n"
            "    @Some(value: T);\n"
            "}\n"
            "var some = Option<int>.Some(42);\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrSymbol *someSymbol;

        if (!analyze_source(state, source, "union_lsp_generic_constructor_type_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union LSP Infers Generic Variant Constructor Symbol Type",
                      "Failed to parse or analyze test source");
            return;
        }

        someSymbol = find_symbol_any_scope(analyzer, "some");
        if (!symbol_has_type_name(someSymbol, "Option<int>")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union LSP Infers Generic Variant Constructor Symbol Type",
                      "Expected 'some' to have inferred type Option<int>");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union LSP Infers Generic Variant Constructor Symbol Type");
}

static void test_union_lsp_registers_no_block_using_tuple_payload(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union LSP Registers No-Block Using Tuple Payload");

    TEST_INFO("using tuple payload semantic binding",
              "No-block using [value] should bind the default @ variant payload in the current scope");

    {
        const TZrChar *source =
            "union Option {\n"
            "    None;\n"
            "    @Some(value: int);\n"
            "}\n"
            "var option: Option = Option.Some(42);\n"
            "using [value] = option;\n"
            "var observed = value;\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrSymbol *valueSymbol;
        SZrSymbol *observedSymbol;

        if (!analyze_source(state, source, "union_lsp_no_block_using_tuple_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union LSP Registers No-Block Using Tuple Payload",
                      "Failed to parse or analyze test source");
            return;
        }

        valueSymbol = find_symbol_any_scope(analyzer, "value");
        observedSymbol = find_symbol_any_scope(analyzer, "observed");
        if (!symbol_has_int_type(valueSymbol) ||
            !symbol_has_int_type(observedSymbol) ||
            analyzer_has_diagnostic_code(analyzer, "initializer_requires_annotation") ||
            analyzer_has_diagnostic_code(analyzer, "cannot_infer_exact_type")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union LSP Registers No-Block Using Tuple Payload",
                      "Expected value/observed to be int without inference diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union LSP Registers No-Block Using Tuple Payload");
}

static void test_union_lsp_registers_checkout_flow_generic_using_payload(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union LSP Registers Checkout Flow Generic Using Payload");

    TEST_INFO("real union using semantic binding",
              "A checkout subtotal carried by Option<int>.Some should unpack through no-block using and infer downstream totals");

    {
        const TZrChar *source =
            "union Option<T> {\n"
            "    None;\n"
            "    @Some(value: T);\n"
            "}\n"
            "var subtotalOption: Option<int> = Option<int>.Some(120);\n"
            "using [subtotal] = subtotalOption;\n"
            "var shipping = 8;\n"
            "var total = subtotal + shipping;\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrSymbol *subtotalSymbol;
        SZrSymbol *totalSymbol;

        if (!analyze_source(state, source, "union_lsp_checkout_flow_generic_using_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union LSP Registers Checkout Flow Generic Using Payload",
                      "Failed to parse or analyze test source");
            return;
        }

        subtotalSymbol = find_symbol_any_scope(analyzer, "subtotal");
        totalSymbol = find_symbol_any_scope(analyzer, "total");
        if (!symbol_has_int_type(subtotalSymbol) ||
            !symbol_has_int_type(totalSymbol) ||
            analyzer_has_diagnostic_code(analyzer, "initializer_requires_annotation") ||
            analyzer_has_diagnostic_code(analyzer, "cannot_infer_exact_type")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union LSP Registers Checkout Flow Generic Using Payload",
                      "Expected subtotal and total to be int without inference diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union LSP Registers Checkout Flow Generic Using Payload");
}

static void test_union_lsp_registers_block_using_struct_mixed_alias_payload(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union LSP Registers Block Using Struct Mixed Alias Payload");

    TEST_INFO("using struct payload semantic binding",
              "Block using {width, h: height} should bind default and aliased local names with payload field types");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Empty;\n"
            "    @Rect { width: int; height: int; }\n"
            "}\n"
            "var shape: Shape = Shape.Rect { width: 4, height: 5 };\n"
            "using (var {width, h: height} = shape) {\n"
            "    var area = width + h;\n"
            "} else {\n"
            "    var area = 0;\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrSymbol *widthSymbol;
        SZrSymbol *heightAliasSymbol;
        SZrSymbol *areaSymbol;

        if (!analyze_source(state, source, "union_lsp_block_using_struct_alias_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union LSP Registers Block Using Struct Mixed Alias Payload",
                      "Failed to parse or analyze test source");
            return;
        }

        widthSymbol = find_symbol_any_scope(analyzer, "width");
        heightAliasSymbol = find_symbol_any_scope(analyzer, "h");
        areaSymbol = find_symbol_any_scope(analyzer, "area");
        if (!symbol_has_int_type(widthSymbol) ||
            !symbol_has_int_type(heightAliasSymbol) ||
            !symbol_has_int_type(areaSymbol) ||
            find_symbol_any_scope(analyzer, "height") != ZR_NULL ||
            analyzer_has_diagnostic_code(analyzer, "initializer_requires_annotation") ||
            analyzer_has_diagnostic_code(analyzer, "cannot_infer_exact_type")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union LSP Registers Block Using Struct Mixed Alias Payload",
                      "Expected width/h/area int symbols, no height local leak, and no inference diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union LSP Registers Block Using Struct Mixed Alias Payload");
}

static void test_union_lsp_registers_switch_unqualified_variant_payloads(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union LSP Registers Switch Unqualified Variant Payloads");

    TEST_INFO("switch payload semantic binding",
              "Switch cases resolved from the subject union should bind tuple and struct payload locals");

    {
        const TZrChar *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: int);\n"
            "    Rect { width: int; height: int; }\n"
            "}\n"
            "var shape: Shape = Shape.Circle(2);\n"
            "switch (shape) {\n"
            "    (Circle(radius)) {\n"
            "        var diameter = radius + radius;\n"
            "    }\n"
            "    (Rect { width: w, height: h }) {\n"
            "        var area = w + h;\n"
            "    }\n"
            "    (Empty) {\n"
            "        var zero = 0;\n"
            "    }\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;

        if (!analyze_source(state, source, "union_lsp_switch_payload_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union LSP Registers Switch Unqualified Variant Payloads",
                      "Failed to parse or analyze test source");
            return;
        }

        if (!symbol_has_int_type(find_symbol_any_scope(analyzer, "radius")) ||
            !symbol_has_int_type(find_symbol_any_scope(analyzer, "diameter")) ||
            !symbol_has_int_type(find_symbol_any_scope(analyzer, "w")) ||
            !symbol_has_int_type(find_symbol_any_scope(analyzer, "h")) ||
            !symbol_has_int_type(find_symbol_any_scope(analyzer, "area")) ||
            analyzer_has_diagnostic_code(analyzer, "initializer_requires_annotation") ||
            analyzer_has_diagnostic_code(analyzer, "cannot_infer_exact_type")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union LSP Registers Switch Unqualified Variant Payloads",
                      "Expected switch payload locals and derived variables to be int without inference diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union LSP Registers Switch Unqualified Variant Payloads");
}

static void test_union_lsp_registers_checkout_result_switch_payload(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Union LSP Registers Checkout Result Switch Payload");

    TEST_INFO("real generic union switch semantic binding",
              "A Result<int,string> checkout branch should bind Ok payload locals from an unqualified switch case");

    {
        const TZrChar *source =
            "union Result<T, E> {\n"
            "    @Ok(value: T);\n"
            "    Err(error: E);\n"
            "}\n"
            "var checkout: Result<int, string> = Result<int, string>.Ok(120);\n"
            "switch (checkout) {\n"
            "    (Ok(amount)) {\n"
            "        var net = amount + 8;\n"
            "    }\n"
            "    (Err(message)) {\n"
            "        var net = 0;\n"
            "    }\n"
            "}\n";
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;

        if (!analyze_source(state, source, "union_lsp_checkout_result_switch_test.zr", &analyzer, &ast)) {
            TEST_FAIL(timer,
                      "Union LSP Registers Checkout Result Switch Payload",
                      "Failed to parse or analyze test source");
            return;
        }

        if (!symbol_has_int_type(find_symbol_any_scope(analyzer, "amount")) ||
            !symbol_has_int_type(find_symbol_any_scope(analyzer, "net")) ||
            analyzer_has_diagnostic_code(analyzer, "initializer_requires_annotation") ||
            analyzer_has_diagnostic_code(analyzer, "cannot_infer_exact_type")) {
            ZrParser_Ast_Free(state, ast);
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
            TEST_FAIL(timer,
                      "Union LSP Registers Checkout Result Switch Payload",
                      "Expected amount/net to be int without inference diagnostics");
            return;
        }

        ZrParser_Ast_Free(state, ast);
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    }

    TEST_PASS(timer, "Union LSP Registers Checkout Result Switch Payload");
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

    test_union_lsp_infers_generic_variant_constructor_symbol_type(state);
    TEST_DIVIDER();
    test_union_lsp_registers_no_block_using_tuple_payload(state);
    TEST_DIVIDER();
    test_union_lsp_registers_checkout_flow_generic_using_payload(state);
    TEST_DIVIDER();
    test_union_lsp_registers_block_using_struct_mixed_alias_payload(state);
    TEST_DIVIDER();
    test_union_lsp_registers_switch_unqualified_variant_payloads(state);
    TEST_DIVIDER();
    test_union_lsp_registers_checkout_result_switch_payload(state);
    TEST_DIVIDER();
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
