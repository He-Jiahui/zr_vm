#ifndef ZR_TESTS_REFERENCE_RECEIVER_FRAME_CONTRACT_CASES_H
#define ZR_TESTS_REFERENCE_RECEIVER_FRAME_CONTRACT_CASES_H

typedef struct SReceiverFrameFixture {
    SZrFunction *function;
    const TZrInstruction *readCall;
    const SZrFunctionFrameSlotLayout *sourceLayout;
    const SZrFunctionFrameSlotLayout *receiverLayout;
    TZrUInt32 resultSlot;
    TZrUInt32 receiverSourceSlot;
    TZrUInt32 cacheIndex;
    TZrUInt32 instructionIndex;
    TZrInt64 result;
    TZrBool executed;
} SReceiverFrameFixture;

static const TZrChar *receiver_frame_fixture_compile(SReceiverFrameFixture *fixture) {
    const TZrChar *source =
            "struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub const fn read(): int { return this.value; }\n"
            "}\n"
            "var snapshot: Snapshot = init Snapshot(7);\n"
            "return snapshot.read();\n";
    SZrFunction *function = compile_source(
            source, "receiver_const_struct_direct_frame.zr");
    const SZrFunction *readFunction = ZR_NULL;
    const TZrInstruction *readCall = ZR_NULL;
    const SZrFunctionFrameSlotLayout *sourceLayout = ZR_NULL;
    const SZrFunctionFrameSlotLayout *receiverLayout = ZR_NULL;
    const TZrChar *failure = ZR_NULL;
    TZrUInt32 resultSlot = UINT32_MAX;
    TZrUInt32 receiverSourceSlot = UINT32_MAX;

    memset(fixture, 0, sizeof(*fixture));

    if (function == ZR_NULL) {
        failure = "Const struct receiver source must compile through the public entry";
        goto cleanup;
    }
    readFunction = find_named_function_recursive(function, "read", 0U);
    if (readFunction == ZR_NULL || function->instructionsList == ZR_NULL) {
        failure = "Compiled source must contain read() and executable instructions";
        goto cleanup;
    }
    receiverLayout = ZrCore_Function_FindFrameSlotLayout(readFunction, 0U);
    for (TZrUInt32 index = 0U; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        const SZrFunctionCallSiteCacheEntry *cache;
        const SZrFunctionMemberEntry *member;
        TZrUInt32 cacheIndex;

        if ((EZrInstructionCode)instruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) {
            continue;
        }
        cacheIndex = instruction->instruction.operand.operand1[0];
        if (function->callSiteCaches == ZR_NULL ||
            cacheIndex >= function->callSiteCacheLength) {
            failure = "Direct member call must reference an existing cache entry";
            goto cleanup;
        }
        cache = &function->callSiteCaches[cacheIndex];
        if (cache->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET ||
            function->memberEntries == ZR_NULL ||
            cache->memberEntryIndex >= function->memberEntryLength) {
            failure = "Direct member call must reference a MEMBER_GET member entry";
            goto cleanup;
        }
        member = &function->memberEntries[cache->memberEntryIndex];
        if (member->symbol == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(member->symbol), "read") != 0) {
            continue;
        }
        if (cache->argumentCount != 1U || readCall != ZR_NULL) {
            failure = "read() must have one direct member call with only its receiver argument";
            goto cleanup;
        }
        readCall = instruction;
    }
    if (readCall == ZR_NULL) {
        failure = "read() must publish the direct KNOWN_VM_MEMBER_CALL under test";
        goto cleanup;
    }
    resultSlot = readCall->instruction.operandExtra;
    receiverSourceSlot = resultSlot + 1U;
    if (resultSlot >= function->stackSize ||
        receiverSourceSlot >= function->stackSize) {
        failure = "Direct member call result and receiver source must be valid frame slots";
        goto cleanup;
    }
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(
            function, receiverSourceSlot);
    if (sourceLayout == ZR_NULL ||
        sourceLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        sourceLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        sourceLayout->byteSize == 0U || sourceLayout->byteAlign == 0U) {
        failure = "Direct readonly receiver source must retain usable inline struct layout";
        goto cleanup;
    }
    if (receiverLayout == ZR_NULL || !receiverLayout->isParameter ||
        receiverLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        receiverLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        (receiverLayout->reserved0 &
         (ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
          ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
          ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS)) !=
                (ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                 ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                 ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS)) {
        failure = "read() parameter zero must retain its borrowed inline receiver contract";
        goto cleanup;
    }
    if (sourceLayout->byteSize != receiverLayout->byteSize ||
        receiverLayout->byteAlign == 0U ||
        sourceLayout->byteAlign < receiverLayout->byteAlign) {
        failure = "Direct caller receiver storage must match the borrowed callee size and alignment";
        goto cleanup;
    }
    if (sourceLayout->byteOffset > function->frameByteSize ||
        sourceLayout->byteSize > function->frameByteSize - sourceLayout->byteOffset ||
        sourceLayout->byteOffset % sourceLayout->byteAlign != 0U ||
        function->frameByteAlign < sourceLayout->byteAlign ||
        receiverLayout->byteOffset > readFunction->frameByteSize ||
        sizeof(SZrFunctionFrameBorrowedAliasBinding) >
                readFunction->frameByteSize - receiverLayout->byteOffset ||
        receiverLayout->byteOffset %
                (TZrUInt32)_Alignof(SZrFunctionFrameBorrowedAliasBinding) != 0U ||
        readFunction->frameByteAlign <
                (TZrUInt32)_Alignof(SZrFunctionFrameBorrowedAliasBinding)) {
        failure = "Caller inline storage and callee borrowed binding must fit their aligned frames";
        goto cleanup;
    }
