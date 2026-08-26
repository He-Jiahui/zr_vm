#ifndef ZR_VM_TEST_SEMANTIC_QUERY_PUBLIC_CONTRACT_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_PUBLIC_CONTRACT_CASES_H

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_parser/compiler.h"

typedef struct SZrSemanticPublicContractFixture {
    SZrSemanticContext *context;
    SZrTypeEnvironment *environment;
    SZrAstNode script;
    SZrAstNode functionDeclaration;
    SZrAstNode variableDeclaration;
    SZrAstNode variableIdentifier;
    SZrAstNode symbolShiftIdentifier;
    SZrAstNode parameterNode;
    SZrIdentifier functionIdentifier;
    SZrIdentifier parameterIdentifier;
    SZrAstNodeArray statements;
    SZrAstNodeArray parameters;
    SZrAstNode *statementNodes[2];
    SZrAstNode *parameterNodes[1];
} SZrSemanticPublicContractFixture;

static void semantic_public_contract_fixture_init(
        SZrSemanticPublicContractFixture *fixture,
        EZrValueType returnTypeKind,
        EZrValueType variableTypeKind,
        EZrAccessModifier variableAccess,
        TZrBool variableFirst) {
    SZrInferredType returnType;
    SZrInferredType variableType;
    SZrString *functionName;
    SZrString *variableName;

    memset(fixture, 0, sizeof(*fixture));
    fixture->context = ZrParser_SemanticContext_New(g_state);
    fixture->environment = ZrParser_TypeEnvironment_New(g_state);
    TEST_ASSERT_NOT_NULL(fixture->context);
    TEST_ASSERT_NOT_NULL(fixture->environment);
    fixture->environment->semanticContext = fixture->context;

    functionName = ZrCore_String_Create(g_state, "answer", 6U);
    variableName = ZrCore_String_Create(g_state, "hidden", 6U);
    TEST_ASSERT_NOT_NULL(functionName);
    TEST_ASSERT_NOT_NULL(variableName);

    init_node(&fixture->script, ZR_AST_SCRIPT, 0U, 80U);
    init_node(&fixture->functionDeclaration, ZR_AST_FUNCTION_DECLARATION, 1U, 30U);
    init_node(&fixture->variableDeclaration, ZR_AST_VARIABLE_DECLARATION, 31U, 60U);
    init_node(&fixture->variableIdentifier, ZR_AST_IDENTIFIER_LITERAL, 35U, 41U);

    fixture->functionIdentifier.name = functionName;
    fixture->functionDeclaration.data.functionDeclaration.name =
            &fixture->functionIdentifier;
    fixture->functionDeclaration.data.functionDeclaration.nameLocation =
            test_range(5U, 11U);
    fixture->variableIdentifier.data.identifier.name = variableName;
    fixture->variableDeclaration.data.variableDeclaration.pattern =
            &fixture->variableIdentifier;
    fixture->variableDeclaration.data.variableDeclaration.accessModifier = variableAccess;

    fixture->statementNodes[variableFirst ? 0U : 1U] = &fixture->variableDeclaration;
    fixture->statementNodes[variableFirst ? 1U : 0U] = &fixture->functionDeclaration;
    fixture->statements.nodes = fixture->statementNodes;
    fixture->statements.count = 2U;
    fixture->statements.capacity = 2U;
    fixture->script.data.script.statements = &fixture->statements;

    ZrParser_InferredType_Init(g_state, &returnType, returnTypeKind);
    ZrParser_InferredType_Init(g_state, &variableType, variableTypeKind);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state,
            fixture->environment,
            functionName,
            &returnType,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL,
            &fixture->functionDeclaration));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            fixture->environment,
            variableName,
            &variableType,
            &fixture->variableIdentifier,
            fixture->variableIdentifier.location));
    ZrParser_InferredType_Free(g_state, &returnType);
    ZrParser_InferredType_Free(g_state, &variableType);
}

static void semantic_public_contract_fixture_free(
        SZrSemanticPublicContractFixture *fixture) {
    if (fixture->environment != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(g_state, fixture->environment);
    }
    if (fixture->context != ZR_NULL) {
        ZrParser_SemanticContext_Free(fixture->context);
    }
}

