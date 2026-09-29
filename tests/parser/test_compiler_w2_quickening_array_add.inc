/* Array<int>.add quickening and binding retirement regression. */
/* Detect the optional constant load and dead receiver-copy chain before add. */
static TZrBool function_has_dead_super_array_add_receiver_setup_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 3; index < function->instructionsLength; index++) {
        const TZrInstruction *addInstruction = &function->instructionsList[index];
        const TZrInstruction *receiverReloadInstruction;
        const TZrInstruction *receiverStageInstruction;
        const TZrInstruction *receiverLoadInstruction;
        TZrUInt32 destinationSlot;
        TZrUInt32 receiverSlot;
        TZrUInt32 reloadIndex;

        if ((EZrInstructionCode)addInstruction->instruction.operationCode !=
            ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)) {
            continue;
        }

        destinationSlot = addInstruction->instruction.operandExtra;
        receiverSlot = addInstruction->instruction.operand.operand1[0];
        reloadIndex = index - 1u;
        if ((EZrInstructionCode)function->instructionsList[reloadIndex].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            if (index < 4) {
                continue;
            }
            reloadIndex--;
        }

        receiverReloadInstruction = &function->instructionsList[reloadIndex];
        receiverStageInstruction = &function->instructionsList[reloadIndex - 1u];
        receiverLoadInstruction = &function->instructionsList[reloadIndex - 2u];
        if ((EZrInstructionCode)receiverReloadInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
            receiverReloadInstruction->instruction.operandExtra == destinationSlot &&
            (TZrUInt32)receiverReloadInstruction->instruction.operand.operand2[0] == receiverSlot &&
            (EZrInstructionCode)receiverStageInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SET_STACK) &&
            receiverStageInstruction->instruction.operandExtra == receiverSlot &&
            (TZrUInt32)receiverStageInstruction->instruction.operand.operand2[0] == destinationSlot &&
            (EZrInstructionCode)receiverLoadInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
            receiverLoadInstruction->instruction.operandExtra == destinationSlot) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_dead_super_array_add_receiver_setup_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 count_bound_super_array_add_sites_recursive(const SZrFunction *function) {
    TZrUInt32 count = 0u;
    if (function == ZR_NULL) return 0u;

    if (function->callSiteCaches != ZR_NULL) {
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < function->callSiteCacheLength; ++cacheIndex) {
            const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[cacheIndex];
            TZrUInt32 instructionIndex = entry->instructionIndex;
            if (instructionIndex >= function->instructionsLength ||
                (EZrInstructionCode)function->instructionsList[instructionIndex].instruction.operationCode !=
                        ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)) {
                continue;
            }
            if (entry->binding.contract.bindingKind != ZR_CALL_BINDING_NONE ||
                entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
                (function->callBindingInstructionMap != ZR_NULL &&
                 instructionIndex < function->callBindingInstructionMapLength &&
                 function->callBindingInstructionMap[instructionIndex] != 0u)) {
                ++count;
            }
        }
    }
    for (TZrUInt32 childIndex = 0u; childIndex < function->childFunctionLength; ++childIndex) {
        count += count_bound_super_array_add_sites_recursive(&function->childFunctionList[childIndex]);
    }
    return count;
}

void test_w2_super_array_add_variable_value_elides_dead_receiver_setup(void) {
    static const char *source =
            "let container = import(\"zr.container\");\n"
            "let {Array} = import(\"zr.container\");\n"
            "var values = new container.Array<int>();\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "var i = 0;\n"
            "var total = 0;\n"
            "while (i < 6) {\n"
            "    var value = (i * 3 + 1) % 17;\n"
            "    values.add((i * 3 + 1) % 17);\n"
            "    buckets[\"last\"] = values;\n"
            "    total = total + value;\n"
            "    i = i + 1;\n"
            "}\n"
            "return total;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;
    TZrUInt32 superArrayAddCount;

    timer.startTime = clock();
    ZR_TEST_START("W2 Super Array Add Variable Value Elides Dead Receiver Setup");
    ZR_TEST_INFO("Array<int>.add receiver setup",
                 "Testing that variable-value Array<int>.add folds the dead receiver materialization copies.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for Array<int>.add receiver setup test");

    sourceName = ZrCore_String_CreateFromNative(state, "w2_super_array_add_variable_value_receiver_setup.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile Array<int>.add receiver setup source");

    superArrayAddCount = count_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT));
    if (superArrayAddCount == 0u) {
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
                0u,
                count_opcode_recursive(
                        function,
                        ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL)),
                "Unfused Array<int>.add should retain its native member-call fallback");
    }
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
            "Array<int>.add receiver setup fold should execute successfully");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(51, result, "Array<int>.add loop result changed");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u, superArrayAddCount,
            "Expected typed Array<int>.add to lower to SUPER_ARRAY_ADD_INT");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u, count_bound_super_array_add_sites_recursive(function),
            "Specialized Array<int>.add sites must not retain active call-binding metadata");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_dead_super_array_add_receiver_setup_recursive(function),
            "No dead GET_STACK/SET_STACK/GET_STACK receiver setup should remain before SUPER_ARRAY_ADD_INT");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Super Array Add Variable Value Elides Dead Receiver Setup");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}
