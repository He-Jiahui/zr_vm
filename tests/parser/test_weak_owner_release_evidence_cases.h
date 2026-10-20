#ifndef ZR_TEST_WEAK_OWNER_RELEASE_EVIDENCE_CASES_H
#define ZR_TEST_WEAK_OWNER_RELEASE_EVIDENCE_CASES_H

#include "type_inference/dataflow/dataflow_ownership_moves.h"

typedef struct SZrWeakOwnerEvidenceFixture {
    SZrCompilerState compiler;
    SZrAstNode *ast;
    SZrString *sourceName;
    const TZrChar *source;
    TZrBool compilerInitialized;
} SZrWeakOwnerEvidenceFixture;

static SZrWeakOwnerEvidenceFixture g_weakOwnerEvidence;

static void weak_owner_evidence_cleanup(void) {
    if (g_weakOwnerEvidence.compilerInitialized) {
        release_compiler_function(&g_weakOwnerEvidence.compiler);
        ZrParser_CompilerState_Free(&g_weakOwnerEvidence.compiler);
    }
    if (g_weakOwnerEvidence.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_weakOwnerEvidence.ast);
    }
    memset(&g_weakOwnerEvidence, 0, sizeof(g_weakOwnerEvidence));
}

static void weak_owner_evidence_compile(const TZrChar *source,
                                        const TZrChar *sourceName,
                                        const TZrChar *errorCode,
                                        TZrUInt32 descriptorId) {
    SZrCompilerState *cs = &g_weakOwnerEvidence.compiler;

    weak_owner_evidence_cleanup();
    g_weakOwnerEvidence.source = source;
    g_weakOwnerEvidence.sourceName = ZrCore_String_Create(
            g_state, (TZrNativeString)sourceName, strlen(sourceName));
    TEST_ASSERT_NOT_NULL_MESSAGE(g_weakOwnerEvidence.sourceName,
                                 "Fixture prerequisite: source name allocation failed");
    g_weakOwnerEvidence.ast = ZrParser_Parse(
            g_state, source, strlen(source), g_weakOwnerEvidence.sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_weakOwnerEvidence.ast,
                                 "Fixture prerequisite: source parsing failed");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_weakOwnerEvidence.ast->type);
    ZrParser_CompilerState_Init(cs, g_state);
    g_weakOwnerEvidence.compilerInitialized = ZR_TRUE;
    cs->suppressErrorOutput = ZR_TRUE;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(cs->currentFunction,
                                 "Fixture prerequisite: compiler function allocation failed");
    compile_script(cs, g_weakOwnerEvidence.ast);
    TEST_ASSERT_NOT_NULL_MESSAGE(cs->semanticContext,
                                 "Fixture prerequisite: compiler semantic context is missing");
    if (errorCode != ZR_NULL) {
        TEST_ASSERT_TRUE_MESSAGE(cs->hasError && cs->hasStructuredError,
                                 "Fixture prerequisite: expected rejected call was not diagnosed");
        TEST_ASSERT_NOT_NULL(cs->structuredError.code);
        TEST_ASSERT_EQUAL_STRING(errorCode, ZrCore_String_GetNativeString(cs->structuredError.code));
        TEST_ASSERT_EQUAL_UINT32(descriptorId, cs->structuredError.descriptorId);
    } else {
        if (cs->hasError || cs->hasStructuredError) {
            printf("Weak evidence compile observation: source=%s error=%u structured=%u "
                   "recoverable=%u fatal=%u message=%s\n",
                   sourceName, (unsigned)cs->hasError, (unsigned)cs->hasStructuredError,
                   (unsigned)cs->hadRecoverableError, (unsigned)cs->hasFatalError,
                   cs->errorMessage != ZR_NULL ? cs->errorMessage : "<null>");
            if (cs->hasStructuredError) {
                printf("  structured descriptor=%u code=%s message=%s source=%s "
                       "start=%d:%d/%llu end=%d:%d/%llu\n",
                       (unsigned)cs->structuredError.descriptorId,
                       cs->structuredError.code != ZR_NULL
                               ? ZrCore_String_GetNativeString(cs->structuredError.code) : "<null>",
                       cs->structuredError.message != ZR_NULL
                               ? ZrCore_String_GetNativeString(cs->structuredError.message) : "<null>",
                       cs->structuredError.location.source != ZR_NULL
                               ? ZrCore_String_GetNativeString(cs->structuredError.location.source) : "<null>",
                       cs->structuredError.location.start.line, cs->structuredError.location.start.column,
                       (unsigned long long)cs->structuredError.location.start.offset,
                       cs->structuredError.location.end.line, cs->structuredError.location.end.column,
                       (unsigned long long)cs->structuredError.location.end.offset);
            }
        }
        TEST_ASSERT_FALSE_MESSAGE(cs->hasError || cs->hasStructuredError,
                                  "Fixture prerequisite: legal weak control did not compile");
    }
}