static void semantic_public_contract_generic_fixture_init(
        SZrSemanticPublicContractFixture *fixture,
        TZrBool shiftSymbolId,
        TZrBool requiresStruct) {
    SZrInferredType genericType;
    SZrTypeGenericParameterInfo genericParameter;
    SZrArray parameterTypes;
    SZrArray genericParameters;
    SZrString *functionName;
    SZrString *parameterName;

    memset(fixture, 0, sizeof(*fixture));
    fixture->context = ZrParser_SemanticContext_New(g_state);
    fixture->environment = ZrParser_TypeEnvironment_New(g_state);
    TEST_ASSERT_NOT_NULL(fixture->context);
    TEST_ASSERT_NOT_NULL(fixture->environment);
    fixture->environment->semanticContext = fixture->context;

    functionName = ZrCore_String_Create(g_state, "identity", 8U);
    parameterName = ZrCore_String_Create(g_state, "value", 5U);
    TEST_ASSERT_NOT_NULL(functionName);
    TEST_ASSERT_NOT_NULL(parameterName);
    init_node(&fixture->script, ZR_AST_SCRIPT, 0U, 60U);
    init_node(&fixture->functionDeclaration, ZR_AST_FUNCTION_DECLARATION, 1U, 40U);
    init_node(&fixture->parameterNode, ZR_AST_PARAMETER, 14U, 24U);
    fixture->functionIdentifier.name = functionName;
    fixture->parameterIdentifier.name = parameterName;
    fixture->functionDeclaration.data.functionDeclaration.name =
            &fixture->functionIdentifier;
    fixture->functionDeclaration.data.functionDeclaration.nameLocation =
            test_range(5U, 13U);
    fixture->parameterNode.data.parameter.name = &fixture->parameterIdentifier;
    fixture->parameterNodes[0] = &fixture->parameterNode;
    fixture->parameters.nodes = fixture->parameterNodes;
    fixture->parameters.count = 1U;
    fixture->parameters.capacity = 1U;
    fixture->functionDeclaration.data.functionDeclaration.params =
            &fixture->parameters;
    fixture->statementNodes[0] = &fixture->functionDeclaration;
    fixture->statements.nodes = fixture->statementNodes;
    fixture->statements.count = 1U;
    fixture->statements.capacity = 1U;
    fixture->script.data.script.statements = &fixture->statements;

    if (shiftSymbolId) {
        SZrInferredType shiftType;
        SZrString *shiftName = ZrCore_String_Create(g_state, "shift", 5U);

        init_node(&fixture->symbolShiftIdentifier, ZR_AST_IDENTIFIER_LITERAL, 50U, 55U);
        fixture->symbolShiftIdentifier.data.identifier.name = shiftName;
        ZrParser_InferredType_Init(g_state, &shiftType, ZR_VALUE_TYPE_INT64);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
                g_state,
                fixture->environment,
                shiftName,
                &shiftType,
                &fixture->symbolShiftIdentifier,
                fixture->symbolShiftIdentifier.location));
        ZrParser_InferredType_Free(g_state, &shiftType);
    }

    ZrParser_InferredType_InitFull(
            g_state,
            &genericType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            ZrCore_String_Create(g_state, "T", 1U));
    ZrCore_Array_Init(g_state, &parameterTypes, sizeof(SZrInferredType), 1U);
    ZrCore_Array_Push(g_state, &parameterTypes, &genericType);
    memset(&genericParameter, 0, sizeof(genericParameter));
    genericParameter.name = genericType.typeName;
    genericParameter.genericKind = ZR_GENERIC_PARAMETER_TYPE;
    genericParameter.requiresStruct = requiresStruct;
    ZrCore_Array_Construct(&genericParameter.constraintTypeNames);
    ZrCore_Array_Init(
            g_state, &genericParameters, sizeof(SZrTypeGenericParameterInfo), 1U);
    ZrCore_Array_Push(g_state, &genericParameters, &genericParameter);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunctionEx(
            g_state,
            fixture->environment,
            functionName,
            &genericType,
            &parameterTypes,
            &genericParameters,
            ZR_NULL,
            &fixture->functionDeclaration));

    ZrCore_Array_Free(g_state, &genericParameters);
    ZrCore_Array_Free(g_state, &parameterTypes);
    ZrParser_InferredType_Free(g_state, &genericType);
}

