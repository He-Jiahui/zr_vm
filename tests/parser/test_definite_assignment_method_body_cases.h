#ifndef ZR_TEST_DEFINITE_ASSIGNMENT_METHOD_BODY_CASES_H
#define ZR_TEST_DEFINITE_ASSIGNMENT_METHOD_BODY_CASES_H

/* Class/struct 方法体 DA/RD CFG 根的端到端覆盖。
 * 事实由 compile_script 发布，不是手工追加；诊断走 query 物化的正常路径。
 * 复用 test_definite_assignment_callable_identity_cases.h 的编译 fixture。 */

static SZrAstNodeArray *da_method_body_statements(SZrAstNode *typeNode, TZrSize memberIndex) {
    SZrAstNodeArray *members;
    SZrAstNode *method;
    SZrAstNode *body;

    TEST_ASSERT_NOT_NULL(typeNode);
    if (typeNode->type == ZR_AST_CLASS_DECLARATION) {
        members = typeNode->data.classDeclaration.members;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, typeNode->type);
        members = typeNode->data.structDeclaration.members;
    }
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_TRUE(memberIndex < members->count);
    method = members->nodes[memberIndex];
    TEST_ASSERT_NOT_NULL(method);
    if (method->type == ZR_AST_CLASS_METHOD) {
        body = method->data.classMethod.body;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_METHOD, method->type);
        body = method->data.structMethod.body;
    }
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    return body->data.block.body;
}

static SZrAstNode *da_method_body_return_read(SZrAstNodeArray *statements, TZrSize index) {
    SZrAstNode *statement;

    TEST_ASSERT_TRUE(index < statements->count);
    statement = statements->nodes[index];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, statement->data.returnStatement.expr->type);
    return statement->data.returnStatement.expr;
}

/* 编译器可能为同一标识符发布多条 READ 事实；每一条都必须带同一个 CFG 状态。 */
static void da_method_body_assert_read_state(SZrSemanticContext *context,
                                             SZrAstNode *readNode,
                                             EZrSemanticDefiniteAssignmentState expected) {
    TZrBool sawRead = ZR_FALSE;

    TEST_ASSERT_TRUE(context->referenceFacts.isValid);
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
                                 "Method body READ must be analyzed by its own callable CFG");
        TEST_ASSERT_EQUAL_INT(expected, fact->definiteAssignmentState);
    }
    TEST_ASSERT_TRUE_MESSAGE(sawRead, "Method body must publish its actual identifier READ");
}

/* 该 code 至少有一条诊断，且每一条都落在这次读取上。 */
static void da_method_body_assert_diagnostics_at(SZrSemanticContext *context,
                                                 const TZrChar *code,
                                                 TZrUInt32 descriptorId,
                                                 EZrStructuredDiagnosticSeverity severity,
                                                 SZrAstNode *readNode) {
    TZrBool sawDiagnostic = ZR_FALSE;

    TEST_ASSERT_TRUE(context->queryDiagnostics.isValid);
    for (TZrSize index = 0U; index < context->queryDiagnostics.length; index++) {
        const SZrStructuredDiagnostic *diagnostic = (const SZrStructuredDiagnostic *)ZrCore_Array_Get(
                &context->queryDiagnostics, index);
        if (diagnostic == ZR_NULL || diagnostic->code == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(diagnostic->code), code) != 0) {
            continue;
        }
        sawDiagnostic = ZR_TRUE;
        TEST_ASSERT_EQUAL_UINT32(descriptorId, diagnostic->descriptorId);
        TEST_ASSERT_EQUAL_INT(severity, diagnostic->severity);
        TEST_ASSERT_NOT_NULL(diagnostic->location.source);
        TEST_ASSERT_NOT_NULL(readNode->location.source);
        TEST_ASSERT_TRUE(ZrCore_String_Equal(readNode->location.source, diagnostic->location.source));
        TEST_ASSERT_EQUAL_UINT64(readNode->location.start.offset, diagnostic->location.start.offset);
        TEST_ASSERT_EQUAL_UINT64(readNode->location.end.offset, diagnostic->location.end.offset);
    }
    TEST_ASSERT_TRUE_MESSAGE(sawDiagnostic, "Method body read must surface its query diagnostic");
}

