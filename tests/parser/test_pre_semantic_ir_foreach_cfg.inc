static const SZrParserCfgBlock *foreach_cfg_block_for_instruction(
        const SZrSemanticIrFunction *function,
        const SZrSemanticIrInstruction *instruction) {
    TZrSize index;

    if (function == ZR_NULL || instruction == ZR_NULL ||
        instruction->id == ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        TZrUInt32 instructionIndex = instruction->id - 1U;

        if (block != ZR_NULL &&
            instructionIndex >= block->firstInstructionIndex &&
            instructionIndex <
                    block->firstInstructionIndex + block->instructionCount) {
            return block;
        }
    }
    return ZR_NULL;
}

static void assert_foreach_iterator_invoke_edges(
        const SZrSemanticIrFunction *function,
        const SZrParserCfgBlock *block) {
    const SZrParserCfgEdge *normalEdge;
    const SZrParserCfgEdge *exceptionEdge;
    const SZrParserCfgBlock *exceptionBlock;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_UINT32(2U, block->successorCount);
    normalEdge = ZrParser_Cfg_BlockEdgeAt(block, 0U);
    exceptionEdge = ZrParser_Cfg_BlockEdgeAt(block, 1U);
    TEST_ASSERT_NOT_NULL(normalEdge);
    TEST_ASSERT_NOT_NULL(exceptionEdge);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_NORMAL, normalEdge->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_EXCEPTION, exceptionEdge->kind);
    TEST_ASSERT_NOT_EQUAL(normalEdge->toBlockId, exceptionEdge->toBlockId);
    exceptionBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, exceptionEdge->toBlockId);
    TEST_ASSERT_NOT_NULL(exceptionBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW,
            exceptionBlock->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(0U, exceptionBlock->instructionCount);
}

