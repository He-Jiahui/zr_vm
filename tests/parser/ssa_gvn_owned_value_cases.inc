/* Ownership and consumption boundaries for expression reuse. */
static void build_gvn_consumed_result_case(SZrExecIrFunction *function,
                                           EZrExecIrOpcode opcode,
                                           EZrExecIrOwnership ownership) {
    TZrExecIrValueId source, values[3], inputs[2];
    SZrExecIrInstruction instruction;
    SZrExecIrRange operands = {0}, result;
    TZrExecIrInstructionId id;
    TZrUInt32 index;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 81u;
    function->entryBlockId = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    source = ZrCore_ExecIr_FunctionAddExternalValue(function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    GVN_CHECK(source != 0u);
    for (index = 0u; index < 3u; ++index) {
        values[index] = ZrCore_ExecIr_FunctionAddValue(function, ZR_VALUE_TYPE_INT64,
                ownership, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        GVN_CHECK(values[index] != 0u);
    }
    inputs[0] = inputs[1] = source;
    if (opcode != ZR_EXEC_IR_OPCODE_CONSTANT) {
        GVN_CHECK(ZrCore_ExecIr_FunctionAppendOperands(function, inputs,
                opcode == ZR_EXEC_IR_OPCODE_ADD ? 2u : 1u, &operands));
    }
    for (index = 0u; index < 3u; ++index) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = (TZrUInt16)(index == 1u ? ZR_EXEC_IR_OPCODE_MOVE : opcode);
        instruction.layoutId = index == 1u ? 0u : 17u;
        instruction.sourceId = 810u + index;
        if (index == 1u) {
            GVN_CHECK(ZrCore_ExecIr_FunctionAppendOperands(
                    function, &values[0], 1u, &instruction.operandRange));
        } else {
            instruction.operandRange = operands;
        }
        GVN_CHECK(ZrCore_ExecIr_FunctionAppendResults(function, &values[index], 1u, &result));
        instruction.resultRange = result;
        GVN_CHECK(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    GVN_CHECK(ZrCore_ExecIr_FunctionAppendOperands(
            function, &values[2], 1u, &instruction.operandRange));
    GVN_CHECK(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    function->blocks[0].instructionRange.count = function->instructionCount;
}

static void test_gvn_owned_results_and_mixed_ownership(void) {
    TZrUInt32 mode, side;
    for (mode = 1u; mode < ZR_EXEC_IR_OWNERSHIP_COUNT; ++mode) {
        for (side = 0u; side < 3u; ++side) {
            SZrExecIrFunction function;
            SZrExecIrDiagnostic diagnostic;
            SZrExecIrRemarkSink remarks = {0};
            build_repeated_unary_function(&function, ZR_EXEC_IR_OPCODE_COPY,
                    ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
            if (side != 1u) function.values[1].ownership = (EZrExecIrOwnership)mode;
            if (side != 0u) function.values[2].ownership = (EZrExecIrOwnership)mode;
            GVN_CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
            GVN_CHECK(ZrParser_ExecIr_RunGvnCse(&function, ZR_NULL, &remarks, &diagnostic));
            GVN_CHECK(function.operandPool[function.instructions[2].operandRange.start] == 1u);
            GVN_CHECK(remarks.count == 0u);
            GVN_CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
            free(remarks.items);
            ZrCore_ExecIr_FreeFunction(&function);
        }
    }
}

static void test_gvn_rejects_malformed_consumption_before_mutation(void) {
    TZrUInt32 malformed;
    for (malformed = 0u; malformed < 5u; ++malformed) {
        SZrExecIrFunction function;
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrRemarkSink remarks = {0};
        TZrUInt32 operandCount;
        SZrExecIrInstruction snapshot[4];
        build_gvn_consumed_result_case(&function, ZR_EXEC_IR_OPCODE_CONSTANT,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
        operandCount = function.operandCount;
        if (malformed == 0u) {
            function.instructions[1].operandRange.start = UINT32_MAX;
        } else if (malformed < 3u) {
            function.operandPool[function.instructions[1].operandRange.start] =
                    malformed == 1u ? 0u : function.valueCount + 1u;
        } else if (malformed == 3u) {
            function.values[1].id = 1u;
        } else {
            function.values[1].ownership = ZR_EXEC_IR_OWNERSHIP_COUNT;
        }
        memcpy(snapshot, function.instructions, sizeof(snapshot));
        GVN_CHECK(!ZrParser_ExecIr_RunGvnCse(&function, ZR_NULL, &remarks, &diagnostic));
        GVN_CHECK(diagnostic.code == (malformed == 0u
                ? ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE : ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE));
        GVN_CHECK(diagnostic.instructionId == (malformed < 3u ? 2u : 0u));
        GVN_CHECK(diagnostic.sourceId == (malformed < 3u ? 811u : 0u));
        GVN_CHECK(function.operandCount == operandCount);
        GVN_CHECK(memcmp(snapshot, function.instructions, sizeof(snapshot)) == 0);
        GVN_CHECK(remarks.count == 0u);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_gvn_other_consumers_and_later_consumption(void) {
    const EZrExecIrOpcode consumers[] = {ZR_EXEC_IR_OPCODE_DROP,
            ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED, ZR_EXEC_IR_OPCODE_MOVE};
    TZrUInt32 variant;
    for (variant = 0u; variant < 3u; ++variant) {
        SZrExecIrFunction function;
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrOwnerAnalysis owners = {0};
        SZrExecIrRemarkSink remarks = {0};
        EZrExecIrOwnership mode = variant == 1u ? ZR_EXEC_IR_OWNERSHIP_UNIQUE
                                              : ZR_EXEC_IR_OWNERSHIP_UNKNOWN;
        build_gvn_consumed_result_case(&function, ZR_EXEC_IR_OPCODE_CONSTANT, mode);
        function.instructions[1].opcode = (TZrUInt16)consumers[variant];
        if (variant != 2u) {
            TZrExecIrMemoryTokenId memoryIn = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                    ZR_EXEC_IR_MEMORY_OWNERSHIP, 1u);
            TZrExecIrMemoryTokenId memoryOut = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                    ZR_EXEC_IR_MEMORY_OWNERSHIP, 2u);
            function.instructions[1].resultRange.count = 0u;
            GVN_CHECK(ZrCore_ExecIr_FunctionAppendMemoryTokens(&function, &memoryIn, 1u,
                    &function.instructions[1].memoryIn));
            GVN_CHECK(ZrCore_ExecIr_FunctionAppendMemoryTokens(&function, &memoryOut, 1u,
                    &function.instructions[1].memoryOut));
            function.instructions[1].effectIn = 1u;
            function.instructions[1].effectOut = 2u;
        } else {
            SZrExecIrInstruction temp = function.instructions[1];
            function.instructions[1] = function.instructions[2];
            function.instructions[2] = temp;
            function.values[3].definition = 2u;
            function.values[2].definition = 3u;
        }
        if (variant == 1u) function.blocks[0].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
        if (!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
            fprintf(stderr, "Consumer variant=%u verifier code=%u instruction=%u\n",
                    (unsigned int)variant, (unsigned int)diagnostic.code,
                    (unsigned int)diagnostic.instructionId);
            exit(EXIT_FAILURE);
        }
        GVN_CHECK(ZrParser_ExecIr_RunGvnCse(&function, ZR_NULL, &remarks, &diagnostic));
        GVN_CHECK(remarks.count == 0u);
        GVN_CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
        GVN_CHECK(ZrCore_ExecIr_OwnerAnalysisBuild(&function, &owners, &diagnostic));
        GVN_CHECK(ZrCore_ExecIr_OwnerStateAt(&owners, 4u, 4u, ZR_EXEC_IR_STATE_BEFORE_EFFECT) ==
                (variant == 1u ? ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED : ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN));
        ZrCore_ExecIr_OwnerAnalysisFree(&owners);
        free(remarks.items);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_gvn_consumed_result_availability(void) {
    const EZrExecIrOpcode opcodes[] = {ZR_EXEC_IR_OPCODE_CONSTANT,
            ZR_EXEC_IR_OPCODE_COPY, ZR_EXEC_IR_OPCODE_ADD, ZR_EXEC_IR_OPCODE_CONVERT};
    TZrUInt32 mode, operation, repetition, invalidStates = 0u;
    for (mode = 0u; mode < ZR_EXEC_IR_OWNERSHIP_COUNT; ++mode) {
        for (operation = 0u; operation < sizeof(opcodes) / sizeof(opcodes[0]); ++operation) {
            SZrExecIrFunction function;
            SZrExecIrDiagnostic diagnostic;
            SZrExecIrRemarkSink remarks = {0};
            SZrExecIrOwnerAnalysis owners = {0};
            EZrExecIrStateMapOwnerState expected = mode == ZR_EXEC_IR_OWNERSHIP_UNKNOWN
                    ? ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN : ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
            build_gvn_consumed_result_case(&function, opcodes[operation], (EZrExecIrOwnership)mode);
            GVN_CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
            for (repetition = 0u; repetition < 3u; ++repetition) {
                EZrExecIrStateMapOwnerState actual;
                GVN_CHECK(ZrCore_ExecIr_OwnerAnalysisBuild(&function, &owners, &diagnostic));
                GVN_CHECK(ZrCore_ExecIr_OwnerStateAt(&owners, 2u, 2u,
                        ZR_EXEC_IR_STATE_AFTER_EFFECT) == ZR_EXEC_IR_STATE_MAP_OWNER_MOVED);
                actual = ZrCore_ExecIr_OwnerStateAt(&owners, 4u, 4u,
                        ZR_EXEC_IR_STATE_BEFORE_EFFECT);
                if (actual != expected) {
                    fprintf(stderr, "Consumed result reuse: ownership=%u opcode=%u pass=%u expected=%u actual=%u\n",
                            (unsigned int)mode, (unsigned int)opcodes[operation],
                            (unsigned int)repetition, (unsigned int)expected, (unsigned int)actual);
                    ++invalidStates;
                }
                ZrCore_ExecIr_OwnerAnalysisFree(&owners);
                GVN_CHECK(ZrParser_ExecIr_RunGvnCse(&function, ZR_NULL, &remarks, &diagnostic));
                GVN_CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
            }
            if (invalidStates == 0u) {
                GVN_CHECK(remarks.count == 0u);
                GVN_CHECK(function.instructions[2].opcode == (TZrUInt16)opcodes[operation]);
            }
            free(remarks.items);
            ZrCore_ExecIr_FreeFunction(&function);
        }
    }
    GVN_CHECK(invalidStates == 0u);
}
