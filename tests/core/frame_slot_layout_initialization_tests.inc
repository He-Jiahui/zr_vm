static void test_frame_value_initialization_preserves_direct_and_checked_arguments(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[8] = {0};
    TZrStackValuePointer frameBase = stack + 1;
    SZrTypeValue *firstByteValue;
    SZrTypeValue *secondByteValue;

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (2u + index) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[index].isParameter = 1u;
    }
    function.parameterCount = 2u;
    function.stackSize = 2u;
    function.frameByteSize =
            4u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 8;
    profileRuntime.recordHelpers = ZR_TRUE;
    firstByteValue = (SZrTypeValue *)((TZrByte *)frameBase +
                                      layouts[0].byteOffset);
    secondByteValue = (SZrTypeValue *)((TZrByte *)frameBase +
                                       layouts[1].byteOffset);

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    ZrCore_Value_InitAsInt(&state, firstByteValue, 11);
    ZrCore_Value_InitAsInt(&state, secondByteValue, 22);
    ZrCore_Function_InitializeFrameLayoutStorage(&state, stack, &function, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, firstByteValue->type);
    TEST_ASSERT_EQUAL_INT64(11, firstByteValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, secondByteValue->type);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_CHECKED]);

    function.directValueFrameSlotCountPlusOne = 0u;
    ZrCore_Value_InitAsInt(&state, firstByteValue, 33);
    ZrCore_Value_InitAsInt(&state, secondByteValue, 44);
    ZrCore_Function_InitializeFrameLayoutStorage(&state, stack, &function, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, firstByteValue->type);
    TEST_ASSERT_EQUAL_INT64(33, firstByteValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, secondByteValue->type);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_CHECKED]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_direct_value_frame_drop_preflights_complete_frame_span(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};
    SZrState state = {0};
    SZrTypeValueOnStack stack[6] = {0};
    TZrStackValuePointer frameBase = stack + 1;

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (2u + index) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    function.stackSize = 2u;
    function.frameByteSize =
            4u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 4;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_EQUAL_UINT32(3u, function.directValueFrameSlotCountPlusOne);
    TEST_ASSERT_FALSE(ZrCore_Function_DropInlineFrameValues(
            &state, &function, frameBase, ZR_NULL, ZR_NULL));
}

static void test_packed_direct_value_frame_drop_keeps_batched_owner_release(void) {
    SZrFunctionFrameSlotLayout layouts[5] = {0};
    SZrFunction function = {0};
    SZrState state = {0};
    SZrTypeValueOnStack stack[12] = {0};
    TZrStackValuePointer frameBase = stack + 1;
    SZrTypeValue *denseOwner;
    SZrTypeValue *byteOwner;

    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(layouts); index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (TZrUInt32)((ZR_ARRAY_COUNT(layouts) + index) *
                            sizeof(SZrTypeValueOnStack));
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    function.stackSize = ZR_ARRAY_COUNT(layouts);
    function.frameByteSize =
            (TZrUInt32)(2u * ZR_ARRAY_COUNT(layouts) *
                        sizeof(SZrTypeValueOnStack));
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = ZR_ARRAY_COUNT(layouts);
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + ZR_ARRAY_COUNT(stack);

    denseOwner = ZrCore_Stack_GetValueNoProfile(frameBase + 2u);
    denseOwner->type = ZR_VALUE_TYPE_OBJECT;
    denseOwner->isGarbageCollectable = ZR_TRUE;
    denseOwner->value.object = (SZrRawObject *)&state;
    denseOwner->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;
    byteOwner = (SZrTypeValue *)((TZrByte *)frameBase + layouts[3].byteOffset);
    byteOwner->type = ZR_VALUE_TYPE_OBJECT;
    byteOwner->isGarbageCollectable = ZR_TRUE;
    byteOwner->value.object = (SZrRawObject *)&state;
    byteOwner->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_ARRAY_COUNT(layouts) + 1u,
            function.directValueFrameSlotCountPlusOne);
    TEST_ASSERT_TRUE(ZrCore_Function_DropInlineFrameValues(
            &state, &function, frameBase, ZR_NULL, ZR_NULL));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(denseOwner->type));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(byteOwner->type));
}

static void test_direct_value_parameter_copy_preflights_complete_frame_span(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};
    SZrState state = {0};
    SZrTypeValueOnStack stack[6] = {0};
    TZrStackValuePointer frameBase = stack + 1;

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (2u + index) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    layouts[0].isParameter = 1u;
    function.parameterCount = 1u;
    function.stackSize = 2u;
    function.frameByteSize =
            4u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 4;

    ZrCore_Value_InitAsInt(&state, &stack[0].value, 41);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_EQUAL_UINT32(2u, function.directValueParameterCountPlusOne);
    TEST_ASSERT_FALSE(ZrCore_Function_CopyValueFrameParameters(
            &state, &function, frameBase, stack, 1u));
}