static SZrFileRange weak_owner_evidence_position(const TZrChar *needle, TZrSize offset) {
    SZrFileRange position;

    TEST_ASSERT_TRUE_MESSAGE(find_position_for_substring(
            g_weakOwnerEvidence.source, g_weakOwnerEvidence.sourceName,
            needle, 0U, offset, &position),
            "Fixture prerequisite: source position is missing");
    return position;
}

static const SZrSemanticReferenceFact *weak_owner_evidence_read(SZrFileRange position) {
    SZrArray *references = &g_weakOwnerEvidence.compiler.semanticContext->referenceFacts;

    for (TZrSize index = 0; index < references->length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(references, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
            fact->range.source == position.source &&
            fact->range.start.offset == position.start.offset && fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID && fact->node != ZR_NULL) {
            return fact;
        }
    }
    TEST_FAIL_MESSAGE("Fixture prerequisite: resolved source-backed READ is missing");
}

static const SZrSemanticReferenceFact *weak_owner_evidence_call(TZrInt32 line) {
    SZrArray *references = &g_weakOwnerEvidence.compiler.semanticContext->referenceFacts;

    for (TZrSize index = 0; index < references->length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(references, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_REFERENCE_CALL &&
            fact->range.source == g_weakOwnerEvidence.sourceName &&
            fact->range.start.line == line && fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            return fact;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticOwnershipFact *weak_owner_evidence_error(
        SZrFileRange position, const SZrSemanticReferenceFact *read) {
    const SZrSemanticOwnershipFact *fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            g_weakOwnerEvidence.compiler.semanticContext, position);

    TEST_ASSERT_NOT_NULL_MESSAGE(fact, "Fixture prerequisite: ownership ERROR is missing");
    TEST_ASSERT_TRUE_MESSAGE(fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
            fact->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK && fact->isViolation &&
            fact->range.source == position.source && fact->node != ZR_NULL &&
            (fact->node == read->node ||
             (fact->node->type == ZR_AST_PRIMARY_EXPRESSION &&
              fact->node->data.primaryExpression.property == read->node &&
              (fact->node->data.primaryExpression.members == ZR_NULL ||
               fact->node->data.primaryExpression.members->count == 0U))),
            "Fixture prerequisite: weak ERROR does not identify the direct argument READ");
    return fact;
}

static void weak_owner_evidence_resolve(void) {
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticFacts_ResolveControlFlowOwnership(
            g_weakOwnerEvidence.compiler.semanticContext, g_weakOwnerEvidence.ast),
            "Fixture prerequisite: control-flow ownership analysis failed");
}

static void weak_owner_evidence_materialize(TZrBool publishCompilerError) {
    SZrParserSemanticQueryScope scope;

    if (publishCompilerError) {
        TEST_ASSERT_TRUE(ZrParser_Compiler_PublishCurrentDiagnostic(&g_weakOwnerEvidence.compiler));
    }
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            g_weakOwnerEvidence.compiler.semanticContext, &scope));
}

