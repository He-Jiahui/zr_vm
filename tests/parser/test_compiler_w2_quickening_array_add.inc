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

void test_w2_public_four_array_zero_fill_loop_fuses(void);

void test_w2_public_four_array_zero_fill_loop_fuses(void) {
    static const char *source =
            "let container = import(\"zr.container\");\n"
            "var cells = 6;\n"
            "var lhs = new container.Array<int>();\n"
            "var rhs = new container.Array<int>();\n"
            "var dst = new container.Array<int>();\n"
            "var scratch = new container.Array<int>();\n"
            "var index = 0;\n"
            "while (index <= cells - 1) {\n"
            "    lhs.add(0);\n"
            "    rhs.add(0);\n"
            "    dst.add(0);\n"
            "    scratch.add(0);\n"
            "    index = index + 1;\n"
            "}\n"
            "index = 0;\n"
            "var checksum = 0;\n"
            "while (index <= cells - 1) {\n"
            "    checksum = checksum + lhs[index] + rhs[index] + dst[index] + scratch[index];\n"
            "    index = index + 1;\n"
            "}\n"
            "return checksum;\n";
    static const EZrInstructionCode observedOpcodes[] = {
            ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL),
            ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8),
            ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT),
            ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4),
            ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST),
            ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST)};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result = {0};
    TZrBool stateCreated = state != ZR_NULL;
    TZrBool registered = ZR_FALSE, compiled = ZR_FALSE, executed = ZR_FALSE, isInt = ZR_FALSE;
    EZrThreadStatus runtimeStatus = ZR_THREAD_STATUS_FINE;
    TZrInt64 checksum = 0;
    TZrUInt32 counts[6] = {0u};
    char diagnostic[2048] = {0};
    TZrSize diagnosticLength = 0u;
    int written;

    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        registered = ZrVmLibContainer_Register(state->global);
        if (registered) {
            SZrString *sourceName = ZrCore_String_CreateFromNative(state, "w2_public_four_array_zero_fill.zr");
            if (sourceName != ZR_NULL) {
                function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
            }
        }
        compiled = function != ZR_NULL;
        if (compiled) {
            for (TZrUInt32 index = 0u; index < 6u; ++index) {
                counts[index] = count_opcode_recursive(function, observedOpcodes[index]);
            }
            written = snprintf(diagnostic, sizeof(diagnostic),
                    "native=%u/%u add=%u add4=%u const4=%u fill4=%u children=%u;",
                    counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], function->childFunctionLength);
            if (written > 0) diagnosticLength = (TZrSize)written;
            if (counts[5] == 0u) {
                TZrUInt32 samples = 0u;
                for (TZrUInt32 index = 0u; index < function->instructionsLength && samples < 8u; ++index) {
                    const SZrInstruction *instruction = &function->instructionsList[index].instruction;
                    TZrBool relevant = ZR_FALSE;
                    for (TZrUInt32 opcodeIndex = 0u; opcodeIndex < 6u; ++opcodeIndex) {
                        if ((EZrInstructionCode)instruction->operationCode == observedOpcodes[opcodeIndex]) relevant = ZR_TRUE;
                    }
                    if (!relevant) continue;
                    written = snprintf(diagnostic + diagnosticLength, sizeof(diagnostic) - diagnosticLength,
                            " [ip%u op=%u x=%u a=%u,%u]", index, (unsigned int)instruction->operationCode,
                            (unsigned int)instruction->operandExtra, (unsigned int)instruction->operand.operand1[0],
                            (unsigned int)instruction->operand.operand1[1]);
                    if (written <= 0 || (TZrSize)written >= sizeof(diagnostic) - diagnosticLength) break;
                    diagnosticLength += (TZrSize)written;
                    ++samples;
                }
                samples = 0u;
                for (TZrUInt32 index = 0u; function->callSiteCaches != ZR_NULL &&
                        index < function->callSiteCacheLength && samples < 4u; ++index) {
                    const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
                    EZrInstructionCode opcode;
                    if (entry->instructionIndex >= function->instructionsLength) continue;
                    opcode = (EZrInstructionCode)function->instructionsList[entry->instructionIndex].instruction.operationCode;
                    if (opcode != observedOpcodes[0] && opcode != observedOpcodes[1]) continue;
                    written = snprintf(diagnostic + diagnosticLength, sizeof(diagnostic) - diagnosticLength,
                            " [cache%u ip=%u kind=%u args=%u member=%u bind=%u op=%u token=%u sig=%u h=%llu mh=%llu rel=%u target=%u]",
                            index, entry->instructionIndex, entry->kind, entry->argumentCount, entry->memberEntryIndex,
                            entry->binding.contract.bindingKind, entry->binding.contract.operation,
                            entry->binding.contract.targetMetadataToken, entry->binding.contract.signatureToken,
                            (unsigned long long)entry->binding.contract.signatureHash,
                            (unsigned long long)entry->binding.contract.moduleSignatureHash,
                            entry->bindingLocation.kind, entry->bindingLocation.targetIndex);
                    if (written <= 0 || (TZrSize)written >= sizeof(diagnostic) - diagnosticLength) break;
                    diagnosticLength += (TZrSize)written;
                    ++samples;
                }
            }
            executed = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
            runtimeStatus = state->threadStatus;
            isInt = executed && ZR_VALUE_IS_TYPE_INT(result.type);
            if (isInt) checksum = result.value.nativeObject.nativeInt64;
            snprintf(diagnostic + diagnosticLength, sizeof(diagnostic) - diagnosticLength,
                    " run=%u status=%u int=%u value=%lld", (unsigned int)executed,
                    (unsigned int)runtimeStatus, (unsigned int)isInt, (long long)checksum);
            ZrCore_Function_Free(state, function);
        }
        ZrTests_Runtime_State_Destroy(state);
    }

    TEST_ASSERT_TRUE_MESSAGE(stateCreated, "Failed to create public four-array test state");
    TEST_ASSERT_TRUE_MESSAGE(registered, "Failed to register zr.container");
    TEST_ASSERT_TRUE_MESSAGE(compiled, "Failed to compile public four-array source");
    TEST_ASSERT_TRUE_MESSAGE(executed, diagnostic);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_THREAD_STATUS_FINE, runtimeStatus, diagnostic);
    TEST_ASSERT_TRUE_MESSAGE(isInt, diagnostic);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, checksum, diagnostic);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, counts[5], diagnostic);
}