static SZrParserSemanticPublicContractQuery semantic_public_contract_query(
        const SZrSemanticPublicContractFixture *fixture) {
    SZrParserSemanticPublicContractQuery query;

    memset(&query, 0, sizeof(query));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PublicContract(
            fixture->context,
            fixture->environment,
            &fixture->script,
            &query));
    return query;
}

static void test_semantic_query_public_contract_ignores_private_variable_type_changes(void) {
    SZrSemanticPublicContractFixture before;
    SZrSemanticPublicContractFixture after;
    SZrParserSemanticPublicContractQuery beforeQuery;
    SZrParserSemanticPublicContractQuery afterQuery;

    semantic_public_contract_fixture_init(
            &before, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64, ZR_ACCESS_PRIVATE, ZR_FALSE);
    semantic_public_contract_fixture_init(
            &after, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_FLOAT, ZR_ACCESS_PRIVATE, ZR_FALSE);
    beforeQuery = semantic_public_contract_query(&before);
    afterQuery = semantic_public_contract_query(&after);

    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)beforeQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)afterQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT64(beforeQuery.hash, afterQuery.hash);

    semantic_public_contract_fixture_free(&after);
    semantic_public_contract_fixture_free(&before);
}

static void test_semantic_query_public_contract_changes_for_public_signatures(void) {
    SZrSemanticPublicContractFixture functionBefore;
    SZrSemanticPublicContractFixture functionAfter;
    SZrSemanticPublicContractFixture variableBefore;
    SZrSemanticPublicContractFixture variableAfter;
    SZrParserSemanticPublicContractQuery functionBeforeQuery;
    SZrParserSemanticPublicContractQuery functionAfterQuery;
    SZrParserSemanticPublicContractQuery variableBeforeQuery;
    SZrParserSemanticPublicContractQuery variableAfterQuery;

    semantic_public_contract_fixture_init(
            &functionBefore, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PRIVATE, ZR_FALSE);
    semantic_public_contract_fixture_init(
            &functionAfter, ZR_VALUE_TYPE_FLOAT, ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PRIVATE, ZR_FALSE);
    semantic_public_contract_fixture_init(
            &variableBefore, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PUBLIC, ZR_FALSE);
    semantic_public_contract_fixture_init(
            &variableAfter, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_FLOAT,
            ZR_ACCESS_PUBLIC, ZR_FALSE);

    functionBeforeQuery = semantic_public_contract_query(&functionBefore);
    functionAfterQuery = semantic_public_contract_query(&functionAfter);
    variableBeforeQuery = semantic_public_contract_query(&variableBefore);
    variableAfterQuery = semantic_public_contract_query(&variableAfter);

    TEST_ASSERT_NOT_EQUAL(functionBeforeQuery.hash, functionAfterQuery.hash);
    TEST_ASSERT_NOT_EQUAL(variableBeforeQuery.hash, variableAfterQuery.hash);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)variableBeforeQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)variableAfterQuery.exportCount);

    semantic_public_contract_fixture_free(&variableAfter);
    semantic_public_contract_fixture_free(&variableBefore);
    semantic_public_contract_fixture_free(&functionAfter);
    semantic_public_contract_fixture_free(&functionBefore);
}

static void test_semantic_query_public_contract_is_stable_across_declaration_order(void) {
    SZrSemanticPublicContractFixture functionFirst;
    SZrSemanticPublicContractFixture variableFirst;
    SZrParserSemanticPublicContractQuery functionFirstQuery;
    SZrParserSemanticPublicContractQuery variableFirstQuery;

    semantic_public_contract_fixture_init(
            &functionFirst, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_FLOAT,
            ZR_ACCESS_PUBLIC, ZR_FALSE);
    semantic_public_contract_fixture_init(
            &variableFirst, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_FLOAT,
            ZR_ACCESS_PUBLIC, ZR_TRUE);
    functionFirstQuery = semantic_public_contract_query(&functionFirst);
    variableFirstQuery = semantic_public_contract_query(&variableFirst);

    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)functionFirstQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)variableFirstQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT64(functionFirstQuery.hash, variableFirstQuery.hash);

    semantic_public_contract_fixture_free(&variableFirst);
    semantic_public_contract_fixture_free(&functionFirst);
}