static TZrBool weak_owner_evidence_has_weak_error(void) {
    SZrArray *facts = &g_weakOwnerEvidence.compiler.semanticContext->ownershipFacts;

    for (TZrSize index = 0; index < facts->length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(facts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
            fact->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK && fact->isViolation) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_weak_call_owner_set_error_retains_release_evidence(void) {
    const TZrChar *source =
            "resource class Resource {}\n"
            "fn observe(resource: ref readonly Resource): int { return 0; }\n"
            "fn run(first: Shared<Resource>, second: Shared<Resource>, choose: bool): int {\n"
            "    var watcher = degrade(first);\n"
            "    if (choose) {\n"
            "        watcher = degrade(second);\n"
            "    }\n"
            "    drop(second);\n"
            "    observe(ref watcher);\n"
            "    return 0;\n"
            "}\n";
    const SZrSemanticReferenceFact *read;
    const SZrSemanticReferenceFact *ownerRead;
    const SZrSemanticOwnershipFact *error;
    const SZrSemanticOwnershipFact *release;
    const SZrSemanticOwnershipFact *copy;
    const SZrStructuredDiagnostic *diagnostic;
    const SZrStructuredDiagnosticRelatedInformation *related = ZR_NULL;
    SZrFileRange argument;
    SZrFileRange owner;
    TZrBool passed;

    weak_owner_evidence_compile(source, "weak_owner_set_error_evidence.zr",
                                "weak_value_requires_wake", 4004U);
    argument = weak_owner_evidence_position("observe(ref watcher)", strlen("observe(ref "));
    owner = weak_owner_evidence_position("drop(second)", strlen("drop("));
    read = weak_owner_evidence_read(argument);
    ownerRead = weak_owner_evidence_read(owner);
    (void)weak_owner_evidence_error(argument, read);
    TEST_ASSERT_EQUAL_UINT64(argument.start.offset,
                            g_weakOwnerEvidence.compiler.structuredError.location.start.offset);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK, read->ownershipQualifier);
    TEST_ASSERT_NULL_MESSAGE(weak_owner_evidence_call(9),
                             "Fixture prerequisite: rejected ref call unexpectedly has a resolved CALL");
    weak_owner_evidence_resolve();
    error = weak_owner_evidence_error(argument, read);
    release = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            g_weakOwnerEvidence.compiler.semanticContext, owner);
    copy = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            g_weakOwnerEvidence.compiler.semanticContext,
            weak_owner_evidence_position("degrade(second)", strlen("degrade(")));
    weak_owner_evidence_materialize(ZR_FALSE);
    diagnostic = find_query_diagnostic_by_code(
            g_weakOwnerEvidence.compiler.semanticContext, "weak_value_requires_wake");
    if (diagnostic != ZR_NULL && diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length == 1U) {
        related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                (SZrArray *)&diagnostic->relatedInformation, 0U);
    }
    passed = error->symbolId == read->symbolId &&
            error->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
            error->ownerLifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
            release != ZR_NULL && release->kind == ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE &&
            release->symbolId == ownerRead->symbolId &&
            release->qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED &&
            error->ownerLifetimeRegionId == release->lifetimeRegionId &&
            error->relatedNode == release->node &&
            copy != ZR_NULL && copy->kind == ZR_SEMANTIC_OWNERSHIP_FACT_COPY &&
            copy->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK && copy->symbolId == read->symbolId &&
            copy->lifetimeRegionId == error->lifetimeRegionId &&
            copy->ownerLifetimeRegionId == error->ownerLifetimeRegionId &&
            weak_owner_evidence_call(9) == ZR_NULL && diagnostic != ZR_NULL &&
            diagnostic->descriptorId == 4004U && diagnostic->location.source == argument.source &&
            diagnostic->location.start.offset == argument.start.offset &&
            count_query_diagnostics_by_code(g_weakOwnerEvidence.compiler.semanticContext,
                                            "weak_value_requires_wake") == 1U &&
            related != ZR_NULL && related->location.source == owner.source &&
            related->location.start.offset == owner.start.offset && related->message != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(related->message), "Owner was released here") == 0;
    if (!passed) {
        printf("Weak owner-set evidence: readSymbol=%u readType=%u place=%u readNode=%p "
               "errorSymbol=%u errorRegion=%u errorOwnerRegion=%u errorNode=%p relatedNode=%p\n",
               (unsigned)read->symbolId, (unsigned)read->typeId, (unsigned)read->placeId,
               (void *)read->node, (unsigned)error->symbolId, (unsigned)error->lifetimeRegionId,
               (unsigned)error->ownerLifetimeRegionId, (void *)error->node, (void *)error->relatedNode);
        printf("  release=%u kind=%u qualifier=%u symbol=%u region=%u ownerRegion=%u node=%p "
               "copy=%u kind=%u qualifier=%u symbol=%u region=%u ownerRegion=%u related=%p\n",
               (unsigned)(release != ZR_NULL), release != ZR_NULL ? (unsigned)release->kind : 0U,
               release != ZR_NULL ? (unsigned)release->qualifier : 0U,
               release != ZR_NULL ? (unsigned)release->symbolId : 0U,
               release != ZR_NULL ? (unsigned)release->lifetimeRegionId : 0U,
               release != ZR_NULL ? (unsigned)release->ownerLifetimeRegionId : 0U,
               release != ZR_NULL ? (void *)release->node : ZR_NULL,
               (unsigned)(copy != ZR_NULL), copy != ZR_NULL ? (unsigned)copy->kind : 0U,
               copy != ZR_NULL ? (unsigned)copy->qualifier : 0U,
               copy != ZR_NULL ? (unsigned)copy->symbolId : 0U,
               copy != ZR_NULL ? (unsigned)copy->lifetimeRegionId : 0U,
               copy != ZR_NULL ? (unsigned)copy->ownerLifetimeRegionId : 0U,
               copy != ZR_NULL ? (void *)copy->relatedNode : ZR_NULL);
        printf("  diagnostic=%u related=%llu resolvedCall=%u\n",
               (unsigned)(diagnostic != ZR_NULL),
               diagnostic != ZR_NULL ? (unsigned long long)diagnostic->relatedInformation.length : 0ULL,
               (unsigned)(weak_owner_evidence_call(9) != ZR_NULL));
        printf("  weak-classification: requiresWake=%u qualifier=%u resolved=%u "
               "readRange=%llu..%llu source=%p\n",
               (unsigned)error->requiresWake, (unsigned)read->ownershipQualifier,
               (unsigned)read->isResolved, (unsigned long long)read->range.start.offset,
               (unsigned long long)read->range.end.offset, (void *)read->range.source);
        for (TZrSize declarationIndex = 0U;
             declarationIndex < g_weakOwnerEvidence.ast->data.script.statements->count;
             declarationIndex++) {
            SZrAstNode *declaration =
                    g_weakOwnerEvidence.ast->data.script.statements->nodes[declarationIndex];
            SZrAstNode *body;
            if (declaration == ZR_NULL || declaration->type != ZR_AST_FUNCTION_DECLARATION ||
                (body = declaration->data.functionDeclaration.body) == ZR_NULL ||
                body->type != ZR_AST_BLOCK || body->data.block.body == ZR_NULL) {
                continue;
            }
            for (TZrSize statementIndex = 0U;
                 statementIndex < body->data.block.body->count; statementIndex++) {
                SZrAstNode *statement = body->data.block.body->nodes[statementIndex];
                SZrAstNode *expression;
                if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT ||
                    statement->location.start.line != argument.start.line ||
                    (expression = statement->data.expressionStatement.expr) == ZR_NULL) {
                    continue;
                }
                printf("  weak-statement: kind=%u expressionKind=%u wake=%u "
                       "statementRange=%llu..%llu expressionRange=%llu..%llu source=%p\n",
                       (unsigned)statement->type, (unsigned)expression->type,
                       (unsigned)ZrParser_DataflowOwnership_StatementWeakReadRequiresWake(
                               g_weakOwnerEvidence.compiler.semanticContext, statement, read),
                       (unsigned long long)statement->location.start.offset,
                       (unsigned long long)statement->location.end.offset,
                       (unsigned long long)expression->location.start.offset,
                       (unsigned long long)expression->location.end.offset,
                       (void *)expression->location.source);
            }
        }
        TEST_FAIL_MESSAGE("Rejected weak ref calls must retain the branch owner release without a resolved CALL fact");
    }
}

