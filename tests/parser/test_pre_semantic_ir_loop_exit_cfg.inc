static SZrAstNode *compile_loop_exit_cfg_fixture(
        SZrCompilerState *compiler,
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar *sourceName,
        TZrSize sourceNameLength) {
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sourceNameLength);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sourceLength, name);
    TZrSize index;

    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    if (compiler->currentFunction == ZR_NULL) {
        return ast;
    }
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(
                compiler, ast->data.script.statements->nodes[index]);
        if (compiler->hasError) {
            break;
        }
    }
    return ast;
}

static void free_loop_exit_cfg_fixture(
        SZrCompilerState *compiler,
        SZrAstNode *ast) {
    if (compiler->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, compiler->currentFunction);
        compiler->currentFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(compiler);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
}

static void assert_for_cfg_fallback_blocks_later_call(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar *sourceName,
        TZrSize sourceNameLength) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sourceLength, sourceName, sourceNameLength);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_while_break_targets_loop_join(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "while (flag) { break; }\n"
            "var after: int = 1;\n";
    static TZrChar sourceName[] = "source_while_break_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *header;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(5U, function->cfg.blocks.length);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    header = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(1U, entry->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, header->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, header->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(body, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            join->id, ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, join->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(5U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_while_continue_targets_loop_header(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "while (flag) { continue; }\n"
            "var after: int = 1;\n";
    static TZrChar sourceName[] = "source_while_continue_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *header;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(5U, function->cfg.blocks.length);
    header = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(2U, header->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, header->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(body, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            header->id, ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, join->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(5U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_while_linear_prefix_can_continue(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "while (flag) { flag = false; continue; }\n"
            "var after: int = 1;\n";
    static TZrChar sourceName[] = "source_while_prefix_continue_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *header;
    const SZrParserCfgBlock *body;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(5U, function->cfg.blocks.length);
    header = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_TRUE(body->instructionCount > 0U);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            header->id, ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_for_emits_typed_condition_body_step_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var flag: bool = true;\n"
            "var seed: int = 0;\n"
            "var index: int = 3;\n"
            "for (index = 0; flag; index = 1) { seed = 2; }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "source_for_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *condition;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *step;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(9U, function->cfg.blocks.length);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 0U);
    condition = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    step = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 4U);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(step);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(1U, entry->successorCount);
    TEST_ASSERT_EQUAL_UINT32(condition->id,
                             ZrParser_Cfg_BlockEdgeAt(entry, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, condition->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, condition->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                          ZrParser_Cfg_BlockEdgeAt(condition, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(body->id,
                             ZrParser_Cfg_BlockEdgeAt(condition, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                          ZrParser_Cfg_BlockEdgeAt(condition, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(join->id,
                             ZrParser_Cfg_BlockEdgeAt(condition, 1U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_UINT32(step->id,
                             ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, step->successorCount);
    TEST_ASSERT_EQUAL_UINT32(condition->id,
                             ZrParser_Cfg_BlockEdgeAt(step, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, join->predecessorCount);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(9U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_for_continue_targets_step_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var flag: bool = true;\n"
            "var seed: int = 0;\n"
            "var index: int = 3;\n"
            "for (index = 0; flag; index = 1) { seed = 2; continue; }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "source_for_continue_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *condition;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *step;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize continueTargetInstructionIndex = compiler.instructions.length;
    TZrSize forwardJumpCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(9U, function->cfg.blocks.length);
    condition = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    step = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 4U);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(step);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_UINT32(step->id,
                             ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, step->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, step->successorCount);
    TEST_ASSERT_EQUAL_UINT32(condition->id,
                             ZrParser_Cfg_BlockEdgeAt(step, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, join->predecessorCount);

    for (index = 0U; index < compiler.pendingJumps.length; index++) {
        const SZrPendingJump *pending =
                (const SZrPendingJump *)ZrCore_Array_Get(
                        &compiler.pendingJumps, index);
        const SZrLabel *label;
        const TZrInstruction *instruction;

        if (pending == ZR_NULL ||
            pending->instructionIndex >= compiler.instructions.length ||
            pending->labelId >= compiler.labels.length) {
            continue;
        }
        label = (const SZrLabel *)ZrCore_Array_Get(
                &compiler.labels, pending->labelId);
        instruction = (const TZrInstruction *)ZrCore_Array_Get(
                &compiler.instructions, pending->instructionIndex);
        if (label != ZR_NULL && label->isResolved &&
            instruction != ZR_NULL &&
            instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(JUMP) &&
            label->instructionIndex > pending->instructionIndex) {
            forwardJumpCount++;
            continueTargetInstructionIndex = label->instructionIndex;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, forwardJumpCount);
    TEST_ASSERT_TRUE(continueTargetInstructionIndex + 2U <
                     compiler.instructions.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(GET_CONSTANT),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     continueTargetInstructionIndex))
                    ->instruction.operationCode);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(SET_STACK),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     continueTargetInstructionIndex + 1U))
                    ->instruction.operationCode);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(JUMP),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     continueTargetInstructionIndex + 2U))
                    ->instruction.operationCode);
    TEST_ASSERT_TRUE(
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     continueTargetInstructionIndex + 2U))
                    ->instruction.operand.operand2[0] < 0);
    TEST_ASSERT_TRUE(
            (TZrInt32)(continueTargetInstructionIndex + 3U) +
                            ((const TZrInstruction *)ZrCore_Array_Get(
                                     &compiler.instructions,
                                     continueTargetInstructionIndex + 2U))
                                    ->instruction.operand.operand2[0] <
                    (TZrInt32)continueTargetInstructionIndex);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(9U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_unreachable_after_loop_exit_blocks_later_cfg_startup(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var flag: bool = true;\n"
            "while (flag) { break; flag = false; }\n"
            "identity(flag);\n";
    static TZrChar sourceName[] = "loop_exit_unreachable_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_suppressed_loop_exit_blocks_later_call_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var flag: bool = true;\n"
            "if (flag) { while (flag) { break; } }\n"
            "identity(flag);\n";
    static TZrChar sourceName[] = "suppressed_loop_exit_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_for_break_targets_join_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var flag: bool = true;\n"
            "var index: int = 0;\n"
            "for (index = 0; flag; index = 1) { index = 2; break; }\n"
            "identity(flag);\n";
    static TZrChar sourceName[] = "source_for_break_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *condition;
    const SZrParserCfgBlock *body;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize breakTargetInstructionIndex = compiler.instructions.length;
    TZrSize forwardJumpCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(8U, function->cfg.blocks.length);
    condition = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    body = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(1U, condition->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, condition->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, body->successorCount);
    TEST_ASSERT_EQUAL_UINT32(join->id,
                             ZrParser_Cfg_BlockEdgeAt(body, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, join->predecessorCount);

    for (index = 0U; index < compiler.pendingJumps.length; index++) {
        const SZrPendingJump *pending =
                (const SZrPendingJump *)ZrCore_Array_Get(
                        &compiler.pendingJumps, index);
        const SZrLabel *label;
        const TZrInstruction *instruction;

        if (pending == ZR_NULL ||
            pending->instructionIndex >= compiler.instructions.length ||
            pending->labelId >= compiler.labels.length) {
            continue;
        }
        label = (const SZrLabel *)ZrCore_Array_Get(
                &compiler.labels, pending->labelId);
        instruction = (const TZrInstruction *)ZrCore_Array_Get(
                &compiler.instructions, pending->instructionIndex);
        if (label != ZR_NULL && label->isResolved &&
            instruction != ZR_NULL &&
            instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(JUMP) &&
            label->instructionIndex > pending->instructionIndex) {
            forwardJumpCount++;
            breakTargetInstructionIndex = label->instructionIndex;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, forwardJumpCount);
    TEST_ASSERT_TRUE(breakTargetInstructionIndex >= 3U);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(GET_CONSTANT),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     breakTargetInstructionIndex - 3U))
                    ->instruction.operationCode);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(SET_STACK),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     breakTargetInstructionIndex - 2U))
                    ->instruction.operationCode);
    TEST_ASSERT_EQUAL_INT(
            ZR_INSTRUCTION_ENUM(JUMP),
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     breakTargetInstructionIndex - 1U))
                    ->instruction.operationCode);
    TEST_ASSERT_TRUE(
            ((const TZrInstruction *)ZrCore_Array_Get(
                     &compiler.instructions,
                     breakTargetInstructionIndex - 1U))
                    ->instruction.operand.operand2[0] < 0);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(8U, output.blockCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_conditioned_for_nonterminal_continue_keeps_fallback(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var flag: bool = true;\n"
            "var index: int = 0;\n"
            "for (index = 0; flag; index = 1) { continue; index = 2; }\n"
            "identity(flag);\n";
    static TZrChar sourceName[] =
            "conditioned_for_nonterminal_continue_fallback.zr";

    assert_for_cfg_fallback_blocks_later_call(
            source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
}

static void test_conditioned_for_nonterminal_break_keeps_fallback(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var flag: bool = true;\n"
            "var index: int = 0;\n"
            "for (index = 0; flag; index = 1) { break; index = 2; }\n"
            "identity(flag);\n";
    static TZrChar sourceName[] =
            "conditioned_for_nonterminal_break_fallback.zr";

    assert_for_cfg_fallback_blocks_later_call(
            source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
}

static void test_unmodeled_foreach_exit_blocks_later_call_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var values = [true];\n"
            "for (var item in values) { continue; }\n"
            "identity(true);\n";
    static TZrChar sourceName[] = "unmodeled_foreach_exit_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_declared_child_loops_do_not_pollute_entry_cfg(void) {
    static const TZrChar source[] =
            "fn child(flag: bool): bool {\n"
            "  while (flag) { break; }\n"
            "  for (;;) { break; }\n"
            "  return flag;\n"
            "}\n"
            "var flag: bool = true;\n"
            "child(flag);\n";
    static TZrChar sourceName[] = "declared_child_loop_isolation.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(5U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}
