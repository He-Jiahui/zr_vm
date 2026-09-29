typedef struct SBindingCallOracleContext {
    TZrUInt32 placeCalls;
    TZrUInt32 callCalls;
    TZrUInt64 observedCallee;
    TZrUInt64 observedArgument;
} SBindingCallOracleContext;

static TZrBool binding_call_place_provider(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SBindingCallOracleContext *context =
            (SBindingCallOracleContext *)userData;
    if (context == ZR_NULL || instruction == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_PROJECT ||
        operands == ZR_NULL || operandCount != 2u || result == ZR_NULL ||
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

static TZrBool binding_call_call_provider(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SBindingCallOracleContext *context =
            (SBindingCallOracleContext *)userData;
    if (context == ZR_NULL || instruction == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_CALL ||
        operands == ZR_NULL || operandCount != 2u || result == ZR_NULL ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        operands[1].kind != ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        return ZR_FALSE;
    }
    ++context->callCalls;
    context->observedCallee = operands[0].as.unsignedInteger;
    context->observedArgument = operands[1].as.unsignedInteger;
    memset(result, 0, sizeof(*result));
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    result->as.unsignedInteger = UINT64_C(0xD0);
    return ZR_TRUE;
}

static void make_binding_call_argument_operand_function(
        SZrExecIrFunction *function) {
    init_function(function, 3u);
    function->id = 1u;
    function->entryBlockId = 1u;
    function->valueCount = 5u;
    function->operandCount = 5u;
    function->resultCount = 2u;
    function->blockCount = 1u;
    function->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 3u;
    function->blocks[0].terminatorInstructionId = 3u;
    function->sealed = ZR_TRUE;

    for (TZrUInt32 index = 0u; index < function->valueCount; ++index) {
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
    function->values[4].definition = 2u;

    set_instruction(function, 0u, ZR_EXEC_IR_OPCODE_PLACE_PROJECT,
                    0u, 2u, 3u);
    set_instruction(function, 1u, ZR_EXEC_IR_OPCODE_CALL,
                    2u, 2u, 5u);
    set_instruction(function, 2u, ZR_EXEC_IR_OPCODE_RETURN,
                    4u, 1u, 0u);
    function->instructions[2].results.start = function->resultCount;
    function->instructions[0].effectOut = 1u;
    function->instructions[1].flags =
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    function->instructions[1].bindingRow = 2u;
    function->instructions[1].effectIn = 1u;
    function->instructions[1].effectOut = 2u;
    function->memoryTokenCount = 4u;
    function->memoryTokenCapacity = 4u;
    function->memoryTokenPool = (TZrExecIrMemoryTokenId *)calloc(
            function->memoryTokenCount, sizeof(*function->memoryTokenPool));
    assert(function->memoryTokenPool != ZR_NULL);
    function->memoryTokenPool[0] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    function->memoryTokenPool[1] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u);
    function->memoryTokenPool[2] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);
    function->memoryTokenPool[3] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u);
    function->instructions[1].memoryIn.start = 0u;
    function->instructions[1].memoryIn.count = 2u;
    function->instructions[1].memoryOut.start = 2u;
    function->instructions[1].memoryOut.count = 2u;
    function->operands[0] = 1u;
    function->operands[1] = 2u;
    /* The callee is independent; PLACE_PROJECT supplies only argument 0. */
    function->operands[2] = 4u;
    function->operands[3] = 3u;
    function->operands[4] = 5u;
}

static void test_binding_call_requires_projected_callee_operand(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    SZrExecIrOracleValue initialValues[5] = {{0}};
    SZrExecIrOracleInput oracleInput;
    SZrExecIrOracleExecutionResult oracleResult;
    SBindingCallOracleContext context = {0};

    make_binding_call_argument_operand_function(&function);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));

    initialValues[0].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[0].as.unsignedInteger = UINT64_C(0x11);
    initialValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[1].as.unsignedInteger = UINT64_C(0x22);
    initialValues[3].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    initialValues[3].as.unsignedInteger = UINT64_C(0xC0);
    memset(&oracleInput, 0, sizeof(oracleInput));
    oracleInput.function = &function;
    oracleInput.initialValues = initialValues;
    oracleInput.initialValueCount = 5u;
    oracleInput.maxSteps = 8u;
    oracleInput.call = binding_call_call_provider;
    oracleInput.userData = &context;
    oracleInput.place = binding_call_place_provider;
    oracleInput.placeUserData = &context;
    ZrCore_ExecIr_OracleResultInit(&oracleResult);
    assert(ZrCore_ExecIr_RunOracleEx(&oracleInput, &oracleResult,
                                     &diagnostic));
    assert(oracleResult.returned);
    assert(oracleResult.returnValue.kind ==
           ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED);
    assert(oracleResult.returnValue.as.unsignedInteger == UINT64_C(0xD0));
    assert(context.placeCalls == 1u);
    assert(context.callCalls == 1u);
    assert(context.observedCallee == UINT64_C(0xC0));
    assert(context.observedArgument == UINT64_C(0xA0));
    ZrCore_ExecIr_OracleResultFree(&oracleResult);

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    /* BINDING_CALL can erase a projected member only when it is the callee. */
    assert(plan.fusedCount == 0u);
    assert(plan.instructionCount == 3u);
    assert(plan.instructions[0].operationCode ==
           ZR_EXEC_IR_OPCODE_PLACE_PROJECT);
    assert(plan.instructions[1].operationCode == ZR_EXEC_IR_OPCODE_CALL);
    assert(plan.fallbackCount != 0u);
    assert(plan.fallbacks[0].reason ==
           ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH);
    assert(ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}