static void weak_owner_evidence_assert_non_wake_argument_classification(const TZrChar *source) {
    const SZrSemanticReferenceFact *read;
    const SZrSemanticOwnershipFact *error;
    SZrAstNode *function;
    SZrAstNode *body;
    SZrAstNode *statement;
    SZrFileRange argument;

    weak_owner_evidence_compile(source, "weak_non_wake_argument_classification.zr",
                                "ownership_mismatch", 2008U);
    argument = weak_owner_evidence_position("consume(watcher)", strlen("consume("));
    read = weak_owner_evidence_read(argument);
    error = weak_owner_evidence_error(argument, read);
    TEST_ASSERT_FALSE(error->requiresWake);
    TEST_ASSERT_NULL(weak_owner_evidence_call(5));
    TEST_ASSERT_NOT_NULL(g_weakOwnerEvidence.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(3U, g_weakOwnerEvidence.ast->data.script.statements->count);
    function = g_weakOwnerEvidence.ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, function->type);
    body = function->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(3U, body->data.block.body->count);
    statement = body->data.block.body->nodes[1];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    TEST_ASSERT_FALSE_MESSAGE(
            ZrParser_DataflowOwnership_StatementWeakReadRequiresWake(
                    g_weakOwnerEvidence.compiler.semanticContext, statement, read),
            "An ownership mismatch without a wake contract must not classify the argument as requiring wake");
    weak_owner_evidence_cleanup();
}