void test_w2_public_array_add_live_result_is_preserved(void);

void test_w2_public_array_add_live_result_is_preserved(void) {
    static const char *source =
            "let container = import(\"zr.container\");\n"
            "var values = new container.Array<int>();\n"
            "var returned = values.add(7);\n"
            "if (returned != null) {\n"
            "    return -1;\n"
            "}\n"
            "return values[0] + values[0];\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result = {0};
    TZrBool stateCreated = state != ZR_NULL;
    TZrBool registered = ZR_FALSE, compiled = ZR_FALSE, executed = ZR_FALSE, isInt = ZR_FALSE;
    EZrThreadStatus runtimeStatus = ZR_THREAD_STATUS_FINE;
    TZrInt64 checksum = 0;
    TZrUInt32 addCount = 0u, liveAddCount = 0u, firstAddIndex = 0u;
    TZrBool foundAdd = ZR_FALSE;
    char diagnostic[1024] = {0};
    TZrSize diagnosticLength = 0u;
    int written;

    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        registered = ZrVmLibContainer_Register(state->global);
        if (registered) {
            SZrString *sourceName = ZrCore_String_CreateFromNative(state, "w2_public_array_add_live_result.zr");
            if (sourceName != ZR_NULL) {
                function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
            }
        }
        compiled = function != ZR_NULL;
        if (compiled) {
            addCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT));
            for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
                const SZrInstruction *instruction = &function->instructionsList[index].instruction;
                if ((EZrInstructionCode)instruction->operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)) continue;
                if (instruction->operandExtra != ZR_INSTRUCTION_USE_RET_FLAG) ++liveAddCount;
                if (!foundAdd) {
                    firstAddIndex = index;
                    foundAdd = ZR_TRUE;
                }
            }
            written = snprintf(diagnostic, sizeof(diagnostic), "add=%u live=%u children=%u;",
                    addCount, liveAddCount, function->childFunctionLength);
            if (written > 0) diagnosticLength = (TZrSize)written;
            if (foundAdd) {
                for (TZrUInt32 index = firstAddIndex;
                        index < function->instructionsLength && index - firstAddIndex < 7u; ++index) {
                    const SZrInstruction *instruction = &function->instructionsList[index].instruction;
                    written = snprintf(diagnostic + diagnosticLength, sizeof(diagnostic) - diagnosticLength,
                            " [ip%u op=%u x=%u a=%u,%u s=%d]", index, (unsigned int)instruction->operationCode,
                            (unsigned int)instruction->operandExtra, (unsigned int)instruction->operand.operand1[0],
                            (unsigned int)instruction->operand.operand1[1], (int)instruction->operand.operand2[0]);
                    if (written <= 0 || (TZrSize)written >= sizeof(diagnostic) - diagnosticLength) break;
                    diagnosticLength += (TZrSize)written;
                }
            }
            executed = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
            runtimeStatus = state->threadStatus;
            isInt = executed && ZR_VALUE_IS_TYPE_INT(result.type);
            if (isInt) checksum = result.value.nativeObject.nativeInt64;
            snprintf(diagnostic + diagnosticLength, sizeof(diagnostic) - diagnosticLength,
                    " run=%u status=%u int=%u value=%lld", (unsigned int)executed,
                    (unsigned int)runtimeStatus, (unsigned int)isInt, (long long)checksum);
            printf("Array.add live result: %s\n", diagnostic);
            ZrCore_Function_Free(state, function);
        }
        ZrTests_Runtime_State_Destroy(state);
    }

    TEST_ASSERT_TRUE_MESSAGE(stateCreated, "Failed to create public Array.add live-result test state");
    TEST_ASSERT_TRUE_MESSAGE(registered, "Failed to register zr.container");
    TEST_ASSERT_TRUE_MESSAGE(compiled, "Failed to compile public Array.add live-result source");
    TEST_ASSERT_TRUE_MESSAGE(executed, diagnostic);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_THREAD_STATUS_FINE, runtimeStatus, diagnostic);
    TEST_ASSERT_TRUE_MESSAGE(isInt, diagnostic);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(14, checksum, diagnostic);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, addCount, diagnostic);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, liveAddCount, diagnostic);
}

