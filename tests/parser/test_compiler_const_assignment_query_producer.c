#include <string.h>

#include "unity.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/const_assignment.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

#include "harness/runtime_support.h"

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

static SZrAstNode *script_statement_at(SZrAstNode *script, TZrSize index) {
    if (script == ZR_NULL || script->type != ZR_AST_SCRIPT ||
        script->data.script.statements == ZR_NULL ||
        index >= script->data.script.statements->count) {
        return ZR_NULL;
    }
    return script->data.script.statements->nodes[index];
}

static SZrAstNode *assignment_at(SZrAstNode *script, TZrSize index) {
    SZrAstNode *statement = script_statement_at(script, index);

    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }
    return statement->data.expressionStatement.expr;
}

static void test_publisher_resolves_const_target_by_symbol_id(void) {
    static TZrChar source[] =
            "let frozen: int = 1;\n"
            "frozen = 2;\n"
            "var mutableValue: int = 3;\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_const_assignment_query_producer_test.zr",
            strlen("compiler_const_assignment_query_producer_test.zr"));
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *constDeclaration = script_statement_at(script, 0U);
    SZrAstNode *assignment = assignment_at(script, 1U);
    SZrAstNode *mutableDeclaration = script_statement_at(script, 2U);
    SZrAstNode *constName;
    SZrAstNode *target;
    SZrCompilerState compiler;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    TZrSymbolId targetSymbolId;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(constDeclaration);
    TEST_ASSERT_NOT_NULL(assignment);
    TEST_ASSERT_NOT_NULL(mutableDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, constDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, assignment->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, mutableDeclaration->type);
    constName = constDeclaration->data.variableDeclaration.pattern;
    target = assignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(constName);
    TEST_ASSERT_NOT_NULL(target);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = script;

    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_RegisterSymbol(
                    compiler.semanticContext,
                    constName->data.identifier.name,
                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    mutableDeclaration,
                    mutableDeclaration->location));
    targetSymbolId = ZrParser_Semantic_RegisterSymbol(
            compiler.semanticContext,
            constName->data.identifier.name,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            constDeclaration,
            constName->location);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, targetSymbolId);

    memset(&reference, 0, sizeof(reference));
    reference.node = target;
    reference.range = target->location;
    reference.declarationRange = constName->location;
    reference.kind = ZR_SEMANTIC_REFERENCE_WRITE;
    reference.symbolId = targetSymbolId;
    reference.name = constName->data.identifier.name;
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            compiler.semanticContext, &reference));

    TEST_ASSERT_TRUE(ZrParser_ConstAssignment_PublishDiagnostic(
            &compiler, script, assignment));
    TEST_ASSERT_FALSE(compiler.hasError);

    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_UINT32(2012U, diagnostics.items[0].descriptorId);
    TEST_ASSERT_EQUAL_STRING(
            "const_assignment",
            ZrCore_String_GetNativeString(diagnostics.items[0].code));
    TEST_ASSERT_EQUAL_UINT64(
            assignment->location.start.offset,
            diagnostics.items[0].location.start.offset);
    TEST_ASSERT_TRUE(diagnostics.items[0].relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(
            1U, (TZrUInt32)diagnostics.items[0].relatedInformation.length);
    TEST_ASSERT_EQUAL_UINT64(
            constName->location.start.offset,
            ((const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                    (SZrArray *)&diagnostics.items[0].relatedInformation, 0U))
                    ->location.start.offset);
    TEST_ASSERT_EQUAL_INT(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostics.items[0].noFixReason);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_publisher_rejects_missing_symbol_record(void) {
    static TZrChar source[] =
            "let frozen: int = 1;\n"
            "frozen = 2;\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "compiler_const_assignment_missing_symbol_test.zr",
            strlen("compiler_const_assignment_missing_symbol_test.zr"));
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *assignment = assignment_at(script, 1U);
    SZrAstNode *target;
    SZrCompilerState compiler;
    SZrSemanticReferenceFact reference;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_NOT_NULL(assignment);
    target = assignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(target);

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.scriptAst = script;

    memset(&reference, 0, sizeof(reference));
    reference.node = target;
    reference.range = target->location;
    reference.declarationRange = target->location;
    reference.kind = ZR_SEMANTIC_REFERENCE_WRITE;
    reference.symbolId = 999U;
    reference.name = target->data.identifier.name;
    reference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            compiler.semanticContext, &reference));

    TEST_ASSERT_FALSE(ZrParser_ConstAssignment_PublishDiagnostic(
            &compiler, script, assignment));
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            compiler.semanticContext, &scope));
    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            compiler.semanticContext, &scope, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)diagnostics.count);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publisher_resolves_const_target_by_symbol_id);
    RUN_TEST(test_publisher_rejects_missing_symbol_record);
    return UNITY_END();
}
