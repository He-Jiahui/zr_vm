#ifndef ZR_TEST_SEMANTIC_ANALYZER_SOURCE_METADATA_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_SOURCE_METADATA_CASES_H

static TZrBool source_metadata_matches_lexical_resources(
        SZrSemanticAnalyzer *analyzer, SZrAstNode *functionNode) {
    SZrSemanticContext *context = analyzer->semanticContext;
    SZrAstNode *body = functionNode->data.functionDeclaration.body;
    SZrAstNode *innerBlock = body->data.block.body->nodes[1];
    SZrAstNode *expectedDeclarations[] = {
            functionNode->data.functionDeclaration.params->nodes[0],
            innerBlock->data.block.body->nodes[0],
            functionNode->data.functionDeclaration.params->nodes[1]};
    EZrOwnershipQualifier expectedOwnership[] = {
            ZR_OWNERSHIP_QUALIFIER_UNIQUE, ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_OWNERSHIP_QUALIFIER_SHARED};
    TZrSymbolId previousSymbol = ZR_SEMANTIC_ID_INVALID;
    TZrLifetimeRegionId previousRegion = ZR_SEMANTIC_ID_INVALID;
    if (context == ZR_NULL || context->cleanupPlan.length != 3 ||
        context->templateSegments.length != 6) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < 3; index++) {
        const SZrDeterministicCleanupStep *step = (const SZrDeterministicCleanupStep *)
                ZrCore_Array_Get(&context->cleanupPlan, index);
        const SZrSemanticSymbolRecord *symbol =
                ZrParser_Semantic_FindSymbolById(context, step->symbolId);
        if (symbol == ZR_NULL || symbol->astNode != expectedDeclarations[index] ||
            step->symbolId == previousSymbol || step->regionId == ZR_SEMANTIC_ID_INVALID ||
            step->regionId == previousRegion || step->ownerRegionId != step->regionId ||
            step->declarationOrder != (TZrInt32)index ||
            step->ownershipQualifier != expectedOwnership[index] ||
            step->ownershipBuiltinKind != (index == 1 ? ZR_OWNERSHIP_BUILTIN_KIND_NONE
                                                       : ZR_OWNERSHIP_BUILTIN_KIND_DROP) ||
            !step->callsClose || !step->callsDestructor) {
            return ZR_FALSE;
        }
        previousSymbol = step->symbolId;
        previousRegion = step->regionId;
    }
    for (TZrSize index = 0; index < 6; index++) {
        const SZrTemplateSegment *segment = (const SZrTemplateSegment *)
                ZrCore_Array_Get(&context->templateSegments, index);
        if (index % 3 == 1) {
            if (!segment->isInterpolation || segment->expression == ZR_NULL) {
                return ZR_FALSE;
            }
        } else if (segment->isInterpolation || segment->staticText == ZR_NULL ||
                   strcmp(ZrCore_String_GetNativeString(segment->staticText),
                          index == 0 ? "a " : "") != 0) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void test_semantic_analyzer_source_metadata_survives_cached_and_forced_analysis(SZrState *state) {
    static const char source[] =
            "resource class Resource { }\n"
            "fn use(resource: Unique<Resource>, shared: Shared<Resource>) {\n"
            "    using (resource) { var message = `a ${1 + 2}`; }\n"
            "    { var resource = 1; using (resource) { var message = `${2}`; } }\n"
            "    using (shared) { }\n"
            "}\n";
    const char *summary = "Semantic Analyzer Source Metadata Survives Cached And Forced Analysis";
    SZrTestTimer timer;
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "source_metadata.zr");
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TZrBool passed = analyzer != ZR_NULL && ast != ZR_NULL;
    TEST_START(summary);
    for (TZrSize pass = 0; passed && pass < 3; pass++) {
        ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(analyzer, pass != 2);
        passed = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) &&
                 analyzer->diagnostics.length == 0 &&
                 source_metadata_matches_lexical_resources(
                         analyzer, ast->data.script.statements->nodes[1]);
    }
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    ZrParser_Ast_Free(state, ast);
    if (!passed) {
        TEST_FAIL(timer, summary, "Cleanup identity/order/ownership or template segments changed across analysis");
        return;
    }
    TEST_PASS(timer, summary);
}

static SZrAstNode *source_metadata_nested_template_at(SZrAstNode *ast, TZrSize index) {
    SZrAstNode *node = ast->data.script.statements->nodes[index == 6 || index == 7 ? 1 : 0];
    switch (index) {
        case 0:
            return node->data.variableDeclaration.value->data.arrayLiteral.elements->nodes[0]
                    ->data.arrayLiteral.elements->nodes[0];
        case 1:
            return node->data.variableDeclaration.value->data.objectLiteral.properties->nodes[0]
                    ->data.keyValuePair.value;
        case 2:
            return node->data.classDeclaration.members->nodes[0]->data.classField.init;
        case 3:
            return node->data.structDeclaration.members->nodes[0]->data.structField.init;
        case 4:
            node = node->data.variableDeclaration.value->data.lambdaExpression.block;
            return node->type == ZR_AST_BLOCK
                    ? node->data.block.body->nodes[0]->data.returnStatement.expr
                    : (node->type == ZR_AST_RETURN_STATEMENT
                            ? node->data.returnStatement.expr : node);
        case 5:
            return node->data.functionDeclaration.params->nodes[0]->data.parameter.defaultValue;
        case 6:
            return node->data.expressionStatement.expr->data.primaryExpression.members->nodes[0]
                    ->data.memberExpression.property;
        case 7:
            node = node->data.functionDeclaration.body->data.block.body->nodes[0]
                    ->data.variableDeclaration.value;
            return node->data.lambdaExpression.block->data.block.body->nodes[0]
                    ->data.usingStatement.body->data.block.body->nodes[0]->data.returnStatement.expr;
        default:
            return ZR_NULL;
    }
}

