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

static TZrUInt32 count_optional_block_opcode(
        const SZrSemanticIrFunction *function,
        const SZrParserCfgBlock *block,
        EZrSemanticIrOpcode opcode) {
    TZrUInt32 count = 0U;
    TZrUInt32 offset;

    for (offset = 0U; offset < block->instructionCount; ++offset) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        function, block->firstInstructionIndex + offset);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == opcode) {
            ++count;
        }
    }
    return count;
}

static void test_source_optional_field_value_merges_present_and_absent_paths(void) {
    static const TZrChar source[] =
            "resource class Service { pub var value: int = 7; }\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var receiver = wake(weak);\n"
            "var result = receiver?.value;\n";
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "optional_field_value_cfg.zr");
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *present;
    const SZrParserCfgBlock *absent;
    const SZrParserCfgBlock *join;
    const SZrSemanticIrInstruction *fieldProject = ZR_NULL;
    const SZrSemanticIrInstruction *fieldLoad = ZR_NULL;
    const SZrSemanticIrInstruction *presentConvert = ZR_NULL;
    const SZrSemanticIrInstruction *presentStore = ZR_NULL;
    const SZrSemanticIrInstruction *absentConstant = ZR_NULL;
    const SZrSemanticIrInstruction *absentStore = ZR_NULL;
    const SZrSemanticIrInstruction *joinLoad = ZR_NULL;
    const SZrParserPlace *fieldPlace;
    const SZrParserPlace *receiverPlace;
    const SZrParserPlaceProjection *fieldProjection;
    const SZrSemanticSymbolRecord *fieldSymbol;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    }
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(5U, function->cfg.blocks.length);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    present = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 1U);
    absent = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 2U);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(present);
    TEST_ASSERT_NOT_NULL(absent);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(2U, entry->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT32(present->id,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->kind);
    TEST_ASSERT_EQUAL_UINT32(absent->id,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(join->id,
            ZrParser_Cfg_BlockEdgeAt(present, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(join->id,
            ZrParser_Cfg_BlockEdgeAt(absent, 0U)->toBlockId);
    TEST_ASSERT_EQUAL_UINT32(2U, join->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, present, ZR_SEMANTIC_IR_PLACE_PROJECT));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, present, ZR_SEMANTIC_IR_LOAD));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, present, ZR_SEMANTIC_IR_CONVERT));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, present, ZR_SEMANTIC_IR_STORE));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, absent, ZR_SEMANTIC_IR_CONSTANT));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, absent, ZR_SEMANTIC_IR_STORE));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, join, ZR_SEMANTIC_IR_LOAD));

    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (index >= present->firstInstructionIndex &&
            index < present->firstInstructionIndex + present->instructionCount) {
            if (instruction->opcode == ZR_SEMANTIC_IR_PLACE_PROJECT) fieldProject = instruction;
            if (instruction->opcode == ZR_SEMANTIC_IR_LOAD) fieldLoad = instruction;
            if (instruction->opcode == ZR_SEMANTIC_IR_CONVERT) presentConvert = instruction;
            if (instruction->opcode == ZR_SEMANTIC_IR_STORE) presentStore = instruction;
        } else if (index >= absent->firstInstructionIndex &&
                   index < absent->firstInstructionIndex + absent->instructionCount) {
            if (instruction->opcode == ZR_SEMANTIC_IR_CONSTANT) absentConstant = instruction;
            if (instruction->opcode == ZR_SEMANTIC_IR_STORE) absentStore = instruction;
        } else if (index >= join->firstInstructionIndex &&
                   index < join->firstInstructionIndex + join->instructionCount &&
                   instruction->opcode == ZR_SEMANTIC_IR_LOAD) {
            joinLoad = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(fieldProject);
    TEST_ASSERT_NOT_NULL(fieldLoad);
    TEST_ASSERT_NOT_NULL(presentConvert);
    TEST_ASSERT_NOT_NULL(presentStore);
    TEST_ASSERT_NOT_NULL(absentConstant);
    TEST_ASSERT_NOT_NULL(absentStore);
    TEST_ASSERT_NOT_NULL(joinLoad);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, fieldProject->symbolId);
    TEST_ASSERT_EQUAL_UINT32(fieldProject->placeId, fieldLoad->placeId);
    TEST_ASSERT_EQUAL_UINT32(fieldProject->typeId, fieldLoad->typeId);
    fieldPlace = ZrParser_PlaceGraph_Get(
            &function->places, fieldProject->placeId);
    TEST_ASSERT_NOT_NULL(fieldPlace);
    TEST_ASSERT_EQUAL_UINT32(1U, fieldPlace->projections.length);
    fieldProjection = ZrParser_Place_ProjectionAt(fieldPlace, 0U);
    TEST_ASSERT_NOT_NULL(fieldProjection);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_PROJECTION_FIELD,
            fieldProjection->kind);
    TEST_ASSERT_EQUAL_UINT32(fieldProject->symbolId,
            fieldProjection->data.symbolId);
    receiverPlace = ZrParser_PlaceGraph_Get(
            &function->places, fieldPlace->parentId);
    TEST_ASSERT_NOT_NULL(receiverPlace);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_BASE_LOCAL,
            receiverPlace->base.kind);
    fieldSymbol = ZrParser_Semantic_FindSymbolById(
            compiler.semanticContext, fieldProject->symbolId);
    TEST_ASSERT_NOT_NULL(fieldSymbol);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FIELD, fieldSymbol->kind);
    TEST_ASSERT_NOT_NULL(fieldSymbol->astNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, fieldSymbol->astNode->type);
    TEST_ASSERT_EQUAL_UINT32(fieldLoad->resultValueId, presentConvert->valueId);
    TEST_ASSERT_EQUAL_UINT32(presentConvert->resultValueId, presentStore->valueId);
    TEST_ASSERT_EQUAL_UINT32(absentConstant->resultValueId, absentStore->valueId);
    TEST_ASSERT_EQUAL_UINT32(presentStore->placeId, absentStore->placeId);
    TEST_ASSERT_EQUAL_UINT32(presentStore->placeId, joinLoad->placeId);
    TEST_ASSERT_EQUAL_UINT32(presentConvert->typeId, absentConstant->typeId);
    TEST_ASSERT_EQUAL_UINT32(presentConvert->typeId, joinLoad->typeId);
    TEST_ASSERT_NOT_EQUAL(fieldLoad->typeId, presentConvert->typeId);

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