static void test_weak_to_shared_mismatch_does_not_require_wake(void) {
    weak_owner_evidence_assert_non_wake_argument_classification(
            "resource class Resource {}\n"
            "fn consume(resource: Shared<Resource>): int { return 0; }\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    consume(watcher);\n"
            "    return 0;\n"
            "}\n");
}

static void test_weak_to_plain_mismatch_does_not_require_wake(void) {
    weak_owner_evidence_assert_non_wake_argument_classification(
            "resource class Resource {}\n"
            "fn consume(resource: Resource): int { return 0; }\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    consume(watcher);\n"
            "    return 0;\n"
            "}\n");
}

static void test_non_wake_weak_ownership_mismatches_preserve_diagnostics(void) {
    const TZrChar *sources[] = {
            "resource class Resource {}\n"
            "fn consume(resource: Shared<Resource>): int { return 0; }\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    drop(owner);\n"
            "    consume(watcher);\n"
            "    return 0;\n"
            "}\n",
            "resource class Resource {}\n"
            "fn consume(resource: Resource): int { return 0; }\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    drop(owner);\n"
            "    consume(watcher);\n"
            "    return 0;\n"
            "}\n"};

    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        const SZrSemanticReferenceFact *read;
        const SZrSemanticOwnershipFact *error;
        const SZrStructuredDiagnostic *diagnostic;
        SZrSemanticOwnershipFact before;
        SZrFileRange argument;
        TZrBool passed;

        weak_owner_evidence_compile(sources[index], "weak_non_wake_mismatch_evidence.zr",
                                    "ownership_mismatch", 2008U);
        argument = weak_owner_evidence_position("consume(watcher)", strlen("consume("));
        read = weak_owner_evidence_read(argument);
        before = *weak_owner_evidence_error(argument, read);
        TEST_ASSERT_NULL(weak_owner_evidence_call(6));
        weak_owner_evidence_resolve();
        error = weak_owner_evidence_error(argument, read);
        weak_owner_evidence_materialize(ZR_TRUE);
        diagnostic = find_query_diagnostic_by_code(
                g_weakOwnerEvidence.compiler.semanticContext, "ownership_mismatch");
        passed = error->symbolId == before.symbolId &&
                error->lifetimeRegionId == before.lifetimeRegionId &&
                error->ownerLifetimeRegionId == before.ownerLifetimeRegionId &&
                error->relatedNode == before.relatedNode && diagnostic != ZR_NULL &&
                diagnostic->descriptorId == 2008U &&
                diagnostic->location.source == argument.source &&
                diagnostic->location.start.offset == argument.start.offset &&
                (!diagnostic->relatedInformation.isValid ||
                 diagnostic->relatedInformation.length == 0U) &&
                weak_owner_evidence_call(6) == ZR_NULL &&
                count_query_diagnostics_by_code(g_weakOwnerEvidence.compiler.semanticContext,
                                                "ownership_mismatch") == 1U &&
                count_query_diagnostics_by_code(g_weakOwnerEvidence.compiler.semanticContext,
                                                "weak_value_requires_wake") == 0U;
        if (!passed) {
            TEST_FAIL_MESSAGE("Weak ownership mismatches must not gain weak-wake diagnostics or owner-release facts");
        }
        weak_owner_evidence_cleanup();
    }
}

static void test_weak_by_value_calls_preserve_release_policy(void) {
    const TZrChar *source =
            "resource class Resource {}\n"
            "fn observe(resource: Weak<Resource>): int { return 0; }\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    drop(owner);\n"
            "    observe(watcher);\n"
            "    return 0;\n"
            "}\n";
    const SZrSemanticReferenceFact *call;
    const SZrSemanticCallArgumentFact *mapping;

    weak_owner_evidence_compile(source, "weak_by_value_release_evidence.zr", ZR_NULL, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK, weak_owner_evidence_read(
            weak_owner_evidence_position("observe(watcher)", strlen("observe(")))->ownershipQualifier);
    call = weak_owner_evidence_call(6);
    TEST_ASSERT_NOT_NULL_MESSAGE(call, "Fixture prerequisite: legal weak by-value call did not resolve");
    TEST_ASSERT_TRUE(call->argumentMappings.isValid);
    TEST_ASSERT_EQUAL_UINT64(1U, call->argumentMappings.length);
    mapping = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)&call->argumentMappings, 0U);
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_INT(ZR_PARAMETER_PASSING_MODE_VALUE, mapping->passingMode);
    weak_owner_evidence_resolve();
    weak_owner_evidence_materialize(ZR_FALSE);
    if (weak_owner_evidence_has_weak_error() ||
        count_query_diagnostics_by_code(g_weakOwnerEvidence.compiler.semanticContext,
                                        "weak_value_requires_wake") != 0U) {
        TEST_FAIL_MESSAGE("Weak by-value calls must not require wake after owner release");
    }
}

