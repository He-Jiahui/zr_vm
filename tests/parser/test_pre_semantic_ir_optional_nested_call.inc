static void test_source_optional_call_skips_nested_argument_invoke(void) {
    static const TZrChar source[] =
            "resource class Service {\n"
            "    pub const fn consume(value: bool): void {}\n"
            "}\n"
            "fn sideEffect(): bool { return true; }\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var receiver = wake(weak);\n"
            "receiver?.consume(sideEffect());\n";
    static TZrChar sourceName[] = "optional_nested_call_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semantic;
    const SZrSemanticSymbolRecord *sideEffectSymbol;
    TZrSymbolId sideEffectSymbolId;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *present;
    const SZrParserCfgBlock *join;
    const SZrParserCfgBlock *argumentInvoke = ZR_NULL;
    const SZrParserCfgBlock *outerInvoke = ZR_NULL;
    const SZrParserCfgBlock *argumentNormal;
    const SZrParserCfgBlock *outerNormal;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 invokeCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_PredeclareFunctionBindings(
            &compiler, ast->data.script.statements);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    sideEffectSymbol = ZrParser_Semantic_FindSymbolByNameAndKind(
            compiler.semanticContext,
            ast->data.script.statements->nodes[1]
                    ->data.functionDeclaration.name->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION);
    TEST_ASSERT_NOT_NULL(sideEffectSymbol);
    sideEffectSymbolId = sideEffectSymbol->id;
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else if (statement->type != ZR_AST_FUNCTION_DECLARATION) {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    }
    TEST_ASSERT_TRUE_MESSAGE(compiler.preSemanticIrCfgActive,
                             "nested optional argument abandoned source CFG");
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semantic);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&semantic->cfg.blocks, semantic->cfg.entryBlockId);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(2U, entry->successorCount);
    present = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&semantic->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->toBlockId);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&semantic->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->toBlockId);
    TEST_ASSERT_NOT_NULL(present);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(1U, present->successorCount);
    for (index = 0U; index < semantic->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semantic, index);
        TZrUInt32 blockIndex;
        if (instruction->opcode != ZR_SEMANTIC_IR_CALL_TYPED &&
            instruction->opcode != ZR_SEMANTIC_IR_CALL_VIRTUAL &&
            instruction->opcode != ZR_SEMANTIC_IR_CALL_DYNAMIC &&
            instruction->opcode != ZR_SEMANTIC_IR_CALL_META) continue;
        for (blockIndex = 0U; blockIndex < semantic->cfg.blocks.length;
             ++blockIndex) {
            const SZrParserCfgBlock *block =
                    (const SZrParserCfgBlock *)ZrCore_Array_Get(
                            (SZrArray *)&semantic->cfg.blocks, blockIndex);
            if (index >= block->firstInstructionIndex &&
                index < block->firstInstructionIndex + block->instructionCount) {
                TEST_ASSERT_EQUAL_UINT32(9U,
                        instruction->sourceRange.start.line);
                if (instruction->symbolId == sideEffectSymbolId) {
                    TEST_ASSERT_NULL(argumentInvoke);
                    argumentInvoke = block;
                } else {
                    TEST_ASSERT_NULL(outerInvoke);
                    outerInvoke = block;
                }
            }
        }
    }
    TEST_ASSERT_NOT_NULL(argumentInvoke);
    TEST_ASSERT_NOT_NULL(outerInvoke);
    TEST_ASSERT_EQUAL_UINT32(argumentInvoke->id,
            ZrParser_Cfg_BlockEdgeAt(present, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, argumentInvoke->successorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, outerInvoke->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(argumentInvoke, 0U)->kind);
    argumentNormal = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&semantic->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(argumentInvoke, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(argumentNormal);
    TEST_ASSERT_EQUAL_UINT32(outerInvoke->id,
            ZrParser_Cfg_BlockEdgeAt(argumentNormal, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_EXCEPTION,
            ZrParser_Cfg_BlockEdgeAt(argumentInvoke, 1U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(outerInvoke, 0U)->kind);
    outerNormal = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&semantic->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(outerInvoke, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(outerNormal);
    TEST_ASSERT_EQUAL_UINT32(join->id,
            ZrParser_Cfg_BlockEdgeAt(outerNormal, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_EXCEPTION,
            ZrParser_Cfg_BlockEdgeAt(outerInvoke, 1U)->kind);
    TEST_ASSERT_NOT_EQUAL(join->id,
            ZrParser_Cfg_BlockEdgeAt(argumentInvoke, 1U)->toBlockId);
    TEST_ASSERT_NOT_EQUAL(join->id,
            ZrParser_Cfg_BlockEdgeAt(outerInvoke, 1U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, join->predecessorCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            semantic, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; ++index) {
        if (output.instructions[index].opcode == ZR_EXEC_IR_OPCODE_INVOKE)
            ++invokeCount;
    }
    TEST_ASSERT_EQUAL_UINT32(2U, invokeCount);
    ZrCore_ExecIr_FreeFunction(&output);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
