static void assert_source_catch_direct_abrupt_keeps_normal_continuation(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar *sourceName,
        TZrSize sourceNameLength,
        EZrSemanticIrOpcode abruptOpcode,
        EZrParserCfgTerminatorKind abruptTerminator,
        TZrBool abruptUsesCatchBinding) {
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sourceNameLength);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sourceLength, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *protectedCall = ZR_NULL;
    const SZrSemanticIrInstruction *suffixCall = ZR_NULL;
    const SZrSemanticIrInstruction *payload = ZR_NULL;
    const SZrSemanticIrInstruction *catchInitialize = ZR_NULL;
    const SZrSemanticIrInstruction *catchLoad = ZR_NULL;
    const SZrSemanticIrInstruction *abrupt = ZR_NULL;
    const SZrParserCfgBlock *protectedCallBlock = ZR_NULL;
    const SZrParserCfgBlock *handlerBlock = ZR_NULL;
    const SZrParserCfgBlock *joinBlock = ZR_NULL;
    const SZrParserCfgBlock *suffixCallBlock = ZR_NULL;
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
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgTerminated);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED &&
            instruction->sourceRange.start.line == 2U) {
            protectedCall = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED &&
                   instruction->sourceRange.start.line == 4U) {
            suffixCall = instruction;
        } else if (instruction->opcode ==
                   ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payload = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
                   payload != ZR_NULL &&
                   instruction->valueId == payload->resultValueId) {
            catchInitialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   catchInitialize != ZR_NULL &&
                   instruction->placeId == catchInitialize->placeId) {
            catchLoad = instruction;
        } else if (instruction->opcode == abruptOpcode &&
                   instruction->sourceRange.start.line == 3U) {
            abrupt = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(protectedCall);
    TEST_ASSERT_NOT_NULL(suffixCall);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_NOT_NULL(catchInitialize);
    TEST_ASSERT_NOT_NULL(abrupt);
    if (abruptUsesCatchBinding) {
        const TZrValueId *operand;

        TEST_ASSERT_NOT_NULL(catchLoad);
        TEST_ASSERT_EQUAL_UINT32(1U, abrupt->operandCount);
        operand = (const TZrValueId *)ZrCore_Array_Get(
                (SZrArray *)&function->valueOperands,
                abrupt->operandStart);
        TEST_ASSERT_NOT_NULL(operand);
        TEST_ASSERT_EQUAL_UINT32(catchLoad->resultValueId, *operand);
        TEST_ASSERT_TRUE(catchLoad->id < abrupt->id);
    }

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);

        TEST_ASSERT_NOT_NULL(block);
        if (protectedCall->id - 1U >= block->firstInstructionIndex &&
            protectedCall->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            protectedCallBlock = block;
        }
        if (payload->id - 1U >= block->firstInstructionIndex &&
            payload->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            handlerBlock = block;
        }
        if (suffixCall->id - 1U >= block->firstInstructionIndex &&
            suffixCall->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            suffixCallBlock = block;
        }
        if (block->kind == ZR_PARSER_CFG_BLOCK_JOIN) {
            TEST_ASSERT_NULL(joinBlock);
            joinBlock = block;
        }
    }
    TEST_ASSERT_NOT_NULL(protectedCallBlock);
    TEST_ASSERT_NOT_NULL(handlerBlock);
    TEST_ASSERT_NOT_NULL(joinBlock);
    TEST_ASSERT_NOT_NULL(suffixCallBlock);
    TEST_ASSERT_NOT_EQUAL(handlerBlock->id, suffixCallBlock->id);
    TEST_ASSERT_EQUAL_UINT32(2U, protectedCallBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_EXCEPTION,
            ZrParser_Cfg_BlockEdgeAt(protectedCallBlock, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            handlerBlock->id,
            ZrParser_Cfg_BlockEdgeAt(protectedCallBlock, 1U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(abruptTerminator, handlerBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, handlerBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, joinBlock->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, joinBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            suffixCallBlock->id,
            ZrParser_Cfg_BlockEdgeAt(joinBlock, 0U)->toBlockId);
    TEST_ASSERT_TRUE(
            catchInitialize->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            catchInitialize->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);
    TEST_ASSERT_TRUE(abrupt->id - 1U >= handlerBlock->firstInstructionIndex);
    TEST_ASSERT_TRUE(
            abrupt->id - 1U <
            handlerBlock->firstInstructionIndex + handlerBlock->instructionCount);

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

static void test_source_catch_direct_rethrow_keeps_normal_continuation(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "try { may_fail(); }\n"
            "catch (error) { throw error; }\n"
            "may_fail();\n";
    static TZrChar sourceName[] = "source_catch_direct_rethrow.zr";

    assert_source_catch_direct_abrupt_keeps_normal_continuation(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U,
            ZR_SEMANTIC_IR_THROW,
            ZR_PARSER_CFG_TERMINATOR_THROW,
            ZR_TRUE);
}

static void test_source_catch_direct_return_keeps_normal_continuation(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "try { may_fail(); }\n"
            "catch (error) { return 9; }\n"
            "may_fail();\n";
    static TZrChar sourceName[] = "source_catch_direct_return.zr";

    assert_source_catch_direct_abrupt_keeps_normal_continuation(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U,
            ZR_SEMANTIC_IR_RETURN,
            ZR_PARSER_CFG_TERMINATOR_RETURN,
            ZR_FALSE);
}

static void test_source_catch_binding_return_keeps_normal_continuation(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "try { may_fail(); }\n"
            "catch (error) { return error; }\n"
            "may_fail();\n";
    static TZrChar sourceName[] = "source_catch_binding_return.zr";

    assert_source_catch_direct_abrupt_keeps_normal_continuation(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U,
            ZR_SEMANTIC_IR_RETURN,
            ZR_PARSER_CFG_TERMINATOR_RETURN,
            ZR_TRUE);
}

static void assert_source_catch_unsupported_abrupt_keeps_fallback(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar *sourceName,
        TZrSize sourceNameLength) {
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sourceNameLength);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sourceLength, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
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
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, payloadCount);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_abrupt_with_trailing_syntax_keeps_fallback(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "may_fail();\n"
            "try { may_fail(); }\n"
            "catch (error) { return 9; error; }\n"
            "may_fail();\n";
    static TZrChar sourceName[] = "source_catch_abrupt_trailing_fallback.zr";

    assert_source_catch_unsupported_abrupt_keeps_fallback(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U);
}

static void test_source_catch_noncanonical_return_keeps_fallback(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "try { may_fail(); }\n"
            "catch (error) { return zr; }\n"
            "may_fail();\n";
    static TZrChar sourceName[] = "source_catch_global_return_fallback.zr";

    assert_source_catch_unsupported_abrupt_keeps_fallback(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U);
}

static void assert_source_catch_abrupt_with_outer_cleanup_keeps_fallback(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar *sourceName,
        TZrSize sourceNameLength) {
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sourceNameLength);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sourceLength, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
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
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD) {
            payloadCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, payloadCount);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_catch_direct_return_with_outer_cleanup_keeps_fallback(
        void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "using (\"resource\") {\n"
            "  try { may_fail(); }\n"
            "  catch (error) { return 9; }\n"
            "}\n";
    static TZrChar sourceName[] = "source_catch_return_cleanup_fallback.zr";

    assert_source_catch_abrupt_with_outer_cleanup_keeps_fallback(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U);
}

static void test_source_catch_rethrow_with_outer_cleanup_keeps_fallback(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "using (\"resource\") {\n"
            "  try { may_fail(); }\n"
            "  catch (error) { throw error; }\n"
            "}\n";
    static TZrChar sourceName[] = "source_catch_rethrow_cleanup_fallback.zr";

    assert_source_catch_abrupt_with_outer_cleanup_keeps_fallback(
            source,
            sizeof(source) - 1U,
            sourceName,
            sizeof(sourceName) - 1U);
}

static void test_declared_child_catch_return_uses_disposable_fallback(void) {
    static const TZrChar source[] =
            "fn may_fail(): int { return 7; }\n"
            "fn child(): int {\n"
            "  try { may_fail(); }\n"
            "  catch (error) { return 9; }\n"
            "  return 1;\n"
            "}\n"
            "may_fail();\n";
    static TZrChar sourceName[] = "child_catch_return_isolation.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    SZrAstNode *childFunction;
    SZrAstNode *childTry;
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
    childFunction = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(childFunction);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_DECLARATION, childFunction->type);
    TEST_ASSERT_NOT_NULL(childFunction->data.functionDeclaration.body);
    TEST_ASSERT_NOT_NULL(
            childFunction->data.functionDeclaration.body->data.block.body);
    childTry = childFunction->data.functionDeclaration.body->data.block.body
                       ->nodes[0];
    TEST_ASSERT_NOT_NULL(childTry);
    compiler.currentFunctionNode = childFunction;
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(
            &compiler, childTry));
    compiler.currentFunctionNode = ZR_NULL;
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
