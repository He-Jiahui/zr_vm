/* Source-backed control-flow fixtures share the pre-SemIR Unity harness. */
static void test_source_if_emits_typed_semantic_control_flow(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { var yes: int = 1; } else { var no: int = 2; }\n";
    SZrString *name = ZrCore_String_Create(g_state, "source_if.zr", 12U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrSemanticIrInstruction *branch;
    const TZrValueId *condition;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(&compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(5U, function->cfg.blocks.length);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_TERMINATOR_BRANCH, entry->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, entry->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                          ZrParser_Cfg_BlockEdgeAt(entry, 0U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                          ZrParser_Cfg_BlockEdgeAt(entry, 1U)->kind);
    branch = ZrParser_SemanticIr_InstructionAt(
            function, entry->firstInstructionIndex + entry->instructionCount - 1U);
    TEST_ASSERT_NOT_NULL(branch);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_BRANCH, branch->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, branch->operandCount);
    condition = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands, branch->operandStart);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, *condition);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
                          ZrParser_SemanticIr_Value(function, *condition)->definitionInstructionId);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_nested_source_if_covers_each_instruction_once(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { if (flag) { var inner: int = 1; } } "
            "else { var outside: int = 2; }\n"
            "var after: int = 3;\n";
    SZrString *name = ZrCore_String_Create(g_state, "nested_if.zr", 12U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize instructionIndex;
    TZrSize blockIndex;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (blockIndex = 0U; blockIndex < ast->data.script.statements->count;
         blockIndex++) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[blockIndex]);
    }
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(8U, function->cfg.blocks.length);
    for (instructionIndex = 0U;
         instructionIndex < function->instructions.length; instructionIndex++) {
        TZrUInt32 owners = 0U;
        for (blockIndex = 0U; blockIndex < function->cfg.blocks.length;
             blockIndex++) {
            const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
                    ZrCore_Array_Get((SZrArray *)&function->cfg.blocks, blockIndex);
            if (instructionIndex >= block->firstInstructionIndex &&
                instructionIndex - block->firstInstructionIndex <
                        block->instructionCount) {
                owners++;
            }
        }
        TEST_ASSERT_EQUAL_UINT32(1U, owners);
    }
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unmodeled_loop_arm_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { while (flag) { break; } }\n";
    SZrString *name = ZrCore_String_Create(g_state, "loop_arm.zr", 11U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
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
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_BRANCH));
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_late_unmodeled_arm_abandons_partial_source_cfg(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { var first: int = 1; }\n"
            "if (flag) { while (flag) { break; } }\n";
    SZrString *name = ZrCore_String_Create(g_state, "late_loop.zr", 12U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
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
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_BRANCH));
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_nested_untracked_condition_abandons_outer_source_cfg(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "var value: int = 1;\n"
            "if (flag) { if (value > 0) { var inner: int = 2; } }\n";
    SZrString *name = ZrCore_String_Create(g_state, "nested_expr.zr", 14U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
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
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_BRANCH));
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_if_lowers_place_provenance_to_exec_ir(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { } else { }\n";
    SZrString *name = ZrCore_String_Create(
            g_state, "source_exec_ir.zr", 17U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrBool built;
    TZrSize index;
    TZrUInt32 placeInstructionCount = 0U;
    TZrUInt32 loadInstructionCount = 0U;
    TZrUInt32 storeInstructionCount = 0U;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semantic);

    ZrCore_ExecIr_FunctionInit(&output);
    built = ZrParser_ExecIr_Build(
            semantic, ZR_NULL, &output, &diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(built);
    for (index = 0U; index < output.instructionCount; index++) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        TZrUInt32 operandIndex;

        if (instruction->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE ||
            instruction->opcode == ZR_EXEC_IR_OPCODE_PLACE_PROJECT) {
            TZrExecIrValueId resultValueId;

            TEST_ASSERT_EQUAL_UINT32(1U, instruction->results.count);
            resultValueId = output.results[instruction->results.start];
            TEST_ASSERT_GREATER_THAN_UINT32(0U, resultValueId);
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(
                    output.valueCount, resultValueId);
            TEST_ASSERT_EQUAL_UINT32(index + 1U,
                    output.values[resultValueId - 1U].definition);
            if (instruction->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE) {
                TZrExecIrValueId provenance;

                TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
                provenance = output.operands[instruction->operands.start];
                TEST_ASSERT_GREATER_THAN_UINT32(0U, provenance);
                TEST_ASSERT_LESS_OR_EQUAL_UINT32(
                        output.valueCount, provenance);
                TEST_ASSERT_BITS_HIGH(
                        ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY,
                        output.values[provenance - 1U].flags);
            }
            placeInstructionCount++;
        } else if (instruction->opcode == ZR_EXEC_IR_OPCODE_LOAD) {
            TZrExecIrValueId address;
            TZrExecIrInstructionId definition;

            TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
            address = output.operands[instruction->operands.start];
            TEST_ASSERT_GREATER_THAN_UINT32(0U, address);
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(output.valueCount, address);
            definition = output.values[address - 1U].definition;
            TEST_ASSERT_GREATER_THAN_UINT32(0U, definition);
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(
                    output.instructionCount, definition);
            TEST_ASSERT_TRUE(
                    output.instructions[definition - 1U].opcode ==
                            ZR_EXEC_IR_OPCODE_PLACE_BASE ||
                    output.instructions[definition - 1U].opcode ==
                            ZR_EXEC_IR_OPCODE_PLACE_PROJECT);
            loadInstructionCount++;
        } else if (instruction->opcode == ZR_EXEC_IR_OPCODE_STORE) {
            TZrExecIrValueId address;
            TZrExecIrInstructionId definition;

            TEST_ASSERT_EQUAL_UINT32(2U, instruction->operands.count);
            address = output.operands[instruction->operands.start];
            TEST_ASSERT_GREATER_THAN_UINT32(0U, address);
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(output.valueCount, address);
            definition = output.values[address - 1U].definition;
            TEST_ASSERT_GREATER_THAN_UINT32(0U, definition);
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(
                    output.instructionCount, definition);
            TEST_ASSERT_TRUE(
                    output.instructions[definition - 1U].opcode ==
                            ZR_EXEC_IR_OPCODE_PLACE_BASE ||
                    output.instructions[definition - 1U].opcode ==
                            ZR_EXEC_IR_OPCODE_PLACE_PROJECT);
            storeInstructionCount++;
        }
        for (operandIndex = 0U; operandIndex < instruction->operands.count;
             operandIndex++) {
            TZrExecIrValueId valueId = output.operands[
                    instruction->operands.start + operandIndex];
            const SZrExecIrValue *value;

            TEST_ASSERT_GREATER_THAN_UINT32(0U, valueId);
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(output.valueCount, valueId);
            value = &output.values[valueId - 1U];
            TEST_ASSERT_TRUE(
                    value->definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
                    (value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0U);
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0U, placeInstructionCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, loadInstructionCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, storeInstructionCount);

    ZrCore_ExecIr_FreeFunction(&output);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_if_isolates_sibling_temporary_values(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { var left: int = 1; } "
            "else { var right: int = 2; }\n";
    SZrString *name = ZrCore_String_Create(
            g_state, "source_branch_temps.zr", 22U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
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
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semantic = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semantic);

    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            semantic, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);

    ZrCore_ExecIr_FreeFunction(&output);
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_short_circuit_arm_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "var flag: bool = true;\n"
            "if (flag) { var local: bool = flag && flag; }\n";
    SZrString *name = ZrCore_String_Create(g_state, "logical_arm.zr", 14U);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, sizeof(source) - 1U, name);
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
    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_EQUAL_UINT64(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_BRANCH));
    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}
