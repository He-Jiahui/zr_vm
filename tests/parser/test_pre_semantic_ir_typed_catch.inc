static void test_source_single_typed_catch_emits_type_dispatch_cfg(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error: string) { error; }\n"
            "identity();\n";
    static TZrChar sourceName[] = "source_single_typed_catch_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *protectedCall = ZR_NULL;
    const SZrSemanticIrInstruction *trailingCall = ZR_NULL;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *typeTest = ZR_NULL;
    const SZrSemanticIrInstruction *initialize = ZR_NULL;
    const SZrSemanticIrInstruction *catchLoad = ZR_NULL;
    const SZrSemanticIrInstruction *unmatchedThrow = ZR_NULL;
    const SZrParserCfgBlock *invokeBlock = ZR_NULL;
    const SZrParserCfgBlock *trailingInvokeBlock = ZR_NULL;
    const SZrParserCfgBlock *dispatchBlock = ZR_NULL;
    const SZrParserCfgBlock *handlerBlock = ZR_NULL;
    const SZrParserCfgBlock *unmatchedBlock = ZR_NULL;
    const SZrParserCfgEdge *exceptionEdge = ZR_NULL;
    const SZrParserCfgEdge *matchEdge = ZR_NULL;
    const SZrParserCfgEdge *unmatchedEdge = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize execTypeTestCount = 0U;
    TZrSize execThrowCount = 0U;
    TZrSize callCount = 0U;
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
            if (protectedCall == ZR_NULL) {
                protectedCall = instruction;
            } else {
                trailingCall = instruction;
            }
            callCount++;
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            TEST_ASSERT_NULL(payload);
            payload = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_TYPE_TEST) {
            TEST_ASSERT_NULL(typeTest);
            typeTest = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
                   payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId) {
            initialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   initialize != ZR_NULL &&
                   instruction->placeId == initialize->placeId) {
            catchLoad = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_THROW) {
            TEST_ASSERT_NULL(unmatchedThrow);
            unmatchedThrow = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(2U, callCount);
    TEST_ASSERT_NOT_NULL(protectedCall);
    TEST_ASSERT_NOT_NULL(trailingCall);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(typeTest);
    TEST_ASSERT_NOT_NULL(initialize);
    TEST_ASSERT_NOT_NULL(catchLoad);
    TEST_ASSERT_NOT_NULL(unmatchedThrow);
    TEST_ASSERT_EQUAL_UINT32(payload->resultValueId, typeTest->valueId);
    TEST_ASSERT_EQUAL_UINT32(0U, typeTest->operandCount);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, typeTest->matchTypeId);
    TEST_ASSERT_NOT_EQUAL(payload->typeId, typeTest->matchTypeId);
    TEST_ASSERT_NOT_EQUAL(typeTest->typeId, typeTest->matchTypeId);
    TEST_ASSERT_EQUAL_UINT32(typeTest->matchTypeId, initialize->typeId);
    TEST_ASSERT_EQUAL_UINT32(initialize->typeId, catchLoad->typeId);
    TEST_ASSERT_EQUAL_UINT32(1U, unmatchedThrow->operandCount);
    TEST_ASSERT_EQUAL_UINT32(
            payload->resultValueId,
            *(const TZrValueId *)ZrCore_Array_Get(
                    (SZrArray *)&function->valueOperands,
                    unmatchedThrow->operandStart));

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (protectedCall->id - 1U >= block->firstInstructionIndex &&
            protectedCall->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            invokeBlock = block;
        }
        if (trailingCall->id - 1U >= block->firstInstructionIndex &&
            trailingCall->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            trailingInvokeBlock = block;
        }
        if (typeTest->id - 1U >= block->firstInstructionIndex &&
            typeTest->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            dispatchBlock = block;
        }
        if (initialize->id - 1U >= block->firstInstructionIndex &&
            initialize->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            handlerBlock = block;
        }
        if (unmatchedThrow->id - 1U >= block->firstInstructionIndex &&
            unmatchedThrow->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            unmatchedBlock = block;
        }
    }
    TEST_ASSERT_NOT_NULL(invokeBlock);
    TEST_ASSERT_NOT_NULL(trailingInvokeBlock);
    TEST_ASSERT_NOT_NULL(dispatchBlock);
    TEST_ASSERT_NOT_NULL(handlerBlock);
    TEST_ASSERT_NOT_NULL(unmatchedBlock);
    TEST_ASSERT_NOT_EQUAL(dispatchBlock->id, handlerBlock->id);
    TEST_ASSERT_NOT_EQUAL(dispatchBlock->id, unmatchedBlock->id);
    TEST_ASSERT_NOT_EQUAL(handlerBlock->id, unmatchedBlock->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH,
            dispatchBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, dispatchBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW,
            unmatchedBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, unmatchedBlock->successorCount);

    for (index = 0U; index < invokeBlock->successorCount; index++) {
        const SZrParserCfgEdge *edge =
                ZrParser_Cfg_BlockEdgeAt(invokeBlock, index);
        TEST_ASSERT_NOT_NULL(edge);
        if (edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION) {
            exceptionEdge = edge;
        }
    }
    for (index = 0U; index < dispatchBlock->successorCount; index++) {
        const SZrParserCfgEdge *edge =
                ZrParser_Cfg_BlockEdgeAt(dispatchBlock, index);
        TEST_ASSERT_NOT_NULL(edge);
        if (edge->kind == ZR_PARSER_CFG_EDGE_TRUE_BRANCH) {
            matchEdge = edge;
        } else if (edge->kind == ZR_PARSER_CFG_EDGE_FALSE_BRANCH) {
            unmatchedEdge = edge;
        }
    }
    TEST_ASSERT_NOT_NULL(exceptionEdge);
    TEST_ASSERT_NOT_NULL(matchEdge);
    TEST_ASSERT_NOT_NULL(unmatchedEdge);
    TEST_ASSERT_EQUAL_UINT32(dispatchBlock->id, exceptionEdge->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(handlerBlock->id, matchEdge->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(unmatchedBlock->id, unmatchedEdge->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, trailingInvokeBlock->successorCount);
    for (index = 0U; index < trailingInvokeBlock->successorCount; index++) {
        const SZrParserCfgEdge *edge =
                ZrParser_Cfg_BlockEdgeAt(trailingInvokeBlock, index);
        TEST_ASSERT_NOT_NULL(edge);
        if (edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION) {
            foundTrailingException = ZR_TRUE;
            TEST_ASSERT_NOT_EQUAL(dispatchBlock->id, edge->toBlockId);
        }
    }
    TEST_ASSERT_TRUE(foundTrailingException);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        if ((EZrExecIrOpcode)instruction->opcode ==
            ZR_EXEC_IR_OPCODE_TYPE_TEST) {
            execTypeTestCount++;
            TEST_ASSERT_EQUAL_UINT32(
                    typeTest->matchTypeId,
                    instruction->matchTypeToken);
            TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
            TEST_ASSERT_EQUAL_UINT32(
                    payload->resultValueId,
                    output.operands[instruction->operands.start]);
        } else if ((EZrExecIrOpcode)instruction->opcode ==
                   ZR_EXEC_IR_OPCODE_THROW) {
            execThrowCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, execTypeTestCount);
    TEST_ASSERT_EQUAL_UINT64(1U, execThrowCount);
    TEST_ASSERT_TRUE(
            (output.blocks[dispatchBlock->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE(
            (output.blocks[handlerBlock->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    TEST_ASSERT_FALSE(
            (output.blocks[unmatchedBlock->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_named_typed_catch_uses_type_dispatch_cfg(void) {
    static const TZrChar source[] =
            "class Fault {}\n"
            "fn identity(): int { return 7; }\n"
            "try { identity(); }\n"
            "catch (error: Fault) { error; }\n";
    static TZrChar sourceName[] = "source_named_typed_catch_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *typeTest = ZR_NULL;
    TZrSize typeTestCount = 0U;
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
        SZrAstNode *statement =
                ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_TYPE_TEST) {
            typeTest = instruction;
            typeTestCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, typeTestCount);
    TEST_ASSERT_NOT_NULL(typeTest);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, typeTest->matchTypeId);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
