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
            "catch (error: object) {}\n"
            "identity(seed);\n";
    static TZrChar typedCatchSourceName[] =
            "typed_catch_cfg_fallback.zr";
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
}
