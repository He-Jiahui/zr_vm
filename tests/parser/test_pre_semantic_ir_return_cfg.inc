static SZrAstNode *compile_return_cfg_fixture(
        SZrCompilerState *compiler,
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar *sourceName,
        TZrSize sourceNameLength) {
    SZrString *name;
    SZrAstNode *ast;
    TZrSize index;

    name = ZrCore_String_Create(
            g_state, sourceName, sourceNameLength);
    ast = ZrParser_Parse(g_state, source, sourceLength, name);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    if (compiler->currentFunction == ZR_NULL) {
        return ast;
    }
    ZrParser_Compiler_PredeclareFunctionBindings(
            compiler, ast->data.script.statements);
    if (compiler->hasError) {
        return ast;
    }
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type ==
            ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(
                compiler, ast->data.script.statements->nodes[index]);
        if (compiler->hasError) {
            break;
        }
    }
    return ast;
}

static void free_return_cfg_fixture(
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

static const SZrParserCfgBlock *return_cfg_block_for_instruction(
        const SZrSemanticIrFunction *function,
        TZrSemanticInstructionId instructionId) {
    TZrSize index;

    if (function == ZR_NULL || instructionId ==
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        if (block != ZR_NULL &&
            instructionId - 1U >= block->firstInstructionIndex &&
            instructionId - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            return block;
        }
    }
    return ZR_NULL;
}

static void test_source_return_terminates_cfg_and_blocks_unreachable_restart(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "return seed;\n"
            "identity(seed);\n"
            "if (true) { identity(seed); }\n"
            "seed = 9;\n"
            "while (false) { identity(seed); }\n"
            "var flag: bool = true && false;\n";
    static TZrChar sourceName[] = "source_return_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *returnInstruction = ZR_NULL;
    const SZrParserCfgBlock *returnBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize returnCount = 0U;
    TZrSize callCount = 0U;
    TZrSize execReturnCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            returnCount++;
            returnInstruction = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, returnCount);
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_NOT_NULL(returnInstruction);
    TEST_ASSERT_EQUAL_UINT32(1U, returnInstruction->operandCount);
    TEST_ASSERT_EQUAL_UINT32(1U, function->cfg.blocks.length);
    TEST_ASSERT_EQUAL_UINT32(
            function->cfg.entryBlockId, function->cfg.exitBlockId);
    returnBlock = return_cfg_block_for_instruction(
            function, returnInstruction->id);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_RETURN, returnBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, returnBlock->successorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        if ((EZrExecIrOpcode)output.instructions[index].opcode ==
            ZR_EXEC_IR_OPCODE_RETURN) {
            execReturnCount++;
            TEST_ASSERT_EQUAL_UINT32(
                    1U, output.instructions[index].operandRange.count);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execReturnCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_source_return_terminates_existing_invoke_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "return identity(seed);\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "source_return_after_call_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *returnInstruction = ZR_NULL;
    const SZrParserCfgBlock *returnBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize returnCount = 0U;
    TZrSize execInvokeCount = 0U;
    TZrSize execReturnCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            returnCount++;
            returnInstruction = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, returnCount);
    TEST_ASSERT_NOT_NULL(returnInstruction);
    TEST_ASSERT_EQUAL_UINT32(1U, returnInstruction->operandCount);
    returnBlock = return_cfg_block_for_instruction(
            function, returnInstruction->id);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_UINT32(function->cfg.exitBlockId, returnBlock->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_RETURN, returnBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, returnBlock->successorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        EZrExecIrOpcode opcode =
                (EZrExecIrOpcode)output.instructions[index].opcode;
        if (opcode == ZR_EXEC_IR_OPCODE_INVOKE) {
            execInvokeCount++;
        } else if (opcode == ZR_EXEC_IR_OPCODE_RETURN) {
            execReturnCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execInvokeCount);
    TEST_ASSERT_EQUAL_UINT64(1U, execReturnCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_declared_child_return_does_not_pollute_entry_cfg(void) {
    static const TZrChar source[] =
            "resource class Value {}\n"
            "fn child() { return own Value(); }\n"
            "return 9;\n";
    static TZrChar sourceName[] = "declared_child_return_cfg.zr";
    SZrCompilerState compiler;
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    const SZrSemanticIrFunction *function;
    TZrSize instructionsBefore;
    TZrSize valuesBefore;
    TZrSize placesBefore;
    TZrSize loansBefore;
    TZrSize slotsBefore;
    TZrSize receiverLoansBefore;
    TZrSize blocksBefore;
    TZrSize constantCount = 0U;
    TZrSize returnCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);

    ZrParser_Compiler_CompileClassDeclaration(
            &compiler, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    instructionsBefore = function->instructions.length;
    valuesBefore = function->values.length;
    placesBefore = function->places.places.length;
    loansBefore = function->loanFacts.length;
    slotsBefore = compiler.preSemanticIrSlots.length;
    receiverLoansBefore = compiler.preSemanticIrReceiverLoanIds.length;
    blocksBefore = function->cfg.blocks.length;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(instructionsBefore, function->instructions.length);
    TEST_ASSERT_EQUAL_UINT64(valuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(placesBefore, function->places.places.length);
    TEST_ASSERT_EQUAL_UINT64(loansBefore, function->loanFacts.length);
    TEST_ASSERT_EQUAL_UINT64(slotsBefore, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_EQUAL_UINT64(
            receiverLoansBefore,
            compiler.preSemanticIrReceiverLoanIds.length);
    TEST_ASSERT_EQUAL_UINT64(blocksBefore, function->cfg.blocks.length);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CONSTANT) {
            constantCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            returnCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, constantCount);
    TEST_ASSERT_EQUAL_UINT64(1U, returnCount);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_source_void_return_uses_null_value_terminator(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "return;\n"
            "identity(7);\n";
    static TZrChar sourceName[] = "source_void_return_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *constantInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *returnInstruction = ZR_NULL;
    const SZrSemanticIrValue *constantValue;
    const TZrValueId *returnOperand;
    const SZrTypeValue *constantPoolValue;
    const SZrParserCfgBlock *returnBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize constantCount = 0U;
    TZrSize returnCount = 0U;
    TZrSize execReturnCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CONSTANT) {
            constantCount++;
            constantInstruction = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            returnCount++;
            returnInstruction = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, constantCount);
    TEST_ASSERT_EQUAL_UINT64(1U, returnCount);
    TEST_ASSERT_NOT_NULL(constantInstruction);
    TEST_ASSERT_NOT_NULL(returnInstruction);
    TEST_ASSERT_TRUE(constantInstruction->hasConstantPoolIndex);
    TEST_ASSERT_NOT_EQUAL(
            ZR_VALUE_ID_INVALID, constantInstruction->resultValueId);
    TEST_ASSERT_EQUAL_UINT32(1U, returnInstruction->operandCount);
    returnOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            returnInstruction->operandStart);
    TEST_ASSERT_NOT_NULL(returnOperand);
    TEST_ASSERT_EQUAL_UINT32(
            constantInstruction->resultValueId, *returnOperand);
    TEST_ASSERT_EQUAL_UINT32(
            constantInstruction->typeId, returnInstruction->typeId);
    constantValue = ZrParser_SemanticIr_Value(
            function, constantInstruction->resultValueId);
    TEST_ASSERT_NOT_NULL(constantValue);
    TEST_ASSERT_EQUAL_UINT32(
            constantInstruction->typeId, constantValue->typeId);
    constantPoolValue = (const SZrTypeValue *)ZrCore_Array_Get(
            &compiler.constants, constantInstruction->constantPoolIndex);
    TEST_ASSERT_NOT_NULL(constantPoolValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, constantPoolValue->type);
    returnBlock = return_cfg_block_for_instruction(
            function, returnInstruction->id);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_RETURN,
            returnBlock->terminatorKind);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        if ((EZrExecIrOpcode)output.instructions[index].opcode ==
            ZR_EXEC_IR_OPCODE_RETURN) {
            execReturnCount++;
            TEST_ASSERT_EQUAL_UINT32(
                    1U, output.instructions[index].operandRange.count);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execReturnCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_nested_two_return_arms_preserve_outer_fallthrough_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) {\n"
            "  if (true) { return seed; } else { return seed; }\n"
            "}\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "nested_two_return_arms_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize valueReturnCount = 0U;
    TZrSize implicitReturnCount = 0U;
    TZrSize returnSinkCount = 0U;
    TZrSize joinCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgTerminated);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            if (instruction->operandCount == 1U) {
                valueReturnCount++;
            } else {
                TEST_ASSERT_EQUAL_UINT32(0U, instruction->operandCount);
                implicitReturnCount++;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    TEST_ASSERT_EQUAL_UINT64(2U, valueReturnCount);
    TEST_ASSERT_EQUAL_UINT64(1U, implicitReturnCount);
    TEST_ASSERT_EQUAL_UINT32(10U, function->cfg.blocks.length);
    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (block->kind == ZR_PARSER_CFG_BLOCK_JOIN) {
            joinCount++;
            TEST_ASSERT_EQUAL_UINT32(1U, block->predecessorCount);
        }
        if (block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_RETURN) {
            returnSinkCount++;
            TEST_ASSERT_EQUAL_UINT32(0U, block->successorCount);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, joinCount);
    TEST_ASSERT_EQUAL_UINT64(3U, returnSinkCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_unmodeled_return_payload_blocks_later_cfg_startup(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "return seed + 1;\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "unmodeled_return_payload_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize returnCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            returnCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(0U, returnCount);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    free_return_cfg_fixture(&compiler, ast);
}

static void test_finally_return_preserves_precleanup_value(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try {\n"
            "  return seed;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "finally_return_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_return_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *returnInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *returnDefinition;
    const SZrSemanticIrValue *returnValue;
    const SZrParserCfgBlock *cleanupBlock = ZR_NULL;
    const SZrParserCfgBlock *returnBlock;
    const SZrParserCfgEdge *edge;
    const TZrValueId *returnOperand;
    TZrSize callCount = 0U;
    TZrSize returnCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
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
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_RETURN) {
            returnCount++;
            returnInstruction = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, returnCount);
    TEST_ASSERT_EQUAL_UINT32(3U, function->cfg.blocks.length);
    TEST_ASSERT_NOT_NULL(returnInstruction);
    TEST_ASSERT_EQUAL_UINT32(1U, returnInstruction->operandCount);
    TEST_ASSERT_EQUAL_INT(4, returnInstruction->sourceRange.start.line);
    returnBlock = return_cfg_block_for_instruction(
            function, returnInstruction->id);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_UINT32(function->cfg.exitBlockId, returnBlock->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_RETURN, returnBlock->terminatorKind);
    returnOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            returnInstruction->operandStart);
    TEST_ASSERT_NOT_NULL(returnOperand);
    returnValue = ZrParser_SemanticIr_Value(function, *returnOperand);
    TEST_ASSERT_NOT_NULL(returnValue);
    returnDefinition = ZrParser_SemanticIr_InstructionAt(
            function, returnValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(returnDefinition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_LOAD, returnDefinition->opcode);
    TEST_ASSERT_EQUAL_INT(4, returnDefinition->sourceRange.start.line);
    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        if (block != ZR_NULL &&
            block->kind == ZR_PARSER_CFG_BLOCK_CLEANUP) {
            cleanupBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(cleanupBlock);
    TEST_ASSERT_EQUAL_UINT32(1U, cleanupBlock->successorCount);
    edge = ZrParser_Cfg_BlockEdgeAt(cleanupBlock, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_CLEANUP, edge->kind);
    TEST_ASSERT_EQUAL_UINT32(returnBlock->id, edge->toBlockId);
    free_return_cfg_fixture(&compiler, ast);
}
