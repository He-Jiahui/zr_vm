#ifndef ZR_TEST_SEMANTIC_METHOD_CFG_FACT_CASES_H
#define ZR_TEST_SEMANTIC_METHOD_CFG_FACT_CASES_H

/* Class/struct 方法体必须各自成为 DA/RD CFG 根。
 * 与外层类型共用一条语句时，无初始化局部会被标成 INIT，分支读只看到最后一次写入。
 * 与另一个方法共用根时，前一个方法的条件写入会流进后一个方法的同符号读取。 */

static SZrAstNodeArray *method_cfg_member_array(SZrAstNode *typeNode) {
    TEST_ASSERT_NOT_NULL(typeNode);
    if (typeNode->type == ZR_AST_CLASS_DECLARATION) {
        return typeNode->data.classDeclaration.members;
    }
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, typeNode->type);
    return typeNode->data.structDeclaration.members;
}

static SZrAstNodeArray *method_cfg_body_statements(SZrAstNode *method) {
    SZrAstNode *body = ZR_NULL;

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

static SZrAstNode *method_cfg_declaration_pattern(SZrAstNode *statement) {
    SZrAstNode *pattern;

    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statement->type);
    pattern = statement->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, pattern->type);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, (TZrUInt32)pattern->location.start.offset);
    return pattern;
}

static SZrAstNode *method_cfg_return_expr(SZrAstNode *statement) {
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    return statement->data.returnStatement.expr;
}

static SZrAstNode *method_cfg_conditional_write(SZrAstNode *statement) {
    SZrAstNode *thenBranch;
    SZrAstNode *writeStatement;
    SZrAstNode *assignment;
    SZrAstNode *left;

    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, statement->type);
    TEST_ASSERT_TRUE(statement->data.ifExpression.isStatement);
    TEST_ASSERT_NULL(statement->data.ifExpression.elseExpr);
    thenBranch = statement->data.ifExpression.thenExpr;
    TEST_ASSERT_NOT_NULL(thenBranch);
    if (thenBranch->type == ZR_AST_BLOCK) {
        TEST_ASSERT_NOT_NULL(thenBranch->data.block.body);
        TEST_ASSERT_EQUAL_UINT64(1U, thenBranch->data.block.body->count);
        writeStatement = thenBranch->data.block.body->nodes[0];
    } else {
        writeStatement = thenBranch;
    }
    TEST_ASSERT_NOT_NULL(writeStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, writeStatement->type);
    assignment = writeStatement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(assignment);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, assignment->type);
    left = assignment->data.assignmentExpression.left;
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, (TZrUInt32)left->location.start.offset);
    return left;
}

