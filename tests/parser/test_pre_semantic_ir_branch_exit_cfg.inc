static const SZrParserCfgBlock *branch_exit_cfg_block_for_opcode(
        const SZrSemanticIrFunction *function,
        EZrSemanticIrOpcode opcode) {
    TZrSize instructionIndex;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    for (instructionIndex = 0U;
         instructionIndex < function->instructions.length;
         instructionIndex++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        function, instructionIndex);
        if (instruction != ZR_NULL && instruction->opcode == opcode) {
            return return_cfg_block_for_instruction(
                    function, instruction->id);
        }
    }
    return ZR_NULL;
}

static void test_source_if_then_return_preserves_fallthrough_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var flag: bool = true;\n"
            "var seed: int = 7;\n"
            "if (flag) { return seed; }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "source_if_then_return_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *returnBlock;
    const SZrParserCfgBlock *joinBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize valueReturnCount = 0U;
    TZrSize implicitReturnCount = 0U;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            if (instruction->operandCount == 1U) {
                valueReturnCount++;
            } else {
                TEST_ASSERT_EQUAL_UINT32(0U, instruction->operandCount);
                implicitReturnCount++;
            }
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, valueReturnCount);
    TEST_ASSERT_EQUAL_UINT64(1U, implicitReturnCount);
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    returnBlock = branch_exit_cfg_block_for_opcode(
            function, ZR_SEMANTIC_IR_RETURN);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_RETURN,
            returnBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, returnBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(8U, function->cfg.blocks.length);
    joinBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(joinBlock);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, joinBlock->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, joinBlock->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_source_if_else_throw_excludes_interrupted_join(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var flag: bool = true;\n"
            "var seed: int = 7;\n"
            "if (flag) { seed = 9; } else { throw seed; }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "source_if_else_throw_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *throwBlock;
    const SZrParserCfgBlock *joinBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize throwCount = 0U;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, throwCount);
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    throwBlock = branch_exit_cfg_block_for_opcode(
            function, ZR_SEMANTIC_IR_THROW);
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW,
            throwBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, throwBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(8U, function->cfg.blocks.length);
    joinBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(joinBlock);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, joinBlock->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, joinBlock->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_nested_if_return_preserves_outer_fallthrough_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var outer: bool = true;\n"
            "var inner: bool = true;\n"
            "var seed: int = 7;\n"
            "if (outer) {\n"
            "  if (inner) { return seed; }\n"
            "  seed = 8;\n"
            "} else { seed = 9; }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "nested_if_return_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *returnBlock;
    const SZrParserCfgBlock *outerJoin;
    const SZrParserCfgBlock *innerJoin;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize valueReturnCount = 0U;
    TZrSize implicitReturnCount = 0U;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(11U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            if (instruction->operandCount == 1U) {
                valueReturnCount++;
            } else {
                TEST_ASSERT_EQUAL_UINT32(0U, instruction->operandCount);
                implicitReturnCount++;
            }
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, valueReturnCount);
    TEST_ASSERT_EQUAL_UINT64(1U, implicitReturnCount);
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    returnBlock = branch_exit_cfg_block_for_opcode(
            function, ZR_SEMANTIC_IR_RETURN);
    outerJoin = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    innerJoin = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 6U);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_NOT_NULL(outerJoin);
    TEST_ASSERT_NOT_NULL(innerJoin);
    TEST_ASSERT_EQUAL_UINT32(0U, returnBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, outerJoin->kind);
    TEST_ASSERT_EQUAL_UINT32(2U, outerJoin->predecessorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, innerJoin->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, innerJoin->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_nested_if_throw_preserves_outer_fallthrough_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var outer: bool = true;\n"
            "var inner: bool = true;\n"
            "var seed: int = 7;\n"
            "if (outer) { seed = 8; } else {\n"
            "  if (inner) { seed = 9; } else { throw seed; }\n"
            "  seed = 10;\n"
            "}\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "nested_if_throw_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *throwBlock;
    const SZrParserCfgBlock *outerJoin;
    const SZrParserCfgBlock *innerJoin;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize throwCount = 0U;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(11U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, throwCount);
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    throwBlock = branch_exit_cfg_block_for_opcode(
            function, ZR_SEMANTIC_IR_THROW);
    outerJoin = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    innerJoin = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 6U);
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_NOT_NULL(outerJoin);
    TEST_ASSERT_NOT_NULL(innerJoin);
    TEST_ASSERT_EQUAL_UINT32(0U, throwBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, outerJoin->kind);
    TEST_ASSERT_EQUAL_UINT32(2U, outerJoin->predecessorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, innerJoin->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, innerJoin->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_source_if_two_abrupt_arms_remains_conservative(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var flag: bool = true;\n"
            "var seed: int = 7;\n"
            "if (flag) { return seed; } else { throw seed; }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "source_if_two_abrupt_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize abruptCount = 0U;
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
        if (instruction->opcode == ZR_SEMANTIC_IR_RETURN ||
            instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            abruptCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, abruptCount);
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_declared_child_if_exit_does_not_pollute_entry_cfg(void) {
    static const TZrChar source[] =
            "fn child(flag: bool): int {\n"
            "  if (flag) { return 1; }\n"
            "  return 2;\n"
            "}\n"
            "var flag: bool = true;\n"
            "child(flag);\n";
    static TZrChar sourceName[] = "declared_child_if_exit_isolation.zr";
    SZrCompilerState compiler;
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    const SZrSemanticIrFunction *function;
    TZrSize instructionsBefore;
    TZrSize valuesBefore;
    TZrSize placesBefore;
    TZrSize slotsBefore;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    instructionsBefore = function->instructions.length;
    valuesBefore = function->values.length;
    placesBefore = function->places.places.length;
    slotsBefore = compiler.preSemanticIrSlots.length;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(instructionsBefore, function->instructions.length);
    TEST_ASSERT_EQUAL_UINT64(valuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(placesBefore, function->places.places.length);
    TEST_ASSERT_EQUAL_UINT64(slotsBefore, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
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
    free_return_cfg_fixture(&compiler, ast);
}
