static void make_typed_increment_branch_function(
        SZrExecIrFunction *function, TZrExecIrTypeToken numericType) {
    TZrUInt32 index;

    init_function(function, 4u);
    free(function->blocks);
    function->blocks = (SZrExecIrBlock *)calloc(3u,
                                                sizeof(*function->blocks));
    assert(function->blocks != NULL);
    function->blockCount = 3u;
    function->blockCapacity = 3u;
    function->id = 1u;
    function->entryBlockId = 1u;
    function->valueCount = 3u;
    function->operandCount = 3u;
    function->resultCount = 1u;
    for (index = 0u; index < function->valueCount; ++index) {
        function->values[index].id = index + 1u;
        function->values[index].typeToken = numericType;
        function->values[index].definition = 0u;
    }
    function->values[0].flags |= ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function->values[1].flags |= ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function->values[2].definition = 1u;

    set_instruction(function, 0u, ZR_EXEC_IR_OPCODE_ADD, 0u, 2u, 3u);
    set_instruction(function, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                    2u, 1u, 0u);
    set_instruction(function, 2u, ZR_EXEC_IR_OPCODE_RETURN, 3u, 0u, 0u);
    set_instruction(function, 3u, ZR_EXEC_IR_OPCODE_RETURN, 3u, 0u, 0u);
    function->instructions[0].results.start = 0u;
    function->instructions[1].results.start = 1u;
    function->instructions[2].results.start = 1u;
    function->instructions[3].results.start = 1u;
    function->instructions[0].typeToken = numericType;
    function->instructions[1].typeToken = numericType;
    function->results[0] = 3u;
    function->operands[0] = 1u;
    function->operands[1] = 2u;
    function->operands[2] = 3u;

    function->successorCount = 2u;
    function->successorCapacity = 2u;
    function->successors = (TZrExecIrBlockId *)calloc(
            function->successorCount, sizeof(*function->successors));
    assert(function->successors != NULL);
    function->successors[0] = 2u;
    function->successors[1] = 3u;
    function->predecessorCount = 2u;
    function->predecessorCapacity = 2u;
    function->predecessors = (TZrExecIrBlockId *)calloc(
            function->predecessorCount, sizeof(*function->predecessors));
    assert(function->predecessors != NULL);
    function->predecessors[0] = 1u;
    function->predecessors[1] = 1u;

    function->blocks[0].id = 1u;
    function->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 2u;
    function->blocks[0].successorRange.start = 0u;
    function->blocks[0].successorRange.count = 2u;
    function->blocks[0].terminatorInstructionId = 2u;
    function->blocks[1].id = 2u;
    function->blocks[1].instructionRange.start = 2u;
    function->blocks[1].instructionRange.count = 1u;
    function->blocks[1].predecessorRange.start = 0u;
    function->blocks[1].predecessorRange.count = 1u;
    function->blocks[1].successorRange.start = 2u;
    function->blocks[1].terminatorInstructionId = 3u;
    function->blocks[2].id = 3u;
    function->blocks[2].instructionRange.start = 3u;
    function->blocks[2].instructionRange.count = 1u;
    function->blocks[2].predecessorRange.start = 1u;
    function->blocks[2].predecessorRange.count = 1u;
    function->blocks[2].successorRange.start = 2u;
    function->blocks[2].terminatorInstructionId = 4u;
    function->instructions[1].successorRange.start = 0u;
    function->instructions[1].successorRange.count = 2u;
    function->sealed = ZR_TRUE;
}