static TZrBool method_cfg_fact_defines_offset(const SZrSemanticReferenceFact *fact,
                                              TZrSize offset) {
    TZrSize index;

    if (fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fact->hasDefinitionRange && fact->definitionRange.start.offset == offset) {
        return ZR_TRUE;
    }
    if (!fact->definitionRanges.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < fact->definitionRanges.length; index++) {
        const SZrFileRange *range = (const SZrFileRange *)ZrCore_Array_Get(
                (SZrArray *)&fact->definitionRanges,
                index);
        if (range != ZR_NULL && range->start.offset == offset) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void method_cfg_append_decl(SZrSemanticContext *context,
                                   SZrAstNode *pattern,
                                   TZrSymbolId symbolId) {
    append_reference_fact(context,
                          pattern,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          symbolId,
                          pattern->location);
}

static void method_cfg_append_use(SZrSemanticContext *context,
                                  SZrAstNode *node,
                                  EZrSemanticReferenceKind kind,
                                  TZrSymbolId symbolId,
                                  SZrFileRange declarationRange) {
    append_reference_fact(context, node, kind, symbolId, declarationRange);
}

static void method_cfg_publish_type(SZrSemanticContext *context,
                                    SZrAstNode *typeNode,
                                    TZrSymbolId sharedSymbol,
                                    TZrSymbolId freshSymbol,
                                    TZrSymbolId readySymbol,
                                    SZrAstNode **outMaybeRead,
                                    SZrAstNode **outEchoRead,
                                    SZrAstNode **outFreshRead,
                                    SZrAstNode **outReadyRead,
                                    TZrSize *outMaybeDeclOffset,
                                    TZrSize *outMaybeWriteOffset,
                                    TZrSize *outFreshDeclOffset,
                                    TZrSize *outReadyDeclOffset) {
    SZrAstNodeArray *members;
    SZrAstNodeArray *maybeStatements;
    SZrAstNodeArray *echoStatements;
    SZrAstNodeArray *freshStatements;
    SZrAstNodeArray *readyStatements;
    SZrAstNode *maybePattern;
    SZrAstNode *maybeWrite;
    SZrAstNode *maybeRead;
    SZrAstNode *echoRead;
    SZrAstNode *freshPattern;
    SZrAstNode *freshRead;
    SZrAstNode *readyPattern;
    SZrAstNode *readyRead;
    members = method_cfg_member_array(typeNode);
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_EQUAL_UINT64(4U, members->count);
    maybeStatements = method_cfg_body_statements(members->nodes[0]);
    echoStatements = method_cfg_body_statements(members->nodes[1]);
    freshStatements = method_cfg_body_statements(members->nodes[2]);
    readyStatements = method_cfg_body_statements(members->nodes[3]);
    TEST_ASSERT_EQUAL_UINT64(3U, maybeStatements->count);
    TEST_ASSERT_EQUAL_UINT64(1U, echoStatements->count);
    TEST_ASSERT_EQUAL_UINT64(2U, freshStatements->count);
    TEST_ASSERT_EQUAL_UINT64(2U, readyStatements->count);
    TEST_ASSERT_NULL(maybeStatements->nodes[0]->data.variableDeclaration.value);
    TEST_ASSERT_NOT_NULL(readyStatements->nodes[0]->data.variableDeclaration.value);
    TEST_ASSERT_NULL(freshStatements->nodes[0]->data.variableDeclaration.value);

    maybePattern = method_cfg_declaration_pattern(maybeStatements->nodes[0]);
    maybeWrite = method_cfg_conditional_write(maybeStatements->nodes[1]);
    maybeRead = method_cfg_return_expr(maybeStatements->nodes[2]);
    echoRead = method_cfg_return_expr(echoStatements->nodes[0]);
    freshPattern = method_cfg_declaration_pattern(freshStatements->nodes[0]);
    freshRead = method_cfg_return_expr(freshStatements->nodes[1]);
    readyPattern = method_cfg_declaration_pattern(readyStatements->nodes[0]);
    readyRead = method_cfg_return_expr(readyStatements->nodes[1]);

    *outMaybeRead = maybeRead;
    *outEchoRead = echoRead;
    *outFreshRead = freshRead;
    *outReadyRead = readyRead;
    *outMaybeDeclOffset = maybePattern->location.start.offset;
    *outMaybeWriteOffset = maybeWrite->location.start.offset;
    *outFreshDeclOffset = freshPattern->location.start.offset;
    *outReadyDeclOffset = readyPattern->location.start.offset;
    TEST_ASSERT_NOT_EQUAL_UINT64(*outMaybeDeclOffset, *outMaybeWriteOffset);
    TEST_ASSERT_NOT_EQUAL_UINT64(*outMaybeWriteOffset, *outFreshDeclOffset);
    TEST_ASSERT_NOT_EQUAL_UINT64(*outMaybeWriteOffset, *outReadyDeclOffset);

    method_cfg_append_decl(context, maybePattern, sharedSymbol);
    method_cfg_append_use(context,
                          maybeWrite,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          sharedSymbol,
                          maybePattern->location);
    method_cfg_append_use(context,
                          maybeRead,
                          ZR_SEMANTIC_REFERENCE_READ,
                          sharedSymbol,
                          maybePattern->location);
    method_cfg_append_use(context,
                          echoRead,
                          ZR_SEMANTIC_REFERENCE_READ,
                          sharedSymbol,
                          maybePattern->location);
    method_cfg_append_decl(context, freshPattern, freshSymbol);
    method_cfg_append_use(context,
                          freshRead,
                          ZR_SEMANTIC_REFERENCE_READ,
                          freshSymbol,
                          freshPattern->location);
    method_cfg_append_decl(context, readyPattern, readySymbol);
    method_cfg_append_use(context,
                          readyRead,
                          ZR_SEMANTIC_REFERENCE_READ,
                          readySymbol,
                          readyPattern->location);
}

static void method_cfg_assert_read(SZrSemanticContext *context,
                                   SZrAstNode *read,
                                   EZrSemanticDefiniteAssignmentState assignment,
                                   TZrBool hasSingleDefinition,
                                   TZrSize singleOffset,
                                   TZrSize absentOffset) {
    const SZrSemanticReferenceFact *fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, read, ZR_SEMANTIC_REFERENCE_READ);

    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_TRUE(fact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(assignment, fact->definiteAssignmentState);
    if (hasSingleDefinition) {
        TEST_ASSERT_TRUE(fact->hasDefinitionRange);
        TEST_ASSERT_EQUAL_UINT64(singleOffset, fact->definitionRange.start.offset);
    } else {
        TEST_ASSERT_FALSE(fact->hasDefinitionRange);
    }
    TEST_ASSERT_FALSE(method_cfg_fact_defines_offset(fact, absentOffset));
}

static void method_cfg_assert_type(SZrSemanticContext *context,
                                   SZrAstNode *maybeRead,
                                   SZrAstNode *echoRead,
                                   SZrAstNode *freshRead,
                                   SZrAstNode *readyRead,
                                   TZrSize maybeDeclOffset,
                                   TZrSize maybeWriteOffset,
                                   TZrSize freshDeclOffset,
                                   TZrSize readyDeclOffset) {
    const SZrSemanticReferenceFact *maybeReadFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, maybeRead, ZR_SEMANTIC_REFERENCE_READ);

    TEST_ASSERT_NOT_NULL(maybeReadFact);
    TEST_ASSERT_TRUE(maybeReadFact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT,
                          maybeReadFact->definiteAssignmentState);
    TEST_ASSERT_FALSE(maybeReadFact->hasDefinitionRange);
    TEST_ASSERT_TRUE(maybeReadFact->definitionRanges.isValid);
    TEST_ASSERT_EQUAL_UINT64(2U, maybeReadFact->definitionRanges.length);
    TEST_ASSERT_TRUE(method_cfg_fact_defines_offset(maybeReadFact, maybeDeclOffset));
    TEST_ASSERT_TRUE(method_cfg_fact_defines_offset(maybeReadFact, maybeWriteOffset));

    method_cfg_assert_read(context,
                           echoRead,
                           ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                           ZR_TRUE,
                           maybeDeclOffset,
                           maybeWriteOffset);
    method_cfg_assert_read(context,
                           freshRead,
                           ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT,
                           ZR_TRUE,
                           freshDeclOffset,
                           maybeWriteOffset);
    method_cfg_assert_read(context,
                           readyRead,
                           ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                           ZR_TRUE,
                           readyDeclOffset,
                           maybeWriteOffset);
}

static void test_cfg_method_bodies_keep_independent_assignment_and_definitions(void) {
    const TZrChar *source =
            "class Box {\n"
            "    pub fn maybe(flag: bool): int {\n"
            "        var seed: int;\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "        }\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn echo(): int {\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn fresh(): int {\n"
            "        var seed: int;\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn ready(): int {\n"
            "        var other: int = 4;\n"
            "        return other;\n"
            "    }\n"
            "}\n"
            "struct Point {\n"
            "    pub fn maybe(flag: bool): int {\n"
            "        var seed: int;\n"
            "        if (flag) {\n"
            "            seed = 1;\n"
            "        }\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn echo(): int {\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn fresh(): int {\n"
            "        var seed: int;\n"
            "        return seed;\n"
            "    }\n"
            "    pub fn ready(): int {\n"
            "        var other: int = 4;\n"
            "        return other;\n"
            "    }\n"
            "}\n";
    const TZrChar *name = "facts_method_cfg_roots.zr";
    SZrString *sourceName = ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name));
    SZrSemanticContext *context;
    SZrAstNode *ast;
    SZrAstNode *classNode;
    SZrAstNode *structNode;
    SZrAstNode *classReads[4];
    SZrAstNode *structReads[4];
    TZrSize classOffsets[4];
    TZrSize structOffsets[4];

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(2U, ast->data.script.statements->count);
    classNode = ast->data.script.statements->nodes[0];
    structNode = ast->data.script.statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structNode->type);

    context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(context);
    method_cfg_publish_type(context,
                            classNode,
                            301U,
                            302U,
                            303U,
                            &classReads[0],
                            &classReads[1],
                            &classReads[2],
                            &classReads[3],
                            &classOffsets[0],
                            &classOffsets[1],
                            &classOffsets[2],
                            &classOffsets[3]);
    method_cfg_publish_type(context,
                            structNode,
                            401U,
                            402U,
                            403U,
                            &structReads[0],
                            &structReads[1],
                            &structReads[2],
                            &structReads[3],
                            &structOffsets[0],
                            &structOffsets[1],
                            &structOffsets[2],
                            &structOffsets[3]);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(context, ast));
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(context, ast));
    method_cfg_assert_type(context,
                           classReads[0],
                           classReads[1],
                           classReads[2],
                           classReads[3],
                           classOffsets[0],
                           classOffsets[1],
                           classOffsets[2],
                           classOffsets[3]);
    method_cfg_assert_type(context,
                           structReads[0],
                           structReads[1],
                           structReads[2],
                           structReads[3],
                           structOffsets[0],
                           structOffsets[1],
                           structOffsets[2],
                           structOffsets[3]);

    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

