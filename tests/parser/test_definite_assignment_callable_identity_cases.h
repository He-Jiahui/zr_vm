#ifndef ZR_TEST_DEFINITE_ASSIGNMENT_CALLABLE_IDENTITY_CASES_H
#define ZR_TEST_DEFINITE_ASSIGNMENT_CALLABLE_IDENTITY_CASES_H

typedef struct SZrDaCallableIdentityFixture {
    SZrCompilerState compiler;
    SZrAstNode *ast;
    TZrBool compilerInitialized;
} SZrDaCallableIdentityFixture;

static SZrDaCallableIdentityFixture g_daCallableIdentity;

static void da_callable_identity_cleanup(void) {
    if (g_daCallableIdentity.compilerInitialized) {
        release_compiler_function(&g_daCallableIdentity.compiler);
        ZrParser_CompilerState_Free(&g_daCallableIdentity.compiler);
    }
    if (g_daCallableIdentity.ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_daCallableIdentity.ast);
    }
    memset(&g_daCallableIdentity, 0, sizeof(g_daCallableIdentity));
}

static SZrSemanticContext *da_callable_identity_compile(const TZrChar *source) {
    SZrCompilerState *cs = &g_daCallableIdentity.compiler;
    const TZrChar *name = "definite_assignment_callable_identity.zr";
    SZrString *sourceName;

    da_callable_identity_cleanup();
    sourceName = ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name));
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Fixture prerequisite: DA source name allocation failed");
    g_daCallableIdentity.ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_daCallableIdentity.ast, "Fixture prerequisite: DA source parsing failed");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_daCallableIdentity.ast->type);
    ZrParser_CompilerState_Init(cs, g_state);
    g_daCallableIdentity.compilerInitialized = ZR_TRUE;
    cs->suppressErrorOutput = ZR_TRUE;
    cs->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(cs->currentFunction, "Fixture prerequisite: DA function allocation failed");
    compile_script(cs, g_daCallableIdentity.ast);
    if (cs->hasError || cs->hasStructuredError) {
        printf("DA callable prerequisite: error=%u structured=%u message=%s\n",
               (unsigned)cs->hasError, (unsigned)cs->hasStructuredError,
               cs->errorMessage != ZR_NULL ? cs->errorMessage : "<null>");
    }
    TEST_ASSERT_FALSE_MESSAGE(cs->hasError || cs->hasStructuredError,
                              "Fixture prerequisite: DA source compilation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(cs->semanticContext, "Fixture prerequisite: DA context missing");
    return cs->semanticContext;
}

static void test_definite_assignment_keeps_same_name_callable_bindings_separate(void) {
    SZrSemanticContext *context = da_callable_identity_compile(
            "fn identity(value: int): int { return value; }\n"
            "fn consume(): int { var value: int = 1; return value; }\n");

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0U, count_query_diagnostics_by_code(context, "uninitialized_read"),
            "Initialized local must not reset another callable's same-name parameter slot");
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "possibly_uninitialized_read"));
}

static void test_definite_assignment_lambda_parameter_using_starts_initialized(void) {
    SZrSemanticContext *context = da_callable_identity_compile(
            "resource class Resource { }\n"
            "fn host(resource: Unique<Resource>) {\n"
            "    var work = fn(resource: Shared<Resource>): string => {\n"
            "        using (resource) { return `a ${1}`; }\n"
            "    };\n"
            "}\n");
    SZrAstNode *host = g_daCallableIdentity.ast->data.script.statements->nodes[1];
    SZrAstNode *work;
    SZrAstNode *lambda;
    SZrAstNode *usingNode;
    const SZrSemanticReferenceFact *read;

    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, host->type);
    TEST_ASSERT_NOT_NULL(host->data.functionDeclaration.body);
    work = host->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, work->type);
    lambda = work->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.block);
    usingNode = lambda->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingNode->type);
    read = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, usingNode->data.usingStatement.resource, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL_MESSAGE(read, "Fixture prerequisite: actual using READ missing");
    TEST_ASSERT_TRUE_MESSAGE(read->isResolved && read->symbolId != ZR_SEMANTIC_ID_INVALID,
                             "Fixture prerequisite: lambda parameter READ identity missing");
    TEST_ASSERT_TRUE_MESSAGE(read->hasDefiniteAssignmentState,
                             "Lambda body READ must be analyzed by its own callable CFG");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT, read->definiteAssignmentState,
                                  "Lambda parameter must not inherit the outer variable declaration's UNINIT");
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "uninitialized_read"));
}

