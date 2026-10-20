#ifndef ZR_VM_TEST_REFERENCE_FUNCTION_IDENTITY_FACT_CASES_H
#define ZR_VM_TEST_REFERENCE_FUNCTION_IDENTITY_FACT_CASES_H

#include <stddef.h>

#include "parser/parser_range_evidence.h"
#include "zr_vm_parser/token_stream.h"

typedef enum EFunctionIdentityCase {
    FUNCTION_IDENTITY_NATIVE_EXTERN,
    FUNCTION_IDENTITY_RUNTIME_READ,
    FUNCTION_IDENTITY_COMPILE_TIME_READ,
    FUNCTION_IDENTITY_UNRESOLVED_READ_UPGRADE
} EFunctionIdentityCase;

static const SZrAstNode *function_identity_name_node(const SZrIdentifier *name) {
    const SZrAstNode *node;

    if (name == ZR_NULL) {
        return ZR_NULL;
    }
    node = (const SZrAstNode *)((const char *)name -
            offsetof(SZrAstNode, data.identifier));
    return node->type == ZR_AST_IDENTIFIER_LITERAL ? node : ZR_NULL;
}

static TZrBool function_identity_same_range(
        SZrFileRange left, SZrFileRange right) {
    return left.source == right.source &&
           left.start.offset == right.start.offset &&
           left.start.line == right.start.line &&
           left.start.column == right.start.column &&
           left.end.offset == right.end.offset &&
           left.end.line == right.end.line &&
           left.end.column == right.end.column &&
           left.originKey == right.originKey &&
           left.ownerKey == right.ownerKey &&
           left.evidenceIndex == right.evidenceIndex &&
           left.flags == right.flags;
}

static const SZrTokenTree *function_identity_tree_at_offset(
        const SZrTokenTree *tree, TZrSize offset) {
    if (tree == ZR_NULL) {
        return ZR_NULL;
    }
    if (ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_GROUP) {
        for (TZrSize index = 0U;
             index < ZrParser_TokenTree_GroupChildCount(tree); index++) {
            const SZrTokenTree *found = function_identity_tree_at_offset(
                    ZrParser_TokenTree_GroupChildAt(tree, index), offset);
            if (found != ZR_NULL) {
                return found;
            }
        }
        return ZR_NULL;
    }
    return ZrParser_TokenTree_Span(tree).startOffset == offset ? tree : ZR_NULL;
}

