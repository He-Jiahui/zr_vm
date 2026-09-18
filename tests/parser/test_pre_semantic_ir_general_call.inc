static void test_source_resolved_function_call_emits_typed_invoke_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "var result: int = identity(seed);\n";
    static TZrChar sourceName[] = "resolved_function_call_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrSemanticSymbolRecord *identitySymbol;
    const SZrSemanticIrInstruction *callInstruction = ZR_NULL;
    const SZrParserCfgBlock *invokeBlock = ZR_NULL;
    const SZrParserCfgBlock *normalBlock = ZR_NULL;
    const SZrParserCfgBlock *exceptionBlock = ZR_NULL;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize callCount = 0U;
    TZrSize invokeCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    identitySymbol = ZrParser_Semantic_FindSymbolByNameAndKind(
            compiler.semanticContext,
            ast->data.script.statements->nodes[0]
                    ->data.functionDeclaration.name->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION);
    TEST_ASSERT_NOT_NULL(identitySymbol);
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
            callInstruction = instruction;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    TEST_ASSERT_NOT_NULL(callInstruction);
    TEST_ASSERT_EQUAL_UINT32(identitySymbol->id, callInstruction->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, callInstruction->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, callInstruction->resultValueId);
    TEST_ASSERT_EQUAL_UINT32(2U, callInstruction->operandCount);

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TEST_ASSERT_NOT_NULL(block);
        if (callInstruction->id - 1U >= block->firstInstructionIndex &&
            callInstruction->id - 1U <
                    block->firstInstructionIndex + block->instructionCount) {
            invokeBlock = block;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(invokeBlock);
    TEST_ASSERT_EQUAL_UINT32(2U, invokeBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(invokeBlock, 0U)->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_EXCEPTION,
            ZrParser_Cfg_BlockEdgeAt(invokeBlock, 1U)->kind);
    normalBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(invokeBlock, 0U)->toBlockId);
    exceptionBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(invokeBlock, 1U)->toBlockId);
    TEST_ASSERT_NOT_NULL(normalBlock);
    TEST_ASSERT_NOT_NULL(exceptionBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW,
            exceptionBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, exceptionBlock->instructionCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        if ((EZrExecIrOpcode)output.instructions[index].opcode ==
            ZR_EXEC_IR_OPCODE_INVOKE) {
            invokeCount++;
            TEST_ASSERT_EQUAL_UINT32(
                    2U, output.instructions[index].successorRange.count);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, invokeCount);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void assert_nested_call_keeps_legacy_cfg(
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
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_nested_calls_do_not_restart_abandoned_source_cfg(void) {
    static const TZrChar ifSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) { identity(seed); }\n";
    static TZrChar ifSourceName[] = "nested_resolved_call_if_fallback.zr";
    static const TZrChar whileSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "while (false) { identity(seed); }\n";
    static TZrChar whileSourceName[] =
            "nested_resolved_call_while_fallback.zr";
    static const TZrChar logicalSource[] =
            "fn predicate(value: bool): bool { return value; }\n"
            "var seed: bool = true;\n"
            "var result: bool = false && predicate(seed);\n";
    static TZrChar logicalSourceName[] =
            "nested_resolved_call_logical_fallback.zr";
    static const TZrChar intermediateCfgSource[] =
            "fn identity(value: int): int { return value; }\n"
            "var seed: int = 7;\n"
            "if (true) {\n"
            "    if (true) { seed = 8; }\n"
            "    identity(seed);\n"
            "}\n";
    static TZrChar intermediateCfgSourceName[] =
            "nested_resolved_call_intermediate_cfg_fallback.zr";

    assert_nested_call_keeps_legacy_cfg(
            ifSource,
            sizeof(ifSource) - 1U,
            ifSourceName,
            sizeof(ifSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            whileSource,
            sizeof(whileSource) - 1U,
            whileSourceName,
            sizeof(whileSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            logicalSource,
            sizeof(logicalSource) - 1U,
            logicalSourceName,
            sizeof(logicalSourceName) - 1U);
    assert_nested_call_keeps_legacy_cfg(
            intermediateCfgSource,
            sizeof(intermediateCfgSource) - 1U,
            intermediateCfgSourceName,
            sizeof(intermediateCfgSourceName) - 1U);
}