static void test_semantic_analyzer_nested_source_metadata_is_complete(SZrState *state) {
    static const char *sources[] = {
            "var xs = [[`a ${1}`]];",
            "var value: object = {\"text\": `a ${1}`};",
            "class Box { pub var text: string = `a ${1}`; }",
            "struct Box { pub var text: string = `a ${1}`; }",
            "var label = fn(): string => `a ${1}`;",
            "fn label(text: string = `a ${1}`) { }",
            "var values: object = {\"text\": 1}; values[`a ${1}`];",
            "resource class Resource { } "
            "fn host(resource: Unique<Resource>) { "
            "var work = fn(resource: Shared<Resource>): string => { "
            "using (resource) { return `a ${1}`; } }; }"};
    const char *summary = "Semantic Analyzer Nested Source Metadata Is Complete";
    SZrTestTimer timer;
    TZrBool passed = ZR_TRUE;
    TEST_START(summary);
    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
        SZrString *sourceName = ZrCore_String_CreateFromNative(state, "nested_source_metadata.zr");
        SZrAstNode *ast = ZrParser_Parse(state, sources[index], strlen(sources[index]), sourceName);
        SZrAstNode *templateNode = ast != ZR_NULL ? source_metadata_nested_template_at(ast, index) : ZR_NULL;
        TZrBool casePassed = analyzer != ZR_NULL && ast != ZR_NULL;
        for (TZrSize pass = 0; casePassed && pass < 3; pass++) {
            SZrSemanticContext *context;
            ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(analyzer, pass != 2);
            casePassed = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
            context = analyzer->semanticContext;
            casePassed = casePassed && context != ZR_NULL && context->templateSegments.length == 3;
            for (TZrSize segmentIndex = 0; casePassed && segmentIndex < 3; segmentIndex++) {
                const SZrTemplateSegment *segment = (const SZrTemplateSegment *)
                        ZrCore_Array_Get(&context->templateSegments, segmentIndex);
                if (segmentIndex == 1) {
                    casePassed = segment->isInterpolation && segment->expression != ZR_NULL &&
                                 segment->expression->type == ZR_AST_INTEGER_LITERAL &&
                                 templateNode != ZR_NULL && templateNode->type == ZR_AST_TEMPLATE_STRING_LITERAL &&
                                 segment->expression == templateNode->data.templateStringLiteral.segments->nodes[1]
                                         ->data.interpolatedSegment.expression;
                } else {
                    casePassed = !segment->isInterpolation && segment->staticText != ZR_NULL &&
                                 strcmp(ZrCore_String_GetNativeString(segment->staticText),
                                        segmentIndex == 0 ? "a " : "") == 0;
                }
            }
            if (casePassed && index == 7) {
                SZrAstNode *host = ast->data.script.statements->nodes[1];
                SZrAstNode *variable = host->data.functionDeclaration.body->data.block.body->nodes[0];
                SZrAstNode *lambda = variable->data.variableDeclaration.value;
                const SZrDeterministicCleanupStep *step = context->cleanupPlan.length == 1
                        ? (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&context->cleanupPlan, 0)
                        : ZR_NULL;
                const SZrSemanticSymbolRecord *symbol = step != ZR_NULL
                        ? ZrParser_Semantic_FindSymbolById(context, step->symbolId) : ZR_NULL;
                casePassed = symbol != ZR_NULL && lambda->type == ZR_AST_LAMBDA_EXPRESSION &&
                             symbol->astNode == lambda->data.lambdaExpression.params->nodes[0] &&
                             step->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED &&
                             step->ownershipBuiltinKind == ZR_OWNERSHIP_BUILTIN_KIND_DROP &&
                             step->regionId != ZR_SEMANTIC_ID_INVALID &&
                             step->ownerRegionId == step->regionId;
            }
        }
        if (casePassed && (index == 1 || index == 6)) {
            casePassed = analyzer->diagnostics.length == 1;
        } else if (casePassed) {
            casePassed = analyzer->diagnostics.length == 0;
        }
        if (!casePassed) {
            fprintf(stderr, "Nested source metadata case %u: %u segments, %u diagnostics\n",
                    (unsigned)index,
                    (unsigned)(analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
                            ? analyzer->semanticContext->templateSegments.length : 0),
                    (unsigned)(analyzer != ZR_NULL ? analyzer->diagnostics.length : 0));
            for (TZrSize diagnosticIndex = 0; analyzer != ZR_NULL &&
                 diagnosticIndex < analyzer->diagnostics.length; diagnosticIndex++) {
                SZrDiagnostic **diagnostic = (SZrDiagnostic **)ZrCore_Array_Get(
                        &analyzer->diagnostics, diagnosticIndex);
                fprintf(stderr, "  %s\n", ZrCore_String_GetNativeString((*diagnostic)->message));
            }
            passed = ZR_FALSE;
        }
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrParser_Ast_Free(state, ast);
    }
    if (!passed) {
        TEST_FAIL(timer, summary, "Nested templates or lexical lambda cleanup metadata were missed or duplicated");
        return;
    }
    TEST_PASS(timer, summary);
}

#endif