static void test_direct_value_parameter_copy_keeps_dense_source_in_place(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[5] = {0};
    TZrStackValuePointer frameBase = stack + 1;
    SZrTypeValue *denseSource = &stack[1].value;
    SZrTypeValue *byteDestination = &stack[2].value;

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout.isParameter = 1u;
    function.parameterCount = 1u;
    function.stackSize = 1u;
    function.frameByteSize =
            2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 5;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Value_InitAsInt(&state, denseSource, 73);
    ZrCore_Value_ResetAsNull(byteDestination);
    byteDestination->ownershipControl =
            (struct SZrOwnershipControl *)&state;
    byteDestination->ownershipWeakRef =
            (struct SZrOwnershipWeakRef *)&state;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    ZrCore_Profile_SetCurrentState(&state);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParameters(
            &state, &function, frameBase, frameBase, 1u));
    ZrCore_Profile_SetCurrentState(ZR_NULL);

    TEST_ASSERT_EQUAL_INT64(73, denseSource->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(
            73, byteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(byteDestination->ownershipControl);
    TEST_ASSERT_NULL(byteDestination->ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
}

static void test_dispatch_frame_value_slot_inline_signals_checked_fallback(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[4] = {0};
    TZrStackValuePointer frameBase = stack + 1;
    SZrTypeValue *value;

    layout.stackSlot = 0u;
    layout.byteOffset = 0u;
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout.reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    function.frameByteSize = (TZrUInt32)sizeof(SZrTypeValue);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    profileRuntime.recordHelpers = ZR_TRUE;

    value = execution_frame_value_slot_dispatch_try_direct_inline(
            &function,
            frameBase,
            0u,
            &profileRuntime,
            ZR_TRUE);
    TEST_ASSERT_EQUAL_PTR(&frameBase[0].value, value);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_CHECKED]);

    value = execution_frame_value_slot_dispatch_try_direct_inline(
            &function,
            frameBase,
            1u,
            &profileRuntime,
            ZR_TRUE);
    TEST_ASSERT_NULL(value);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_CHECKED]);
}

static void test_direct_value_copy_probe_skips_speculative_getters(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                index * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[index].reserved0 =
                ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    }
    function.frameByteSize =
            2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.stackSize = 2u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;

    TEST_ASSERT_FALSE(execution_frame_value_slot_copy_requires_inline_probe(
            &function, 0u, 1u));

    layouts[1].reserved0 = 0u;
    TEST_ASSERT_TRUE(execution_frame_value_slot_copy_requires_inline_probe(
            &function, 0u, 1u));
}

static TZrUInt32 test_dispatch_copy_probe_call_count;

static TZrBool test_dispatch_copy_probe_stub(
        SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    (void)state;
    (void)function;
    (void)frameBase;
    (void)destinationSlot;
    (void)sourceSlot;
    test_dispatch_copy_probe_call_count++;
    return ZR_FALSE;
}

static void test_dispatch_direct_value_copy_bypasses_inline_probe_helper(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};
    SZrState state = {0};
    SZrTypeValueOnStack stack[6] = {0};
    TZrStackValuePointer frameBase = stack + 1;

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                index * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].typeLayoutId =
                ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[index].reserved0 =
                ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    }
    function.frameByteSize =
            2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.stackSize = 2u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    function.directValueFrameSlotCountPlusOne = 3u;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 6;
    test_dispatch_copy_probe_call_count = 0u;

    TEST_ASSERT_FALSE(execution_inline_frame_try_copy_stack_slot_dispatch(
            &state,
            &function,
            frameBase,
            0u,
            1u,
            test_dispatch_copy_probe_stub));
    TEST_ASSERT_EQUAL_UINT32(0u, test_dispatch_copy_probe_call_count);

    layouts[0].reserved0 = 0u;
    layouts[1].reserved0 = 0u;
    TEST_ASSERT_FALSE(execution_inline_frame_try_copy_stack_slot_dispatch(
            &state,
            &function,
            frameBase,
            0u,
            1u,
            test_dispatch_copy_probe_stub));
    TEST_ASSERT_EQUAL_UINT32(0u, test_dispatch_copy_probe_call_count);

    function.directValueFrameSlotCountPlusOne = 0u;
    TEST_ASSERT_FALSE(execution_inline_frame_try_copy_stack_slot_dispatch(
            &state,
            &function,
            frameBase,
            0u,
            1u,
            test_dispatch_copy_probe_stub));
    TEST_ASSERT_EQUAL_UINT32(1u, test_dispatch_copy_probe_call_count);
}

static void test_strict_direct_value_summary_skips_precall_inline_parameter_scan(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};

    layouts[0].stackSlot = 0u;
    layouts[0].isParameter = ZR_TRUE;
    layouts[0].slotKind =
            (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[1].stackSlot = 1u;
    layouts[1].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    function.directValueFrameSlotCountPlusOne = 3u;

    TEST_ASSERT_FALSE(function_precall_has_inline_frame_parameters(&function));

    function.directValueFrameSlotCountPlusOne = 0u;
    TEST_ASSERT_TRUE(function_precall_has_inline_frame_parameters(&function));
}

static void test_direct_value_frame_summary_rejects_gapped_mirror(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (2u + index * 2u) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    function.stackSize = 2u;
    function.frameByteSize = 6u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_FALSE(ZrCore_Function_HasDirectValueFrameSlotSummary(&function));
}

static void test_packed_direct_value_slot_uses_fixed_stride_address(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};
    SZrTypeValueOnStack stack[6] = {0};
    TZrStackValuePointer frameBase = stack + 1;
    SZrTypeValue *expected;

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (2u + index) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    function.stackSize = 2u;
    function.frameByteSize = 4u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    expected = (SZrTypeValue *)((TZrByte *)frameBase +
                                3u * sizeof(SZrTypeValueOnStack));
    TEST_ASSERT_EQUAL_PTR(
            expected,
            execution_frame_value_slot_dispatch_try_packed_direct_inline(
                    &function, frameBase, 1u, ZR_NULL, ZR_FALSE));
    TEST_ASSERT_NULL(execution_frame_value_slot_dispatch_try_packed_direct_inline(
            &function, frameBase, 2u, ZR_NULL, ZR_FALSE));

    function.directValueFrameSlotCountPlusOne = 0u;
    TEST_ASSERT_NULL(execution_frame_value_slot_dispatch_try_packed_direct_inline(
            &function, frameBase, 1u, ZR_NULL, ZR_FALSE));
}
