#ifndef ZR_VM_TEST_SEMANTIC_QUERY_META_OVERRIDE_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_META_OVERRIDE_CASES_H

static void test_compiled_source_publishes_meta_function_override_relation(void) {
    const TZrChar *source =
            "class Base {\n"
            "    pub virtual @call(): int { return 1; }\n"
            "}\n"
            "class Derived : Base {\n"
            "    pub override @call(): int { return super.call() + 1; }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_meta_override.zr");
    SZrAstNode *ast;
    SZrAstNode *baseMethodNode;
    SZrAstNode *derivedMethodNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *baseMethod;
    const SZrSemanticSymbolRecord *derivedMethod;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[1]);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[0]->data.classDeclaration.members);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[1]->data.classDeclaration.members);
    baseMethodNode = ast->data.script.statements->nodes[0]
                             ->data.classDeclaration.members->nodes[0];
    derivedMethodNode = ast->data.script.statements->nodes[1]
                                ->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(baseMethodNode);
    TEST_ASSERT_NOT_NULL(derivedMethodNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, baseMethodNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, derivedMethodNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    baseMethod = relation_find_symbol_by_node(cs.semanticContext, baseMethodNode);
    derivedMethod = relation_find_symbol_by_node(
            cs.semanticContext, derivedMethodNode);
    TEST_ASSERT_NOT_NULL(baseMethod);
    TEST_ASSERT_NOT_NULL(derivedMethod);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, derivedMethod->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_OVERRIDE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(derivedMethod->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(baseMethod->id, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(derivedMethod->typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(baseMethod->typeId, relation->targetTypeId);
    TEST_ASSERT_EQUAL_UINT(
            derivedMethodNode->location.start.offset,
            relation->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            baseMethodNode->location.start.offset,
            relation->targetRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            cs.semanticContext, baseMethod->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_OVERRIDE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(derivedMethod->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(baseMethod->id, relation->targetSymbolId);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#endif
