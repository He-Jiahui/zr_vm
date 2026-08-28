#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
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

static SZrFileRange range_for_substring(const TZrChar *source,
                                        SZrString *sourceName,
                                        const TZrChar *needle) {
    SZrFileRange range;
    const TZrChar *match = strstr(source, needle);
    const TZrChar *cursor = source;

    memset(&range, 0, sizeof(range));
    TEST_ASSERT_NOT_NULL(match);
    range.source = sourceName;
    range.start.offset = (TZrSize)(match - source);
    range.start.line = 1;
    range.start.column = 1;
    while (cursor < match) {
        if (*cursor == '\n') {
            range.start.line++;
            range.start.column = 1;
        } else {
            range.start.column++;
        }
        cursor++;
    }
    range.end = range.start;
    range.end.offset += strlen(needle);
    range.end.column += (TZrInt32)strlen(needle);
    return range;
}

static const SZrStructuredDiagnostic *find_diagnostic(
        const SZrSemanticContext *context,
        const TZrChar *code) {
    TZrSize index;

    if (context == ZR_NULL || code == ZR_NULL ||
        !context->queryDiagnostics.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->queryDiagnostics.length; index++) {
        const SZrStructuredDiagnostic *diagnostic =
                (const SZrStructuredDiagnostic *)ZrCore_Array_Get(
                        (SZrArray *)&context->queryDiagnostics, index);
        if (diagnostic != ZR_NULL && diagnostic->code != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), code) == 0) {
            return diagnostic;
        }
    }
    return ZR_NULL;
}

static void assert_owner_reference_return_diagnostic(
        const TZrChar *source,
        const TZrChar *sourceNameText,
        const TZrChar *code,
        TZrUInt32 descriptorId,
        EZrOwnershipQualifier qualifier,
        const TZrChar *sourceMessage) {
    SZrCompilerState compiler;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange sourceUseRange;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics query;
    const SZrStructuredDiagnostic *diagnostic;
    const SZrStructuredDiagnosticRelatedInformation *sourceRelated;
    const SZrStructuredDiagnosticRelatedInformation *lifetimeRelated;
    const SZrSemanticOwnershipFact *fact;

    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    sourceUseRange = range_for_substring(source, sourceName, "resource;");
    sourceUseRange.end.offset--;
    sourceUseRange.end.column--;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = ast;

    TEST_ASSERT_FALSE(ZrParser_Compiler_ValidateReferenceEscapes(&compiler, ast));
    TEST_ASSERT_TRUE(compiler.hasError);
    TEST_ASSERT_TRUE(compiler.hasStructuredError);
    TEST_ASSERT_EQUAL_UINT32(descriptorId, compiler.structuredError.descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            code,
            ZrCore_String_GetNativeString(compiler.structuredError.code));
    TEST_ASSERT_EQUAL_UINT64(
            sourceUseRange.start.offset,
            compiler.structuredError.location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            sourceUseRange.end.offset,
            compiler.structuredError.location.end.offset);
    TEST_ASSERT_TRUE(compiler.structuredError.relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            2U,
            (TZrUInt32)compiler.structuredError.relatedInformation.length);
    sourceRelated =
            (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                    &compiler.structuredError.relatedInformation, 0U);
    lifetimeRelated =
            (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                    &compiler.structuredError.relatedInformation, 1U);
    TEST_ASSERT_NOT_NULL(sourceRelated);
    TEST_ASSERT_EQUAL_STRING(
            sourceMessage,
            ZrCore_String_GetNativeString(sourceRelated->message));
    TEST_ASSERT_EQUAL_UINT64(
            sourceUseRange.start.offset,
            sourceRelated->location.start.offset);
    TEST_ASSERT_NOT_NULL(lifetimeRelated);
    TEST_ASSERT_EQUAL_STRING(
            "Source lifetime ends here",
            ZrCore_String_GetNativeString(lifetimeRelated->message));
    TEST_ASSERT_EQUAL_UINT64(
            lifetimeRelated->location.start.offset,
            lifetimeRelated->location.end.offset);

    fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            compiler.semanticContext, sourceUseRange);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_OWNERSHIP_FACT_ERROR, fact->kind);
    TEST_ASSERT_EQUAL_INT(qualifier, fact->qualifier);
    TEST_ASSERT_TRUE(fact->isViolation);
    TEST_ASSERT_NOT_NULL(fact->relatedNode);

    TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&compiler));
    compiler.hasError = ZR_FALSE;
    ZrParser_Compiler_ClearStructuredError(&compiler);
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&query, 0, sizeof(query));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &query));
    diagnostic = find_diagnostic(compiler.semanticContext, code);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_UINT32(descriptorId, diagnostic->descriptorId);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic->noFixReason);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unique_owner_writable_reference_return_publishes_loan_escape(void) {
    static const TZrChar source[] =
            "resource class Resource {}\n"
            "fn leak(resource: Unique<Resource>): ref Resource {\n"
            "    return ref resource;\n"
            "}\n";

    assert_owner_reference_return_diagnostic(
            source,
            "compiler_loan_return_escape_test.zr",
            "loan_escape",
            4003U,
            ZR_OWNERSHIP_QUALIFIER_LOANED,
            "Loan source is here");
}

static void test_shared_owner_readonly_reference_return_publishes_borrow_escape(void) {
    static const TZrChar source[] =
            "resource class Resource {}\n"
            "fn leak(resource: Shared<Resource>): ref readonly Resource {\n"
            "    return ref resource;\n"
            "}\n";

    assert_owner_reference_return_diagnostic(
            source,
            "compiler_borrow_return_escape_test.zr",
            "borrow_escape",
            4002U,
            ZR_OWNERSHIP_QUALIFIER_BORROWED,
            "Borrow source is here");
}

static void test_caller_reference_parameter_can_return_to_caller(void) {
    static const TZrChar source[] =
            "resource class Resource {}\n"
            "fn passthrough(resource: ref Resource): ref Resource {\n"
            "    return ref resource;\n"
            "}\n";
    SZrCompilerState compiler;
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_reference_passthrough_test.zr",
            strlen("compiler_reference_passthrough_test.zr"));
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = ast;

    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidateReferenceEscapes(&compiler, ast));
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_FALSE(compiler.hasStructuredError);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unique_owner_writable_reference_return_publishes_loan_escape);
    RUN_TEST(test_shared_owner_readonly_reference_return_publishes_borrow_escape);
    RUN_TEST(test_caller_reference_parameter_can_return_to_caller);
    return UNITY_END();
}
