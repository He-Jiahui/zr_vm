#ifndef ZR_TEST_SEMANTIC_USING_RELEASE_EVIDENCE_CASES_H
#define ZR_TEST_SEMANTIC_USING_RELEASE_EVIDENCE_CASES_H

#include "zr_vm_parser/semantic_source_metadata.h"

typedef struct SZrUsingReleaseEvidenceFixture {
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrInferredType resourceType;
    SZrInferredType expressionType;
    SZrString *sourceName;
    TZrBool compilerInitialized;
    TZrBool resourceTypeInitialized;
    TZrBool expressionTypeInitialized;
} SZrUsingReleaseEvidenceFixture;

static SZrUsingReleaseEvidenceFixture g_usingReleaseEvidence;

static void using_release_evidence_cleanup(void) {
    if (g_usingReleaseEvidence.expressionTypeInitialized) {
        ZrParser_InferredType_Free(g_state, &g_usingReleaseEvidence.expressionType);
    }
    if (g_usingReleaseEvidence.resourceTypeInitialized) {
        ZrParser_InferredType_Free(g_state, &g_usingReleaseEvidence.resourceType);
    }
    if (g_usingReleaseEvidence.compilerInitialized) {
        release_compiler_function(&g_usingReleaseEvidence.compiler);
        ZrParser_CompilerState_Free(&g_usingReleaseEvidence.compiler);
    }
    if (g_usingReleaseEvidence.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_usingReleaseEvidence.ast);
    }
    memset(&g_usingReleaseEvidence, 0, sizeof(g_usingReleaseEvidence));
}

static void using_release_evidence_require_healthy(const TZrChar *stage) {
    SZrCompilerState *cs = &g_usingReleaseEvidence.compiler;

    if (cs->hasError || cs->hasStructuredError) {
        printf("Using evidence prerequisite: stage=%s error=%u structured=%u message=%s\n",
               stage, (unsigned)cs->hasError, (unsigned)cs->hasStructuredError,
               cs->errorMessage != ZR_NULL ? cs->errorMessage : "<null>");
    }
    TEST_ASSERT_FALSE_MESSAGE(cs->hasError || cs->hasStructuredError,
                              "Fixture prerequisite: staged using preparation failed");
}

static const SZrTypeBinding *using_release_evidence_binding(SZrAstNode *identifier) {
    const SZrTypeBinding *binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            g_usingReleaseEvidence.compiler.typeEnv,
            identifier->data.identifier.name, identifier->data.identifier.syntaxContext);

    TEST_ASSERT_NOT_NULL_MESSAGE(binding, "Fixture prerequisite: source-backed binding is missing");
    TEST_ASSERT_TRUE_MESSAGE(binding->hasDeclarationRange &&
            binding->declarationRange.source == g_usingReleaseEvidence.sourceName &&
            binding->symbolId != ZR_SEMANTIC_ID_INVALID && binding->typeId != ZR_SEMANTIC_ID_INVALID,
            "Fixture prerequisite: source-backed binding identity is missing");
    return binding;
}

static const SZrSemanticReferenceFact *using_release_evidence_read(SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_usingReleaseEvidence.compiler.semanticContext, node, ZR_SEMANTIC_REFERENCE_READ);
}

static void using_release_evidence_infer_expression(SZrAstNode *expression) {
    SZrCompilerState *cs = &g_usingReleaseEvidence.compiler;
    TZrBool inferred;

    ZrParser_InferredType_Init(g_state, &g_usingReleaseEvidence.expressionType, ZR_VALUE_TYPE_OBJECT);
    g_usingReleaseEvidence.expressionTypeInitialized = ZR_TRUE;
    inferred = ZrParser_ExpressionType_Infer(cs, expression, &g_usingReleaseEvidence.expressionType);
    ZrParser_InferredType_Free(g_state, &g_usingReleaseEvidence.expressionType);
    g_usingReleaseEvidence.expressionTypeInitialized = ZR_FALSE;
    TEST_ASSERT_TRUE_MESSAGE(inferred, "Fixture prerequisite: borrowed expression inference failed");
    using_release_evidence_require_healthy("borrowed expression");
}