cleanup:
    fixture->function = function;
    fixture->readCall = readCall;
    fixture->sourceLayout = sourceLayout;
    fixture->receiverLayout = receiverLayout;
    fixture->resultSlot = resultSlot;
    fixture->receiverSourceSlot = receiverSourceSlot;
    fixture->cacheIndex = readCall != ZR_NULL
            ? readCall->instruction.operand.operand1[0] : UINT32_MAX;
    fixture->instructionIndex = readCall != ZR_NULL
            ? (TZrUInt32)(readCall - function->instructionsList) : UINT32_MAX;
    return failure;
}

static const TZrChar *receiver_frame_fixture_execute(SReceiverFrameFixture *fixture) {
    fixture->executed = ZR_TRUE;
    if (!ZrTests_Runtime_Function_ExecuteExpectInt64(
                g_state, fixture->function, &fixture->result)) {
        return "Compiled direct readonly receiver call must execute through the public helper";
    }
    if (fixture->result != 7) {
        return "Compiled direct readonly receiver call must return 7";
    }
    return ZR_NULL;
}

static void receiver_frame_fixture_free(
        SReceiverFrameFixture *fixture, const TZrChar *failure) {
    SZrFunction *function = fixture->function;
    const TZrInstruction *readCall = fixture->readCall;
    const SZrFunctionFrameSlotLayout *sourceLayout = fixture->sourceLayout;
    const SZrFunctionFrameSlotLayout *receiverLayout = fixture->receiverLayout;
    TZrUInt32 resultSlot = fixture->resultSlot;
    TZrUInt32 receiverSourceSlot = fixture->receiverSourceSlot;
    TZrBool executed = fixture->executed;

    if (failure != ZR_NULL) {
        printf("receiver-frame observation: opcode=%u resultSlot=%u sourceSlot=%u executed=%u\n",
               readCall != ZR_NULL ? (unsigned)readCall->instruction.operationCode : UINT32_MAX,
               (unsigned)resultSlot, (unsigned)receiverSourceSlot, (unsigned)executed);
        if (function != ZR_NULL) {
            printf("receiver-frame caller: stack=%u frameBytes=%u frameAlign=%u directCalls=%u knownCalls=%u\n",
                   (unsigned)function->stackSize, (unsigned)function->frameByteSize,
                   (unsigned)function->frameByteAlign,
                   (unsigned)count_instruction_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)),
                   (unsigned)count_instruction_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)));
        }
        if (sourceLayout != ZR_NULL) {
            printf("receiver-frame source: kind=%u flags=%u offset=%u bytes=%u align=%u typeLayout=%u\n",
                   (unsigned)sourceLayout->slotKind, (unsigned)sourceLayout->reserved0,
                   (unsigned)sourceLayout->byteOffset, (unsigned)sourceLayout->byteSize,
                   (unsigned)sourceLayout->byteAlign, (unsigned)sourceLayout->typeLayoutId);
        }
        if (receiverLayout != ZR_NULL) {
            printf("receiver-frame callee: parameter=%u kind=%u flags=%u offset=%u bytes=%u align=%u\n",
                   (unsigned)receiverLayout->isParameter, (unsigned)receiverLayout->slotKind,
                   (unsigned)receiverLayout->reserved0, (unsigned)receiverLayout->byteOffset,
                   (unsigned)receiverLayout->byteSize, (unsigned)receiverLayout->byteAlign);
        }
    }
    if (function != ZR_NULL) {
        ZrCore_Function_Free(g_state, function);
    }
}

static void test_const_struct_receiver_direct_call_has_usable_inline_source(void) {
    SReceiverFrameFixture fixture;
    const TZrChar *failure = receiver_frame_fixture_compile(&fixture);

    if (failure == ZR_NULL) failure = receiver_frame_fixture_execute(&fixture);
    receiver_frame_fixture_free(&fixture, failure);
    TEST_ASSERT_NULL_MESSAGE(failure, failure);
}

