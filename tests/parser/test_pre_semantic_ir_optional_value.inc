static void test_source_optional_value_merges_present_and_absent_paths(void) {
    static const TZrChar source[] =
            "resource class Service {\n"
            "    pub const fn read(): int { return 7; }\n"
            "}\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var receiver = wake(weak);\n"
            "var result = receiver?.read();\n";
    static TZrChar sourceName[] = "optional_value_cfg.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *present;
    const SZrParserCfgBlock *absent;
    const SZrParserCfgBlock *join;
    const SZrParserCfgBlock *invoke;
    const SZrParserCfgBlock *normal;
    const SZrParserCfgBlock *exception;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrPlaceId mergePlaceId = ZR_PLACE_ID_INVALID;
    TZrValueId callResultValueId = ZR_VALUE_ID_INVALID;
    TZrValueId presentValueId = ZR_VALUE_ID_INVALID;
    TZrValueId absentValueId = ZR_VALUE_ID_INVALID;
    TZrTypeId mergeTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrSize callCount = 0U;
    TZrSize convertCount = 0U;
    TZrSize constantCount = 0U;
    TZrSize storeCount = 0U;
    TZrSize loadCount = 0U;
    TZrSize invokeCount = 0U;
    TZrSize exceptionBlockCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
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
    TEST_ASSERT_EQUAL_UINT64(8U, function->cfg.blocks.length);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    present = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    absent = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    invoke = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 4U);
    normal = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 5U);
    exception = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 6U);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(present);
    TEST_ASSERT_NOT_NULL(absent);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_NOT_NULL(invoke);
    TEST_ASSERT_NOT_NULL(normal);
    TEST_ASSERT_NOT_NULL(exception);
    TEST_ASSERT_EQUAL_UINT32(2U, entry->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            present->id, ZrParser_Cfg_BlockEdgeAt(entry, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            absent->id, ZrParser_Cfg_BlockEdgeAt(entry, 1U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, present->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            invoke->id, ZrParser_Cfg_BlockEdgeAt(present, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, invoke->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(invoke, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            normal->id, ZrParser_Cfg_BlockEdgeAt(invoke, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_EXCEPTION,
            ZrParser_Cfg_BlockEdgeAt(invoke, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            exception->id, ZrParser_Cfg_BlockEdgeAt(invoke, 1U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, normal->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            join->id, ZrParser_Cfg_BlockEdgeAt(normal, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(1U, absent->successorCount);
    TEST_ASSERT_EQUAL_UINT32(
            join->id, ZrParser_Cfg_BlockEdgeAt(absent, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, join->predecessorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW,
            exception->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, exception->instructionCount);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED ||
            instruction->opcode == ZR_SEMANTIC_IR_CALL_VIRTUAL ||
            instruction->opcode == ZR_SEMANTIC_IR_CALL_DYNAMIC ||
            instruction->opcode == ZR_SEMANTIC_IR_CALL_META) {
            callCount++;
            callResultValueId = instruction->resultValueId;
            TEST_ASSERT_EQUAL_UINT32(1U, instruction->operandCount);
            TEST_ASSERT_EQUAL_UINT32(
                    invoke->firstInstructionIndex + invoke->instructionCount -
                            1U,
                    (TZrUInt32)index);
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CONVERT &&
                   index >= normal->firstInstructionIndex &&
                   index < normal->firstInstructionIndex +
                                   normal->instructionCount) {
            convertCount++;
            TEST_ASSERT_EQUAL_UINT32(
                    callResultValueId, instruction->valueId);
            presentValueId = instruction->resultValueId;
            mergeTypeId = instruction->typeId;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_CONSTANT &&
                   index >= absent->firstInstructionIndex &&
                   index < absent->firstInstructionIndex +
                                   absent->instructionCount) {
            constantCount++;
            TEST_ASSERT_TRUE(instruction->hasConstantPoolIndex);
            absentValueId = instruction->resultValueId;
            TEST_ASSERT_EQUAL_UINT32(mergeTypeId, instruction->typeId);
        } else if (instruction->opcode == ZR_SEMANTIC_IR_STORE &&
                   ((index >= normal->firstInstructionIndex &&
                     index < normal->firstInstructionIndex +
                                     normal->instructionCount) ||
                    (index >= absent->firstInstructionIndex &&
                     index < absent->firstInstructionIndex +
                                     absent->instructionCount))) {
            storeCount++;
            if (mergePlaceId == ZR_PLACE_ID_INVALID) {
                mergePlaceId = instruction->placeId;
            }
            TEST_ASSERT_EQUAL_UINT32(mergePlaceId, instruction->placeId);
            TEST_ASSERT_EQUAL_UINT32(mergeTypeId, instruction->typeId);
            if (index >= normal->firstInstructionIndex &&
                index < normal->firstInstructionIndex +
                                normal->instructionCount) {
                TEST_ASSERT_EQUAL_UINT32(presentValueId, instruction->valueId);
            } else {
                TEST_ASSERT_EQUAL_UINT32(absentValueId, instruction->valueId);
            }
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   index >= join->firstInstructionIndex &&
                   index < join->firstInstructionIndex +
                                   join->instructionCount) {
            loadCount++;
            TEST_ASSERT_EQUAL_UINT32(mergePlaceId, instruction->placeId);
            TEST_ASSERT_EQUAL_UINT32(mergeTypeId, instruction->typeId);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, convertCount);
    TEST_ASSERT_EQUAL_UINT64(1U, constantCount);
    TEST_ASSERT_EQUAL_UINT64(2U, storeCount);
    TEST_ASSERT_EQUAL_UINT64(1U, loadCount);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, mergePlaceId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        switch ((EZrExecIrOpcode)output.instructions[index].opcode) {
            case ZR_EXEC_IR_OPCODE_INVOKE:
                invokeCount++;
                TEST_ASSERT_EQUAL_UINT32(
                        2U, output.instructions[index].successorRange.count);
                break;
            default:
                break;
        }
    }
    for (index = 0U; index < output.blockCount; index++) {
        if ((output.blocks[index].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0U) {
            exceptionBlockCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, invokeCount);
    TEST_ASSERT_EQUAL_UINT64(1U, exceptionBlockCount);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unmodeled_optional_value_abandons_partial_source_cfg(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { var yes: int = 1; } else { var no: int = 2; }\n"
            "var receiver = null;\n"
            "var result = receiver?.value;\n";
    SZrString *name = ZrCore_String_Create(
            g_state, "optional_value_fallback.zr", 26U);
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
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_BRANCH));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
