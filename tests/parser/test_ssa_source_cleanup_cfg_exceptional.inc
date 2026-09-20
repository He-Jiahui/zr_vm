static void test_invoke_try_finally_rethrows_exception_after_cleanup(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var seed: int = 7;\n"
            "try {\n"
            "  identity();\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "invoke_try_finally_cleanup.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *callInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *payloadInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *throwInstruction;
    const SZrSemanticIrInstruction *throwValueDefinition;
    const SZrSemanticIrValue *throwValue;
    const SZrParserCfgBlock *invokeBlock = ZR_NULL;
    const SZrParserCfgBlock *exceptionBlock = ZR_NULL;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *throwBlock;
    const SZrParserCfgEdge *exceptionEdge = ZR_NULL;
    const SZrParserCfgEdge *throwEdge;
    const SZrParserCfgEdge *joinEdge;
    const TZrValueId *throwOperand;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
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
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callInstruction = instruction;
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadInstruction = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(callInstruction);
    TEST_ASSERT_NOT_NULL(payloadInstruction);
    TEST_ASSERT_EQUAL_UINT32(0U, payloadInstruction->operandCount);

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);

        TEST_ASSERT_NOT_NULL(block);
        if (callInstruction->id - 1U >= block->firstInstructionIndex &&
            callInstruction->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            invokeBlock = block;
        }
        if (payloadInstruction->id - 1U >= block->firstInstructionIndex &&
            payloadInstruction->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            exceptionBlock = block;
        }
    }
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    throwBlock = find_block_terminator(
            function, ZR_PARSER_CFG_TERMINATOR_THROW);
    TEST_ASSERT_NOT_NULL(invokeBlock);
    TEST_ASSERT_NOT_NULL(exceptionBlock);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_EQUAL_UINT32(2U, invokeBlock->successorCount);
    for (index = 0U; index < invokeBlock->successorCount; index++) {
        const SZrParserCfgEdge *edge =
                ZrParser_Cfg_BlockEdgeAt(invokeBlock, index);

        TEST_ASSERT_NOT_NULL(edge);
        if (edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION) {
            exceptionEdge = edge;
        }
    }
    TEST_ASSERT_NOT_NULL(exceptionEdge);
    TEST_ASSERT_EQUAL_UINT32(exceptionBlock->id, exceptionEdge->toBlockId);
    assert_single_edge(
            exceptionBlock, ZR_PARSER_CFG_EDGE_CLEANUP, cleanup->id);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->predecessorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->successorCount);
    throwEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    joinEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 1U);
    TEST_ASSERT_NOT_NULL(throwEdge);
    TEST_ASSERT_NOT_NULL(joinEdge);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_CASE, throwEdge->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, joinEdge->kind);
    TEST_ASSERT_EQUAL_UINT32(throwBlock->id, throwEdge->toBlockId);
    TEST_ASSERT_TRUE(block_has_source_line(function, cleanup, 6U));

    throwInstruction = block_tail(function, throwBlock);
    TEST_ASSERT_NOT_NULL(throwInstruction);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_THROW, throwInstruction->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, throwInstruction->operandCount);
    throwOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            throwInstruction->operandStart);
    TEST_ASSERT_NOT_NULL(throwOperand);
    TEST_ASSERT_NOT_EQUAL(
            payloadInstruction->resultValueId, *throwOperand);
    throwValue = ZrParser_SemanticIr_Value(function, *throwOperand);
    TEST_ASSERT_NOT_NULL(throwValue);
    throwValueDefinition = ZrParser_SemanticIr_InstructionAt(
            function, throwValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(throwValueDefinition);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_LOAD, throwValueDefinition->opcode);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[exceptionBlock->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}
static void test_invoke_argument_is_captured_before_exception_cleanup(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try {\n"
            "  identity(seed);\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] =
            "invoke_argument_try_finally_cleanup.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *callInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *payloadInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *argumentDefinition;
    const SZrSemanticIrValue *argumentValue;
    const SZrParserCfgBlock *cleanup;
    const TZrValueId *argumentOperand;
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
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callInstruction = instruction;
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadInstruction = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(callInstruction);
    TEST_ASSERT_NOT_NULL(payloadInstruction);
    TEST_ASSERT_EQUAL_UINT32(2U, callInstruction->operandCount);
    argumentOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            callInstruction->operandStart + 1U);
    TEST_ASSERT_NOT_NULL(argumentOperand);
    argumentValue = ZrParser_SemanticIr_Value(function, *argumentOperand);
    TEST_ASSERT_NOT_NULL(argumentValue);
    argumentDefinition = ZrParser_SemanticIr_InstructionAt(
            function, argumentValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(argumentDefinition);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_LOAD, argumentDefinition->opcode);
    TEST_ASSERT_EQUAL_INT(4, argumentDefinition->sourceRange.start.line);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->predecessorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);

    free_source(&compiler, ast);
}

static void test_literal_argument_is_captured_before_exception_cleanup(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(7); } finally { seed = 9; }\n";
    static TZrChar sourceName[] =
            "literal_argument_try_finally_cleanup.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *callInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *argumentDefinition;
    const SZrSemanticIrValue *argumentValue;
    const SZrParserCfgBlock *cleanup;
    const TZrValueId *argumentOperand;
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
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callInstruction = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(callInstruction);
    TEST_ASSERT_EQUAL_UINT32(2U, callInstruction->operandCount);
    argumentOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            callInstruction->operandStart + 1U);
    TEST_ASSERT_NOT_NULL(argumentOperand);
    argumentValue = ZrParser_SemanticIr_Value(function, *argumentOperand);
    TEST_ASSERT_NOT_NULL(argumentValue);
    argumentDefinition = ZrParser_SemanticIr_InstructionAt(
            function, argumentValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(argumentDefinition);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_CONSTANT, argumentDefinition->opcode);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->predecessorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);

    free_source(&compiler, ast);
}

static void test_converted_argument_try_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "fn widen(value: float): float { return value; }\n"
            "var seed: int = 7;\n"
            "try { widen(seed); } finally { seed = 9; }\n";
    static TZrChar sourceName[] =
            "converted_argument_try_finally_fallback.zr";
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

static void test_two_invoke_try_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var seed: int = 7;\n"
            "try { identity(); identity(); } finally { seed = 9; }\n"
            "seed;\n";
    static TZrChar sourceName[] =
            "two_invoke_try_finally_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U, sourceName);
    const SZrSemanticIrFunction *function;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(
                ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD,
                instruction->opcode);
    }

    free_source(&compiler, ast);
}

static void test_try_catch_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "var seed: int = 7;\n"
            "try { seed = 8; } catch (error) {} finally { seed = 9; }\n";
    static TZrChar sourceName[] = "try_catch_finally_fallback.zr";
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
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));

    free_source(&compiler, ast);
}