/* 方法里无初始化局部的读取是 UNINIT，并报 uninitialized_read。 */
static void da_method_body_expect_uninitialized_local(const TZrChar *source) {
    SZrSemanticContext *context = da_callable_identity_compile(source);
    SZrAstNodeArray *statements;
    SZrAstNode *readNode;

    TEST_ASSERT_NOT_NULL(g_daCallableIdentity.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_daCallableIdentity.ast->data.script.statements->count);
    statements = da_method_body_statements(
            g_daCallableIdentity.ast->data.script.statements->nodes[0], 0U);
    TEST_ASSERT_EQUAL_UINT64(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statements->nodes[0]->type);
    TEST_ASSERT_NULL(statements->nodes[0]->data.variableDeclaration.value);
    readNode = da_method_body_return_read(statements, 1U);

    da_method_body_assert_read_state(context, readNode, ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT);
    da_method_body_assert_diagnostics_at(context, "uninitialized_read", 3002U,
                                         ZR_STRUCTURED_DIAGNOSTIC_ERROR, readNode);
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "possibly_uninitialized_read"));
}

static void test_definite_assignment_class_method_body_reports_uninitialized_local(void) {
    da_method_body_expect_uninitialized_local(
            "class Box {\n"
            "    pub fn fresh(): int {\n"
            "        var missing: int;\n"
            "        return missing;\n"
            "    }\n"
            "}\n");
}

static void test_definite_assignment_struct_method_body_reports_uninitialized_local(void) {
    da_method_body_expect_uninitialized_local(
            "struct Point {\n"
            "    pub fn fresh(): int {\n"
            "        var missing: int;\n"
            "        return missing;\n"
            "    }\n"
            "}\n");
}

/* 单边 if 写入后读取：DA 合流成 MAYBE_INIT，RD 同时保留声明和那次写入。
 * 直线求解只会给出源码顺序上最后一次写入。 */
static void da_method_body_expect_branch_join(const TZrChar *source) {
    SZrSemanticContext *context = da_callable_identity_compile(source);
    SZrAstNodeArray *statements;
    SZrAstNode *declaration;
    SZrAstNode *branch;
    SZrAstNode *thenBlock;
    SZrAstNode *writeNode;
    SZrAstNode *readNode;
    const SZrSemanticReferenceFact *declarationFact;
    const SZrSemanticReferenceFact *writeFact;
    const SZrSemanticReferenceFact *readFact;
    TZrSize declarationOffset;
    TZrSize writeOffset;
    TZrBool sawDeclaration = ZR_FALSE;
    TZrBool sawWrite = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(g_daCallableIdentity.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_daCallableIdentity.ast->data.script.statements->count);
    statements = da_method_body_statements(
            g_daCallableIdentity.ast->data.script.statements->nodes[0], 0U);
    TEST_ASSERT_EQUAL_UINT64(3U, statements->count);
    declaration = statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NULL(declaration->data.variableDeclaration.value);
    branch = statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, branch->type);
    TEST_ASSERT_NULL(branch->data.ifExpression.elseExpr);
    thenBlock = branch->data.ifExpression.thenExpr;
    TEST_ASSERT_NOT_NULL(thenBlock);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, thenBlock->type);
    TEST_ASSERT_NOT_NULL(thenBlock->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, thenBlock->data.block.body->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, thenBlock->data.block.body->nodes[0]->type);
    writeNode = thenBlock->data.block.body->nodes[0]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(writeNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, writeNode->type);
    writeNode = writeNode->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(writeNode);
    readNode = da_method_body_return_read(statements, 2U);

    da_method_body_assert_read_state(context, readNode, ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT);
    da_method_body_assert_diagnostics_at(context, "possibly_uninitialized_read", 3003U,
                                         ZR_STRUCTURED_DIAGNOSTIC_WARNING, readNode);
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "uninitialized_read"));

    declarationFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, declaration, ZR_SEMANTIC_REFERENCE_DECLARATION);
    writeFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, writeNode, ZR_SEMANTIC_REFERENCE_WRITE);
    TEST_ASSERT_NOT_NULL_MESSAGE(declarationFact, "Fixture prerequisite: method local DECLARATION missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(writeFact, "Fixture prerequisite: method local WRITE missing");
    TEST_ASSERT_EQUAL_UINT64(declarationFact->symbolId, writeFact->symbolId);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(context));
    declarationFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, declaration, ZR_SEMANTIC_REFERENCE_DECLARATION);
    writeFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, writeNode, ZR_SEMANTIC_REFERENCE_WRITE);
    readFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, readNode, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(declarationFact);
    TEST_ASSERT_NOT_NULL(writeFact);
    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_TRUE(declarationFact->hasDefinitionRange);
    TEST_ASSERT_TRUE(writeFact->hasDefinitionRange);
    declarationOffset = declarationFact->definitionRange.start.offset;
    writeOffset = writeFact->definitionRange.start.offset;
    TEST_ASSERT_NOT_EQUAL_UINT64(declarationOffset, writeOffset);
    TEST_ASSERT_TRUE(readFact->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT64(writeOffset, readFact->definitionRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(
            context, g_daCallableIdentity.ast));
    readFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, readNode, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(readFact);
    TEST_ASSERT_FALSE_MESSAGE(readFact->hasDefinitionRange,
                              "Method body branch join must not keep a single reaching definition");
    TEST_ASSERT_TRUE(readFact->definitionRanges.isValid);
    TEST_ASSERT_EQUAL_UINT64(2U, readFact->definitionRanges.length);
    for (TZrSize index = 0U; index < readFact->definitionRanges.length; index++) {
        const SZrFileRange *range = (const SZrFileRange *)ZrCore_Array_Get(
                (SZrArray *)&readFact->definitionRanges, index);
        TEST_ASSERT_NOT_NULL(range);
        if (range->start.offset == declarationOffset) {
            sawDeclaration = ZR_TRUE;
        } else if (range->start.offset == writeOffset) {
            sawWrite = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(sawDeclaration);
    TEST_ASSERT_TRUE(sawWrite);
}

static void test_definite_assignment_class_method_body_joins_conditional_write(void) {
    da_method_body_expect_branch_join(
            "class Box {\n"
            "    pub fn maybe(flag: bool): int {\n"
            "        var seed: int;\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "        }\n"
            "        return seed;\n"
            "    }\n"
            "}\n");
}

static void test_definite_assignment_struct_method_body_joins_conditional_write(void) {
    da_method_body_expect_branch_join(
            "struct Point {\n"
            "    pub fn maybe(flag: bool): int {\n"
            "        var seed: int;\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "        }\n"
            "        return seed;\n"
            "    }\n"
            "}\n");
}