static TZrBool function_identity_native_leaf(
        SZrTokenStream *stream,
        SZrAstOwnership *owner,
        const SZrAstNode *node,
        const char *source,
        const char *sourceName,
        const char *nameStart,
        const char *name) {
    const SZrTokenTree *tree = ZR_NULL;
    SZrTokenSpan span;
    SZrSyntaxContext context;
    SZrParserRangeEvidenceDescription description = {0};
    const char *sourceBytes;
    const char *nativeName;
    const char *projectedName;
    TZrSize sourceLength = 0U;
    TZrSize sourceNameLength = 0U;
    TZrSize offset;

    if (stream == ZR_NULL || owner == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL || node->ownership != owner ||
        source == ZR_NULL || sourceName == ZR_NULL ||
        nameStart == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    offset = (TZrSize)(nameStart - source);
    for (TZrSize index = 0U;
         index < ZrParser_TokenStream_Count(stream) && tree == ZR_NULL;
         index++) {
        tree = function_identity_tree_at_offset(
                ZrParser_TokenStream_At(stream, index), offset);
    }
    if (tree == ZR_NULL ||
        ZrParser_TokenTree_Kind(tree) != ZR_TOKEN_TREE_IDENT ||
        !ZrParser_RangeEvidence_Validate(owner, node->location) ||
        !ZrParser_RangeEvidence_Describe(owner, node->location, &description)) {
        return ZR_FALSE;
    }
    span = ZrParser_TokenTree_Span(tree);
    context = ZrParser_TokenTree_Context(tree);
    sourceBytes = ZrParser_TokenStream_SourceBytes(
            stream, span.sourceId, &sourceLength);
    nativeName = ZrParser_TokenStream_SourceName(
            stream, span.sourceId, &sourceNameLength);
    projectedName = node->location.source != ZR_NULL
            ? ZrCore_String_GetNativeString(node->location.source) : ZR_NULL;
    return description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
           description.first != 0U && description.first == description.last &&
           node->location.originKey != 0U && node->location.ownerKey != 0U &&
           node->location.evidenceIndex != 0U && node->location.flags == 0U &&
           span.sourceId != ZR_TOKEN_SOURCE_INVALID &&
           sourceBytes != ZR_NULL && sourceLength == strlen(source) &&
           memcmp(sourceBytes, source, sourceLength) == 0 &&
           nativeName != ZR_NULL && sourceNameLength == strlen(sourceName) &&
           memcmp(nativeName, sourceName, sourceNameLength) == 0 &&
           projectedName != ZR_NULL && strcmp(projectedName, sourceName) == 0 &&
           node->data.identifier.name != ZR_NULL &&
           strcmp(ZrCore_String_GetNativeString(node->data.identifier.name), name) == 0 &&
           span.startOffset == offset && span.endOffset == offset + strlen(name) &&
           node->location.start.offset == span.startOffset &&
           node->location.start.line == span.startLine &&
           node->location.start.column == span.startColumn &&
           node->location.end.offset == span.endOffset &&
           node->location.end.line == span.endLine &&
           node->location.end.column == span.endColumn &&
           node->data.identifier.syntaxContext.table == context.table &&
           node->data.identifier.syntaxContext.markId == context.markId &&
           context.table == ZrParser_TokenStream_ContextTable(stream);
}

static TZrSize function_identity_read_count(
        SZrSemanticContext *context, const SZrAstNode *node) {
    TZrSize count = 0U;

    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts, index);
        if (fact != ZR_NULL && fact->node == node &&
            fact->kind == ZR_SEMANTIC_REFERENCE_READ) {
            count++;
        }
    }
    return count;
}

static TZrBool function_identity_seed_unresolved_read(
        SZrCompilerState *cs, SZrAstNode *use,
        SZrSemanticReferenceFact *preserved) {
    SZrSemanticReferenceFact seed = {0};
    SZrSemanticCallArgumentFact argument = {0};
    const SZrSemanticReferenceFact *stored;
    TZrBool appended = ZR_FALSE;

    seed.node = use;
    seed.range = use->location;
    seed.kind = ZR_SEMANTIC_REFERENCE_READ;
    seed.name = use->data.identifier.name;
    seed.signatureDisplay = ZrCore_String_CreateFromNative(
            g_state, "preserved callable signature");
    seed.externalOwnerIdentity = ZrCore_String_CreateFromNative(
            g_state, "preserved.fixture.owner");
    seed.definitionRange = use->location;
    seed.hasDefinitionRange = ZR_TRUE;
    seed.placeId = 1U;
    seed.definiteAssignmentState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT;
    seed.hasDefiniteAssignmentState = ZR_TRUE;
    ZrCore_Array_Init(g_state, &seed.definitionRanges, sizeof(SZrFileRange), 1U);
    ZrCore_Array_Init(g_state, &seed.argumentMappings,
                     sizeof(SZrSemanticCallArgumentFact), 1U);
    if (seed.signatureDisplay == ZR_NULL || seed.externalOwnerIdentity == ZR_NULL ||
        !seed.definitionRanges.isValid || !seed.argumentMappings.isValid) {
        goto cleanup;
    }
    argument.argumentRange = use->location;
    argument.passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
    ZrCore_Array_Push(g_state, &seed.definitionRanges, &seed.definitionRange);
    ZrCore_Array_Push(g_state, &seed.argumentMappings, &argument);
    if (seed.definitionRanges.length != 1U || seed.argumentMappings.length != 1U ||
        !ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &seed)) {
        goto cleanup;
    }
    stored = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext, use, ZR_SEMANTIC_REFERENCE_READ);
    if (stored != ZR_NULL && !stored->isResolved &&
        stored->signatureDisplay != ZR_NULL && stored->externalOwnerIdentity != ZR_NULL &&
        stored->definitionRanges.length == 1U && stored->argumentMappings.length == 1U) {
        *preserved = *stored;
        appended = ZR_TRUE;
    }