static void test_semantic_query_public_contract_normalizes_generic_owner_ids(void) {
    SZrSemanticPublicContractFixture original;
    SZrSemanticPublicContractFixture shifted;
    SZrSemanticPublicContractFixture constrained;
    SZrParserSemanticPublicContractQuery originalQuery;
    SZrParserSemanticPublicContractQuery shiftedQuery;
    SZrParserSemanticPublicContractQuery constrainedQuery;

    semantic_public_contract_generic_fixture_init(&original, ZR_FALSE, ZR_FALSE);
    semantic_public_contract_generic_fixture_init(&shifted, ZR_TRUE, ZR_FALSE);
    semantic_public_contract_generic_fixture_init(&constrained, ZR_FALSE, ZR_TRUE);
    originalQuery = semantic_public_contract_query(&original);
    shiftedQuery = semantic_public_contract_query(&shifted);
    constrainedQuery = semantic_public_contract_query(&constrained);

    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)originalQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)shiftedQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT64(originalQuery.hash, shiftedQuery.hash);
    TEST_ASSERT_NOT_EQUAL(originalQuery.hash, constrainedQuery.hash);

    semantic_public_contract_fixture_free(&constrained);
    semantic_public_contract_fixture_free(&shifted);
    semantic_public_contract_fixture_free(&original);
}

static void test_semantic_query_public_contract_hashes_parameter_names(void) {
    SZrSemanticPublicContractFixture before;
    SZrSemanticPublicContractFixture after;
    SZrParserSemanticPublicContractQuery beforeQuery;
    SZrParserSemanticPublicContractQuery afterQuery;

    semantic_public_contract_generic_fixture_init(&before, ZR_FALSE, ZR_FALSE);
    semantic_public_contract_generic_fixture_init(&after, ZR_FALSE, ZR_FALSE);
    after.parameterIdentifier.name = ZrCore_String_Create(g_state, "item", 4U);
    TEST_ASSERT_NOT_NULL(after.parameterIdentifier.name);

    beforeQuery = semantic_public_contract_query(&before);
    afterQuery = semantic_public_contract_query(&after);

    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)beforeQuery.exportCount);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)afterQuery.exportCount);
    TEST_ASSERT_NOT_EQUAL(beforeQuery.hash, afterQuery.hash);

    semantic_public_contract_fixture_free(&after);
    semantic_public_contract_fixture_free(&before);
}

static void test_semantic_query_public_contract_rejects_unnormalized_public_surfaces(void) {
    SZrSemanticPublicContractFixture defaultParameter;
    SZrSemanticPublicContractFixture variadic;
    SZrSemanticPublicContractFixture publicConst;
    SZrParserSemanticPublicContractQuery query;

    semantic_public_contract_generic_fixture_init(
            &defaultParameter, ZR_FALSE, ZR_FALSE);
    defaultParameter.parameterNode.data.parameter.defaultValue =
            &defaultParameter.functionDeclaration;
    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            defaultParameter.context,
            defaultParameter.environment,
            &defaultParameter.script,
            &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_generic_fixture_init(&variadic, ZR_FALSE, ZR_FALSE);
    variadic.functionDeclaration.data.functionDeclaration.args =
            &variadic.parameterNode.data.parameter;
    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            variadic.context, variadic.environment, &variadic.script, &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_fixture_init(
            &publicConst,
            ZR_VALUE_TYPE_INT64,
            ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PUBLIC,
            ZR_FALSE);
    publicConst.variableDeclaration.data.variableDeclaration.isConst = ZR_TRUE;
    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            publicConst.context,
            publicConst.environment,
            &publicConst.script,
            &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_fixture_free(&publicConst);
    semantic_public_contract_fixture_free(&variadic);
    semantic_public_contract_fixture_free(&defaultParameter);
}

