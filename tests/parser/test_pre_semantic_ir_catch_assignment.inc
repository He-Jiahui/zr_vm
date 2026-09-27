static void test_source_catch_reads_preinvoke_assignment_value(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var result: int = 1;\n"
            "try { result = identity(); }\n"
            "catch (error) { result; }\n"
            "result;\n";
    static TZrChar sourceName[] = "source_catch_interrupted_assignment.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *call = ZR_NULL;
    const SZrSemanticIrInstruction *initialization = ZR_NULL;
    const SZrSemanticIrInstruction *assignment = ZR_NULL;
    const SZrSemanticIrInstruction *catchRead = ZR_NULL;
    const SZrSemanticIrInstruction *joinRead = ZR_NULL;
    const SZrParserCfgBlock *callBlock = ZR_NULL;
    const SZrParserCfgBlock *assignmentBlock = ZR_NULL;
    const SZrParserCfgBlock *catchBlock = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 catchReadCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        if (ast->data.script.statements->nodes[index]->type !=
            ZR_AST_FUNCTION_DECLARATION) {
            ZrParser_Statement_Compile(
                    &compiler, ast->data.script.statements->nodes[index]);
        }
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->sourceRange.start.line == 2U &&
            instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE) {
            initialization = instruction;
        } else if (instruction->sourceRange.start.line == 3U &&
                   instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            call = instruction;
        } else if (instruction->sourceRange.start.line == 3U &&
                   instruction->opcode == ZR_SEMANTIC_IR_STORE) {
            assignment = instruction;
        } else if (instruction->sourceRange.start.line == 4U &&
                   instruction->opcode == ZR_SEMANTIC_IR_LOAD) {
            catchRead = instruction;
        } else if (instruction->sourceRange.start.line == 5U &&
                   instruction->opcode == ZR_SEMANTIC_IR_LOAD) {
            joinRead = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(initialization);
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_NOT_NULL(assignment);
    TEST_ASSERT_NOT_NULL(catchRead);
    TEST_ASSERT_NOT_NULL(joinRead);
    TEST_ASSERT_EQUAL_UINT32(initialization->placeId, assignment->placeId);
    TEST_ASSERT_EQUAL_UINT32(assignment->placeId, catchRead->placeId);
    TEST_ASSERT_EQUAL_UINT32(assignment->placeId, joinRead->placeId);
    TEST_ASSERT_EQUAL_UINT32(call->resultValueId, assignment->valueId);
    for (index = 0U; index < function->cfg.blocks.length; ++index) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (call->id - 1U >= block->firstInstructionIndex &&
            call->id - 1U < block->firstInstructionIndex + block->instructionCount)
            callBlock = block;
        if (assignment->id - 1U >= block->firstInstructionIndex &&
            assignment->id - 1U < block->firstInstructionIndex + block->instructionCount)
            assignmentBlock = block;
        if (catchRead->id - 1U >= block->firstInstructionIndex &&
            catchRead->id - 1U < block->firstInstructionIndex + block->instructionCount)
            catchBlock = block;
    }
    TEST_ASSERT_NOT_NULL(callBlock);
    TEST_ASSERT_NOT_NULL(assignmentBlock);
    TEST_ASSERT_NOT_NULL(catchBlock);
    TEST_ASSERT_NOT_EQUAL(callBlock->id, assignmentBlock->id);
    TEST_ASSERT_NOT_EQUAL(callBlock->id, catchBlock->id);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(callBlock, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(assignmentBlock->id,
            ZrParser_Cfg_BlockEdgeAt(callBlock, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_EXCEPTION,
            ZrParser_Cfg_BlockEdgeAt(callBlock, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(catchBlock->id,
            ZrParser_Cfg_BlockEdgeAt(callBlock, 1U)->toBlockId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        if (instruction->sourceId != catchRead->id) continue;
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_OPCODE_COPY, instruction->opcode);
        TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
        TEST_ASSERT_EQUAL_UINT32(
                initialization->valueId,
                output.operands[instruction->operands.start]);
        TEST_ASSERT_NOT_EQUAL(
                call->resultValueId,
                output.operands[instruction->operands.start]);
        ++catchReadCount;
    }
    TEST_ASSERT_EQUAL_UINT32(1U, catchReadCount);
    {
        const SZrExecIrPhi *joinPhi = ZR_NULL;
        for (index = 0U; index < output.instructionCount; ++index) {
            const SZrExecIrInstruction *instruction = &output.instructions[index];
            if (instruction->sourceId != joinRead->id) continue;
            TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_OPCODE_COPY, instruction->opcode);
            TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
            for (TZrUInt32 phiIndex = 0U; phiIndex < output.phiCount; ++phiIndex) {
                if (output.phiPool[phiIndex].result ==
                    output.operands[instruction->operands.start]) {
                    joinPhi = &output.phiPool[phiIndex];
                }
            }
        }
        TEST_ASSERT_NOT_NULL(joinPhi);
        TEST_ASSERT_EQUAL_UINT32(2U, joinPhi->incomings.count);
        TEST_ASSERT_EQUAL_UINT32(catchBlock->id + 1U,
                output.phiIncoming[joinPhi->incomings.start].predecessor);
        TEST_ASSERT_EQUAL_UINT32(initialization->valueId,
                output.phiIncoming[joinPhi->incomings.start].value);
        TEST_ASSERT_EQUAL_UINT32(assignmentBlock->id + 1U,
                output.phiIncoming[joinPhi->incomings.start + 1U].predecessor);
        TEST_ASSERT_EQUAL_UINT32(call->resultValueId,
                output.phiIncoming[joinPhi->incomings.start + 1U].value);
    }
    ZrCore_ExecIr_FreeFunction(&output);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_compound_assignment_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "fn identity(): int { return 7; }\n"
            "var result: int = 1;\n"
            "try { result += identity(); }\n"
            "catch (error) { result; }\n";
    static TZrChar sourceName[] = "source_catch_compound_assignment.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        if (ast->data.script.statements->nodes[index]->type !=
            ZR_AST_FUNCTION_DECLARATION) {
            ZrParser_Statement_Compile(
                    &compiler, ast->data.script.statements->nodes[index]);
        }
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD,
                instruction->opcode);
    }
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