static void using_release_evidence_check(const TZrChar *source, const TZrChar *sourceName,
                                        EZrOwnershipQualifier qualifier, const TZrChar *failure) {
    SZrCompilerState *cs = &g_usingReleaseEvidence.compiler;
    SZrAstNodeArray *statements;
    SZrAstNode *usingNode;
    SZrAstNode *resource;
    SZrAstNode *inside;
    SZrAstNode *outside;
    SZrSemanticContext *context;
    const SZrTypeBinding *resourceBinding;
    const SZrTypeBinding *borrowedBinding;
    const SZrSemanticReferenceFact *resourceRead;
    const SZrSemanticReferenceFact *insideRead;
    const SZrSemanticReferenceFact *outsideRead;
    const SZrSemanticOwnershipFact *release;
    const SZrSemanticOwnershipFact *insideOwnership;
    const SZrSemanticOwnershipFact *outsideOwnership;
    const SZrDeterministicCleanupStep *cleanup;
    const SZrStructuredDiagnostic *diagnostic;
    const SZrStructuredDiagnosticRelatedInformation *related = ZR_NULL;
    SZrParserSemanticQueryScope scope;
    TZrSize cleanupCount;
    TZrSize resourceReadCount = 0U;
    TZrBool passed;

    using_release_evidence_cleanup();
    g_usingReleaseEvidence.sourceName = ZrCore_String_Create(
            g_state, (TZrNativeString)sourceName, strlen(sourceName));
    TEST_ASSERT_NOT_NULL_MESSAGE(g_usingReleaseEvidence.sourceName,
                                 "Fixture prerequisite: using source name allocation failed");
    g_usingReleaseEvidence.ast = ZrParser_Parse(
            g_state, source, strlen(source), g_usingReleaseEvidence.sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_usingReleaseEvidence.ast,
                                 "Fixture prerequisite: using source parsing failed");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_usingReleaseEvidence.ast->type);
    statements = g_usingReleaseEvidence.ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    TEST_ASSERT_EQUAL_UINT64(6U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, statements->nodes[0]->type);
    usingNode = statements->nodes[4];
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingNode->type);
    TEST_ASSERT_TRUE(usingNode->data.usingStatement.isBlockScoped);
    TEST_ASSERT_EQUAL_INT(ZR_USING_GUARD_DROP, usingNode->data.usingStatement.guardKind);
    resource = usingNode->data.usingStatement.resource;
    TEST_ASSERT_NOT_NULL(resource);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, resource->type);
    TEST_ASSERT_NOT_NULL(usingNode->data.usingStatement.body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, usingNode->data.usingStatement.body->type);
    TEST_ASSERT_NOT_NULL(usingNode->data.usingStatement.body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, usingNode->data.usingStatement.body->data.block.body->count);
    inside = usingNode->data.usingStatement.body->data.block.body->nodes[0];
    outside = statements->nodes[5];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, inside->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, outside->type);
    inside = inside->data.expressionStatement.expr;
    outside = outside->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(inside);
    TEST_ASSERT_NOT_NULL(outside);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, inside->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, outside->type);

    ZrParser_CompilerState_Init(cs, g_state);
    g_usingReleaseEvidence.compilerInitialized = ZR_TRUE;
    cs->suppressErrorOutput = ZR_TRUE;
    cs->currentAst = g_usingReleaseEvidence.ast;
    cs->scriptAst = g_usingReleaseEvidence.ast;
    cs->isScriptLevel = ZR_TRUE;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(cs->currentFunction,
                                 "Fixture prerequisite: using compiler function allocation failed");
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    enter_scope(cs);
    TEST_ASSERT_EQUAL_UINT64(1U, cs->scopeStack.length);
    ZrParser_Compiler_CompileClassDeclaration(cs, statements->nodes[0]);
    using_release_evidence_require_healthy("resource class");
    for (TZrSize index = 1U; index < 4U; index++) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statements->nodes[index]->type);
        ZrParser_Statement_Compile(cs, statements->nodes[index]);
        using_release_evidence_require_healthy("source declaration");
    }
    context = cs->semanticContext;
    resourceBinding = using_release_evidence_binding(resource);
    borrowedBinding = using_release_evidence_binding(outside);
    TEST_ASSERT_EQUAL_INT(qualifier, resourceBinding->type.ownershipQualifier);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_BORROWED, borrowedBinding->type.ownershipQualifier);
    TEST_ASSERT_NULL_MESSAGE(using_release_evidence_read(resource),
                             "Fixture prerequisite: using resource was inferred before the metadata query");

    /* Exercise the metadata path before code generation can publish this READ. */
    ZrParser_InferredType_Init(g_state, &g_usingReleaseEvidence.resourceType, ZR_VALUE_TYPE_OBJECT);
    g_usingReleaseEvidence.resourceTypeInitialized = ZR_TRUE;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticMetadata_InferUsingResourceType(
            cs, resource, &g_usingReleaseEvidence.resourceType),
            "Fixture prerequisite: using resource type inference failed");
    TEST_ASSERT_EQUAL_INT(qualifier, g_usingReleaseEvidence.resourceType.ownershipQualifier);
    cleanupCount = context->cleanupPlan.length;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticMetadata_RecordUsingCleanup(
            cs, usingNode, &g_usingReleaseEvidence.resourceType),
            "Fixture prerequisite: using cleanup metadata publication failed");
    TEST_ASSERT_EQUAL_UINT64(cleanupCount + 1U, context->cleanupPlan.length);
    cleanup = (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&context->cleanupPlan, cleanupCount);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_EQUAL_UINT32(resourceBinding->symbolId, cleanup->symbolId);
    TEST_ASSERT_EQUAL_INT(qualifier, cleanup->ownershipQualifier);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_DROP, cleanup->ownershipBuiltinKind);
    using_release_evidence_require_healthy("using metadata");
    ZrParser_InferredType_Free(g_state, &g_usingReleaseEvidence.resourceType);
    g_usingReleaseEvidence.resourceTypeInitialized = ZR_FALSE;

    using_release_evidence_infer_expression(inside);
    using_release_evidence_infer_expression(outside);
    insideRead = using_release_evidence_read(inside);
    outsideRead = using_release_evidence_read(outside);
    TEST_ASSERT_TRUE_MESSAGE(insideRead != ZR_NULL && outsideRead != ZR_NULL &&
            insideRead->isResolved && outsideRead->isResolved &&
            insideRead->symbolId == borrowedBinding->symbolId &&
            outsideRead->symbolId == borrowedBinding->symbolId &&
            insideRead->typeId == borrowedBinding->typeId && outsideRead->typeId == borrowedBinding->typeId &&
            insideRead->placeId == borrowedBinding->placeId && outsideRead->placeId == borrowedBinding->placeId &&
            insideRead->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED &&
            outsideRead->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED,
            "Fixture prerequisite: body and outside borrowed READ identities are missing");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticFacts_ResolveControlFlowOwnership(
            context, g_usingReleaseEvidence.ast), "Fixture prerequisite: using ownership CFG analysis failed");
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope),
                             "Fixture prerequisite: using diagnostic materialization failed");
    release = ZrParser_SemanticFacts_FindOwnershipAtPosition(context, resource->location);
    insideOwnership = ZrParser_SemanticFacts_FindOwnershipAtPosition(context, inside->location);
    outsideOwnership = ZrParser_SemanticFacts_FindOwnershipAtPosition(context, outside->location);
    diagnostic = find_query_diagnostic_by_code(context, "borrow_escape");
    if (diagnostic != ZR_NULL && diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length == 1U) {
        related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                (SZrArray *)&diagnostic->relatedInformation, 0U);
    }
    resourceRead = using_release_evidence_read(resource);
    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
        if (reference != ZR_NULL && reference->node == resource &&
            reference->kind == ZR_SEMANTIC_REFERENCE_READ) {
            resourceReadCount++;
        }
    }
    passed = resourceReadCount == 1U && resourceRead != ZR_NULL && resourceRead->isResolved &&
            resourceRead->symbolId == resourceBinding->symbolId && resourceRead->typeId == resourceBinding->typeId &&
            resourceRead->placeId == resourceBinding->placeId && resourceRead->ownershipQualifier == qualifier &&
            resourceRead->range.source == g_usingReleaseEvidence.sourceName &&
            release != ZR_NULL && release->kind == ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE &&
            release->qualifier == qualifier && release->symbolId == resourceBinding->symbolId &&
            release->node == resource && release->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
            release->ownerLifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
            (insideOwnership == ZR_NULL || !insideOwnership->isViolation) &&
            outsideOwnership != ZR_NULL && outsideOwnership->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
            outsideOwnership->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED && outsideOwnership->isViolation &&
            outsideOwnership->symbolId == borrowedBinding->symbolId &&
            outsideOwnership->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
            outsideOwnership->ownerLifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
            outsideOwnership->ownerLifetimeRegionId ==
                    (qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED
                             ? release->lifetimeRegionId : release->ownerLifetimeRegionId) &&
            outsideOwnership->relatedNode == resource && diagnostic != ZR_NULL &&
            diagnostic->location.source == g_usingReleaseEvidence.sourceName &&
            diagnostic->location.start.offset == outside->location.start.offset &&
            count_query_diagnostics_by_code(context, "borrow_escape") == 1U && related != ZR_NULL &&
            related->location.source == g_usingReleaseEvidence.sourceName &&
            related->location.start.offset == resource->location.start.offset && related->message != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(related->message), "Owner was released here") == 0;
    if (!passed) {
        printf("Using release evidence: source=%s qualifier=%u readCount=%llu read=%u "
               "bindingSymbol=%u bindingType=%u bindingPlace=%u\n",
               sourceName, (unsigned)qualifier, (unsigned long long)resourceReadCount,
               (unsigned)(resourceRead != ZR_NULL), (unsigned)resourceBinding->symbolId,
               (unsigned)resourceBinding->typeId, (unsigned)resourceBinding->placeId);
        if (resourceRead != ZR_NULL) {
            printf("  readSymbol=%u readType=%u readPlace=%u readQualifier=%u resolved=%u\n",
                   (unsigned)resourceRead->symbolId, (unsigned)resourceRead->typeId,
                   (unsigned)resourceRead->placeId, (unsigned)resourceRead->ownershipQualifier,
                   (unsigned)resourceRead->isResolved);
        }
        printf("  release=%u kind=%u symbol=%u region=%u ownerRegion=%u insideViolation=%u "
               "outside=%u kind=%u symbol=%u related=%p diagnostic=%u relatedCount=%llu\n",
               (unsigned)(release != ZR_NULL), release != ZR_NULL ? (unsigned)release->kind : 0U,
               release != ZR_NULL ? (unsigned)release->symbolId : 0U,
               release != ZR_NULL ? (unsigned)release->lifetimeRegionId : 0U,
               release != ZR_NULL ? (unsigned)release->ownerLifetimeRegionId : 0U,
               (unsigned)(insideOwnership != ZR_NULL && insideOwnership->isViolation),
               (unsigned)(outsideOwnership != ZR_NULL),
               outsideOwnership != ZR_NULL ? (unsigned)outsideOwnership->kind : 0U,
               outsideOwnership != ZR_NULL ? (unsigned)outsideOwnership->symbolId : 0U,
               outsideOwnership != ZR_NULL ? (void *)outsideOwnership->relatedNode : ZR_NULL,
               (unsigned)(diagnostic != ZR_NULL),
               diagnostic != ZR_NULL ? (unsigned long long)diagnostic->relatedInformation.length : 0ULL);
        TEST_FAIL_MESSAGE(failure);
    }
}

static void test_using_shared_identifier_publishes_scope_release_evidence(void) {
    const TZrChar *source =
            "resource class Resource { pub @constructor() {} }\n"
            "var seed = own Resource();\n"
            "var owner = share(seed);\n"
            "var borrowed = ref owner;\n"
            "using (owner) {\n"
            "    borrowed;\n"
            "}\n"
            "borrowed;\n";

    using_release_evidence_check(source, "using_shared_identifier_release_evidence.zr",
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            "Using Shared identifiers must publish resource reads and scope-exit release evidence");
}

static void test_using_borrowed_identifier_publishes_scope_release_evidence(void) {
    const TZrChar *source =
            "resource class Resource { pub @constructor() {} }\n"
            "var seed = own Resource();\n"
            "var owner = share(seed);\n"
            "var borrowed = ref owner;\n"
            "using (borrowed) {\n"
            "    borrowed;\n"
            "}\n"
            "borrowed;\n";

    using_release_evidence_check(source, "using_borrowed_identifier_release_evidence.zr",
            ZR_OWNERSHIP_QUALIFIER_BORROWED,
            "Using borrowed identifiers must publish resource reads and scope-exit release evidence");
}

#endif
