/* The base call-info owns live stack roots, but no VM frame layout. */
static SZrState *gc_test_make_native_base_function_root_with_inactive_frame(void) {
    SZrState *state = createTestState();
    SZrFunction *function;
    SZrFunctionFrameSlotLayout *layout;
    TZrStackValuePointer rootSlot;
    TZrStackValuePointer inactiveSlot;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_FALSE(ZR_CALL_INFO_IS_VM(state->callInfoList));
    TEST_ASSERT_NULL(state->baseCallInfo.metadataFunction);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    layout = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*layout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layout);
    memset(layout, 0, sizeof(*layout));
    layout->stackSlot = 0u;
    layout->byteOffset = 8u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout->byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout->slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function->stackSize = 1u;
    function->frameByteSize = 9u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function->frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValueOnStack);
    function->frameSlotLayouts = layout;
    function->frameSlotLayoutLength = 1u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(function);

    rootSlot = state->stackBase.valuePointer;
    inactiveSlot = rootSlot + 9u;
    TEST_ASSERT_TRUE(inactiveSlot < state->stackTail.valuePointer);
    ZrCore_Stack_SetRawObjectValue(state, rootSlot, (SZrRawObject *)function);
    state->stackTop.valuePointer = rootSlot + 1u;
    TEST_ASSERT_TRUE(inactiveSlot > state->stackTop.valuePointer + 2u);
    ZrCore_Value_ResetAsNull(&inactiveSlot->value);
    inactiveSlot->value.type = ZR_VALUE_TYPE_OBJECT;
    inactiveSlot->value.value.object = (SZrRawObject *)(uintptr_t)0x12u;
    inactiveSlot->value.isGarbageCollectable = ZR_TRUE;

    return state;
}

static void gc_test_assert_native_root_survived_and_destroy(SZrState *state) {
    TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
    TZrStackValuePointer inactiveSlot = rootSlot + 9u;

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, rootSlot->value.type);
    TEST_ASSERT_TRUE(rootSlot->value.isGarbageCollectable);
    TEST_ASSERT_NOT_NULL(rootSlot->value.value.object);
    TEST_ASSERT_EQUAL_UINT32(
            1u, ((SZrFunction *)rootSlot->value.value.object)->frameSlotLayoutLength);

    ZrCore_Value_ResetAsNull(&inactiveSlot->value);
    destroyTestState(state);
}

static void test_gc_native_base_function_root_skips_inactive_frame_on_full_collection(void) {
    SZrState *state = gc_test_make_native_base_function_root_with_inactive_frame();
    SZrGarbageCollector *collector = state->global->garbageCollector;

    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_EQUAL_INT(
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL,
            collector->statsSnapshot.lastCollectionKind);
    gc_test_assert_native_root_survived_and_destroy(state);
}

static void test_gc_native_base_function_root_skips_inactive_frame_on_minor_rewrite(void) {
    SZrState *state = gc_test_make_native_base_function_root_with_inactive_frame();
    SZrGarbageCollector *collector = state->global->garbageCollector;
    TZrUInt32 previousMinorEpoch = collector->minorCollectionEpoch;

    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 4096;
    collector->gcLastStepWork = 0;
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_TRUE(collector->minorCollectionEpoch > previousMinorEpoch);
    gc_test_assert_native_root_survived_and_destroy(state);
}