static void test_source_foreach_emits_iterator_invoke_cycle(void) {
    static const TZrChar source[] =
            "var values = [true];\n"
            "for (var item in values) { continue; }\n"
            "var after: int = 1;\n";
    static TZrChar sourceName[] = "source_foreach_iterator_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *init = ZR_NULL;
    const SZrSemanticIrInstruction *moveNext = ZR_NULL;
    const SZrSemanticIrInstruction *current = ZR_NULL;
    const SZrParserCfgBlock *initBlock;
    const SZrParserCfgBlock *moveNextBlock;
    const SZrParserCfgBlock *conditionBlock;
    const SZrParserCfgBlock *currentBlock;
    const SZrParserCfgBlock *bodyBlock;
    const SZrSemanticIrInstruction *bindingInitialize = ZR_NULL;
    const TZrValueId *operand;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize semanticIteratorCount = 0U;
    TZrSize execIteratorCount = 0U;
    TZrSize legacyInitCount = 0U;
    TZrSize legacyMoveNextCount = 0U;
    TZrSize legacyCurrentCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        switch (instruction->opcode) {
            case ZR_SEMANTIC_IR_ITER_INIT:
                init = instruction;
                semanticIteratorCount++;
                break;
            case ZR_SEMANTIC_IR_ITER_MOVE_NEXT:
                moveNext = instruction;
                semanticIteratorCount++;
                break;
            case ZR_SEMANTIC_IR_ITER_CURRENT:
                current = instruction;
                semanticIteratorCount++;
                break;
            default:
                break;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(3U, semanticIteratorCount);
    TEST_ASSERT_NOT_NULL(init);
    TEST_ASSERT_NOT_NULL(moveNext);
    TEST_ASSERT_NOT_NULL(current);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, init->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, moveNext->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, current->typeId);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, init->resultValueId);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, moveNext->resultValueId);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, current->resultValueId);
    TEST_ASSERT_EQUAL_UINT32(1U, init->operandCount);
    TEST_ASSERT_EQUAL_UINT32(1U, moveNext->operandCount);
    TEST_ASSERT_EQUAL_UINT32(1U, current->operandCount);
    operand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands, moveNext->operandStart);
    TEST_ASSERT_NOT_NULL(operand);
    TEST_ASSERT_EQUAL_UINT32(init->resultValueId, *operand);
    operand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands, current->operandStart);
    TEST_ASSERT_NOT_NULL(operand);
    TEST_ASSERT_EQUAL_UINT32(init->resultValueId, *operand);

    initBlock = foreach_cfg_block_for_instruction(function, init);
    moveNextBlock = foreach_cfg_block_for_instruction(function, moveNext);
    currentBlock = foreach_cfg_block_for_instruction(function, current);
    assert_foreach_iterator_invoke_edges(function, initBlock);
    assert_foreach_iterator_invoke_edges(function, moveNextBlock);
    assert_foreach_iterator_invoke_edges(function, currentBlock);

    conditionBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(moveNextBlock, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(conditionBlock);
    TEST_ASSERT_EQUAL_UINT32(2U, conditionBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(conditionBlock, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            currentBlock->id,
            ZrParser_Cfg_BlockEdgeAt(conditionBlock, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(conditionBlock, 1U)->kind);
    bodyBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(currentBlock, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(bodyBlock);
    TEST_ASSERT_EQUAL_UINT32(1U, bodyBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(bodyBlock, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            moveNextBlock->id,
            ZrParser_Cfg_BlockEdgeAt(bodyBlock, 0U)->toBlockId);
    for (index = bodyBlock->firstInstructionIndex;
         index < bodyBlock->firstInstructionIndex + bodyBlock->instructionCount;
         index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        if (instruction != ZR_NULL &&
            instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
            instruction->valueId == current->resultValueId) {
            bindingInitialize = instruction;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(bindingInitialize);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, bindingInitialize->placeId);

    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(
                        &compiler.instructions, index);

        TEST_ASSERT_NOT_NULL(instruction);
        switch ((EZrInstructionCode)instruction->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
                legacyInitCount++;
                break;
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
                legacyMoveNextCount++;
                break;
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
                legacyCurrentCount++;
                break;
            default:
                break;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(1U, legacyInitCount);
    TEST_ASSERT_EQUAL_UINT64(1U, legacyMoveNextCount);
    TEST_ASSERT_EQUAL_UINT64(1U, legacyCurrentCount);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    for (index = 0U; index < output.instructionCount; index++) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];

        if (instruction->opcode == ZR_EXEC_IR_OPCODE_ITER_INIT ||
            instruction->opcode == ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT ||
            instruction->opcode == ZR_EXEC_IR_OPCODE_ITER_CURRENT) {
            TEST_ASSERT_EQUAL_UINT32(1U, instruction->operands.count);
            TEST_ASSERT_EQUAL_UINT32(1U, instruction->results.count);
            TEST_ASSERT_EQUAL_UINT32(2U, instruction->successorRange.count);
            execIteratorCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(3U, execIteratorCount);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_foreach_binding_flows_through_body(void) {
    static const TZrChar source[] =
            "var values = [true];\n"
            "for (var item in values) { var copy: bool = item; }\n"
            "var after: int = 1;\n";
    static TZrChar sourceName[] = "source_foreach_binding_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *current = ZR_NULL;
    const SZrSemanticIrInstruction *bindingInitialize = ZR_NULL;
    const SZrSemanticIrInstruction *bindingLoad = ZR_NULL;
    const SZrParserCfgBlock *currentBlock;
    const SZrParserCfgBlock *bodyBlock;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        if (instruction != ZR_NULL &&
            instruction->opcode == ZR_SEMANTIC_IR_ITER_CURRENT) {
            current = instruction;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(current);
    currentBlock = foreach_cfg_block_for_instruction(function, current);
    TEST_ASSERT_NOT_NULL(currentBlock);
    bodyBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(currentBlock, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(bodyBlock);
    for (index = bodyBlock->firstInstructionIndex;
         index < bodyBlock->firstInstructionIndex + bodyBlock->instructionCount;
         index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE &&
            instruction->valueId == current->resultValueId) {
            bindingInitialize = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                   bindingInitialize != ZR_NULL &&
                   instruction->placeId == bindingInitialize->placeId) {
            bindingLoad = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(bindingInitialize);
    TEST_ASSERT_NOT_NULL(bindingLoad);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, bindingLoad->resultValueId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_foreach_restores_outer_slot_snapshot(void) {
    static const TZrChar source[] =
            "var outer: bool = true;\n"
            "var values = [false];\n"
            "for (var item in values) { outer = item; }\n"
            "var after: bool = outer;\n";
    static TZrChar sourceName[] = "source_foreach_slot_restore.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    SZrString *outerName;
    TZrUInt32 outerStackSlot;
    TZrSize slotCountBeforeLoop;
    SZrCompilerSemanticIrSlot beforeLoop;
    const SZrCompilerSemanticIrSlot *slot;
    const SZrSemanticIrFunction *function;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(4U, ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    outerName = ast->data.script.statements->nodes[0]
                        ->data.variableDeclaration.pattern
                        ->data.identifier.name;
    outerStackSlot = find_local_var(&compiler, outerName);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, outerStackSlot);
    slot = compiler_semantic_ir_find_slot(&compiler, outerStackSlot);
    TEST_ASSERT_NOT_NULL(slot);
    slotCountBeforeLoop = compiler.preSemanticIrSlots.length;
    beforeLoop = *slot;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    slot = compiler_semantic_ir_find_slot(&compiler, outerStackSlot);
    TEST_ASSERT_NOT_NULL(slot);
    TEST_ASSERT_EQUAL_UINT64(
            slotCountBeforeLoop, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.stackSlot, slot->stackSlot);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.placeId, slot->placeId);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.valueId, slot->valueId);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.typeId, slot->typeId);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.symbolId, slot->symbolId);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.loanId, slot->loanId);
    TEST_ASSERT_EQUAL_UINT32(beforeLoop.regionId, slot->regionId);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[3]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_foreach_preserves_iterable_assignment(void) {
    static const TZrChar source[] =
            "var values = [true];\n"
            "var replacement = [false];\n"
            "for (var item in (values = replacement)) {}\n"
            "var after = values;\n";
    static TZrChar sourceName[] =
            "source_foreach_iterable_assignment.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    SZrString *valuesName;
    TZrUInt32 valuesStackSlot;
    TZrSize slotCountBeforeLoop;
    SZrCompilerSemanticIrSlot beforeLoop;
    const SZrCompilerSemanticIrSlot *slot;
    const SZrSemanticIrFunction *function;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(4U, ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[0]);
    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    valuesName = ast->data.script.statements->nodes[0]
                        ->data.variableDeclaration.pattern
                        ->data.identifier.name;
    valuesStackSlot = find_local_var(&compiler, valuesName);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, valuesStackSlot);
    slot = compiler_semantic_ir_find_slot(&compiler, valuesStackSlot);
    TEST_ASSERT_NOT_NULL(slot);
    slotCountBeforeLoop = compiler.preSemanticIrSlots.length;
    beforeLoop = *slot;

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    slot = compiler_semantic_ir_find_slot(&compiler, valuesStackSlot);
    TEST_ASSERT_NOT_NULL(slot);
    TEST_ASSERT_EQUAL_UINT64(
            slotCountBeforeLoop, compiler.preSemanticIrSlots.length);
    TEST_ASSERT_NOT_EQUAL(beforeLoop.valueId, slot->valueId);

    ZrParser_Statement_Compile(
            &compiler, ast->data.script.statements->nodes[3]);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_source_foreach_break_targets_iterator_join(void) {
    static const TZrChar source[] =
            "var values = [true];\n"
            "for (var item in values) { break; }\n"
            "var after: int = 1;\n";
    static TZrChar sourceName[] = "source_foreach_break_cfg.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    const SZrSemanticIrInstruction *moveNext = ZR_NULL;
    const SZrSemanticIrInstruction *current = ZR_NULL;
    const SZrParserCfgBlock *moveNextBlock;
    const SZrParserCfgBlock *conditionBlock;
    const SZrParserCfgBlock *currentBlock;
    const SZrParserCfgBlock *bodyBlock;
    TZrUInt32 joinBlockId;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_ITER_MOVE_NEXT) {
            moveNext = instruction;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_ITER_CURRENT) {
            current = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(moveNext);
    TEST_ASSERT_NOT_NULL(current);
    moveNextBlock = foreach_cfg_block_for_instruction(function, moveNext);
    currentBlock = foreach_cfg_block_for_instruction(function, current);
    TEST_ASSERT_NOT_NULL(moveNextBlock);
    TEST_ASSERT_NOT_NULL(currentBlock);
    conditionBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(moveNextBlock, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(conditionBlock);
    joinBlockId = ZrParser_Cfg_BlockEdgeAt(conditionBlock, 1U)->toBlockId;
    bodyBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(currentBlock, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(bodyBlock);
    TEST_ASSERT_EQUAL_UINT32(1U, bodyBlock->successorCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZrParser_Cfg_BlockEdgeAt(bodyBlock, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(
            joinBlockId,
            ZrParser_Cfg_BlockEdgeAt(bodyBlock, 0U)->toBlockId);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    ZrCore_ExecIr_FreeFunction(&output);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_destructuring_foreach_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var values = [[true]];\n"
            "for (var [item] in values) { continue; }\n"
            "identity(true);\n";
    static TZrChar sourceName[] = "destructuring_foreach_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize legacyInitCount = 0U;
    TZrSize legacyMoveNextCount = 0U;
    TZrSize legacyCurrentCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_INIT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_MOVE_NEXT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(
                        &compiler.instructions, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(ITER_INIT)) {
            legacyInitCount++;
        } else if (instruction->instruction.operationCode ==
                   ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) {
            legacyMoveNextCount++;
        } else if (instruction->instruction.operationCode ==
                   ZR_INSTRUCTION_ENUM(ITER_CURRENT)) {
            legacyCurrentCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    TEST_ASSERT_EQUAL_UINT64(1U, legacyInitCount);
    TEST_ASSERT_EQUAL_UINT64(1U, legacyMoveNextCount);
    TEST_ASSERT_EQUAL_UINT64(1U, legacyCurrentCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_foreach_nested_unmodeled_condition_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "fn identity(value: bool): bool { return value; }\n"
            "var values = [true];\n"
            "for (var item in values) {\n"
            "    if (item && (1 == 1)) {}\n"
            "}\n"
            "identity(true);\n";
    static TZrChar sourceName[] =
            "foreach_nested_condition_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;
    TZrSize callCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_INIT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_MOVE_NEXT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);

        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_CALL_TYPED) {
            callCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(0U, callCount);
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_foreach_cleanup_exit_rolls_back_iterator_cfg(void) {
    static const TZrChar source[] =
            "var values = [true];\n"
            "for (var item in values) {\n"
            "    var owner: Unique<string>;\n"
            "    continue;\n"
            "}\n";
    static TZrChar sourceName[] = "foreach_cleanup_cfg_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_INIT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_MOVE_NEXT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_foreach_inferred_body_declaration_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "var values = [true];\n"
            "for (var item in values) { var copy = item; }\n";
    static TZrChar sourceName[] =
            "foreach_inferred_cleanup_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_loop_exit_cfg_fixture(
            &compiler, source, sizeof(source) - 1U,
            sourceName, sizeof(sourceName) - 1U);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_INIT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_MOVE_NEXT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    free_loop_exit_cfg_fixture(&compiler, ast);
}

static void test_foreach_close_contract_keeps_legacy_cfg(void) {
    static const TZrChar source[] =
            "class Closable { pub @close() {} }\n"
            "var values = [true];\n"
            "for (var item in values) { var resource: Closable; }\n";
    static TZrChar sourceName[] =
            "foreach_close_contract_fallback.zr";
    SZrString *name = ZrCore_String_Create(
            g_state, sourceName, sizeof(sourceName) - 1U);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_UINT64(3U, ast->data.script.statements->count);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    ZrParser_Compiler_CompileClassDeclaration(
            &compiler, ast->data.script.statements->nodes[0]);
    for (index = 1U;
         index < ast->data.script.statements->count && !compiler.hasError;
         index++) {
        ZrParser_Statement_Compile(
                &compiler, ast->data.script.statements->nodes[index]);
    }
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_INIT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_MOVE_NEXT));
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_ITER_CURRENT));
    free_loop_exit_cfg_fixture(&compiler, ast);
}
