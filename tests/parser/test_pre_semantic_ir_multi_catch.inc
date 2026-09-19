static const SZrParserCfgBlock *multi_catch_instruction_block(const SZrSemanticIrFunction *function,
                                                              const SZrSemanticIrInstruction *instruction) {
    TZrSize index;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *) ZrCore_Array_Get((SZrArray *) &function->cfg.blocks, index);
        if (block != ZR_NULL && instruction->id - 1U >= block->firstInstructionIndex &&
            instruction->id - 1U < block->firstInstructionIndex + block->instructionCount) {
            return block;
        }
    }
    return ZR_NULL;
}

static const SZrParserCfgEdge *multi_catch_edge(const SZrParserCfgBlock *block, EZrParserCfgEdgeKind kind) {
    TZrSize index;

    if (block == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < block->successorCount; index++) {
        const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);
        if (edge != ZR_NULL && edge->kind == kind) {
            return edge;
        }
    }
    return ZR_NULL;
}

static void test_source_multiple_typed_catches_emit_ordered_dispatch(void) {
    static const TZrChar source[] = "fn identity(): int { return 7; }\n"
                                    "try { identity(); }\n"
                                    "catch (first: string) { first; }\n"
                                    "catch (second: int) { second; }\n";
    static TZrChar sourceName[] = "source_multiple_typed_catch_cfg.zr";
    SZrString *name = ZrCore_String_Create(g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *typeTests[2] = {ZR_NULL, ZR_NULL};
    const SZrSemanticIrInstruction *initializes[2] = {ZR_NULL, ZR_NULL};
    const SZrSemanticIrInstruction *unmatchedThrow = ZR_NULL;
    const SZrSemanticIrInstruction *protectedCall = ZR_NULL;
    const SZrParserCfgBlock *invokeBlock;
    const SZrParserCfgBlock *dispatchBlocks[2];
    const SZrParserCfgBlock *handlerBlocks[2];
    const SZrParserCfgBlock *unmatchedBlock;
    const SZrParserCfgEdge *edge;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize payloadCount = 0U;
    TZrSize typeTestCount = 0U;
    TZrSize initializeCount = 0U;
    TZrSize throwCount = 0U;
    TZrSize execTypeTestCount = 0U;
    TZrSize execThrowCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(&compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type == ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(&compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            protectedCall = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
            payloadCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_TYPE_TEST) {
            if (typeTestCount < ZR_ARRAY_COUNT(typeTests)) {
                typeTests[typeTestCount] = instruction;
            }
            typeTestCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            unmatchedThrow = instruction;
            throwCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE && payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId) {
            if (initializeCount < ZR_ARRAY_COUNT(initializes)) {
                initializes[initializeCount] = instruction;
            }
            initializeCount++;
        }
    }

    TEST_ASSERT_EQUAL_UINT64(1U, payloadCount);
    TEST_ASSERT_EQUAL_UINT64(2U, typeTestCount);
    TEST_ASSERT_EQUAL_UINT64(2U, initializeCount);
    TEST_ASSERT_EQUAL_UINT64(1U, throwCount);
    TEST_ASSERT_NOT_NULL(protectedCall);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(typeTests[0]);
    TEST_ASSERT_NOT_NULL(typeTests[1]);
    TEST_ASSERT_NOT_NULL(initializes[0]);
    TEST_ASSERT_NOT_NULL(initializes[1]);
    TEST_ASSERT_NOT_NULL(unmatchedThrow);
    TEST_ASSERT_EQUAL_UINT32(1U, unmatchedThrow->operandCount);
    TEST_ASSERT_EQUAL_UINT32(payload->resultValueId, typeTests[0]->valueId);
    TEST_ASSERT_EQUAL_UINT32(payload->resultValueId, typeTests[1]->valueId);
    TEST_ASSERT_NOT_EQUAL(typeTests[0]->matchTypeId, typeTests[1]->matchTypeId);
    TEST_ASSERT_EQUAL_UINT32(typeTests[0]->matchTypeId, initializes[0]->typeId);
    TEST_ASSERT_EQUAL_UINT32(typeTests[1]->matchTypeId, initializes[1]->typeId);
    TEST_ASSERT_EQUAL_UINT32(payload->resultValueId,
                             *(const TZrValueId *) ZrCore_Array_Get((SZrArray *) &function->valueOperands,
                                                                    unmatchedThrow->operandStart));

    invokeBlock = multi_catch_instruction_block(function, protectedCall);
    dispatchBlocks[0] = multi_catch_instruction_block(function, typeTests[0]);
    dispatchBlocks[1] = multi_catch_instruction_block(function, typeTests[1]);
    handlerBlocks[0] = multi_catch_instruction_block(function, initializes[0]);
    handlerBlocks[1] = multi_catch_instruction_block(function, initializes[1]);
    unmatchedBlock = multi_catch_instruction_block(function, unmatchedThrow);
    TEST_ASSERT_NOT_NULL(invokeBlock);
    TEST_ASSERT_NOT_NULL(dispatchBlocks[0]);
    TEST_ASSERT_NOT_NULL(dispatchBlocks[1]);
    TEST_ASSERT_NOT_NULL(handlerBlocks[0]);
    TEST_ASSERT_NOT_NULL(handlerBlocks[1]);
    TEST_ASSERT_NOT_NULL(unmatchedBlock);
    TEST_ASSERT_NOT_EQUAL(dispatchBlocks[0]->id, dispatchBlocks[1]->id);
    TEST_ASSERT_NOT_EQUAL(dispatchBlocks[0]->id, handlerBlocks[0]->id);
    TEST_ASSERT_NOT_EQUAL(dispatchBlocks[1]->id, handlerBlocks[1]->id);

    edge = multi_catch_edge(invokeBlock, ZR_PARSER_CFG_EDGE_EXCEPTION);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(dispatchBlocks[0]->id, edge->toBlockId);
    edge = multi_catch_edge(dispatchBlocks[0], ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(handlerBlocks[0]->id, edge->toBlockId);
    edge = multi_catch_edge(dispatchBlocks[0], ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(dispatchBlocks[1]->id, edge->toBlockId);
    edge = multi_catch_edge(dispatchBlocks[1], ZR_PARSER_CFG_EDGE_TRUE_BRANCH);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(handlerBlocks[1]->id, edge->toBlockId);
    edge = multi_catch_edge(dispatchBlocks[1], ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(unmatchedBlock->id, edge->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, handlerBlocks[0]->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, handlerBlocks[1]->successorCount);
    TEST_ASSERT_EQUAL_UINT32(ZrParser_Cfg_BlockEdgeAt(handlerBlocks[0], 0U)->toBlockId,
                             ZrParser_Cfg_BlockEdgeAt(handlerBlocks[1], 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_THROW, unmatchedBlock->terminatorKind);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        if ((EZrExecIrOpcode) instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST) {
            TEST_ASSERT_TRUE(execTypeTestCount < ZR_ARRAY_COUNT(typeTests));
            TEST_ASSERT_EQUAL_UINT32(typeTests[execTypeTestCount]->matchTypeId, instruction->matchTypeToken);
            TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
            TEST_ASSERT_EQUAL_UINT32(payload->resultValueId, output.operands[instruction->operands.start]);
            execTypeTestCount++;
        } else if ((EZrExecIrOpcode) instruction->opcode == ZR_EXEC_IR_OPCODE_THROW) {
            execThrowCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, execTypeTestCount);
    TEST_ASSERT_EQUAL_UINT64(1U, execThrowCount);
    TEST_ASSERT_TRUE((output.blocks[dispatchBlocks[0]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE((output.blocks[dispatchBlocks[1]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE((output.blocks[handlerBlocks[0]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE((output.blocks[handlerBlocks[1]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE((output.blocks[unmatchedBlock->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_terminal_catch_all_closes_ordered_dispatch(void) {
    static const TZrChar source[] = "fn identity(): int { return 7; }\n"
                                    "try { identity(); }\n"
                                    "catch (first: string) { first; }\n"
                                    "catch (second: int) { second; }\n"
                                    "catch (fallback) { fallback; }\n";
    static TZrChar sourceName[] = "source_terminal_catch_all_cfg.zr";
    SZrString *name = ZrCore_String_Create(g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *typeTests[2] = {ZR_NULL, ZR_NULL};
    const SZrSemanticIrInstruction *initializes[3] = {ZR_NULL, ZR_NULL, ZR_NULL};
    const SZrParserCfgBlock *dispatchBlocks[2];
    const SZrParserCfgBlock *handlerBlocks[3];
    const SZrParserCfgEdge *edge;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize payloadCount = 0U;
    TZrSize typeTestCount = 0U;
    TZrSize initializeCount = 0U;
    TZrSize throwCount = 0U;
    TZrSize execTypeTestCount = 0U;
    TZrSize execThrowCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(&compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type == ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(&compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
            payloadCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_TYPE_TEST) {
            if (typeTestCount < ZR_ARRAY_COUNT(typeTests)) {
                typeTests[typeTestCount] = instruction;
            }
            typeTestCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE && payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId) {
            if (initializeCount < ZR_ARRAY_COUNT(initializes)) {
                initializes[initializeCount] = instruction;
            }
            initializeCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, payloadCount);
    TEST_ASSERT_EQUAL_UINT64(2U, typeTestCount);
    TEST_ASSERT_EQUAL_UINT64(3U, initializeCount);
    TEST_ASSERT_EQUAL_UINT64(0U, throwCount);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(typeTests[0]);
    TEST_ASSERT_NOT_NULL(typeTests[1]);
    TEST_ASSERT_NOT_NULL(initializes[0]);
    TEST_ASSERT_NOT_NULL(initializes[1]);
    TEST_ASSERT_NOT_NULL(initializes[2]);
    TEST_ASSERT_EQUAL_UINT32(payload->resultValueId, typeTests[0]->valueId);
    TEST_ASSERT_EQUAL_UINT32(payload->resultValueId, typeTests[1]->valueId);
    TEST_ASSERT_EQUAL_UINT32(typeTests[0]->matchTypeId, initializes[0]->typeId);
    TEST_ASSERT_EQUAL_UINT32(typeTests[1]->matchTypeId, initializes[1]->typeId);
    TEST_ASSERT_EQUAL_UINT32(payload->typeId, initializes[2]->typeId);

    dispatchBlocks[0] = multi_catch_instruction_block(function, typeTests[0]);
    dispatchBlocks[1] = multi_catch_instruction_block(function, typeTests[1]);
    handlerBlocks[0] = multi_catch_instruction_block(function, initializes[0]);
    handlerBlocks[1] = multi_catch_instruction_block(function, initializes[1]);
    handlerBlocks[2] = multi_catch_instruction_block(function, initializes[2]);
    TEST_ASSERT_NOT_NULL(dispatchBlocks[0]);
    TEST_ASSERT_NOT_NULL(dispatchBlocks[1]);
    TEST_ASSERT_NOT_NULL(handlerBlocks[0]);
    TEST_ASSERT_NOT_NULL(handlerBlocks[1]);
    TEST_ASSERT_NOT_NULL(handlerBlocks[2]);
    edge = multi_catch_edge(dispatchBlocks[0], ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(dispatchBlocks[1]->id, edge->toBlockId);
    edge = multi_catch_edge(dispatchBlocks[1], ZR_PARSER_CFG_EDGE_FALSE_BRANCH);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT32(handlerBlocks[2]->id, edge->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, handlerBlocks[0]->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, handlerBlocks[1]->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, handlerBlocks[2]->successorCount);
    TEST_ASSERT_EQUAL_UINT32(ZrParser_Cfg_BlockEdgeAt(handlerBlocks[0], 0U)->toBlockId,
                             ZrParser_Cfg_BlockEdgeAt(handlerBlocks[1], 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(ZrParser_Cfg_BlockEdgeAt(handlerBlocks[1], 0U)->toBlockId,
                             ZrParser_Cfg_BlockEdgeAt(handlerBlocks[2], 0U)->toBlockId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        if ((EZrExecIrOpcode) instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST) {
            TEST_ASSERT_TRUE(execTypeTestCount < ZR_ARRAY_COUNT(typeTests));
            TEST_ASSERT_EQUAL_UINT32(typeTests[execTypeTestCount]->matchTypeId, instruction->matchTypeToken);
            execTypeTestCount++;
        } else if ((EZrExecIrOpcode) instruction->opcode == ZR_EXEC_IR_OPCODE_THROW) {
            execThrowCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, execTypeTestCount);
    TEST_ASSERT_EQUAL_UINT64(0U, execThrowCount);
    TEST_ASSERT_TRUE((output.blocks[dispatchBlocks[0]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE((output.blocks[dispatchBlocks[1]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE((output.blocks[handlerBlocks[2]->id].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_nonterminal_catch_all_keeps_legacy_cfg(void) {
    static const TZrChar source[] = "fn identity(value: int): int { return value; }\n"
                                    "var seed: int = 7;\n"
                                    "try { identity(seed); }\n"
                                    "catch (fallback) {}\n"
                                    "catch (later: string) {}\n"
                                    "identity(seed);\n";
    static TZrChar sourceName[] = "source_nonterminal_catch_all_fallback.zr";

    assert_nested_call_keeps_legacy_cfg(source, sizeof(source) - 1U, sourceName, sizeof(sourceName) - 1U);
}

static void test_source_multiple_catches_discard_all_handlers_after_late_fallback(void) {
    static const TZrChar source[] = "try { unresolved(); }\n"
                                    "catch (first: string) { first; }\n"
                                    "catch (second) { second; }\n";
    static TZrChar sourceName[] = "source_multiple_catch_late_fallback.zr";
    SZrString *name = ZrCore_String_Create(g_state, sourceName, sizeof(sourceName) - 1U);
    SZrString *callableName = ZrCore_String_CreateFromNative(g_state, "unresolved");
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize payloadCount = 0U;
    TZrSize firstCatchLoadCount = 0U;
    TZrSize secondCatchLoadCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, allocate_local_var(&compiler, callableName));
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(&compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD && instruction->sourceRange.start.line == 2U) {
            firstCatchLoadCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD && instruction->sourceRange.start.line == 3U) {
            secondCatchLoadCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, payloadCount);
    TEST_ASSERT_EQUAL_UINT64(0U, firstCatchLoadCount);
    TEST_ASSERT_EQUAL_UINT64(0U, secondCatchLoadCount);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_EQUAL_UINT32(ZR_PARSER_CFG_INVALID_BLOCK_ID, compiler.preSemanticIrCfgCatchBlock);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_multiple_catches_preserve_local_abrupt_handler(void) {
    static const TZrChar source[] = "fn identity(): int { return 7; }\n"
                                    "try { identity(); }\n"
                                    "catch (first: string) { throw first; }\n"
                                    "catch (second: int) { second; }\n"
                                    "identity();\n";
    static TZrChar sourceName[] = "source_multiple_catch_abrupt_cfg.zr";
    SZrString *name = ZrCore_String_Create(g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *protectedCall = ZR_NULL;
    const SZrSemanticIrInstruction *trailingCall = ZR_NULL;
    const SZrSemanticIrInstruction *firstTypeTest = ZR_NULL;
    const SZrSemanticIrInstruction *firstInitialize = ZR_NULL;
    const SZrParserCfgBlock *firstHandlerBlock;
    const SZrParserCfgBlock *firstDispatchBlock;
    const SZrParserCfgBlock *trailingInvokeBlock;
    TZrSize callCount = 0U;
    TZrSize typeTestCount = 0U;
    TZrSize throwCount = 0U;
    TZrBool trailingTargetsLocalDispatch = ZR_FALSE;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(&compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        if (ast->data.script.statements->nodes[index]->type == ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        ZrParser_Statement_Compile(&compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            if (protectedCall == ZR_NULL) {
                protectedCall = instruction;
            } else {
                trailingCall = instruction;
            }
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_TYPE_TEST) {
            if (firstTypeTest == ZR_NULL) {
                firstTypeTest = instruction;
            }
            typeTestCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE && payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId && firstInitialize == ZR_NULL) {
            firstInitialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            throwCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, callCount);
    TEST_ASSERT_EQUAL_UINT64(2U, typeTestCount);
    TEST_ASSERT_EQUAL_UINT64(2U, throwCount);
    TEST_ASSERT_NOT_NULL(protectedCall);
    TEST_ASSERT_NOT_NULL(trailingCall);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(firstTypeTest);
    TEST_ASSERT_NOT_NULL(firstInitialize);
    firstDispatchBlock = multi_catch_instruction_block(function, firstTypeTest);
    firstHandlerBlock = multi_catch_instruction_block(function, firstInitialize);
    trailingInvokeBlock = multi_catch_instruction_block(function, trailingCall);
    TEST_ASSERT_NOT_NULL(firstDispatchBlock);
    TEST_ASSERT_NOT_NULL(firstHandlerBlock);
    TEST_ASSERT_NOT_NULL(trailingInvokeBlock);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_THROW, firstHandlerBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, firstHandlerBlock->successorCount);
    for (index = 0U; index < trailingInvokeBlock->successorCount; index++) {
        const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(trailingInvokeBlock, index);
        TEST_ASSERT_NOT_NULL(edge);
        if (edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION && edge->toBlockId == firstDispatchBlock->id) {
            trailingTargetsLocalDispatch = ZR_TRUE;
        }
    }
    TEST_ASSERT_FALSE(trailingTargetsLocalDispatch);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