cleanup:
    if (seed.definitionRanges.isValid) {
        ZrCore_Array_Free(g_state, &seed.definitionRanges);
    }
    if (seed.argumentMappings.isValid) {
        ZrCore_Array_Free(g_state, &seed.argumentMappings);
    }
    return appended;
}

static TZrBool function_identity_owned_payload_is_preserved(
        const SZrSemanticReferenceFact *fact,
        const SZrSemanticReferenceFact *preserved) {
    const SZrFileRange *definition;
    const SZrSemanticCallArgumentFact *argument;

    if (fact == ZR_NULL || fact->signatureDisplay != preserved->signatureDisplay ||
        fact->externalOwnerIdentity != preserved->externalOwnerIdentity ||
        fact->placeId != preserved->placeId ||
        fact->hasDefinitionRange != preserved->hasDefinitionRange ||
        !function_identity_same_range(fact->definitionRange, preserved->definitionRange) ||
        fact->hasDefiniteAssignmentState != preserved->hasDefiniteAssignmentState ||
        fact->definiteAssignmentState != preserved->definiteAssignmentState ||
        fact->definitionRanges.length != 1U || fact->argumentMappings.length != 1U ||
        strcmp(ZrCore_String_GetNativeString(fact->signatureDisplay),
               "preserved callable signature") != 0 ||
        strcmp(ZrCore_String_GetNativeString(fact->externalOwnerIdentity),
               "preserved.fixture.owner") != 0) {
        return ZR_FALSE;
    }
    definition = (const SZrFileRange *)ZrCore_Array_Get(
            (SZrArray *)&fact->definitionRanges, 0U);
    argument = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)&fact->argumentMappings, 0U);
    return definition != ZR_NULL && argument != ZR_NULL &&
           definition == ZrCore_Array_Get((SZrArray *)&preserved->definitionRanges, 0U) &&
           argument == ZrCore_Array_Get((SZrArray *)&preserved->argumentMappings, 0U) &&
           function_identity_same_range(*definition, preserved->definitionRange) &&
           function_identity_same_range(argument->argumentRange, preserved->range) &&
           argument->argumentIndex == 0U && argument->parameterIndex == 0U &&
           argument->passingMode == ZR_PARAMETER_PASSING_MODE_VALUE;
}

