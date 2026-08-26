#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrFileRange diagnostic_range(TZrSize startOffset, TZrSize endOffset) {
    TZrChar sourceName[] = "semantic_query_diagnostics.zr";
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, sourceName, strlen(sourceName));
    return range;
}

static void build_diagnostic(SZrStructuredDiagnostic *diagnostic) {
    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_Build(
            g_state,
            diagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_WARNING,
            diagnostic_range(8U, 12U),
            "semantic_no_safe_fix",
            "The diagnostic has no safe automatic edit.",
            "Resolving the condition requires choosing a new semantic contract.",
            "Review the declaration and select the intended contract."));
}

static void test_no_fix_reason_survives_fact_and_query_materialization(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticDiagnosticFact fact;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    build_diagnostic(&fact.diagnostic);
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_SetNoFixReason(
            &fact.diagnostic,
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION));
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendDiagnostic(context, &fact));
    ZrParser_StructuredDiagnostic_Free(g_state, &fact.diagnostic);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT(1U, diagnostics.count);
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostics.items[0].noFixReason);
    TEST_ASSERT_FALSE(diagnostics.items[0].fixes.isValid);

    ZrParser_SemanticContext_Free(context);
}

static void test_fix_and_no_fix_reason_are_mutually_exclusive(void) {
    SZrStructuredDiagnostic noFixDiagnostic;
    SZrStructuredDiagnostic fixDiagnostic;
    SZrStructuredDiagnostic copy;
    SZrFileRange editRange = diagnostic_range(10U, 10U);

    build_diagnostic(&noFixDiagnostic);
    TEST_ASSERT_FALSE(ZrParser_StructuredDiagnostic_SetNoFixReason(
            &noFixDiagnostic, ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED));
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_SetNoFixReason(
            &noFixDiagnostic, ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT));
    TEST_ASSERT_FALSE(ZrParser_StructuredDiagnostic_AddFix(
            g_state,
            &noFixDiagnostic,
            "Apply an unsafe edit",
            editRange,
            "replacement",
            ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT));
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_Copy(
            g_state, &copy, &noFixDiagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT, copy.noFixReason);
    ZrParser_StructuredDiagnostic_Free(g_state, &copy);
    ZrParser_StructuredDiagnostic_Free(g_state, &noFixDiagnostic);

    build_diagnostic(&fixDiagnostic);
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_AddFix(
            g_state,
            &fixDiagnostic,
            "Apply the edit",
            editRange,
            "replacement",
            ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE));
    TEST_ASSERT_FALSE(ZrParser_StructuredDiagnostic_SetNoFixReason(
            &fixDiagnostic,
            ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT));
    ZrParser_StructuredDiagnostic_Free(g_state, &fixDiagnostic);
}

static void test_syntax_no_fix_builders_publish_explicit_reasons(void) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location = diagnostic_range(20U, 24U);
    SZrFileRange fixLocation = diagnostic_range(24U, 24U);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(
            g_state, &diagnostic, location));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic.noFixReason);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionalConsequent(
            g_state, &diagnostic, location));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic.noFixReason);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate(
            g_state, &diagnostic, location));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic.noFixReason);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_BuildMissingConditionalColon(
            g_state, &diagnostic, location, fixLocation, ZR_FALSE));
    TEST_ASSERT_FALSE(diagnostic.fixes.isValid);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT,
            diagnostic.noFixReason);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_fix_reason_survives_fact_and_query_materialization);
    RUN_TEST(test_fix_and_no_fix_reason_are_mutually_exclusive);
    RUN_TEST(test_syntax_no_fix_builders_publish_explicit_reasons);
    return UNITY_END();
}
