/* Focused LOAD_ADD_INT type-domain cases.  This include is placed after the
 * shared ExecIR fixture helpers in test_ssa_generated_fusion.c. */

static void make_load_add_function(SZrExecIrFunction *function,
                                   TZrExecIrTypeToken numericType) {
    const TZrExecIrMemoryTokenId heapToken = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);

    init_function(function, 3u);
    function->id = 1u;
    function->entryBlockId = 1u;
    function->values[0].id = 1u;
    function->values[0].definition = 0u;
    function->values[0].typeToken = ZR_VALUE_TYPE_NATIVE_POINTER;
    function->values[0].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function->values[1].id = 2u;
    function->values[1].definition = 0u;
    function->values[1].typeToken = numericType;
    function->values[1].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function->values[2].id = 3u;
    function->values[2].definition = 1u;
    function->values[2].typeToken = numericType;
    function->values[3].id = 4u;
    function->values[3].definition = 2u;
    function->values[3].typeToken = numericType;
    function->valueCount = 4u;

    set_instruction(function, 0u, ZR_EXEC_IR_OPCODE_LOAD, 0u, 1u, 3u);
    set_instruction(function, 1u, ZR_EXEC_IR_OPCODE_ADD, 1u, 2u, 4u);
    set_instruction(function, 2u, ZR_EXEC_IR_OPCODE_RETURN, 3u, 1u, 0u);
    function->instructions[0].typeToken = numericType;
    function->instructions[1].typeToken = numericType;
    function->results[0] = 3u;
    function->results[1] = 4u;
    function->resultCount = 2u;
    function->operands[0] = 1u;
    function->operands[1] = 3u;
    function->operands[2] = 2u;
    function->operands[3] = 4u;
    function->operandCount = 4u;
    function->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 3u;
    function->blocks[0].terminatorInstructionId = 3u;
    assert(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            function, &heapToken, 1u, &function->instructions[0].memoryIn));
}

static TZrBool load_add_double_memory(
        void *userData,
        const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    TZrUInt32 *loadCount = (TZrUInt32 *)userData;
    if (instruction == ZR_NULL || operands == ZR_NULL || result == ZR_NULL ||
        operation != ZR_EXEC_IR_ORACLE_MEMORY_LOAD || operandCount != 1u ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_LOAD) {
        return ZR_FALSE;
    }
    ++*loadCount;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
    result->as.floating = 40.5;
    return ZR_TRUE;
}

static void assert_load_add_type_falls_back(TZrExecIrTypeToken numericType) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecBcPatternOptions options;
    SZrExecIrDiagnostic diagnostic;
    TZrBool sawTypeFallback = ZR_FALSE;

    make_load_add_function(&function, numericType);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 0u);
    assert(plan.instructionCount == 3u);
    assert(plan.instructions[0].operationCode == ZR_EXEC_IR_OPCODE_LOAD);
    assert(plan.instructions[1].operationCode == ZR_EXEC_IR_OPCODE_ADD);
    for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
        if (plan.fallbacks[index].pattern ==
                    ZR_EXEC_BC_FUSION_PATTERN_LOAD_ADD_INT &&
            plan.fallbacks[index].reason ==
                    ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH) {
            sawTypeFallback = ZR_TRUE;
        }
    }
    assert(sawTypeFallback);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_load_add_int_requires_signed_i64_types(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecBcFusionPlan repeatedPlan;
    SZrExecBcPatternOptions options;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleValue initialValues[4];
    SZrExecIrDiagnostic diagnostic;
    const SZrExecBcFusionPatternInfo *patternInfo;
    TZrUInt32 loadCount = 0u;

    make_load_add_function(&function, ZR_VALUE_TYPE_DOUBLE);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));

    memset(&input, 0, sizeof(input));
    memset(initialValues, 0, sizeof(initialValues));
    initialValues[0].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[0].as.unsignedInteger = 0x1000u;
    initialValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
    initialValues[1].as.floating = 1.25;
    input.function = &function;
    input.initialValues = initialValues;
    input.initialValueCount = 4u;
    input.maxSteps = 8u;
    input.memory = load_add_double_memory;
    input.memoryUserData = &loadCount;
    ZrCore_ExecIr_OracleResultInit(&result);
    assert(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    assert(loadCount == 1u);
    assert(result.returned);
    assert(result.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT);
    assert(result.returnValue.as.floating == 41.75);
    ZrCore_ExecIr_OracleResultFree(&result);

    ZrCore_ExecIr_FreeFunction(&function);
    assert_load_add_type_falls_back(ZR_VALUE_TYPE_DOUBLE);
    assert_load_add_type_falls_back(ZR_VALUE_TYPE_UINT64);
    assert_load_add_type_falls_back(ZR_VALUE_TYPE_INT32);

    make_load_add_function(&function, ZR_VALUE_TYPE_INT64);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    ZrParser_ExecBcFusionPlan_Init(&repeatedPlan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 1u);
    assert(plan.instructionCount == 2u);
    assert(plan.sideEntryCount == 1u);
    assert(plan.sideEntries[0].pattern ==
           ZR_EXEC_BC_FUSION_PATTERN_LOAD_ADD_INT);
    assert(plan.sideEntries[0].headOpcode == ZR_EXEC_IR_OPCODE_LOAD);
    assert(plan.sideEntries[0].tailOpcode == ZR_EXEC_IR_OPCODE_ADD);
    patternInfo = ZrParser_ExecBcFusion_PatternInfo(
            ZR_EXEC_BC_FUSION_PATTERN_LOAD_ADD_INT);
    assert(patternInfo != ZR_NULL);
    assert((patternInfo->constraints &
            ZR_EXEC_BC_FUSION_CONSTRAINT_SIGNED_I64_LOAD_ADD) != 0u);
    assert(plan.patternSchemaHash ==
           ZrParser_ExecBcFusion_PatternSchemaHash());
    assert(ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options,
                                             &repeatedPlan, &diagnostic));
    assert(repeatedPlan.fusedCount == 1u);
    assert(repeatedPlan.generatedHash == plan.generatedHash);
    assert(ZrParser_ExecBcFusion_Validate(&repeatedPlan, &diagnostic));
    ZrParser_ExecBcFusionPlan_Free(&repeatedPlan);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}