static void function_identity_run_case(
        EFunctionIdentityCase caseKind, const char *finalMessage) {
    static const char *externSource =
            "native extern(\"fixture\") {\n"
            "    fn NativeAdd(lhs: i64, rhs: i64): i64;\n"
            "}\n"
            "NativeAdd(1, 2);";
    static const char *readSource =
            "fn FunctionValue(value: i64): i64 { return value; }\n"
            "FunctionValue;";
    const char *source = caseKind == FUNCTION_IDENTITY_NATIVE_EXTERN
            ? externSource : readSource;
    const char *name = caseKind == FUNCTION_IDENTITY_NATIVE_EXTERN
            ? "NativeAdd" : "FunctionValue";
    const char *sourceNameText = "reference_function_identity_native.zr";
    const char *nameStart = strstr(source, name);
    const char *useStart = nameStart != ZR_NULL
            ? strstr(nameStart + strlen(name), name) : ZR_NULL;
    const char *failure = "Fixture prerequisite: incomplete function identity setup";
    SZrSyntaxContextTable *table = ZR_NULL;
    SZrTokenStream *stream = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrAstNode *declaration = ZR_NULL;
    const SZrAstNode *nameNode = ZR_NULL;
    SZrAstNode *use = ZR_NULL;
    SZrAstNode *expression = ZR_NULL;
    SZrCompilerState *cs = ZR_NULL;
    SZrTypeEnvironment *originalRuntime = ZR_NULL;
    SZrTypeEnvironment *emptyRuntime = ZR_NULL;
    SZrTypeEnvironment *emptyParent = ZR_NULL;
    SZrFunctionTypeInfo *function = ZR_NULL;
    SZrFunctionTypeInfo *canonical = ZR_NULL;
    SZrFunctionTypeInfo *unexpected = ZR_NULL;
    SZrInferredType intType = {0};
    SZrInferredType result = {0};
    SZrArray parameterTypes = {0};
    SZrArray passingModes = {0};
    const SZrSemanticSymbolRecord *symbol;
    const SZrSemanticReferenceFact *fact;
    const SZrSemanticReferenceFact *declarationFact;
    SZrSemanticReferenceFact preservedRead = {0};
    TZrSymbolId symbolId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId typeId = ZR_SEMANTIC_ID_INVALID;
    SZrFileRange declarationRange = {0};
    SZrFileRange useRange = {0};
    SZrSyntaxContext useContext = {0};
    TZrSize firstReadCount = 0U;
    TZrSize firstFactCount = 0U;
    TZrBool intTypeInitialized = ZR_FALSE;
    TZrBool resultInitialized = ZR_FALSE;
    TZrBool passed = ZR_FALSE;
    TZrBool firstMatched = ZR_FALSE;
    TZrSize parameterCount = caseKind == FUNCTION_IDENTITY_NATIVE_EXTERN ? 2U : 1U;

    table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    if (table == ZR_NULL ||
        !ZrParser_TokenStream_FromText(
                g_state, table, sourceNameText, strlen(sourceNameText),
                source, strlen(source), ZR_NULL, &stream) ||
        stream == ZR_NULL || !ZrParser_TokenStream_IsSealed(stream) ||
        ZrParser_TokenStream_ContextTable(stream) != table) {
        failure = "Fixture prerequisite: real sealed source token stream is required";
        goto cleanup;
    }
    sourceName = ZrCore_String_Create(
            g_state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    if (sourceName == ZR_NULL) {
        failure = "Fixture prerequisite: source name allocation failed";
        goto cleanup;
    }
    ast = ZrParser_ParseTokenStream(g_state, stream, sourceName);
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->ownership == ZR_NULL ||
        ast->data.script.syntaxContextTable !=
                ZrParser_TokenStream_ContextTable(stream)) {
        failure = "Fixture prerequisite: real native parser AST owner is required";
        goto cleanup;
    }
    declaration = script_statement_at(ast, 0U);
    if (caseKind == FUNCTION_IDENTITY_NATIVE_EXTERN) {
        if (declaration == ZR_NULL || declaration->type != ZR_AST_EXTERN_BLOCK ||
            declaration->data.externBlock.declarations == ZR_NULL ||
            declaration->data.externBlock.declarations->count != 1U) {
            failure = "Fixture prerequisite: native extern declaration is required";
            goto cleanup;
        }
        declaration = declaration->data.externBlock.declarations->nodes[0];
        if (declaration == ZR_NULL ||
            declaration->type != ZR_AST_EXTERN_FUNCTION_DECLARATION) {
            failure = "Fixture prerequisite: native extern function AST is required";
            goto cleanup;
        }
        nameNode = function_identity_name_node(
                declaration->data.externFunctionDeclaration.name);
    } else {
        if (declaration == ZR_NULL ||
            declaration->type != ZR_AST_FUNCTION_DECLARATION) {
            failure = "Fixture prerequisite: source function declaration is required";
            goto cleanup;
        }
        nameNode = function_identity_name_node(
                declaration->data.functionDeclaration.name);
    }
    expression = expression_statement_expression_at(ast, 1U);
    if (expression == ZR_NULL) {
        failure = "Fixture prerequisite: source use expression is required";
        goto cleanup;
    }
    use = expression->type == ZR_AST_PRIMARY_EXPRESSION
            ? expression->data.primaryExpression.property : expression;
    if (declaration->ownership != ast->ownership ||
        !function_identity_native_leaf(
                stream, ast->ownership, nameNode,
                source, sourceNameText, nameStart, name) ||
        !function_identity_native_leaf(
                stream, ast->ownership, use,
                source, sourceNameText, useStart, name)) {
        failure = "Fixture prerequisite: real declaration/use leaf provenance is required";
        goto cleanup;
    }
    if (caseKind != FUNCTION_IDENTITY_NATIVE_EXTERN &&
        !function_identity_same_range(
                nameNode->location, declaration->data.functionDeclaration.nameLocation)) {
        failure = "Fixture prerequisite: function nameLocation must be the real name leaf";
        goto cleanup;
    }

    cs = (SZrCompilerState *)malloc(sizeof(*cs));
    if (cs == ZR_NULL) {
        failure = "Fixture prerequisite: compiler state allocation failed";
        goto cleanup;
    }
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    if (cs->semanticContext == ZR_NULL || cs->typeEnv == ZR_NULL ||
        cs->compileTimeTypeEnv == ZR_NULL) {
        failure = "Fixture prerequisite: real compiler semantic environments are required";
        goto cleanup;
    }
    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    intTypeInitialized = ZR_TRUE;
    ZrCore_Array_Init(g_state, &parameterTypes, sizeof(SZrInferredType), parameterCount);
    ZrCore_Array_Init(g_state, &passingModes, sizeof(EZrParameterPassingMode), parameterCount);
    if (!parameterTypes.isValid || !passingModes.isValid) {
        failure = "Fixture prerequisite: parameter metadata allocation failed";
        goto cleanup;
    }
    for (TZrSize index = 0U; index < parameterCount; index++) {
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        ZrCore_Array_Push(g_state, &parameterTypes, &intType);
        ZrCore_Array_Push(g_state, &passingModes, &passingMode);
    }
    if (parameterTypes.length != parameterCount ||
        passingModes.length != parameterCount ||
        !ZrParser_TypeEnvironment_RegisterFunctionEx(
                g_state, cs->typeEnv, nameNode->data.identifier.name,
                &intType, &parameterTypes, ZR_NULL, &passingModes, declaration) ||
        !ZrParser_TypeEnvironment_LookupFunctionWithContext(
                cs->typeEnv, use->data.identifier.name,
                use->data.identifier.syntaxContext, &function) ||
        function == ZR_NULL) {
        failure = "Fixture prerequisite: public source-function registration must succeed";
        goto cleanup;
    }
    symbolId = function->symbolId;
    typeId = function->typeId;
    declarationRange = nameNode->location;
    useRange = use->location;
    useContext = use->data.identifier.syntaxContext;
    symbol = ZrParser_Semantic_FindSymbolById(cs->semanticContext, symbolId);
    if (symbolId == ZR_SEMANTIC_ID_INVALID || typeId == ZR_SEMANTIC_ID_INVALID ||
        symbol == ZR_NULL || symbol->astNode != declaration ||
        symbol->typeId != typeId) {
        failure = "Fixture prerequisite: function identity must come from the real declaration";
        goto cleanup;
    }
    if (caseKind != FUNCTION_IDENTITY_NATIVE_EXTERN &&
        (!function->hasDeclarationRange ||
         !function_identity_same_range(function->declarationRange, declarationRange))) {
        failure = "Fixture prerequisite: ordinary function canonical name range is required";
        goto cleanup;
    }
    if (caseKind == FUNCTION_IDENTITY_COMPILE_TIME_READ) {
        if (!ZrParser_TypeEnvironment_RegisterCanonicalFunction(
                    g_state, cs->compileTimeTypeEnv, function->name,
                    &function->returnType, &function->paramTypes,
                    &function->parameterPassingModes, symbolId, typeId,
                    function->declarationRange) ||
            !ZrParser_TypeEnvironment_LookupFunctionWithContext(
                    cs->compileTimeTypeEnv, use->data.identifier.name,
                    useContext, &canonical) ||
            canonical == ZR_NULL || canonical->declarationNode != ZR_NULL ||
            canonical->symbolId != symbolId || canonical->typeId != typeId ||
            !canonical->hasDeclarationRange ||
            !function_identity_same_range(canonical->declarationRange, declarationRange)) {
            failure = "Fixture prerequisite: canonical compile-time callable must retain identity without declarationNode";
            goto cleanup;
        }
        emptyRuntime = ZrParser_TypeEnvironment_New(g_state);
        emptyParent = ZrParser_TypeEnvironment_New(g_state);
        if (emptyRuntime == ZR_NULL || emptyParent == ZR_NULL) {
            failure = "Fixture prerequisite: empty runtime lookup chain allocation failed";
            goto cleanup;
        }
        emptyRuntime->semanticContext = cs->semanticContext;
        emptyParent->semanticContext = cs->semanticContext;
        emptyRuntime->parent = emptyParent;
        originalRuntime = cs->typeEnv;
        cs->typeEnv = emptyRuntime;
        if (ZrParser_TypeEnvironment_LookupFunctionWithContext(
                    cs->typeEnv, use->data.identifier.name, useContext, &unexpected) ||
            ZrParser_TypeEnvironment_LookupFunctionWithContext(
                    emptyParent, use->data.identifier.name, useContext, &unexpected)) {
            failure = "Fixture prerequisite: runtime and parent must not resolve the compile-time use";
            goto cleanup;
        }
    }
    if (caseKind == FUNCTION_IDENTITY_UNRESOLVED_READ_UPGRADE &&
        !function_identity_seed_unresolved_read(cs, use, &preservedRead)) {
        failure = "Fixture prerequisite: unresolved READ must own cloned payloads";
        goto cleanup;
    }
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    resultInitialized = ZR_TRUE;
    failure = finalMessage;
    if (caseKind == FUNCTION_IDENTITY_NATIVE_EXTERN) {
        if (!ZrParser_ExpressionType_Infer(cs, expression, &result)) {
            failure = "Fixture prerequisite: real native extern call inference must succeed";
            goto cleanup;
        }
        /* Both borrowed fact pointers are acquired after all fact-producing calls. */
        fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                cs->semanticContext, expression, ZR_SEMANTIC_REFERENCE_CALL);
        declarationFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                cs->semanticContext, declaration, ZR_SEMANTIC_REFERENCE_DECLARATION);
        passed = result.baseType == ZR_VALUE_TYPE_INT64 &&
                 function->hasDeclarationRange &&
                 function_identity_same_range(function->declarationRange, declarationRange) &&
                 fact != ZR_NULL && fact->isResolved &&
                 fact->symbolId == symbolId && fact->typeId == typeId &&
                 function_identity_same_range(fact->range, useRange) &&
                 function_identity_same_range(fact->declarationRange, declarationRange) &&
                 declarationFact != ZR_NULL && declarationFact->isResolved &&
                 declarationFact->symbolId == symbolId &&
                 declarationFact->typeId == typeId &&
                 function_identity_same_range(declarationFact->range, declarationRange);
    } else {
        for (TZrSize iteration = 0U; iteration < 2U; iteration++) {
            passed = ZR_FALSE;
            fact = ZR_NULL;
            if (iteration != 0U) {
                ZrParser_InferredType_Free(g_state, &result);
                ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
            }
            if (!ZrParser_IdentifierType_Infer(cs, use, &result) ||
                result.baseType != ZR_VALUE_TYPE_CLOSURE) {
                failure = "Fixture prerequisite: direct function-value identifier inference must succeed twice";
                goto cleanup;
            }
            /* Do not retain this array-borrowed pointer across the next inference. */
            fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                    cs->semanticContext, use, ZR_SEMANTIC_REFERENCE_READ);
            passed = fact != ZR_NULL && fact->isResolved &&
                     fact->node == use && fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
                     fact->symbolId == symbolId && fact->typeId == typeId &&
                     fact->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION &&
                     fact->syntaxContext.table == useContext.table &&
                     fact->syntaxContext.markId == useContext.markId &&
                     function_identity_same_range(fact->range, useRange) &&
                     function_identity_same_range(fact->declarationRange, declarationRange) &&
                     function_identity_read_count(cs->semanticContext, use) == 1U;
            if (caseKind == FUNCTION_IDENTITY_UNRESOLVED_READ_UPGRADE) {
                passed = passed && function_identity_owned_payload_is_preserved(
                        fact, &preservedRead);
            }
            if (iteration == 0U) {
                firstMatched = passed;
                firstReadCount = function_identity_read_count(cs->semanticContext, use);
                firstFactCount = cs->semanticContext->referenceFacts.length;
            }
        }
        passed = passed && firstMatched && firstReadCount == 1U &&
                 cs->semanticContext->referenceFacts.length == firstFactCount;
        if (caseKind == FUNCTION_IDENTITY_COMPILE_TIME_READ) {
            passed = passed && canonical->declarationNode == ZR_NULL &&
                     canonical->symbolId == symbolId && canonical->typeId == typeId;
        }
    }