/* 类型声明只是声明方法，不执行方法体，所以它不能收下方法体里的事实。
 * 方法根在 script 根之后运行，会盖掉方法内读取的结果；能看出类型声明是否收下方法体写入的，
 * 是类型声明之后 script 自己的读取：收下的话，这次读取会变成 INIT，到达定义会指向方法里的写入。
 * 两次写入用只有范围的合成节点，走确定赋值的范围回退；到达定义本来就按范围归属。 */
static void test_cfg_type_declarations_leave_method_body_writes_to_method_roots(void) {
    const TZrChar *source =
            "var total: int;\n"
            "class Box {\n"
            "    pub fn bump(): int {\n"
            "        total = 1;\n"
            "        return total;\n"
            "    }\n"
            "}\n"
            "struct Point {\n"
            "    pub fn bump(): int {\n"
            "        total = 2;\n"
            "        return total;\n"
            "    }\n"
            "}\n"
            "return total;\n";
    const TZrChar *name = "facts_method_cfg_type_statement.zr";
    const TZrSymbolId symbolId = 501U;
    SZrString *sourceName = ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name));
    SZrSemanticContext *context;
    SZrAstNode *ast;
    SZrAstNodeArray *statements;
    SZrAstNode *pattern;
    SZrAstNode *scriptRead;
    SZrAstNode *methodReads[2];
    SZrAstNode writes[2];
    SZrFileRange writeRanges[2];
    const SZrSemanticReferenceFact *scriptFact;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    statements = ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    TEST_ASSERT_EQUAL_UINT64(4U, statements->count);
    pattern = method_cfg_declaration_pattern(statements->nodes[0]);
    TEST_ASSERT_NULL(statements->nodes[0]->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, statements->nodes[1]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, statements->nodes[2]->type);
    scriptRead = method_cfg_return_expr(statements->nodes[3]);
    TEST_ASSERT_TRUE(find_source_range(source, sourceName, "total = 1", 0, 0, strlen("total"),
                                       &writeRanges[0]));
    TEST_ASSERT_TRUE(find_source_range(source, sourceName, "total = 2", 0, 0, strlen("total"),
                                       &writeRanges[1]));
    for (index = 0; index < 2; index++) {
        SZrAstNodeArray *members = method_cfg_member_array(statements->nodes[index + 1]);
        SZrAstNodeArray *body;

        TEST_ASSERT_NOT_NULL(members);
        TEST_ASSERT_EQUAL_UINT64(1U, members->count);
        body = method_cfg_body_statements(members->nodes[0]);
        TEST_ASSERT_EQUAL_UINT64(2U, body->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, body->nodes[0]->type);
        methodReads[index] = method_cfg_return_expr(body->nodes[1]);
        init_identifier_node_with_range(&writes[index], writeRanges[index]);
    }
    TEST_ASSERT_NOT_EQUAL_UINT64(pattern->location.start.offset, writeRanges[0].start.offset);
    TEST_ASSERT_NOT_EQUAL_UINT64(pattern->location.start.offset, writeRanges[1].start.offset);
    TEST_ASSERT_NOT_EQUAL_UINT64(writeRanges[0].start.offset, writeRanges[1].start.offset);

    context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(context);
    method_cfg_append_decl(context, pattern, symbolId);
    for (index = 0; index < 2; index++) {
        method_cfg_append_use(context,
                              &writes[index],
                              ZR_SEMANTIC_REFERENCE_WRITE,
                              symbolId,
                              pattern->location);
        method_cfg_append_use(context,
                              methodReads[index],
                              ZR_SEMANTIC_REFERENCE_READ,
                              symbolId,
                              pattern->location);
    }
    method_cfg_append_use(context,
                          scriptRead,
                          ZR_SEMANTIC_REFERENCE_READ,
                          symbolId,
                          pattern->location);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(context, ast));
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(context, ast));

    scriptFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, scriptRead, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(scriptFact);
    TEST_ASSERT_TRUE(scriptFact->hasDefiniteAssignmentState);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT,
                                  scriptFact->definiteAssignmentState,
                                  "A type declaration must not apply a method body write to the script flow");
    TEST_ASSERT_TRUE(scriptFact->hasDefinitionRange);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(pattern->location.start.offset,
                                     scriptFact->definitionRange.start.offset,
                                     "A method body write must not reach a script read after the type declaration");
    TEST_ASSERT_FALSE(method_cfg_fact_defines_offset(scriptFact, writeRanges[0].start.offset));
    TEST_ASSERT_FALSE(method_cfg_fact_defines_offset(scriptFact, writeRanges[1].start.offset));

    for (index = 0; index < 2; index++) {
        method_cfg_assert_read(context,
                               methodReads[index],
                               ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT,
                               ZR_TRUE,
                               writeRanges[index].start.offset,
                               writeRanges[1 - index].start.offset);
    }

    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

#endif