static void test_direct_weak_optional_call_keeps_source_cfg(void) {
    static const TZrChar source[] =
            "resource class Service {\n"
            "    pub const fn consume(value: bool): void {}\n"
            "}\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var side: bool = false;\n"
            "weak?.consume(side = true);\n";
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "direct_weak_optional_cfg.zr");
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrReceiverGuardFact *guard;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *present;
    const SZrParserCfgBlock *join;
    const SZrParserCfgBlock *exception = ZR_NULL;
    SZrAstNode *statement;
    SZrAstNode *expression;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrValueId wakeValueId = ZR_VALUE_ID_INVALID;
    TZrSemanticInstructionId wakeInstructionId =
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
    TZrUInt32 wakeCount = 0U;
    TZrUInt32 guardedStoreCount = 0U;
    TZrUInt32 normalDropCount = 0U;
    TZrUInt32 execDropCount = 0U;
    TZrUInt32 execWakeCount = 0U;
    TZrUInt32 execInvokeCount = 0U;
    TZrUInt32 execThrowCount = 0U;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    }
    TEST_ASSERT_TRUE_MESSAGE(
            compiler.preSemanticIrCfgActive, "weak CFG abandoned during lowering");
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    statement = ast->data.script.statements->nodes[
            ast->data.script.statements->count - 1U];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    expression = statement->data.expressionStatement.expr;
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0U, expression->data.primaryExpression.members->count);
    guard = ZrParser_SemanticFacts_FindReceiverGuardByNode(
            compiler.semanticContext,
            expression->data.primaryExpression.members->nodes[0]);
    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_WEAK_WAKE, guard->kind);
    TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_OPTIONAL, guard->mode);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_THAN_UINT32(2U, function->cfg.blocks.length);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(2U, entry->successorCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->kind);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->kind);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(entry, 1U)->toBlockId);
    TEST_ASSERT_NOT_NULL(join);
    present = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            ZrParser_Cfg_BlockEdgeAt(entry, 0U)->toBlockId);
    TEST_ASSERT_NOT_NULL(present);
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT &&
            instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_WAKE) {
            ++wakeCount;
            wakeValueId = instruction->resultValueId;
            wakeInstructionId = instruction->id;
            TEST_ASSERT_LESS_THAN_UINT32(
                    entry->firstInstructionIndex + entry->instructionCount,
                    (TZrUInt32)index);
        }
        if (instruction->opcode == ZR_SEMANTIC_IR_STORE &&
            instruction->sourceRange.start.line == 8U) {
            ++guardedStoreCount;
            TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
                    present->firstInstructionIndex, (TZrUInt32)index);
            TEST_ASSERT_LESS_THAN_UINT32(
                    present->firstInstructionIndex + present->instructionCount,
                    (TZrUInt32)index);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1U, wakeCount);
    TEST_ASSERT_EQUAL_UINT32(1U, guardedStoreCount);
    for (index = 0U; index < function->cfg.blocks.length; ++index) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        if (block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_THROW) {
            exception = block;
        } else if (block != join) {
            normalDropCount += count_optional_block_opcode(
                    function, block, ZR_SEMANTIC_IR_DROP);
        }
    }
    TEST_ASSERT_NOT_NULL(exception);
    TEST_ASSERT_EQUAL_UINT32(1U, normalDropCount);
    TEST_ASSERT_EQUAL_UINT32(0U, count_optional_block_opcode(
            function, join, ZR_SEMANTIC_IR_DROP));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, exception, ZR_SEMANTIC_IR_THROW));
    TEST_ASSERT_EQUAL_UINT32(1U, count_optional_block_opcode(
            function, exception, ZR_SEMANTIC_IR_DROP));
    {
        const SZrSemanticIrInstruction *branch =
                ZrParser_SemanticIr_InstructionAt(
                        function, entry->firstInstructionIndex +
                                      entry->instructionCount - 1U);
        const TZrValueId *operand;
        TEST_ASSERT_NOT_NULL(branch);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_BRANCH, branch->opcode);
        TEST_ASSERT_EQUAL_UINT32(1U, branch->operandCount);
        operand = (const TZrValueId *)ZrCore_Array_Get(
                (SZrArray *)&function->valueOperands, branch->operandStart);
        TEST_ASSERT_NOT_NULL(operand);
        TEST_ASSERT_EQUAL_UINT32(wakeValueId, *operand);
    }
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    for (index = 0U; index < output.instructionCount; ++index) {
        if (output.instructions[index].sourceId == wakeInstructionId) {
            TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_WAKE,
                    output.instructions[index].opcode);
            TEST_ASSERT_GREATER_THAN_UINT32(0U,
                    output.instructions[index].memoryIn.count);
            TEST_ASSERT_GREATER_THAN_UINT32(0U,
                    output.instructions[index].memoryOut.count);
            ++execWakeCount;
        }
        switch ((EZrExecIrOpcode)output.instructions[index].opcode) {
            case ZR_EXEC_IR_OPCODE_DROP: ++execDropCount; break;
            case ZR_EXEC_IR_OPCODE_INVOKE: ++execInvokeCount; break;
            case ZR_EXEC_IR_OPCODE_THROW: ++execThrowCount; break;
            default: break;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2U, execDropCount);
    TEST_ASSERT_EQUAL_UINT32(1U, execWakeCount);
    TEST_ASSERT_EQUAL_UINT32(1U, execInvokeCount);
    TEST_ASSERT_EQUAL_UINT32(1U, execThrowCount);
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_direct_weak_optional_value_merges_after_guard(void) {
    static const TZrChar source[] =
            "resource class Service {\n"
            "    pub const fn read(): int { return 7; }\n"
            "}\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var result = weak?.read();\n";
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "direct_weak_optional_value.zr");
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrSize index;
    TZrUInt32 wakeCount = 0U;
    TZrUInt32 joinLoads = 0U;
    TZrUInt32 normalDropCount = 0U;
    TZrUInt32 exceptionDropCount = 0U;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    }
    TEST_ASSERT_TRUE_MESSAGE(
            compiler.preSemanticIrCfgActive, "weak value CFG abandoned during lowering");
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, function->cfg.entryBlockId);
    join = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, 3U);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_UINT32(2U, entry->successorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, join->predecessorCount);
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT &&
            instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_WAKE) {
            ++wakeCount;
        }
        if (instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
            index >= join->firstInstructionIndex &&
            index < join->firstInstructionIndex + join->instructionCount) {
            ++joinLoads;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1U, wakeCount);
    TEST_ASSERT_EQUAL_UINT32(1U, joinLoads);
    TEST_ASSERT_EQUAL_UINT32(0U, count_optional_block_opcode(
            function, join, ZR_SEMANTIC_IR_DROP));
    for (index = 0U; index < function->cfg.blocks.length; ++index) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        if (block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_THROW) {
            exceptionDropCount += count_optional_block_opcode(
                    function, block, ZR_SEMANTIC_IR_DROP);
        } else if (block != join) {
            normalDropCount += count_optional_block_opcode(
                    function, block, ZR_SEMANTIC_IR_DROP);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1U, normalDropCount);
    TEST_ASSERT_EQUAL_UINT32(1U, exceptionDropCount);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&output);

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unmodeled_weak_optional_value_abandons_source_cfg(void) {
    static const TZrChar source[] =
            "resource class Service {\n"
            "    pub const fn read(): int { return 7; }\n"
            "}\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var result = weak?.read();\n";
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "weak_optional_fallback.zr");
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize index;
    TZrUInt32 wakeCount = 0U;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.preSemanticIrCfgStartupSuppressed = ZR_TRUE;
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    }
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_BRANCH));
    for (index = 0U; index < function->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT &&
            instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_WAKE) {
            ++wakeCount;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0U, wakeCount);

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