static void test_semantic_query_public_contract_rejects_poisoned_or_unsupported_modules(void) {
    SZrSemanticPublicContractFixture fixture;
    SZrParserSemanticPublicContractQuery query;
    SZrStructuredDiagnostic diagnostic;
    SZrSemanticDiagnosticFact diagnosticFact;
    SZrAstNode publicStruct;

    semantic_public_contract_fixture_init(
            &fixture, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PRIVATE, ZR_FALSE);
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    ZrCore_Array_Push(g_state, &fixture.context->queryDiagnostics, &diagnostic);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)fixture.context->queryDiagnostics.length);
    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            fixture.context, fixture.environment, &fixture.script, &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);
    fixture.context->queryDiagnostics.length = 0U;

    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_Build(
            g_state,
            &diagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            test_range(17U, 23U),
            "compiler_error",
            "Published compiler diagnostic",
            ZR_NULL,
            ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_StructuredDiagnostic_SetNoFixReason(
            &diagnostic,
            ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT));
    memset(&diagnosticFact, 0, sizeof(diagnosticFact));
    diagnosticFact.diagnostic = diagnostic;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendDiagnostic(
            fixture.context, &diagnosticFact));
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
    TEST_ASSERT_EQUAL_UINT32(
            1U, (TZrUInt32)fixture.context->diagnosticFacts.length);
    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            fixture.context, fixture.environment, &fixture.script, &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);
    ZrParser_SemanticFacts_Reset(fixture.context);

    init_node(&publicStruct, ZR_AST_STRUCT_DECLARATION, 61U, 79U);
    publicStruct.data.structDeclaration.accessModifier = ZR_ACCESS_PUBLIC;
    fixture.statementNodes[1] = &publicStruct;
    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            fixture.context, fixture.environment, &fixture.script, &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_fixture_free(&fixture);
}

static void test_semantic_query_public_contract_rejects_mismatched_semantic_owners(void) {
    SZrSemanticPublicContractFixture contextFixture;
    SZrSemanticPublicContractFixture environmentFixture;
    SZrParserSemanticPublicContractQuery query;

    semantic_public_contract_fixture_init(
            &contextFixture,
            ZR_VALUE_TYPE_INT64,
            ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PRIVATE,
            ZR_FALSE);
    semantic_public_contract_fixture_init(
            &environmentFixture,
            ZR_VALUE_TYPE_INT64,
            ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PRIVATE,
            ZR_FALSE);

    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            contextFixture.context,
            environmentFixture.environment,
            &environmentFixture.script,
            &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_fixture_free(&environmentFixture);
    semantic_public_contract_fixture_free(&contextFixture);
}

static void test_semantic_query_public_contract_rejects_noncanonical_generic_constraints(void) {
    SZrSemanticPublicContractFixture fixture;
    SZrParserSemanticPublicContractQuery query;
    SZrFunctionTypeInfo **functionInfo;
    SZrTypeGenericParameterInfo *genericParameter;
    SZrString *constraintName;

    semantic_public_contract_generic_fixture_init(&fixture, ZR_FALSE, ZR_FALSE);
    functionInfo = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
            &fixture.environment->functionReturnTypes, 0U);
    TEST_ASSERT_NOT_NULL(functionInfo);
    TEST_ASSERT_NOT_NULL(*functionInfo);
    genericParameter = (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
            &(*functionInfo)->genericParameters, 0U);
    TEST_ASSERT_NOT_NULL(genericParameter);
    constraintName = ZrCore_String_Create(g_state, "Constraint", 10U);
    TEST_ASSERT_NOT_NULL(constraintName);
    ZrCore_Array_Init(
            g_state,
            &genericParameter->constraintTypeNames,
            sizeof(SZrString *),
            1U);
    ZrCore_Array_Push(
            g_state, &genericParameter->constraintTypeNames, &constraintName);

    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            fixture.context, fixture.environment, &fixture.script, &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_fixture_free(&fixture);
}

static void test_semantic_query_public_contract_rejects_removed_intermediate_wire_value(void) {
    SZrSemanticPublicContractFixture fixture;
    SZrParserSemanticPublicContractQuery query;
    SZrAstNode removedIntermediateStatement;

    semantic_public_contract_fixture_init(
            &fixture,
            ZR_VALUE_TYPE_INT64,
            ZR_VALUE_TYPE_INT64,
            ZR_ACCESS_PRIVATE,
            ZR_FALSE);
    init_node(
            &removedIntermediateStatement,
            (EZrAstNodeType)ZR_AST_CONSTANT_REMOVED_INTERMEDIATE_STATEMENT,
            61U,
            79U);
    fixture.statementNodes[1] = &removedIntermediateStatement;

    query.hash = 99U;
    query.exportCount = 99U;
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PublicContract(
            fixture.context, fixture.environment, &fixture.script, &query));
    TEST_ASSERT_EQUAL_UINT64(0U, query.hash);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)query.exportCount);

    semantic_public_contract_fixture_free(&fixture);
}

#endif