static void test_definite_assignment_still_reports_uninitialized_local(void) {
    SZrSemanticContext *context = da_callable_identity_compile(
            "fn consume(): int { var value: int; return value; }\n");
    SZrAstNode *functionNode;
    SZrAstNode *body;
    SZrAstNode *declaration;
    SZrAstNode *returnNode;
    SZrAstNode *readNode;
    const SZrSemanticReferenceFact *declarationFact;
    TZrBool sawRead = ZR_FALSE;
    TZrBool sawDiagnostic = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(g_daCallableIdentity.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_daCallableIdentity.ast->data.script.statements->count);
    functionNode = g_daCallableIdentity.ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    body = functionNode->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(2U, body->data.block.body->count);
    declaration = body->data.block.body->nodes[0];
    returnNode = body->data.block.body->nodes[1];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NULL(declaration->data.variableDeclaration.value);
    TEST_ASSERT_NOT_NULL(returnNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    readNode = returnNode->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(readNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, readNode->type);
    declarationFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, declaration, ZR_SEMANTIC_REFERENCE_DECLARATION);
    TEST_ASSERT_NOT_NULL(declarationFact);
    TEST_ASSERT_TRUE(declarationFact->isResolved);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, declarationFact->symbolId);

    TEST_ASSERT_TRUE(context->referenceFacts.isValid);
    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact = (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &context->referenceFacts, index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_READ || fact->node != readNode) {
            continue;
        }
        sawRead = ZR_TRUE;
        TEST_ASSERT_TRUE(fact->isResolved);
        TEST_ASSERT_EQUAL_UINT64(declarationFact->symbolId, fact->symbolId);
        TEST_ASSERT_TRUE(fact->hasDefiniteAssignmentState);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT, fact->definiteAssignmentState);
    }
    TEST_ASSERT_TRUE_MESSAGE(sawRead, "The uninitialized return must publish its actual identifier READ");

    TEST_ASSERT_TRUE(context->queryDiagnostics.isValid);
    for (TZrSize index = 0U; index < context->queryDiagnostics.length; index++) {
        const SZrStructuredDiagnostic *diagnostic = (const SZrStructuredDiagnostic *)ZrCore_Array_Get(
                &context->queryDiagnostics, index);
        if (diagnostic == ZR_NULL || diagnostic->code == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), "uninitialized_read") != 0) {
            continue;
        }
        sawDiagnostic = ZR_TRUE;
        TEST_ASSERT_EQUAL_UINT32(3002U, diagnostic->descriptorId);
        TEST_ASSERT_NOT_NULL(diagnostic->location.source);
        TEST_ASSERT_NOT_NULL(readNode->location.source);
        TEST_ASSERT_TRUE(ZrCore_String_Equal(diagnostic->location.source, readNode->location.source));
        TEST_ASSERT_EQUAL_UINT64(readNode->location.start.offset, diagnostic->location.start.offset);
        TEST_ASSERT_EQUAL_INT(readNode->location.start.line, diagnostic->location.start.line);
        TEST_ASSERT_EQUAL_INT(readNode->location.start.column, diagnostic->location.start.column);
        TEST_ASSERT_EQUAL_UINT64(readNode->location.end.offset, diagnostic->location.end.offset);
        TEST_ASSERT_EQUAL_INT(readNode->location.end.line, diagnostic->location.end.line);
        TEST_ASSERT_EQUAL_INT(readNode->location.end.column, diagnostic->location.end.column);
    }
    TEST_ASSERT_TRUE_MESSAGE(sawDiagnostic, "The actual uninitialized READ must retain descriptor 3002");
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "possibly_uninitialized_read"));
}

