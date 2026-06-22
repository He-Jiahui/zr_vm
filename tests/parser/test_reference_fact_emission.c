#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

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

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static SZrAstNode *script_statement_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= index) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

static SZrAstNode *expression_statement_expression_at(SZrAstNode *ast, TZrSize index) {
    SZrAstNode *statement = script_statement_at(ast, index);

    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static void test_assignment_identifier_records_write_reference_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *declarationPattern;
    SZrAstNode *assignmentExpr;
    SZrAstNode *assignmentTarget;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticReferenceFact *writeFact;
    const SZrSemanticReferenceFact *declarationFact;
    const char *source = "var seed = 1;\nseed = 3;";

    sourceName = ZrCore_String_Create(g_state,
                                      "reference_fact_assignment_write_test.zr",
                                      strlen("reference_fact_assignment_write_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    declaration = script_statement_at(ast, 0);
    assignmentExpr = expression_statement_expression_at(ast, 1);

    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    declarationPattern = declaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(declarationPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, declarationPattern->type);

    TEST_ASSERT_NOT_NULL(assignmentExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, assignmentExpr->type);
    assignmentTarget = assignmentExpr->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(assignmentTarget);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, assignmentTarget->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            declarationPattern->data.identifier.name,
            &seedType,
            declarationPattern,
            declarationPattern->location));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, assignmentExpr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    writeFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                               assignmentTarget->location);
    declarationFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                                     declarationPattern->location);

    TEST_ASSERT_NOT_NULL(writeFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_WRITE, writeFact->kind);
    TEST_ASSERT_TRUE(writeFact->isResolved);
    TEST_ASSERT_NOT_NULL(writeFact->name);
    TEST_ASSERT_EQUAL_STRING("seed", ZrCore_String_GetNativeString(writeFact->name));
    TEST_ASSERT_EQUAL_UINT64(assignmentTarget->location.start.offset, writeFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(assignmentTarget->location.end.offset, writeFact->range.end.offset);
    TEST_ASSERT_EQUAL_UINT64(declarationPattern->location.start.offset,
                             writeFact->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(declarationPattern->location.end.offset,
                             writeFact->declarationRange.end.offset);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, writeFact->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, writeFact->typeId);
    TEST_ASSERT_TRUE(writeFact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                          writeFact->definiteAssignmentState);

    TEST_ASSERT_NOT_NULL(declarationFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_DECLARATION, declarationFact->kind);
    TEST_ASSERT_EQUAL_UINT32(writeFact->symbolId, declarationFact->symbolId);
    TEST_ASSERT_EQUAL_UINT32(writeFact->typeId, declarationFact->typeId);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_reference_facts_resolve_linear_reaching_definition_to_latest_write(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *declarationPattern;
    SZrAstNode *assignmentExpr;
    SZrAstNode *assignmentTarget;
    SZrAstNode *readExpr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticReferenceFact *readFact;
    const char *source =
            "var seed = 1;\n"
            "seed = 3;\n"
            "seed;";

    sourceName = ZrCore_String_Create(g_state,
                                      "reference_fact_reaching_definitions_test.zr",
                                      strlen("reference_fact_reaching_definitions_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    declaration = script_statement_at(ast, 0);
    assignmentExpr = expression_statement_expression_at(ast, 1);
    readExpr = expression_statement_expression_at(ast, 2);

    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    declarationPattern = declaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(declarationPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, declarationPattern->type);

    TEST_ASSERT_NOT_NULL(assignmentExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, assignmentExpr->type);
    assignmentTarget = assignmentExpr->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(assignmentTarget);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, assignmentTarget->type);

    TEST_ASSERT_NOT_NULL(readExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, readExpr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            declarationPattern->data.identifier.name,
            &seedType,
            declarationPattern,
            declarationPattern->location));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, assignmentExpr, &result));
    ZrParser_InferredType_Free(g_state, &result);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, readExpr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs->semanticContext));

    readFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext, readExpr->location);
    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_READ, readFact->kind);
    TEST_ASSERT_TRUE(readFact->isResolved);
    TEST_ASSERT_TRUE(readFact->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT64(assignmentTarget->location.start.offset,
                             readFact->definitionRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(assignmentTarget->location.end.offset,
                             readFact->definitionRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(declarationPattern->location.start.offset,
                             readFact->declarationRange.start.offset);
    TEST_ASSERT_NOT_EQUAL(readFact->declarationRange.start.offset,
                          readFact->definitionRange.start.offset);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_member_access_records_member_reference_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *seedDeclaration;
    SZrAstNode *seedPattern;
    SZrAstNode *memberExpression;
    SZrAstNode *memberNode;
    SZrAstNode *memberProperty;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticReferenceFact *memberAccessFact;
    const char *source =
            "var seed = 1;\n"
            "seed.value;";

    sourceName = ZrCore_String_Create(g_state,
                                      "reference_fact_member_access_test.zr",
                                      strlen("reference_fact_member_access_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    seedDeclaration = script_statement_at(ast, 0);
    memberExpression = expression_statement_expression_at(ast, 1);

    TEST_ASSERT_NOT_NULL(seedDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, seedDeclaration->type);
    seedPattern = seedDeclaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(seedPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, seedPattern->type);

    TEST_ASSERT_NOT_NULL(memberExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, memberExpression->type);
    TEST_ASSERT_NOT_NULL(memberExpression->data.primaryExpression.members);
    memberNode = memberExpression->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(memberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, memberNode->type);
    TEST_ASSERT_FALSE(memberNode->data.memberExpression.computed);
    memberProperty = memberNode->data.memberExpression.property;
    TEST_ASSERT_NOT_NULL(memberProperty);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            seedPattern->data.identifier.name,
            &seedType,
            seedPattern,
            seedPattern->location));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, memberExpression, &result));

    memberAccessFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                                      memberProperty->location);

    TEST_ASSERT_NOT_NULL(memberAccessFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, memberAccessFact->kind);
    TEST_ASSERT_FALSE(memberAccessFact->isResolved);
    TEST_ASSERT_NOT_NULL(memberAccessFact->name);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(memberAccessFact->name));
    TEST_ASSERT_EQUAL_UINT64(memberProperty->location.start.offset, memberAccessFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(memberProperty->location.end.offset, memberAccessFact->range.end.offset);
    TEST_ASSERT_EQUAL_UINT64(memberAccessFact->range.start.offset,
                             memberAccessFact->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(memberAccessFact->range.end.offset,
                             memberAccessFact->declarationRange.end.offset);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, memberAccessFact->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, memberAccessFact->typeId);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_computed_member_access_records_member_reference_without_hiding_index_read(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *seedDeclaration;
    SZrAstNode *seedPattern;
    SZrAstNode *indexDeclaration;
    SZrAstNode *indexPattern;
    SZrAstNode *memberExpression;
    SZrAstNode *memberNode;
    SZrAstNode *indexProperty;
    SZrFileRange bracketRange;
    SZrInferredType seedType;
    SZrInferredType indexType;
    SZrInferredType result;
    const SZrSemanticReferenceFact *memberAccessFact;
    const SZrSemanticReferenceFact *indexReadFact;
    const char *source =
            "var seed = 1;\n"
            "var index = 0;\n"
            "seed[index];";

    sourceName = ZrCore_String_Create(g_state,
                                      "reference_fact_computed_member_access_test.zr",
                                      strlen("reference_fact_computed_member_access_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    seedDeclaration = script_statement_at(ast, 0);
    indexDeclaration = script_statement_at(ast, 1);
    memberExpression = expression_statement_expression_at(ast, 2);

    TEST_ASSERT_NOT_NULL(seedDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, seedDeclaration->type);
    seedPattern = seedDeclaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(seedPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, seedPattern->type);

    TEST_ASSERT_NOT_NULL(indexDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, indexDeclaration->type);
    indexPattern = indexDeclaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(indexPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, indexPattern->type);

    TEST_ASSERT_NOT_NULL(memberExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, memberExpression->type);
    TEST_ASSERT_NOT_NULL(memberExpression->data.primaryExpression.members);
    memberNode = memberExpression->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(memberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, memberNode->type);
    TEST_ASSERT_TRUE(memberNode->data.memberExpression.computed);
    indexProperty = memberNode->data.memberExpression.property;
    TEST_ASSERT_NOT_NULL(indexProperty);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, indexProperty->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            seedPattern->data.identifier.name,
            &seedType,
            seedPattern,
            seedPattern->location));
    ZrParser_InferredType_Init(g_state, &indexType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            indexPattern->data.identifier.name,
            &indexType,
            indexPattern,
            indexPattern->location));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, memberExpression, &result));

    bracketRange = indexProperty->location;
    TEST_ASSERT_GREATER_THAN_UINT64(memberNode->location.start.offset,
                                    bracketRange.start.offset);
    bracketRange.start.offset -= 1;
    bracketRange.end.offset = bracketRange.start.offset;
    if (bracketRange.start.column > 0) {
        bracketRange.start.column -= 1;
    }
    bracketRange.end = bracketRange.start;

    memberAccessFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                                      bracketRange);
    indexReadFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                                   indexProperty->location);

    TEST_ASSERT_NOT_NULL(memberAccessFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS, memberAccessFact->kind);
    TEST_ASSERT_FALSE(memberAccessFact->isResolved);
    TEST_ASSERT_NOT_NULL(memberAccessFact->name);
    TEST_ASSERT_EQUAL_STRING("index", ZrCore_String_GetNativeString(memberAccessFact->name));
    TEST_ASSERT_EQUAL_UINT64(memberNode->location.start.offset, memberAccessFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(memberNode->location.end.offset, memberAccessFact->range.end.offset);
    TEST_ASSERT_EQUAL_UINT64(memberAccessFact->range.start.offset,
                             memberAccessFact->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(memberAccessFact->range.end.offset,
                             memberAccessFact->declarationRange.end.offset);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, memberAccessFact->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, memberAccessFact->typeId);

    TEST_ASSERT_NOT_NULL(indexReadFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_READ, indexReadFact->kind);
    TEST_ASSERT_TRUE(indexReadFact->isResolved);
    TEST_ASSERT_NOT_NULL(indexReadFact->name);
    TEST_ASSERT_EQUAL_STRING("index", ZrCore_String_GetNativeString(indexReadFact->name));
    TEST_ASSERT_EQUAL_UINT64(indexProperty->location.start.offset, indexReadFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(indexProperty->location.end.offset, indexReadFact->range.end.offset);
    TEST_ASSERT_EQUAL_UINT64(indexPattern->location.start.offset,
                             indexReadFact->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(indexPattern->location.end.offset,
                             indexReadFact->declarationRange.end.offset);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &indexType);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_assignment_member_targets_record_member_write_reference_facts(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *seedDeclaration;
    SZrAstNode *seedPattern;
    SZrAstNode *indexDeclaration;
    SZrAstNode *indexPattern;
    SZrAstNode *memberAssignment;
    SZrAstNode *indexAssignment;
    SZrAstNode *memberTarget;
    SZrAstNode *indexTarget;
    SZrAstNode *memberNode;
    SZrAstNode *indexMemberNode;
    SZrAstNode *memberProperty;
    SZrAstNode *indexProperty;
    SZrInferredType seedType;
    SZrInferredType indexType;
    SZrInferredType result;
    const SZrSemanticReferenceFact *memberWriteFact;
    const SZrSemanticReferenceFact *indexWriteFact;
    const char *source =
            "var seed = 1;\n"
            "var index = 0;\n"
            "seed.value = 3;\n"
            "seed[index] = 4;";

    sourceName = ZrCore_String_Create(g_state,
                                      "reference_fact_member_write_test.zr",
                                      strlen("reference_fact_member_write_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    seedDeclaration = script_statement_at(ast, 0);
    indexDeclaration = script_statement_at(ast, 1);
    memberAssignment = expression_statement_expression_at(ast, 2);
    indexAssignment = expression_statement_expression_at(ast, 3);

    TEST_ASSERT_NOT_NULL(seedDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, seedDeclaration->type);
    seedPattern = seedDeclaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(seedPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, seedPattern->type);

    TEST_ASSERT_NOT_NULL(indexDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, indexDeclaration->type);
    indexPattern = indexDeclaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(indexPattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, indexPattern->type);

    TEST_ASSERT_NOT_NULL(memberAssignment);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, memberAssignment->type);
    memberTarget = memberAssignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(memberTarget);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, memberTarget->type);
    TEST_ASSERT_NOT_NULL(memberTarget->data.primaryExpression.members);
    memberNode = memberTarget->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(memberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, memberNode->type);
    memberProperty = memberNode->data.memberExpression.property;
    TEST_ASSERT_NOT_NULL(memberProperty);

    TEST_ASSERT_NOT_NULL(indexAssignment);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, indexAssignment->type);
    indexTarget = indexAssignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(indexTarget);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, indexTarget->type);
    TEST_ASSERT_NOT_NULL(indexTarget->data.primaryExpression.members);
    indexMemberNode = indexTarget->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(indexMemberNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, indexMemberNode->type);
    TEST_ASSERT_TRUE(indexMemberNode->data.memberExpression.computed);
    indexProperty = indexMemberNode->data.memberExpression.property;
    TEST_ASSERT_NOT_NULL(indexProperty);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            seedPattern->data.identifier.name,
            &seedType,
            seedPattern,
            seedPattern->location));
    ZrParser_InferredType_Init(g_state, &indexType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            cs->typeEnv,
            indexPattern->data.identifier.name,
            &indexType,
            indexPattern,
            indexPattern->location));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, memberAssignment, &result));
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, indexAssignment, &result));

    memberWriteFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                                     memberProperty->location);
    indexWriteFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext,
                                                                    indexProperty->location);

    TEST_ASSERT_NOT_NULL(memberWriteFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, memberWriteFact->kind);
    TEST_ASSERT_FALSE(memberWriteFact->isResolved);
    TEST_ASSERT_NOT_NULL(memberWriteFact->name);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(memberWriteFact->name));
    TEST_ASSERT_EQUAL_UINT64(memberProperty->location.start.offset, memberWriteFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(memberProperty->location.end.offset, memberWriteFact->range.end.offset);

    TEST_ASSERT_NOT_NULL(indexWriteFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_MEMBER_WRITE, indexWriteFact->kind);
    TEST_ASSERT_FALSE(indexWriteFact->isResolved);
    TEST_ASSERT_NOT_NULL(indexWriteFact->name);
    TEST_ASSERT_EQUAL_STRING("index", ZrCore_String_GetNativeString(indexWriteFact->name));
    TEST_ASSERT_EQUAL_UINT64(indexProperty->location.start.offset, indexWriteFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(indexProperty->location.end.offset, indexWriteFact->range.end.offset);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &indexType);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_resolved_function_call_records_call_reference_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *functionDecl;
    SZrAstNode *callExpr;
    SZrAstNode *callTarget;
    SZrInferredType intType;
    SZrArray parameterTypes;
    SZrInferredType result;
    const SZrSemanticReferenceFact *callFact;
    const SZrSemanticReferenceFact *declarationFact;
    const char *source =
            "pick(value: int): int { return value; }\n"
            "pick(42);";

    sourceName = ZrCore_String_Create(g_state, "reference_fact_call_test.zr", 27);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    functionDecl = script_statement_at(ast, 0);
    callExpr = expression_statement_expression_at(ast, 1);

    TEST_ASSERT_NOT_NULL(functionDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionDecl->type);
    TEST_ASSERT_NOT_NULL(callExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, callExpr->type);
    TEST_ASSERT_NOT_NULL(callExpr->data.primaryExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, callExpr->data.primaryExpression.property->type);
    callTarget = callExpr->data.primaryExpression.property;

    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &parameterTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &parameterTypes, &intType);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state,
            cs->typeEnv,
            functionDecl->data.functionDeclaration.name->name,
            &intType,
            &parameterTypes,
            ZR_NULL,
            ZR_NULL,
            functionDecl));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, callExpr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    callFact = ZrParser_SemanticFacts_FindReferenceAtPosition(cs->semanticContext, callTarget->location);
    declarationFact = ZrParser_SemanticFacts_FindReferenceAtPosition(
            cs->semanticContext,
            functionDecl->data.functionDeclaration.nameLocation);

    TEST_ASSERT_NOT_NULL(callFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_CALL, callFact->kind);
    TEST_ASSERT_TRUE(callFact->isResolved);
    TEST_ASSERT_NOT_NULL(callFact->name);
    TEST_ASSERT_EQUAL_STRING("pick", ZrCore_String_GetNativeString(callFact->name));
    TEST_ASSERT_EQUAL_UINT64(callTarget->location.start.offset, callFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(callTarget->location.end.offset, callFact->range.end.offset);
    TEST_ASSERT_EQUAL_UINT64(functionDecl->data.functionDeclaration.nameLocation.start.offset,
                             callFact->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(functionDecl->data.functionDeclaration.nameLocation.end.offset,
                             callFact->declarationRange.end.offset);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, callFact->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, callFact->typeId);

    TEST_ASSERT_NOT_NULL(declarationFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_DECLARATION, declarationFact->kind);
    TEST_ASSERT_EQUAL_UINT32(callFact->symbolId, declarationFact->symbolId);
    TEST_ASSERT_EQUAL_UINT32(callFact->typeId, declarationFact->typeId);

    ZrParser_InferredType_Free(g_state, &result);
    ZrCore_Array_Free(g_state, &parameterTypes);
    ZrParser_InferredType_Free(g_state, &intType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_assignment_identifier_records_write_reference_fact);
    RUN_TEST(test_reference_facts_resolve_linear_reaching_definition_to_latest_write);
    RUN_TEST(test_member_access_records_member_reference_fact);
    RUN_TEST(test_computed_member_access_records_member_reference_without_hiding_index_read);
    RUN_TEST(test_assignment_member_targets_record_member_write_reference_facts);
    RUN_TEST(test_resolved_function_call_records_call_reference_fact);
    return UNITY_END();
}