void test_w2_public_array_add_live_result_preserves_using_cleanup(void);

void test_w2_public_array_add_live_result_preserves_using_cleanup(void) {
    static const char *source =
            "let container = import(\"zr.container\");\n"
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "var checksum = 0;\n"
            "if (true) {\n"
            "    var resource = new CloseProbe();\n"
            "    using (resource) {\n"
            "        var values = new container.Array<int>();\n"
            "        var returned = values.add(7);\n"
            "        if (returned != null) { return -1; }\n"
            "        checksum = values[0] + values[0];\n"
            "    }\n"
            "}\n"
            "if (CloseProbe.calls != 1) { return -2; }\n"
            "return CloseProbe.calls * 100 + checksum;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result = {0};
    TZrBool stateCreated = state != ZR_NULL;
    TZrBool registered = ZR_FALSE, compiled = ZR_FALSE, executed = ZR_FALSE, isInt = ZR_FALSE;
    EZrThreadStatus runtimeStatus = ZR_THREAD_STATUS_FINE;
    TZrInt64 checksum = 0;
    char diagnostic[256] = {0};

    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        registered = ZrVmLibContainer_Register(state->global);
        if (registered) {
            SZrString *sourceName = ZrCore_String_CreateFromNative(state, "w2_public_array_add_using_cleanup.zr");
            if (sourceName != ZR_NULL) {
                function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
            }
        }
        compiled = function != ZR_NULL;
        if (compiled) {
            executed = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
            runtimeStatus = state->threadStatus;
            isInt = executed && ZR_VALUE_IS_TYPE_INT(result.type);
            if (isInt) checksum = result.value.nativeObject.nativeInt64;
            snprintf(diagnostic, sizeof(diagnostic), "run=%u status=%u int=%u value=%lld",
                    (unsigned int)executed, (unsigned int)runtimeStatus, (unsigned int)isInt, (long long)checksum);
            printf("Array.add using cleanup: %s\n", diagnostic);
            ZrCore_Function_Free(state, function);
        }
        ZrTests_Runtime_State_Destroy(state);
    }

    TEST_ASSERT_TRUE_MESSAGE(stateCreated, "Failed to create public Array.add using-cleanup test state");
    TEST_ASSERT_TRUE_MESSAGE(registered, "Failed to register zr.container");
    TEST_ASSERT_TRUE_MESSAGE(compiled, "Failed to compile public Array.add using-cleanup source");
    TEST_ASSERT_TRUE_MESSAGE(executed, diagnostic);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_THREAD_STATUS_FINE, runtimeStatus, diagnostic);
    TEST_ASSERT_TRUE_MESSAGE(isInt, diagnostic);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(114, checksum, diagnostic);
}