static void assert_optional_field_keeps_conservative_source_cfg(
        const TZrChar *source, TZrNativeString sourceName) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, sourceName);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), name);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *function;
    TZrSize index;
    TZrBool sawSourceIf = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    for (index = 0U; index < ast->data.script.statements->count; ++index) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
        if (statement->type == ZR_AST_IF_EXPRESSION) {
            TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
            sawSourceIf = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(sawSourceIf);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(
            function, ZR_SEMANTIC_IR_BRANCH));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_optional_getter_value_keeps_conservative_source_cfg(void) {
    static const TZrChar source[] =
            "resource class Service {\n"
            "    pub property value: int { get { return 7; } }\n"
            "}\n"
            "var flag: bool = true;\n"
            "if (flag) { var yes: int = 1; } else { var no: int = 2; }\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var receiver = wake(weak);\n"
            "var result = receiver?.value;\n";

    assert_optional_field_keeps_conservative_source_cfg(
            source, "optional_getter_fallback.zr");
}

static void test_optional_unannotated_field_keeps_conservative_source_cfg(void) {
    static const TZrChar source[] =
            "resource class Service { pub var value = 7; }\n"
            "var flag: bool = true;\n"
            "if (flag) { var yes: int = 1; } else { var no: int = 2; }\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var receiver = wake(weak);\n"
            "var result = receiver?.value;\n";

    assert_optional_field_keeps_conservative_source_cfg(
            source, "optional_unannotated_field_fallback.zr");
}

static void test_optional_field_chain_keeps_conservative_source_cfg(void) {
    static const TZrChar source[] =
            "resource class Child {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Service {\n"
            "    pub var child: Unique<Child>;\n"
            "    pub @constructor() { this.child = own Child(7); }\n"
            "}\n"
            "var flag: bool = true;\n"
            "if (flag) { var yes: int = 1; } else { var no: int = 2; }\n"
            "var seed = own Service();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var receiver = wake(weak);\n"
            "var result = receiver?.child.value;\n";

    assert_optional_field_keeps_conservative_source_cfg(
            source, "optional_field_chain_fallback.zr");
}