static SZrFunctionFrameSlotLayout *receiver_frame_mutable_layout(
        SZrFunction *function, TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) return ZR_NULL;
    for (TZrUInt32 index = 0U; index < function->frameSlotLayoutLength; index++) {
        if (function->frameSlotLayouts[index].stackSlot == stackSlot)
            return &function->frameSlotLayouts[index];
    }
    return ZR_NULL;
}

static void assert_const_struct_receiver_layout_failure(TZrBool corruptCallee) {
    SReceiverFrameFixture fixture;
    const TZrChar *failure = receiver_frame_fixture_compile(&fixture);
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunctionFrameSlotLayout *mutatedLayout = ZR_NULL;
    SZrTypeValue ignoredResult;
    SZrCallBindingDiagnostic diagnostic;
    SZrFunction *targetFunction = ZR_NULL;
    TZrUInt32 expectedInstruction = UINT32_MAX;
    TZrMetadataToken expectedToken = 0U;
    TZrUInt32 originalLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    TZrUInt32 invalidLayoutId;
    TZrUInt64 generation = UINT64_MAX;
    TZrUInt32 targetKind = UINT32_MAX;
    TZrBool mutated = ZR_FALSE;
    TZrBool executed = ZR_FALSE;

    memset(&diagnostic, 0, sizeof(diagnostic));
    if (failure != ZR_NULL) goto cleanup;
    failure = receiver_frame_fixture_execute(&fixture);
    if (failure != ZR_NULL) goto cleanup;
    if (fixture.function->callSiteCaches == ZR_NULL ||
        fixture.cacheIndex >= fixture.function->callSiteCacheLength) {
        failure = "Successful public execution must retain the scanned member cache";
        goto cleanup;
    }
    entry = &fixture.function->callSiteCaches[fixture.cacheIndex];
    if (entry->instructionIndex != fixture.instructionIndex ||
        entry->binding.contract.targetMetadataToken == 0U ||
        entry->binding.generation == 0U ||
        entry->binding.generation != fixture.function->callBindingGeneration ||
        entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_VM ||
        entry->binding.target.vm.function == ZR_NULL) {
        failure = "Successful public execution must warm the actual direct member binding";
        goto cleanup;
    }
    expectedInstruction = entry->instructionIndex;
    expectedToken = entry->binding.contract.targetMetadataToken;
    mutatedLayout = receiver_frame_mutable_layout(
            corruptCallee ? entry->binding.target.vm.function : fixture.function,
            corruptCallee ? 0U : fixture.receiverSourceSlot);
    if (mutatedLayout == ZR_NULL ||
        mutatedLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        mutatedLayout->typeLayoutId != fixture.sourceLayout->typeLayoutId ||
        (corruptCallee && !mutatedLayout->isParameter)) {
        failure = "Layout mutation must select the actual caller or bound callee receiver row";
        goto cleanup;
    }
    originalLayoutId = mutatedLayout->typeLayoutId;
    invalidLayoutId = corruptCallee
            ? (originalLayoutId == 0U ? 1U : 0U) : fixture.function->prototypeCount;
    if (invalidLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        invalidLayoutId == originalLayoutId) {
        failure = "Receiver rejection requires one distinct non-NONE layout ID";
        goto cleanup;
    }
    mutatedLayout->typeLayoutId = invalidLayoutId;
    mutated = ZR_TRUE;
    executed = ZrTests_Runtime_Function_ExecuteCaptureFailure(
            g_state, fixture.function, &ignoredResult);
    diagnostic = g_state->lastCallBindingError;
    entry = &fixture.function->callSiteCaches[fixture.cacheIndex];
    generation = entry->binding.generation;
    targetKind = entry->binding.target.targetKind;
    targetFunction = entry->binding.target.vm.function;

cleanup:
    if (mutated) mutatedLayout->typeLayoutId = originalLayoutId;
    receiver_frame_fixture_free(&fixture, failure);
    TEST_ASSERT_NULL_MESSAGE(failure, failure);
    TEST_ASSERT_FALSE(executed);
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_LAYOUT_MISMATCH, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT32(expectedInstruction, diagnostic.instructionIndex);
    TEST_ASSERT_EQUAL_UINT32(expectedToken, diagnostic.targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT64(0U, generation);
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_TARGET_NONE, targetKind);
    TEST_ASSERT_NULL(targetFunction);
}

static void test_const_struct_receiver_rejects_out_of_range_source_layout(void) {
    assert_const_struct_receiver_layout_failure(ZR_FALSE);
}

static void test_const_struct_receiver_rejects_callee_layout_mismatch(void) {
    assert_const_struct_receiver_layout_failure(ZR_TRUE);
}

#endif
