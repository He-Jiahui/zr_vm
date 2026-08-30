#ifndef ZR_VM_TEST_SEMANTIC_QUERY_CALL_CANDIDATE_CONSISTENCY_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_CALL_CANDIDATE_CONSISTENCY_CASES_H

static void test_call_candidates_fail_closed_when_overload_set_omits_selected_target(void) {
    const TZrChar *source =
            "fn choose(value: int): int { return value; }\n"
            "fn choose(value: string): int { return 0; }\n"
            "fn caller(): int { return choose(1); }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_call_candidate_consistency.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *selected;
    const SZrSemanticSymbolRecord *other;
    SZrSemanticOverloadSetRecord *overloads = ZR_NULL;
    SZrArray candidates;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(3U, ast->data.script.statements->count);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    selected = call_find_symbol_by_node(
            cs.semanticContext, ast->data.script.statements->nodes[0]);
    other = call_find_symbol_by_node(
            cs.semanticContext, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_NOT_NULL(selected);
    TEST_ASSERT_NOT_NULL(other);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, selected->overloadSetId);

    for (index = 0U; index < cs.semanticContext->overloadSets.length; index++) {
        SZrSemanticOverloadSetRecord *candidateSet =
                (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(
                        &cs.semanticContext->overloadSets, index);
        if (candidateSet != ZR_NULL &&
            candidateSet->id == selected->overloadSetId) {
            overloads = candidateSet;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(overloads);
    TEST_ASSERT_GREATER_THAN_UINT(0U, overloads->members.length);
    for (index = 0U; index < overloads->members.length; index++) {
        TZrSymbolId *memberId = (TZrSymbolId *)ZrCore_Array_Get(
                &overloads->members, index);
        TEST_ASSERT_NOT_NULL(memberId);
        *memberId = other->id;
    }

    ZrCore_Array_Construct(&candidates);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_CallCandidatesAt(
            cs.semanticContext,
            call_source_position(source, sourceName, "choose", 2U),
            ZR_NULL,
            &candidates));
    TEST_ASSERT_EQUAL_UINT(0U, candidates.length);

    ZrCore_Array_Free(g_state, &candidates);
    call_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#endif