static void assert_add_condition_oracle_path(
        const SZrExecIrFunction *function,
        TZrInt64 left,
        TZrInt64 right,
        TZrExecIrBlockId expectedBlock) {
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleValue initialValues[2];
    SZrExecIrDiagnostic diagnostic;

    memset(&input, 0, sizeof(input));
    memset(initialValues, 0, sizeof(initialValues));
    initialValues[0].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    initialValues[0].as.signedInteger = left;
    initialValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    initialValues[1].as.signedInteger = right;
    input.function = function;
    input.initialValues = initialValues;
    input.initialValueCount = 2u;
    input.maxSteps = 16u;
    ZrCore_ExecIr_OracleResultInit(&result);
    assert(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    assert(result.returned);
    assert(!result.terminatedByThrow);
    assert(result.currentBlock == expectedBlock);
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_increment_loop_branch_requires_signed_i64_add_types(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecBcFusionPlan repeatedPlan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    const SZrExecBcFusionPatternInfo *patternInfo;
    TZrUInt64 stableHash;

    make_typed_increment_branch_function(&function, ZR_VALUE_TYPE_INT64);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));
    assert_add_condition_oracle_path(&function, 4, 1, 2u);
    assert_add_condition_oracle_path(&function, 5, -5, 3u);
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    ZrParser_ExecBcFusionPlan_Init(&repeatedPlan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 1u);
    assert(plan.instructionCount == 3u);
    assert(plan.estimatedCodeBytes ==
           plan.instructionCount * sizeof(SZrInstruction));
    assert(plan.sideEntries[0].pattern ==
           ZR_EXEC_BC_FUSION_PATTERN_INCREMENT_LOOP_BRANCH);
    patternInfo = ZrParser_ExecBcFusion_PatternInfo(plan.sideEntries[0].pattern);
    assert(patternInfo != ZR_NULL);
    assert((patternInfo->constraints &
            ZR_EXEC_BC_FUSION_CONSTRAINT_SIGNED_I64_CONDITION) != 0u);
    assert(plan.patternSchemaHash ==
           ZrParser_ExecBcFusion_PatternSchemaHash());
    assert(ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    stableHash = plan.generatedHash;
    plan.patternSchemaHash ^= 1u;
    assert(ZrParser_ExecBcFusion_Hash(&plan) != stableHash);
    plan.generatedHash = ZrParser_ExecBcFusion_Hash(&plan);
    assert(!ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    plan.patternSchemaHash = ZrParser_ExecBcFusion_PatternSchemaHash();
    plan.generatedHash = stableHash;
    assert(ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options,
                                             &repeatedPlan, &diagnostic));
    assert(repeatedPlan.generatedHash == plan.generatedHash);
    assert(plan.sideEntries[0].branchTargetCount == 2u);
    assert(plan.sideEntries[0].branchTargets[0] == 2u);
    assert(plan.sideEntries[0].branchTargets[1] == 3u);
    assert(plan.sideEntries[0].branchTargetPcs[0] == 1u);
    assert(plan.sideEntries[0].branchTargetPcs[1] == 2u);
    assert(plan.sourceMapCount == 4u);
    assert(plan.sourceMaps[0].originalInstructionId == 1u);
    assert(plan.sourceMaps[0].sourceId == 1000u);
    assert(plan.sourceMaps[1].originalInstructionId == 2u);
    assert(plan.sourceMaps[1].sourceId == 1001u);
    ZrParser_ExecBcFusionPlan_Free(&repeatedPlan);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);

    make_typed_increment_branch_function(&function, ZR_VALUE_TYPE_OBJECT);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 0u);
    assert(plan.instructionCount == 4u);
    assert(plan.instructions[0].operationCode == ZR_EXEC_IR_OPCODE_ADD);
    assert(plan.instructions[1].operationCode ==
           ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    assert(plan.fallbackCount != 0u);
    assert(plan.fallbacks[0].reason ==
           ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_fusion_typed_pool_access_rejects_empty_suffix(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction instruction;
    TZrExecIrValueId guardedPoolEntry = ZR_EXEC_IR_VALUE_ID_INVALID;
    TZrExecIrValueId value = ZR_EXEC_IR_VALUE_ID_INVALID;

    ZrCore_ExecIr_FunctionInit(&function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.operands.start = 0u;
    instruction.operands.count = 1u;
    function.operandCount = 0u;
    function.operands = &guardedPoolEntry;
    assert(!zr_fusion_operand_at(&function, &instruction, 0u, &value));

    instruction.results.start = 0u;
    instruction.results.count = 1u;
    function.resultCount = 0u;
    function.results = &guardedPoolEntry;
    assert(!zr_fusion_result_at(&function, &instruction, 0u, &value));

    function.operands = ZR_NULL;
    function.results = ZR_NULL;
    ZrCore_ExecIr_FreeFunction(&function);
}