static void test_guarded_weak_receivers_preserve_release_policy(void) {
    const TZrChar *source =
            "resource class Resource {\n"
            "    const fn ping(): int { return 0; }\n"
            "}\n"
            "fn run(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    watcher.ping();\n"
            "    drop(owner);\n"
            "    watcher.ping();\n"
            "    return 0;\n"
            "}\n";
    SZrArray *guards;
    TZrSize directGuards = 0U;

    weak_owner_evidence_compile(source, "weak_guarded_receiver_release_evidence.zr", ZR_NULL, 0U);
    guards = &g_weakOwnerEvidence.compiler.semanticContext->receiverGuardFacts;
    for (TZrSize index = 0; index < guards->length; index++) {
        const SZrReceiverGuardFact *guard =
                (const SZrReceiverGuardFact *)ZrCore_Array_Get(guards, index);
        if (guard != ZR_NULL && guard->kind == ZR_RECEIVER_GUARD_WEAK_WAKE &&
            guard->mode == ZR_RECEIVER_GUARD_DIRECT && guard->node != ZR_NULL &&
            guard->receiver != ZR_NULL && guard->range.source == g_weakOwnerEvidence.sourceName) {
            directGuards++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(2U, directGuards,
                                    "Fixture prerequisite: both direct weak accesses need real receiver guards");
    TEST_ASSERT_NOT_NULL_MESSAGE(weak_owner_evidence_call(6),
                                 "Fixture prerequisite: guarded call before release did not resolve");
    TEST_ASSERT_NOT_NULL_MESSAGE(weak_owner_evidence_call(8),
                                 "Fixture prerequisite: guarded call after release did not resolve");
    weak_owner_evidence_resolve();
    weak_owner_evidence_materialize(ZR_FALSE);
    if (weak_owner_evidence_has_weak_error() ||
        count_query_diagnostics_by_code(g_weakOwnerEvidence.compiler.semanticContext,
                                        "weak_value_requires_wake") != 0U) {
        TEST_FAIL_MESSAGE("Guarded weak receivers must not gain weak-wake diagnostics after owner release");
    }
}

static void test_weak_degrade_accepts_an_incoming_shared_parameter(void) {
    static const TZrChar source[] =
            "resource class Resource {}\n"
            "fn incomingOwner(owner: Shared<Resource>): int {\n"
            "    var watcher = degrade(owner);\n"
            "    return 0;\n"
            "}\n";
    SZrCompilerState *cs = &g_weakOwnerEvidence.compiler;
    TZrBool compilationFailed;

    weak_owner_evidence_cleanup();
    g_weakOwnerEvidence.source = source;
    g_weakOwnerEvidence.sourceName = ZrCore_String_CreateFromNative(
            g_state, "incoming_shared_parameter_degrade.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_weakOwnerEvidence.sourceName,
                                 "Fixture prerequisite: source name allocation failed");
    g_weakOwnerEvidence.ast = ZrParser_Parse(
            g_state, source, strlen(source), g_weakOwnerEvidence.sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_weakOwnerEvidence.ast,
                                 "Fixture prerequisite: source parsing failed");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_weakOwnerEvidence.ast->type);
    ZrParser_CompilerState_Init(cs, g_state);
    g_weakOwnerEvidence.compilerInitialized = ZR_TRUE;
    cs->suppressErrorOutput = ZR_TRUE;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(cs->currentFunction,
                                 "Fixture prerequisite: compiler function allocation failed");

    compile_script(cs, g_weakOwnerEvidence.ast);
    compilationFailed = (TZrBool)(cs->hasError || cs->hasStructuredError ||
                                  cs->hadRecoverableError || cs->hasFatalError);
    if (compilationFailed) {
        printf("Incoming parameter compile observation: error=%u structured=%u "
               "recoverable=%u fatal=%u message=%s\n",
               (unsigned)cs->hasError, (unsigned)cs->hasStructuredError,
               (unsigned)cs->hadRecoverableError, (unsigned)cs->hasFatalError,
               cs->errorMessage != ZR_NULL ? cs->errorMessage : "<null>");
    }
    TEST_ASSERT_FALSE_MESSAGE(
            compilationFailed,
            "degrade must accept a real incoming Shared parameter without a prior load");
}

#endif
