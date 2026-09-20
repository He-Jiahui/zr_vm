static const SZrParserCfgBlock *find_block_source_line(
        const SZrSemanticIrFunction *function,
        TZrInt32 line) {
    TZrSize index;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);

        if (block != ZR_NULL &&
            block_has_source_line(function, block, line)) {
            return block;
        }
    }
    return ZR_NULL;
}

static void test_terminal_break_try_finally_routes_to_loop_join(void) {
    static const TZrChar source[] =
            "var keep: bool = true;\n"
            "var seed: int = 7;\n"
            "while (keep) {\n"
            "  try {\n"
            "    break;\n"
            "  } finally {\n"
            "    seed = 9;\n"
            "  }\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "terminal_break_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *completion;
    const SZrParserCfgBlock *loopJoin;
    const SZrParserCfgEdge *completionEdge;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    loopJoin = find_block_source_line(function, 10);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(loopJoin);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup->successorCount);
    completionEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    TEST_ASSERT_NOT_NULL(completionEdge);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_CLEANUP, completionEdge->kind);
    completion = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            completionEdge->toBlockId);
    TEST_ASSERT_NOT_NULL(completion);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH,
            completion->terminatorKind);
    assert_single_edge(
            completion, ZR_PARSER_CFG_EDGE_NORMAL, loopJoin->id);
    TEST_ASSERT_TRUE(block_has_source_line(function, cleanup, 7));
    TEST_ASSERT_TRUE(block_has_source_line(function, completion, 5));

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}

static void test_conditional_break_try_finally_dispatches_to_loop_join(void) {
    static const TZrChar source[] =
            "var keep: bool = true;\n"
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "while (keep) {\n"
            "  try {\n"
            "    if (choose) { break; }\n"
            "    seed = 8;\n"
            "  } finally {\n"
            "    seed = 9;\n"
            "  }\n"
            "  seed = 10;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "conditional_break_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *completion;
    const SZrParserCfgBlock *normalJoin;
    const SZrParserCfgBlock *loopJoin;
    const SZrParserCfgEdge *completionEdge;
    const SZrParserCfgEdge *normalEdge;
    const SZrSemanticIrInstruction *dispatch;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    loopJoin = find_block_source_line(function, 13);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(loopJoin);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->successorCount);
    completionEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    normalEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 1U);
    TEST_ASSERT_NOT_NULL(completionEdge);
    TEST_ASSERT_NOT_NULL(normalEdge);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_CASE, completionEdge->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, normalEdge->kind);
    completion = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            completionEdge->toBlockId);
    normalJoin = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            normalEdge->toBlockId);
    TEST_ASSERT_NOT_NULL(completion);
    TEST_ASSERT_NOT_NULL(normalJoin);
    TEST_ASSERT_TRUE(block_has_source_line(function, normalJoin, 11));
    assert_single_edge(
            completion, ZR_PARSER_CFG_EDGE_NORMAL, loopJoin->id);
    dispatch = block_tail(function, cleanup);
    TEST_ASSERT_NOT_NULL(dispatch);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_SWITCH, dispatch->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, dispatch->operandCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}

static void test_terminal_continue_try_finally_routes_to_condition(void) {
    static const TZrChar source[] =
            "var keep: bool = true;\n"
            "var seed: int = 7;\n"
            "while (keep) {\n"
            "  try {\n"
            "    continue;\n"
            "  } finally {\n"
            "    seed = 9;\n"
            "  }\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "terminal_continue_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *completion;
    const SZrParserCfgBlock *condition;
    const SZrParserCfgEdge *completionEdge;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, cleanup->terminatorKind);
    completionEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    TEST_ASSERT_NOT_NULL(completionEdge);
    completion = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            completionEdge->toBlockId);
    TEST_ASSERT_NOT_NULL(completion);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_CLEANUP, completionEdge->kind);
    condition = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(completion, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(completion, 0U)->kind);
    TEST_ASSERT_TRUE(block_has_source_line(function, condition, 3));

    free_source(&compiler, ast);
}

static void test_conditional_continue_try_finally_dispatches_to_condition(void) {
    static const TZrChar source[] =
            "var keep: bool = true;\n"
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "while (keep) {\n"
            "  try {\n"
            "    if (choose) { continue; }\n"
            "    seed = 8;\n"
            "  } finally {\n"
            "    seed = 9;\n"
            "  }\n"
            "  seed = 10;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "conditional_continue_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *completion;
    const SZrParserCfgBlock *condition;
    const SZrParserCfgBlock *normalJoin;
    const SZrParserCfgEdge *completionEdge;
    const SZrParserCfgEdge *normalEdge;
    const SZrSemanticIrInstruction *dispatch;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);
    completionEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    normalEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 1U);
    TEST_ASSERT_NOT_NULL(completionEdge);
    TEST_ASSERT_NOT_NULL(normalEdge);
    completion = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            completionEdge->toBlockId);
    normalJoin = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            normalEdge->toBlockId);
    TEST_ASSERT_NOT_NULL(completion);
    TEST_ASSERT_NOT_NULL(normalJoin);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_CASE, completionEdge->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, normalEdge->kind);
    condition = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(completion, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(completion, 0U)->kind);
    TEST_ASSERT_TRUE(block_has_source_line(function, condition, 4));
    TEST_ASSERT_TRUE(block_has_source_line(function, normalJoin, 11));
    dispatch = block_tail(function, cleanup);
    TEST_ASSERT_NOT_NULL(dispatch);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_SWITCH, dispatch->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, dispatch->operandCount);

    free_source(&compiler, ast);
}

static void test_two_continue_try_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "var keep: bool = true;\n"
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "while (keep) {\n"
            "  try {\n"
            "    if (choose) { continue; }\n"
            "    continue;\n"
            "  } finally {\n"
            "    seed = 9;\n"
            "  }\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "two_continue_try_finally_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));

    free_source(&compiler, ast);
}

static void test_two_break_sites_try_finally_stay_on_legacy_path(void) {
    static const TZrChar source[] =
            "var keep: bool = true;\n"
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "while (keep) {\n"
            "  try {\n"
            "    if (choose) { break; }\n"
            "    break;\n"
            "  } finally {\n"
            "    seed = 9;\n"
            "  }\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "two_break_try_finally_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));

    free_source(&compiler, ast);
}