cleanup:
    if (cs != ZR_NULL && originalRuntime != ZR_NULL) {
        cs->typeEnv = originalRuntime;
    }
    if (emptyRuntime != ZR_NULL) {
        emptyRuntime->parent = ZR_NULL;
        ZrParser_TypeEnvironment_Free(g_state, emptyRuntime);
    }
    if (emptyParent != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(g_state, emptyParent);
    }
    if (resultInitialized) {
        ZrParser_InferredType_Free(g_state, &result);
    }
    if (passingModes.isValid) {
        ZrCore_Array_Free(g_state, &passingModes);
    }
    if (parameterTypes.isValid) {
        ZrCore_Array_Free(g_state, &parameterTypes);
    }
    if (intTypeInitialized) {
        ZrParser_InferredType_Free(g_state, &intType);
    }
    if (cs != ZR_NULL) {
        ZrParser_CompilerState_Free(cs);
        free(cs);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
    ZrParser_TokenStream_Release(stream);
    ZrParser_SyntaxContextTable_Release(table);
    TEST_ASSERT_TRUE_MESSAGE(passed, failure);
}

static void test_native_extern_function_name_range_preserves_canonical_identity(void) {
    function_identity_run_case(
            FUNCTION_IDENTITY_NATIVE_EXTERN,
            "Native extern name range must retain parser provenance and canonical identity");
}

static void test_runtime_bare_function_value_records_one_canonical_read(void) {
    function_identity_run_case(
            FUNCTION_IDENTITY_RUNTIME_READ,
            "Runtime bare function value must publish one canonical use-site READ");
}

static void test_compile_time_bare_function_value_records_one_canonical_read(void) {
    function_identity_run_case(
            FUNCTION_IDENTITY_COMPILE_TIME_READ,
            "Compile-time bare function value must publish one canonical use-site READ without declarationNode");
}

static void test_function_value_read_upgrade_preserves_owned_payloads(void) {
    function_identity_run_case(
            FUNCTION_IDENTITY_UNRESOLVED_READ_UPGRADE,
            "Canonical function READ upgrade must preserve existing owned payloads");
}

#endif
