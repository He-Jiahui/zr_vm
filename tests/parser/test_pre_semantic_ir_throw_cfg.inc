static void test_source_throw_terminates_cfg_and_blocks_unreachable_restart(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "throw seed;\n"
            "identity(seed);\n"
            "if (true) { identity(seed); }\n"
            "seed = 9;\n"
            "while (false) { identity(seed); }\n"
            "var flag: bool = true && false;\n";
    static TZrChar sourceName[] = "source_throw_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *throwInstruction = ZR_NULL;
    const SZrParserCfgBlock *throwBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize throwCount = 0U;
    TZrSize callCount = 0U;
    TZrSize execThrowCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type ==
            ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
            throwInstruction = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, throwCount);
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_NOT_NULL(throwInstruction);
    TEST_ASSERT_EQUAL_UINT32(1U, throwInstruction->operandCount);
    TEST_ASSERT_EQUAL_UINT32(1U, function->cfg.blocks.length);
    TEST_ASSERT_EQUAL_UINT32(
            function->cfg.entryBlockId, function->cfg.exitBlockId);
    throwBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW, throwBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, throwBlock->successorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        if ((EZrExecIrOpcode)output.instructions[index].opcode ==
            ZR_EXEC_IR_OPCODE_THROW) {
            execThrowCount++;
            TEST_ASSERT_EQUAL_UINT32(
                    1U, output.instructions[index].operandRange.count);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execThrowCount);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_throw_terminates_existing_invoke_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "var result: int = identity(seed);\n"
            "throw result;\n"
            "identity(result);\n";
    static TZrChar sourceName[] = "source_throw_after_call_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *throwInstruction = ZR_NULL;
    const SZrParserCfgBlock *throwBlock = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize throwCount = 0U;
    TZrSize execInvokeCount = 0U;
    TZrSize execThrowCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type ==
            ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
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
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
            throwInstruction = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, throwCount);
    TEST_ASSERT_NOT_NULL(throwInstruction);

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (throwInstruction->id - 1U >= block->firstInstructionIndex &&
            throwInstruction->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            throwBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_EQUAL_UINT32(function->cfg.exitBlockId, throwBlock->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW, throwBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, throwBlock->successorCount);

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
        } else if (opcode == ZR_EXEC_IR_OPCODE_THROW) {
            execThrowCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execInvokeCount);
    TEST_ASSERT_EQUAL_UINT64(1U, execThrowCount);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_conditional_source_throw_blocks_later_cfg_startup(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) { if (true) { throw seed; } }\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "conditional_source_throw_fallback.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize throwCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type ==
            ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
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
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(0U, throwCount);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unmodeled_throw_payload_blocks_later_cfg_startup(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "throw seed + 1;\n"
            "identity(seed);\n";
    static TZrChar sourceName[] = "unmodeled_throw_payload_fallback.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize throwCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type ==
            ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
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
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(0U, throwCount);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
