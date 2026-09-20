static void test_source_single_catch_all_emits_exception_handler_cfg(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var seed: int = 7;\n"
            "try { identity(); }\n"
            "catch (error) {}\n"
            "identity();\n";
    static TZrChar sourceName[] = "source_single_catch_all_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *callInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *trailingCallInstruction = ZR_NULL;
    const SZrSemanticIrInstruction *payloadInstruction = ZR_NULL;
    const SZrParserCfgBlock *invokeBlock = ZR_NULL;
    const SZrParserCfgBlock *trailingInvokeBlock = ZR_NULL;
    const SZrParserCfgBlock *handlerBlock = ZR_NULL;
    const SZrParserCfgEdge *exceptionEdge = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize payloadCount = 0U;
    TZrSize execPayloadCount = 0U;
    TZrBool foundTrailingException = ZR_FALSE;
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
            if (callInstruction == ZR_NULL) {
                callInstruction = instruction;
            } else {
                trailingCallInstruction = instruction;
            }
            callCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadCount++;
            payloadInstruction = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, payloadCount);
    TEST_ASSERT_NOT_NULL(callInstruction);
    TEST_ASSERT_NOT_NULL(trailingCallInstruction);
    TEST_ASSERT_NOT_NULL(payloadInstruction);
    TEST_ASSERT_NOT_EQUAL(
            ZR_VALUE_ID_INVALID, payloadInstruction->resultValueId);
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
            handlerBlock = block;
        }
        if (trailingCallInstruction->id - 1U >=
                    block->firstInstructionIndex &&
            trailingCallInstruction->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            trailingInvokeBlock = block;
        }
    }
    TEST_ASSERT_NOT_NULL(invokeBlock);
    TEST_ASSERT_NOT_NULL(handlerBlock);
    TEST_ASSERT_NOT_NULL(trailingInvokeBlock);
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
    TEST_ASSERT_EQUAL_UINT32(handlerBlock->id, exceptionEdge->toBlockId);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH,
            handlerBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, trailingInvokeBlock->successorCount);
    for (index = 0U; index < trailingInvokeBlock->successorCount; index++) {
        const SZrParserCfgEdge *edge =
                ZrParser_Cfg_BlockEdgeAt(trailingInvokeBlock, index);
        TEST_ASSERT_NOT_NULL(edge);
        if (edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION) {
            foundTrailingException = ZR_TRUE;
            TEST_ASSERT_NOT_EQUAL(handlerBlock->id, edge->toBlockId);
        }
    }
    TEST_ASSERT_TRUE(foundTrailingException);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_PARSER_CFG_INVALID_BLOCK_ID,
            compiler.preSemanticIrCfgCatchBlock);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgCatchUsed);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        if ((EZrExecIrOpcode)output.instructions[index].opcode ==
            ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD) {
            TZrSize blockIndex;
            const SZrExecIrBlock *execHandlerBlock = ZR_NULL;
            execPayloadCount++;
            for (blockIndex = 0U; blockIndex < output.blockCount;
                 blockIndex++) {
                const SZrExecIrBlock *block = &output.blocks[blockIndex];
                if (index >= block->instructions.start &&
                    index < block->instructions.start +
                                    block->instructions.count) {
                    execHandlerBlock = block;
                    break;
                }
            }
            TEST_ASSERT_NOT_NULL(execHandlerBlock);
            TEST_ASSERT_TRUE(
                    (execHandlerBlock->flags &
                     ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execPayloadCount);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_payload_initializes_readable_binding(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error) { error; }\n"
            "identity();\n";
    static TZrChar sourceName[] = "source_catch_payload_binding.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *initialize = ZR_NULL;
    const SZrSemanticIrInstruction *load = ZR_NULL;
    const SZrParserCfgBlock *handlerBlock = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize payloadCount = 0U;
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
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
            payloadCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
                   payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId) {
            initialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   initialize != ZR_NULL &&
                   instruction->placeId == initialize->placeId) {
            load = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, payloadCount);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(initialize);
    TEST_ASSERT_NOT_NULL(load);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, initialize->placeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, initialize->symbolId);
    TEST_ASSERT_EQUAL_UINT32(initialize->typeId, load->typeId);

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (payload->id - 1U >= block->firstInstructionIndex &&
            payload->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            handlerBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(handlerBlock);
    TEST_ASSERT_TRUE(initialize->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            initialize->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_TRUE(load->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            load->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH,
            handlerBlock->terminatorKind);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_flows_payload_through_handler_local(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar sourceName[] = "source_catch_handler_local.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *catchInitialize = ZR_NULL;
    const SZrSemanticIrInstruction *catchLoad = ZR_NULL;
    const SZrSemanticIrInstruction *localConvert = ZR_NULL;
    const SZrSemanticIrInstruction *localInitialize = ZR_NULL;
    const SZrSemanticIrInstruction *localLoad = ZR_NULL;
    const SZrParserCfgBlock *handlerBlock = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
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
        if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
                   payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId) {
            catchInitialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   catchInitialize != ZR_NULL &&
                   instruction->placeId == catchInitialize->placeId) {
            catchLoad = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CONVERT &&
                   catchLoad != ZR_NULL &&
                   instruction->valueId == catchLoad->resultValueId) {
            localConvert = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
                   localConvert != ZR_NULL &&
                   instruction->valueId == localConvert->resultValueId) {
            localInitialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   localInitialize != ZR_NULL &&
                   instruction->placeId == localInitialize->placeId) {
            localLoad = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(catchInitialize);
    TEST_ASSERT_NOT_NULL(catchLoad);
    TEST_ASSERT_NOT_NULL(localConvert);
    TEST_ASSERT_NOT_NULL(localInitialize);
    TEST_ASSERT_NOT_NULL(localLoad);
    TEST_ASSERT_NOT_EQUAL(catchInitialize->placeId, localInitialize->placeId);
    TEST_ASSERT_EQUAL_UINT32(catchLoad->typeId, localConvert->typeId);
    TEST_ASSERT_EQUAL_UINT32(localConvert->typeId, localInitialize->typeId);
    TEST_ASSERT_EQUAL_UINT32(localInitialize->typeId, localLoad->typeId);

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);

        TEST_ASSERT_NOT_NULL(block);
        if (payload->id - 1U >= block->firstInstructionIndex &&
            payload->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            handlerBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(handlerBlock);
    TEST_ASSERT_TRUE(
            catchInitialize->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            catchInitialize->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_TRUE(
            catchLoad->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            catchLoad->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_TRUE(
            localConvert->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            localConvert->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_TRUE(
            localInitialize->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            localInitialize->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_TRUE(
            localLoad->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            localLoad->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH,
            handlerBlock->terminatorKind);
    for (index = handlerBlock->firstInstructionIndex;
         index < handlerBlock->firstInstructionIndex +
                         handlerBlock->instructionCount;
         index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_IR_DROP, instruction->opcode);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_IR_END_LOAN, instruction->opcode);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_IR_CLEANUP, instruction->opcode);
    }

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_captures_simple_argument_before_invoke(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error) { error; }\n";
    static TZrChar sourceName[] = "source_catch_argument_state.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *call = ZR_NULL;
    const SZrSemanticIrInstruction *argumentDefinition = ZR_NULL;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrParserCfgBlock *argumentBlock = ZR_NULL;
    const SZrParserCfgBlock *invokeBlock = ZR_NULL;
    const SZrParserCfgBlock *handlerBlock = ZR_NULL;
    const TZrValueId *argumentValueId;
    TZrBool handlerHasArgumentOperand = ZR_FALSE;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
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
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED &&
            instruction->sourceRange.start.line == 3U) {
            call = instruction;
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_EQUAL_UINT32(2U, call->operandCount);
    argumentValueId = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands, call->operandStart + 1U);
    TEST_ASSERT_NOT_NULL(argumentValueId);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->resultValueId == *argumentValueId) {
            argumentDefinition = instruction;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(argumentDefinition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_LOAD, argumentDefinition->opcode);
    TEST_ASSERT_TRUE(argumentDefinition->id < call->id);

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (call->id - 1U >= block->firstInstructionIndex &&
            call->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            invokeBlock = block;
        }
        if (argumentDefinition->id - 1U >= block->firstInstructionIndex &&
            argumentDefinition->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            argumentBlock = block;
        }
        if (payload->id - 1U >= block->firstInstructionIndex &&
            payload->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            handlerBlock = block;
        }
    }
    TEST_ASSERT_NOT_NULL(argumentBlock);
    TEST_ASSERT_NOT_NULL(invokeBlock);
    TEST_ASSERT_NOT_NULL(handlerBlock);
    TEST_ASSERT_NOT_EQUAL(argumentBlock->id, invokeBlock->id);
    TEST_ASSERT_NOT_EQUAL(invokeBlock->id, handlerBlock->id);
    TEST_ASSERT_EQUAL_UINT32(1U, argumentBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(argumentBlock, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            invokeBlock->id,
            ZrParser_Cfg_BlockEdgeAt(argumentBlock, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, invokeBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            handlerBlock->id,
            ZrParser_Cfg_BlockEdgeAt(invokeBlock, 1U)->toBlockId);
    for (index = handlerBlock->firstInstructionIndex;
         index < handlerBlock->firstInstructionIndex +
                         handlerBlock->instructionCount;
         index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TZrSize operandIndex;

        TEST_ASSERT_NOT_NULL(instruction);
        for (operandIndex = 0U;
             operandIndex < instruction->operandCount;
             operandIndex++) {
            const TZrValueId *operand =
                    (const TZrValueId *)ZrCore_Array_Get(
                            (SZrArray *)&function->valueOperands,
                            instruction->operandStart + operandIndex);
            TEST_ASSERT_NOT_NULL(operand);
            if (*operand == *argumentValueId) {
                handlerHasArgumentOperand = ZR_TRUE;
            }
        }
    }
    TEST_ASSERT_FALSE(handlerHasArgumentOperand);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_captures_literal_argument_before_invoke(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "try { identity(7); }\n"
            "catch (error) { error; }\n";
    static TZrChar sourceName[] = "source_catch_literal_argument.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *call = ZR_NULL;
    const SZrSemanticIrInstruction *argumentDefinition = ZR_NULL;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const TZrValueId *argumentValueId;
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
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED &&
            instruction->sourceRange.start.line == 2U) {
            call = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_EQUAL_UINT32(2U, call->operandCount);
    argumentValueId = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands, call->operandStart + 1U);
    TEST_ASSERT_NOT_NULL(argumentValueId);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->resultValueId == *argumentValueId) {
            argumentDefinition = instruction;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(argumentDefinition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_CONSTANT, argumentDefinition->opcode);
    TEST_ASSERT_TRUE(argumentDefinition->id < call->id);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_declared_child_try_does_not_pollute_entry_cfg(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "fn child(): int {\n"
            "  try { identity(); } catch (error) {}\n"
            "  return 1;\n"
            "}\n"
            "identity();\n";
    static TZrChar sourceName[] = "declared_child_try_cfg_isolation.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize instructionsBefore;
    TZrSize valuesBefore;
    TZrSize blocksBefore;
    TZrSize callCount = 0U;
    TZrSize payloadCount = 0U;
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
    blocksBefore = function->cfg.blocks.length;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(
            instructionsBefore, function->instructions.length);
    TEST_ASSERT_EQUAL_UINT64(valuesBefore, function->values.length);
    TEST_ASSERT_EQUAL_UINT64(blocksBefore, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_PARSER_CFG_INVALID_BLOCK_ID,
            compiler.preSemanticIrCfgCatchBlock);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    TEST_ASSERT_EQUAL_UINT64(0U, payloadCount);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_late_call_fallback_discards_catch_binding_read(void) {
    static const TZrChar source[] =
            "try { unresolved(); }\n"
            "catch (error) { error; }\n";
    static TZrChar sourceName[] = "late_call_fallback_catch_read.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrString *callableName = ZrCore_String_CreateFromNative(
            g_state, "unresolved");
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize payloadCount = 0U;
    TZrSize catchLoadCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_NOT_EQUAL(
            ZR_PARSER_SLOT_NONE,
            allocate_local_var(&compiler, callableName));
    for (index = 0U; index < ast->data.script.statements->count; index++) {
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
        if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadCount++;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   instruction->sourceRange.start.line == 2U) {
            catchLoadCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, payloadCount);
    TEST_ASSERT_EQUAL_UINT64(0U, catchLoadCount);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_PARSER_CFG_INVALID_BLOCK_ID,
            compiler.preSemanticIrCfgCatchBlock);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unmodeled_try_scope_does_not_start_detached_call_cfg(void) {
    static const TZrChar inactiveSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error) { seed = 8; }\n"
            "finally { seed = 9; }\n";
    static TZrChar inactiveSourceName[] = "try_call_cfg_fallback.zr";
    static const TZrChar activeSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) { seed = 8; }\n"
            "try { identity(seed); }\n"
            "catch (error) { seed = 9; }\n";
    static TZrChar activeSourceName[] = "try_abandons_active_cfg.zr";
    static const TZrChar trailingCallSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { seed = 8; }\n"
            "catch (error) { seed = 9; }\n"
            "identity(seed);\n";
    static TZrChar trailingCallSourceName[] =
            "call_after_try_cfg_fallback.zr";
    static const TZrChar nestedTrailingCallSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) {\n"
            "    try { seed = 8; }\n"
            "    catch (error) { seed = 9; }\n"
            "}\n"
            "identity(seed);\n";
    static TZrChar nestedTrailingCallSourceName[] =
            "call_after_nested_try_cfg_fallback.zr";
    static const TZrChar nonemptyCatchSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error) { seed = 8; }\n"
            "identity(seed);\n";
    static TZrChar nonemptyCatchSourceName[] =
            "nonempty_catch_cfg_fallback.zr";
    static const TZrChar typedCatchSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error: object) { seed = 8; }\n"
            "identity(seed);\n";
    static TZrChar typedCatchSourceName[] =
            "typed_catch_cfg_fallback.zr";
    static const TZrChar unresolvedTypedCatchSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error: MissingCatchType) {}\n"
            "identity(seed);\n";
    static TZrChar unresolvedTypedCatchSourceName[] =
            "unresolved_typed_catch_cfg_fallback.zr";
    static const TZrChar arrayTypedCatchSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (error: string[]) {}\n"
            "identity(seed);\n";
    static TZrChar arrayTypedCatchSourceName[] =
            "array_typed_catch_cfg_fallback.zr";
    static const TZrChar multipleCatchSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(seed); }\n"
            "catch (first) {}\n"
            "catch (second) {}\n"
            "identity(seed);\n";
    static TZrChar multipleCatchSourceName[] =
            "multiple_catch_cfg_fallback.zr";
    static const TZrChar emptyCatchWithoutCallSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { seed = 8; }\n"
            "catch (error) {}\n"
            "identity(seed);\n";
    static TZrChar emptyCatchWithoutCallSourceName[] =
            "empty_catch_without_call_cfg_fallback.zr";
    static const TZrChar nestedArgumentCallSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "try { identity(identity(seed)); }\n"
            "catch (error) {}\n"
            "identity(seed);\n";
    static TZrChar nestedArgumentCallSourceName[] =
            "nested_argument_call_cfg_fallback.zr";
    static const TZrChar multipleArgumentCallSource[] =
            "fn first(left: int, right: int): int { return left; }\n"
            "var seed: int = 7;\n"
            "try { first(seed, seed); }\n"
            "catch (error) {}\n";
    static TZrChar multipleArgumentCallSourceName[] =
            "multiple_argument_call_cfg_fallback.zr";
    static const TZrChar convertedArgumentCallSource[] =
            "fn widen(value: float): float { return value; }\n"
            "var seed: int = 7;\n"
            "try { widen(seed); }\n"
            "catch (error) {}\n";
    static TZrChar convertedArgumentCallSourceName[] =
            "converted_argument_call_cfg_fallback.zr";
    static const TZrChar typedHandlerLocalSource[] =
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed: object = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar typedHandlerLocalSourceName[] =
            "typed_handler_local_cfg_fallback.zr";
    static const TZrChar nonpropagatingHandlerLocalSource[] =
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  error;\n"
            "}\n";
    static TZrChar nonpropagatingHandlerLocalSourceName[] =
            "nonpropagating_handler_local_cfg_fallback.zr";
    static const TZrChar shadowedCatchBindingSource[] =
            "fn identity(): int { return 7; }\n"
            "var error: int = 1;\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar shadowedCatchBindingSourceName[] =
            "shadowed_catch_binding_cfg_fallback.zr";
    static const TZrChar shadowedHandlerLocalSource[] =
            "fn identity(): int { return 7; }\n"
            "var observed: int = 1;\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar shadowedHandlerLocalSourceName[] =
            "shadowed_handler_local_cfg_fallback.zr";
    static const TZrChar callableCatchBindingSource[] =
            "fn error(): int { return 7; }\n"
            "try { error(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar callableCatchBindingSourceName[] =
            "callable_catch_binding_cfg_fallback.zr";
    static const TZrChar callableHandlerLocalSource[] =
            "fn identity(): int { return 7; }\n"
            "fn observed(): int { return 8; }\n"
            "try { identity(); }\n"
            "catch (error) {\n"
            "  var observed = error;\n"
            "  observed;\n"
            "}\n";
    static TZrChar callableHandlerLocalSourceName[] =
            "callable_handler_local_cfg_fallback.zr";

    assert_nested_call_keeps_legacy_cfg(
            inactiveSource,
            sizeof(inactiveSource) - 1U,
            inactiveSourceName,
            sizeof(inactiveSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            activeSource,
            sizeof(activeSource) - 1U,
            activeSourceName,
            sizeof(activeSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            trailingCallSource,
            sizeof(trailingCallSource) - 1U,
            trailingCallSourceName,
            sizeof(trailingCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            nestedTrailingCallSource,
            sizeof(nestedTrailingCallSource) - 1U,
            nestedTrailingCallSourceName,
            sizeof(nestedTrailingCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            nonemptyCatchSource,
            sizeof(nonemptyCatchSource) - 1U,
            nonemptyCatchSourceName,
            sizeof(nonemptyCatchSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            typedCatchSource,
            sizeof(typedCatchSource) - 1U,
            typedCatchSourceName,
            sizeof(typedCatchSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            unresolvedTypedCatchSource,
            sizeof(unresolvedTypedCatchSource) - 1U,
            unresolvedTypedCatchSourceName,
            sizeof(unresolvedTypedCatchSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            arrayTypedCatchSource,
            sizeof(arrayTypedCatchSource) - 1U,
            arrayTypedCatchSourceName,
            sizeof(arrayTypedCatchSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            multipleCatchSource,
            sizeof(multipleCatchSource) - 1U,
            multipleCatchSourceName,
            sizeof(multipleCatchSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            emptyCatchWithoutCallSource,
            sizeof(emptyCatchWithoutCallSource) - 1U,
            emptyCatchWithoutCallSourceName,
            sizeof(emptyCatchWithoutCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            nestedArgumentCallSource,
            sizeof(nestedArgumentCallSource) - 1U,
            nestedArgumentCallSourceName,
            sizeof(nestedArgumentCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            multipleArgumentCallSource,
            sizeof(multipleArgumentCallSource) - 1U,
            multipleArgumentCallSourceName,
            sizeof(multipleArgumentCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            convertedArgumentCallSource,
            sizeof(convertedArgumentCallSource) - 1U,
            convertedArgumentCallSourceName,
            sizeof(convertedArgumentCallSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            typedHandlerLocalSource,
            sizeof(typedHandlerLocalSource) - 1U,
            typedHandlerLocalSourceName,
            sizeof(typedHandlerLocalSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            nonpropagatingHandlerLocalSource,
            sizeof(nonpropagatingHandlerLocalSource) - 1U,
            nonpropagatingHandlerLocalSourceName,
            sizeof(nonpropagatingHandlerLocalSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            shadowedCatchBindingSource,
            sizeof(shadowedCatchBindingSource) - 1U,
            shadowedCatchBindingSourceName,
            sizeof(shadowedCatchBindingSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            shadowedHandlerLocalSource,
            sizeof(shadowedHandlerLocalSource) - 1U,
            shadowedHandlerLocalSourceName,
            sizeof(shadowedHandlerLocalSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            callableCatchBindingSource,
            sizeof(callableCatchBindingSource) - 1U,
            callableCatchBindingSourceName,
            sizeof(callableCatchBindingSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            callableHandlerLocalSource,
            sizeof(callableHandlerLocalSource) - 1U,
            callableHandlerLocalSourceName,
            sizeof(callableHandlerLocalSourceName) - 1U);
}