/* 正确的方法体不能因为有了独立 CFG 根而多出诊断。
 * 抽象方法没有 body；参数在方法入口已初始化；两条分支都写入后是 INIT；
 * 各方法里同名的局部互不影响。 */
static void test_definite_assignment_method_bodies_keep_initialized_reads_clean(void) {
    SZrSemanticContext *context = da_callable_identity_compile(
            "abstract class Shape {\n"
            "    pub abstract fn area(): int;\n"
            "    pub fn echo(value: int): int {\n"
            "        return value;\n"
            "    }\n"
            "    pub fn pick(flag: bool): int {\n"
            "        var seed: int;\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "        } else {\n"
            "            seed = 2;\n"
            "        }\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn ready(): int {\n"
            "        var seed: int = 4;\n"
            "        return seed;\n"
            "    }\n"
            "}\n"
            "struct Point {\n"
            "    pub fn echo(value: int): int {\n"
            "        return value;\n"
            "    }\n"
            "    pub fn ready(): int {\n"
            "        var seed: int = 4;\n"
            "        return seed;\n"
            "    }\n"
            "}\n");
    SZrAstNode *classNode;
    SZrAstNode *structNode;
    SZrAstNodeArray *statements;

    TEST_ASSERT_NOT_NULL(g_daCallableIdentity.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(2U, g_daCallableIdentity.ast->data.script.statements->count);
    classNode = g_daCallableIdentity.ast->data.script.statements->nodes[0];
    structNode = g_daCallableIdentity.ast->data.script.statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT64(4U, classNode->data.classDeclaration.members->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, classNode->data.classDeclaration.members->nodes[0]->type);
    TEST_ASSERT_NULL_MESSAGE(classNode->data.classDeclaration.members->nodes[0]->data.classMethod.body,
                             "Fixture prerequisite: abstract method must have no body");

    statements = da_method_body_statements(classNode, 1U);
    da_method_body_assert_read_state(context, da_method_body_return_read(statements, 0U),
                                     ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT);
    statements = da_method_body_statements(classNode, 2U);
    da_method_body_assert_read_state(context, da_method_body_return_read(statements, 2U),
                                     ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT);
    statements = da_method_body_statements(classNode, 3U);
    da_method_body_assert_read_state(context, da_method_body_return_read(statements, 1U),
                                     ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT);
    statements = da_method_body_statements(structNode, 0U);
    da_method_body_assert_read_state(context, da_method_body_return_read(statements, 0U),
                                     ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT);
    statements = da_method_body_statements(structNode, 1U);
    da_method_body_assert_read_state(context, da_method_body_return_read(statements, 1U),
                                     ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT);

    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "uninitialized_read"));
    TEST_ASSERT_EQUAL_UINT64(0U, count_query_diagnostics_by_code(context, "possibly_uninitialized_read"));
}

#endif