static void test_definite_assignment_lambda_body_reports_uninitialized_local(void) {
    SZrSemanticContext *context = da_callable_identity_compile(
            "fn host() { var work = fn(): int => { var missing: int; return missing; }; }\n");
    SZrAstNode *host = g_daCallableIdentity.ast->data.script.statements->nodes[0];
    SZrAstNode *work;
    SZrAstNode *lambda;
    SZrAstNode *declaration;
    SZrAstNode *readNode;
    const SZrSemanticReferenceFact *declarationFact;
    const SZrStructuredDiagnostic *diagnostic;
    TZrBool sawRead = ZR_FALSE;

    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, host->type);
    TEST_ASSERT_NOT_NULL(host->data.functionDeclaration.body);
    work = host->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, work->type);
    lambda = work->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.block);
    TEST_ASSERT_EQUAL_UINT64(2U, lambda->data.lambdaExpression.block->data.block.body->count);
    declaration = lambda->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NULL(declaration->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT,
            lambda->data.lambdaExpression.block->data.block.body->nodes[1]->type);
    readNode = lambda->data.lambdaExpression.block->data.block.body->nodes[1]->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(readNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, readNode->type);
    declarationFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, declaration, ZR_SEMANTIC_REFERENCE_DECLARATION);
    TEST_ASSERT_NOT_NULL(declarationFact);
    TEST_ASSERT_TRUE(declarationFact->isResolved);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, declarationFact->symbolId);

    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact = (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &context->referenceFacts, index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_READ || fact->node != readNode) {
            continue;
        }
        sawRead = ZR_TRUE;
        TEST_ASSERT_TRUE(fact->isResolved);
        TEST_ASSERT_EQUAL_UINT64(declarationFact->symbolId, fact->symbolId);
        TEST_ASSERT_TRUE(fact->hasDefiniteAssignmentState);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT, fact->definiteAssignmentState);
    }
    TEST_ASSERT_TRUE_MESSAGE(sawRead, "Lambda local must retain its actual READ facts");
    diagnostic = find_query_diagnostic_by_code(context, "uninitialized_read");
    TEST_ASSERT_NOT_NULL_MESSAGE(diagnostic, "Lambda body must retain a real uninitialized-read diagnostic");
    TEST_ASSERT_EQUAL_UINT32(3002U, diagnostic->descriptorId);
    TEST_ASSERT_NOT_NULL(diagnostic->location.source);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(readNode->location.source, diagnostic->location.source));
    TEST_ASSERT_EQUAL_UINT64(readNode->location.start.offset, diagnostic->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(readNode->location.end.offset, diagnostic->location.end.offset);
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "possibly_uninitialized_read"));
}

static void test_definite_assignment_finds_lambda_inside_if_body(void) {
    SZrSemanticContext *context = da_callable_identity_compile(
            "fn host() { if (true) { var work = fn(value: int): int => { return value; }; } }\n");
    SZrAstNode *host = g_daCallableIdentity.ast->data.script.statements->nodes[0];
    SZrAstNode *condition;
    SZrAstNode *work;
    SZrAstNode *lambda;
    SZrAstNode *readNode;
    TZrBool sawRead = ZR_FALSE;

    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, host->type);
    TEST_ASSERT_NOT_NULL(host->data.functionDeclaration.body);
    condition = host->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, condition->type);
    TEST_ASSERT_NOT_NULL(condition->data.ifExpression.thenExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, condition->data.ifExpression.thenExpr->type);
    work = condition->data.ifExpression.thenExpr->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, work->type);
    lambda = work->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT,
            lambda->data.lambdaExpression.block->data.block.body->nodes[0]->type);
    readNode = lambda->data.lambdaExpression.block->data.block.body->nodes[0]->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(readNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, readNode->type);

    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact = (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &context->referenceFacts, index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_READ || fact->node != readNode) {
            continue;
        }
        sawRead = ZR_TRUE;
        TEST_ASSERT_TRUE(fact->isResolved);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, fact->symbolId);
        TEST_ASSERT_TRUE_MESSAGE(fact->hasDefiniteAssignmentState,
                                 "Nested lambda body must receive its own CFG analysis");
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT, fact->definiteAssignmentState);
    }
    TEST_ASSERT_TRUE(sawRead);
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "uninitialized_read"));
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "possibly_uninitialized_read"));
}

#endif
