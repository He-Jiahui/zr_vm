typedef struct SIndexStoreOracleContext {
    TZrUInt32 placeCalls;
    TZrUInt32 storeCalls;
    TZrUInt64 storedAddress;
    TZrUInt64 storedValue;
} SIndexStoreOracleContext;

static TZrBool index_store_place_provider(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SIndexStoreOracleContext *context =
            (SIndexStoreOracleContext *)userData;
    if (context == ZR_NULL || instruction == ZR_NULL || operands == ZR_NULL ||
        result == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_PROJECT ||
        operandCount != 2u ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        operands[1].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        return ZR_FALSE;
    }
    ++context->placeCalls;
    memset(result, 0, sizeof(*result));
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    result->as.unsignedInteger = UINT64_C(0xA0);
    return ZR_TRUE;
}

static TZrBool index_store_memory_provider(
        void *userData,
        const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SIndexStoreOracleContext *context =
            (SIndexStoreOracleContext *)userData;
    if (context == ZR_NULL || instruction == ZR_NULL || operands == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_STORE ||
        operation != ZR_EXEC_IR_ORACLE_MEMORY_STORE || operandCount != 2u ||
        result != ZR_NULL ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        operands[1].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        return ZR_FALSE;
    }
    ++context->storeCalls;
    context->storedAddress = operands[0].as.unsignedInteger;
    context->storedValue = operands[1].as.unsignedInteger;
    return ZR_TRUE;
}

static void make_index_store_value_operand_function(
        SZrExecIrFunction *function) {
    TZrUInt32 index;

    init_function(function, 3u);
    function->id = 1u;
    function->entryBlockId = 1u;
    function->valueCount = 4u;
    function->operandCount = 5u;
    function->resultCount = 1u;
    function->blockCount = 1u;
    function->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 3u;
    function->blocks[0].terminatorInstructionId = 3u;
    function->sealed = ZR_TRUE;

    for (index = 0u; index < function->valueCount; ++index) {
        function->values[index].id = index + 1u;
        function->values[index].definition =
                ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
        function->values[index].typeToken = 9u;
        function->values[index].flags = 0u;
    }
    function->values[0].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function->values[1].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function->values[2].definition = 1u;
    function->values[2].flags = ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS;
    function->values[3].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;

    set_instruction(function, 0u, ZR_EXEC_IR_OPCODE_PLACE_PROJECT,
                    0u, 2u, 3u);
    set_instruction(function, 1u, ZR_EXEC_IR_OPCODE_STORE,
                    2u, 2u, 0u);
    set_instruction(function, 2u, ZR_EXEC_IR_OPCODE_RETURN,
                    4u, 1u, 0u);
    function->instructions[2].results.start = function->resultCount;
    function->instructions[0].typeToken = 9u;
    function->instructions[0].layoutId = 1u;
    function->instructions[0].effectOut = 1u;
    function->instructions[1].typeToken = 9u;
    function->instructions[1].layoutId = 1u;
    function->instructions[1].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    function->instructions[1].effectIn = 1u;
    function->instructions[1].effectOut = 2u;
    function->instructions[1].memoryOut.start = 0u;
    function->instructions[1].memoryOut.count = 1u;
    function->operands[0] = 1u;
    function->operands[1] = 2u;
    function->operands[2] = 4u;
    function->operands[3] = 3u;
    function->operands[4] = 4u;

    function->memoryTokenCount = 1u;
    function->memoryTokenCapacity = 1u;
    function->memoryTokenPool = (TZrExecIrMemoryTokenId *)calloc(
            1u, sizeof(*function->memoryTokenPool));
    assert(function->memoryTokenPool != ZR_NULL);
    function->memoryTokenPool[0] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
}

static void test_index_store_variant_requires_projected_address_operand(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    SZrExecIrOracleValue initialValues[4] = {{0}};
    SZrExecIrOracleInput oracleInput;
    SZrExecIrOracleExecutionResult oracleResult;
    SIndexStoreOracleContext context = {0};

    make_index_store_value_operand_function(&function);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));

    initialValues[0].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[0].as.unsignedInteger = UINT64_C(0x11);
    initialValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[1].as.unsignedInteger = UINT64_C(2);
    initialValues[3].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[3].as.unsignedInteger = UINT64_C(0xB0);
    memset(&oracleInput, 0, sizeof(oracleInput));
    oracleInput.function = &function;
    oracleInput.initialValues = initialValues;
    oracleInput.initialValueCount = 4u;
    oracleInput.maxSteps = 8u;
    oracleInput.place = index_store_place_provider;
    oracleInput.placeUserData = &context;
    oracleInput.memory = index_store_memory_provider;
    oracleInput.memoryUserData = &context;
    ZrCore_ExecIr_OracleResultInit(&oracleResult);
    assert(ZrCore_ExecIr_RunOracleEx(&oracleInput, &oracleResult,
                                     &diagnostic));
    assert(oracleResult.returned);
    assert(oracleResult.returnValue.kind ==
           ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED);
    assert(oracleResult.returnValue.as.unsignedInteger == UINT64_C(0xB0));
    assert(context.placeCalls == 1u);
    assert(context.storeCalls == 1u);
    assert(context.storedAddress == UINT64_C(0xB0));
    assert(context.storedValue == UINT64_C(0xA0));
    ZrCore_ExecIr_OracleResultFree(&oracleResult);

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    /* The projected place is STORE's value; the write targets the separate
     * address operand, so this must stay as two instructions. */
    assert(plan.fusedCount == 0u);
    assert(plan.instructionCount == 3u);
    assert(plan.instructions[0].operationCode ==
           ZR_EXEC_IR_OPCODE_PLACE_PROJECT);
    assert(plan.instructions[1].operationCode == ZR_EXEC_IR_OPCODE_STORE);
    assert(plan.fallbackCount != 0u);
    assert(plan.fallbacks[0].reason ==
           ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH);
    assert(ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}
