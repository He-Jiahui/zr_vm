#include "unity.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/src/zr_vm_core/function_precall_internal.h"
#include "zr_vm_core/src/zr_vm_core/execution/execution_frame_value_slot_fast.h"
#include "zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h"

#define ZR_TEST_LAYOUT_COUNT 1024u
#ifndef ZR_TEST_DENSE_LOOKUP_ITERATIONS
#define ZR_TEST_DENSE_LOOKUP_ITERATIONS 2000000u
#endif
#ifndef ZR_TEST_SPARSE_LOOKUP_ITERATIONS
#define ZR_TEST_SPARSE_LOOKUP_ITERATIONS 20000u
#endif
#define ZR_TEST_TIMING_SAMPLE_COUNT 5u

void setUp(void) {}

void tearDown(void) {}

static TZrBool measure_lookup_ticks(const SZrFunction *function,
                                    TZrUInt32 stackSlot,
                                    TZrUInt32 iterations,
                                    volatile uint64_t *hitCount,
                                    clock_t *outTicks) {
    clock_t start;
    clock_t end;

    if (hitCount == ZR_NULL || outTicks == ZR_NULL) {
        return ZR_FALSE;
    }
    start = clock();
    if (start == (clock_t)-1) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < iterations; ++index) {
        const SZrFunctionFrameSlotLayout *layout =
                ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
        *hitCount += (uint64_t)(layout != ZR_NULL);
    }
    end = clock();
    if (end == (clock_t)-1 || end < start) {
        return ZR_FALSE;
    }
    *outTicks = end - start;
    return ZR_TRUE;
}

static void sort_lookup_ticks(clock_t *ticks) {
    for (TZrUInt32 index = 1u; index < ZR_TEST_TIMING_SAMPLE_COUNT; ++index) {
        clock_t value = ticks[index];
        TZrUInt32 insertionIndex = index;

        while (insertionIndex > 0u && ticks[insertionIndex - 1u] > value) {
            ticks[insertionIndex] = ticks[insertionIndex - 1u];
            insertionIndex--;
        }
        ticks[insertionIndex] = value;
    }
}

static void test_dense_lookup_returns_exact_canonical_entry(void) {
    SZrFunctionFrameSlotLayout layouts[3] = {0};
    SZrFunction function = {0};

    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        layouts[index].stackSlot = index;
    }
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 3u;

    TEST_ASSERT_EQUAL_PTR(
            &layouts[2], ZrCore_Function_FindFrameSlotLayout(&function, 2u));
}

static void test_reordered_in_range_lookup_returns_exact_fallback_entry(void) {
    SZrFunctionFrameSlotLayout layouts[3] = {0};
    SZrFunction function = {0};

    layouts[0].stackSlot = 1u;
    layouts[1].stackSlot = 2u;
    layouts[2].stackSlot = 0u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 3u;

    TEST_ASSERT_EQUAL_PTR(
            &layouts[0], ZrCore_Function_FindFrameSlotLayout(&function, 1u));
}

static void test_sparse_out_of_range_lookup_returns_exact_fallback_entry(void) {
    SZrFunctionFrameSlotLayout layouts[3] = {0};
    SZrFunction function = {0};

    layouts[0].stackSlot = 8u;
    layouts[1].stackSlot = 16u;
    layouts[2].stackSlot = 32u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 3u;

    TEST_ASSERT_EQUAL_PTR(
            &layouts[2], ZrCore_Function_FindFrameSlotLayout(&function, 32u));
}

static void test_out_of_range_lookup_miss_returns_null(void) {
    SZrFunctionFrameSlotLayout layouts[3] = {0};
    SZrFunction function = {0};

    layouts[0].stackSlot = 8u;
    layouts[1].stackSlot = 16u;
    layouts[2].stackSlot = 32u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 3u;

    TEST_ASSERT_NULL(ZrCore_Function_FindFrameSlotLayout(&function, 64u));
}

static void test_null_function_and_null_layouts_return_null(void) {
    SZrFunction function = {0};

    function.frameSlotLayoutLength = 1u;

    TEST_ASSERT_NULL(ZrCore_Function_FindFrameSlotLayout(ZR_NULL, 0u));
    TEST_ASSERT_NULL(ZrCore_Function_FindFrameSlotLayout(&function, 0u));
}

static void test_direct_value_slot_uses_verified_byte_offset(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrTypeValueOnStack frames[4] = {0};
    SZrTypeValue *firstValue;
    SZrTypeValue *relocatedValue;

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_TRUE((layout.reserved0 &
                      ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
    firstValue = ZrCore_Function_TryGetDirectFrameValueSlot(
            &function, frames, 0u);
    relocatedValue = ZrCore_Function_TryGetDirectFrameValueSlot(
            &function, frames + 2, 0u);
    TEST_ASSERT_EQUAL_PTR(&frames[1].value, firstValue);
    TEST_ASSERT_EQUAL_PTR(&frames[3].value, relocatedValue);
}

static void test_direct_value_frame_place_preserves_checked_boundaries(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrState state = {0};
    SZrTypeValueOnStack stack[6] = {0};
    SZrStackFramePlace place = {0};

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 6;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            &state, &function, stack + 1, 0u, &place));
    TEST_ASSERT_EQUAL_PTR(&stack[2].value, place.address);
    TEST_ASSERT_EQUAL_INT64(
            (TZrMemoryOffset)(2u * sizeof(SZrTypeValueOnStack)),
            place.byteOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrTypeValue),
                             place.byteSize);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)_Alignof(SZrTypeValue),
                             place.byteAlign);

    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(
            &state, &function, stack + 3, 0u, &place));
    TEST_ASSERT_EQUAL_PTR(&stack[4].value, place.address);
    TEST_ASSERT_EQUAL_INT64(
            (TZrMemoryOffset)(4u * sizeof(SZrTypeValueOnStack)),
            place.byteOffset);

    state.stackTail.valuePointer = stack + 4;
    TEST_ASSERT_FALSE(ZrCore_Function_MakeFrameSlotPlace(
            &state, &function, stack + 3, 0u, &place));
}

static void test_direct_value_slot_finalization_rejects_unsafe_layouts(void) {
    SZrFunctionFrameSlotLayout layouts[3] = {0};
    SZrFunction function = {0};
    TZrUInt32 minimumByteOffset =
            3u * (TZrUInt32)sizeof(SZrTypeValueOnStack);

    for (TZrUInt32 index = 0u; index < 3u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset = minimumByteOffset;
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[index].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    }
    layouts[0].reserved0 |= ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS;
    layouts[1].byteOffset++;
    layouts[2].byteOffset = UINT32_MAX;
    function.stackSize = 3u;
    function.frameByteSize = minimumByteOffset +
                             3u * (TZrUInt32)sizeof(SZrTypeValue);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 3u;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    for (TZrUInt32 index = 0u; index < 3u; index++) {
        TEST_ASSERT_FALSE((layouts[index].reserved0 &
                           ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
    }
}

static void test_direct_value_slot_rejects_untrusted_derived_flags(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};
    SZrTypeValueOnStack frames[4] = {0};
    TZrUInt32 minimumByteOffset =
            2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = 1u - index;
        layouts[index].byteOffset = minimumByteOffset;
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[index].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    }
    function.stackSize = 2u;
    function.frameByteSize = 4u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;

    TEST_ASSERT_NULL(ZrCore_Function_TryGetDirectFrameValueSlot(
            &function, frames, 1u));

    layouts[0].stackSlot = 0u;
    layouts[0].byteAlign = 3u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_NULL(ZrCore_Function_TryGetDirectFrameValueSlot(
            &function, frames, 0u));

    layouts[0].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    layouts[1].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    function.stackSize = 1u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_FALSE((layouts[0].reserved0 &
                       ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
    TEST_ASSERT_FALSE((layouts[1].reserved0 &
                       ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
}

static void test_finalized_direct_value_slot_survives_frame_initialization(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrState state = {0};
    SZrTypeValueOnStack stack[3] = {0};
    SZrTypeValue *value;

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 3;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    ZrCore_Function_InitializeFrameLayoutStorage(&state, stack, &function, 0u);

    TEST_ASSERT_TRUE((layout.reserved0 &
                      ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
    value = ZrCore_Function_TryGetDirectFrameValueSlot(&function, stack + 1, 0u);
    TEST_ASSERT_EQUAL_PTR(&stack[2].value, value);
}

static void test_direct_value_slot_skips_inline_member_probe(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameByteSize = layout.byteOffset + layout.byteSize;
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_TRUE(ZrCore_Function_IsDirectFrameValueSlot(&function, 0u));

    layout.reserved0 &= (TZrUInt16)~ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Function_IsDirectFrameValueSlot(&function, 0u));

    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layout.typeLayoutId = 1u;
    TEST_ASSERT_FALSE(ZrCore_Function_IsDirectFrameValueSlot(&function, 0u));
}

static void test_frame_value_slot_profile_helper_names_are_append_only(void) {
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_slot_direct",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)9));
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_slot_checked",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)10));
}

static void test_frame_value_parameter_copy_profile_helper_names_are_append_only(void) {
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_parameter_copy_direct",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)11));
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_parameter_copy_checked",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)12));
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_parameter_copy_empty",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)13));
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_parameter_layout_visit",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)14));
}

static void test_frame_value_drop_profile_helper_names_are_append_only(void) {
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_drop_direct",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)15));
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_drop_checked",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)16));
}

static void test_frame_value_initialization_profile_helper_names_are_append_only(void) {
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_initialization_direct",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)17));
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_initialization_checked",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)18));
}

static void test_frame_value_copy_probe_profile_helper_name_is_append_only(void) {
    TEST_ASSERT_EQUAL_STRING(
            "frame_value_copy_probe",
            ZrCore_Profile_HelperKindName((EZrProfileHelperKind)19));
}

static void test_direct_value_parameter_summary_clears_stale_metadata(void) {
    SZrFunction function = {0};

    function.directValueParameterCountPlusOne = 7u;
    function.directValueParameterScanLength = 11u;
    function.directValueFrameSlotCountPlusOne = 13u;

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_EQUAL_UINT32(0u, function.directValueParameterCountPlusOne);
    TEST_ASSERT_EQUAL_UINT32(0u, function.directValueParameterScanLength);
    TEST_ASSERT_EQUAL_UINT32(0u, function.directValueFrameSlotCountPlusOne);
}

static void test_direct_value_frame_drop_summary_requires_all_direct_value_slots(void) {
    SZrFunctionFrameSlotLayout layouts[2] = {0};
    SZrFunction function = {0};

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

    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_EQUAL_UINT32(3u, function.directValueFrameSlotCountPlusOne);

    layouts[1].slotKind =
            (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);

    TEST_ASSERT_EQUAL_UINT32(0u, function.directValueFrameSlotCountPlusOne);
}

static void test_io_loader_rebuilds_direct_value_parameter_summary(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrIoFunctionFrameSlotLayout layout;
    TZrInstruction instruction;
    SZrIoFunction ioFunction;
    SZrIoModule module;
    SZrIoSource source;
    SZrFunction *runtimeFunction;

    TEST_ASSERT_NOT_NULL(state);
    memset(&layout, 0, sizeof(layout));
    memset(&instruction, 0, sizeof(instruction));
    memset(&ioFunction, 0, sizeof(ioFunction));
    memset(&module, 0, sizeof(module));
    memset(&source, 0, sizeof(source));

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout.isParameter = 1u;
    ioFunction.parametersLength = 1u;
    ioFunction.stackSize = 1u;
    ioFunction.sourceVersionPatch =
            ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT;
    ioFunction.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    ioFunction.frameByteSize =
            layout.byteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    ioFunction.frameSlotLayoutsLength = 1u;
    ioFunction.frameSlotLayouts = &layout;
    instruction.instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_GLOBAL);
    instruction.instruction.operandExtra = 32u;
    ioFunction.instructionsLength = 1u;
    ioFunction.instructions = &instruction;
    module.entryFunction = &ioFunction;
    source.modulesLength = 1u;
    source.modules = &module;

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, &source);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_TRUE((runtimeFunction->frameSlotLayouts[0].reserved0 &
                      ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(
            2u, runtimeFunction->directValueParameterCountPlusOne);
    TEST_ASSERT_EQUAL_UINT32(
            1u, runtimeFunction->directValueParameterScanLength);
    TEST_ASSERT_EQUAL_UINT32(
            2u, runtimeFunction->directValueFrameSlotCountPlusOne);
    TEST_ASSERT_EQUAL_UINT32(
            34u, runtimeFunction->generatedFrameSlotCountPlusOne);
    TEST_ASSERT_EQUAL_UINT32(
            33u, ZrCore_Function_GetGeneratedFrameSlotCount(runtimeFunction));
    ZrCore_Function_Free(state, runtimeFunction);

    layout.reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    TEST_ASSERT_NULL(ZrCore_Io_LoadEntryFunctionToRuntime(state, &source));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_value_parameter_copy_profiles_empty_argument_fast_path(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[5] = {0};

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout.isParameter = 1u;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 5;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParameters(
            &state, &function, stack + 1, stack + 4, 0u));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_EMPTY]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_value_parameter_copy_visits_only_required_layout_prefix(void) {
    SZrFunctionFrameSlotLayout layouts[3] = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[10] = {0};

    for (TZrUInt32 index = 0u; index < 3u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (3u + index) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    layouts[0].isParameter = 1u;
    function.parameterCount = 1u;
    function.stackSize = 3u;
    function.frameByteSize =
            6u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 3u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 10;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    ZrCore_Value_InitAsInt(&state, &stack[8].value, 314);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParameters(
            &state, &function, stack, stack + 8, 1u));
    TEST_ASSERT_EQUAL_INT64(314, stack[3].value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(314, stack[0].value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_direct_value_slot_profile_counts_distinguish_direct_and_checked(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack frames[3] = {0};

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_EQUAL_PTR(
            &frames[1].value,
            ZrCore_Function_TryGetDirectFrameValueSlot(&function, frames, 0u));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_CHECKED]);

    layout.reserved0 &=
            (TZrUInt16)~ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    TEST_ASSERT_NULL(
            ZrCore_Function_TryGetDirectFrameValueSlot(&function, frames, 0u));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_CHECKED]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_value_parameter_copy_profile_counts_direct_and_checked_paths(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[8] = {0};
    TZrStackValuePointer frameBase = stack + 1;
    TZrStackValuePointer argumentBase = stack + 5;
    SZrTypeValue *byteDestination = &stack[2].value;
    SZrTypeValue *denseDestination = &stack[1].value;

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout.isParameter = 1u;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 8;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_EQUAL_UINT32(2u, function.directValueParameterCountPlusOne);
    TEST_ASSERT_EQUAL_UINT32(1u, function.directValueParameterScanLength);
    ZrCore_Value_InitAsInt(&state, &argumentBase->value, 42);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParameters(
            &state, &function, frameBase, argumentBase, 1u));
    TEST_ASSERT_EQUAL_INT64(42, byteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(42, denseDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED]);

    layout.reserved0 &=
            (TZrUInt16)~ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    TEST_ASSERT_EQUAL_UINT32(2u, function.directValueParameterCountPlusOne);
    ZrCore_Value_ResetAsNull(byteDestination);
    ZrCore_Value_ResetAsNull(denseDestination);
    ZrCore_Value_InitAsInt(&state, &argumentBase->value, 73);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParameters(
            &state, &function, frameBase, argumentBase, 1u));
    TEST_ASSERT_EQUAL_INT64(73, byteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(73, denseDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_value_parameter_copy_from_frame_requires_direct_source(void) {
    SZrFunctionFrameSlotLayout calleeLayout = {0};
    SZrFunctionFrameSlotLayout sourceLayout = {0};
    SZrFunction calleeFunction = {0};
    SZrFunction sourceFunction = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[12] = {0};
    TZrStackValuePointer sourceFrameBase = stack + 1;
    TZrStackValuePointer calleeFrameBase = stack + 5;
    SZrTypeValue *sourceValue = &stack[2].value;
    SZrTypeValue *byteDestination = &stack[6].value;
    SZrTypeValue *denseDestination = &stack[5].value;

    calleeLayout.stackSlot = 0u;
    calleeLayout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    calleeLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    calleeLayout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    calleeLayout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    calleeLayout.isParameter = 1u;
    sourceLayout = calleeLayout;
    sourceLayout.isParameter = 0u;
    calleeFunction.stackSize = 1u;
    calleeFunction.frameByteSize =
            2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    calleeFunction.frameSlotLayouts = &calleeLayout;
    calleeFunction.frameSlotLayoutLength = 1u;
    sourceFunction.stackSize = 1u;
    sourceFunction.frameByteSize =
            2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    sourceFunction.frameSlotLayouts = &sourceLayout;
    sourceFunction.frameSlotLayoutLength = 1u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 12;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&calleeFunction);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&sourceFunction);
    ZrCore_Value_InitAsInt(&state, sourceValue, 91);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParametersFromFrame(
            &state,
            &calleeFunction,
            calleeFrameBase,
            &sourceFunction,
            sourceFrameBase,
            0u,
            1u));
    TEST_ASSERT_EQUAL_INT64(91, byteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(91, denseDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED]);

    sourceLayout.reserved0 &=
            (TZrUInt16)~ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    ZrCore_Value_ResetAsNull(byteDestination);
    ZrCore_Value_ResetAsNull(denseDestination);
    ZrCore_Value_InitAsInt(&state, sourceValue, 123);
    TEST_ASSERT_TRUE(ZrCore_Function_CopyValueFrameParametersFromFrame(
            &state,
            &calleeFunction,
            calleeFrameBase,
            &sourceFunction,
            sourceFrameBase,
            0u,
            1u));
    TEST_ASSERT_EQUAL_INT64(123, byteDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT64(123, denseDestination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_frame_value_drop_profile_counts_direct_and_checked_paths(void) {
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrProfileRuntime profileRuntime = {0};
    SZrTypeValueOnStack stack[6] = {0};
    TZrStackValuePointer frameBase = stack + 1;

    layout.stackSlot = 0u;
    layout.byteOffset = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameByteSize = 2u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    global.profileRuntime = &profileRuntime;
    state.global = &global;
    state.stackBase.valuePointer = stack;
    state.stackTail.valuePointer = stack + 6;
    profileRuntime.recordHelpers = ZR_TRUE;

    ZrCore_Profile_SetCurrentState(&state);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_TRUE(ZrCore_Function_DropInlineFrameValues(
            &state, &function, frameBase, ZR_NULL, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_DROP_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_DROP_CHECKED]);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE]);

    function.directValueFrameSlotCountPlusOne = 0u;
    TEST_ASSERT_TRUE(ZrCore_Function_DropInlineFrameValues(
            &state, &function, frameBase, ZR_NULL, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_DROP_DIRECT]);
    TEST_ASSERT_EQUAL_UINT64(
            1u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_DROP_CHECKED]);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

#include "frame_slot_layout_initialization_tests.inc"

static void test_dense_frame_slot_lookup_is_constant_time(void) {
    static SZrFunctionFrameSlotLayout denseLayouts[ZR_TEST_LAYOUT_COUNT];
    static SZrFunctionFrameSlotLayout sparseLayouts[ZR_TEST_LAYOUT_COUNT];
    SZrFunction denseFunction = {0};
    SZrFunction sparseFunction = {0};
    volatile uint64_t hitCount = 0u;
    clock_t denseSamples[ZR_TEST_TIMING_SAMPLE_COUNT];
    clock_t sparseSamples[ZR_TEST_TIMING_SAMPLE_COUNT];
    clock_t denseTicks;
    clock_t sparseTicks;
    uint64_t denseScaledTicks;
    uint64_t sparseScaledTicks;

    for (TZrUInt32 index = 0u; index < ZR_TEST_LAYOUT_COUNT; ++index) {
        denseLayouts[index].stackSlot = index;
        sparseLayouts[index].stackSlot = ZR_TEST_LAYOUT_COUNT + index;
    }

    denseFunction.frameSlotLayouts = denseLayouts;
    denseFunction.frameSlotLayoutLength = ZR_TEST_LAYOUT_COUNT;
    sparseFunction.frameSlotLayouts = sparseLayouts;
    sparseFunction.frameSlotLayoutLength = ZR_TEST_LAYOUT_COUNT;

    TEST_ASSERT_TRUE_MESSAGE(
            measure_lookup_ticks(&denseFunction,
                                 ZR_TEST_LAYOUT_COUNT - 1u,
                                 ZR_TEST_DENSE_LOOKUP_ITERATIONS,
                                 &hitCount,
                                 &denseTicks),
            "clock() failed for dense lookup warm-up");
    TEST_ASSERT_TRUE_MESSAGE(
            measure_lookup_ticks(&sparseFunction,
                                 (2u * ZR_TEST_LAYOUT_COUNT) - 1u,
                                 ZR_TEST_SPARSE_LOOKUP_ITERATIONS,
                                 &hitCount,
                                 &sparseTicks),
            "clock() failed for sparse lookup warm-up");

    for (TZrUInt32 sample = 0u; sample < ZR_TEST_TIMING_SAMPLE_COUNT; ++sample) {
        if ((sample & 1u) == 0u) {
            TEST_ASSERT_TRUE_MESSAGE(
                    measure_lookup_ticks(&denseFunction,
                                         ZR_TEST_LAYOUT_COUNT - 1u,
                                         ZR_TEST_DENSE_LOOKUP_ITERATIONS,
                                         &hitCount,
                                         &denseSamples[sample]),
                    "clock() failed for dense lookup sample");
            TEST_ASSERT_TRUE_MESSAGE(
                    measure_lookup_ticks(&sparseFunction,
                                         (2u * ZR_TEST_LAYOUT_COUNT) - 1u,
                                         ZR_TEST_SPARSE_LOOKUP_ITERATIONS,
                                         &hitCount,
                                         &sparseSamples[sample]),
                    "clock() failed for sparse lookup sample");
        } else {
            TEST_ASSERT_TRUE_MESSAGE(
                    measure_lookup_ticks(&sparseFunction,
                                         (2u * ZR_TEST_LAYOUT_COUNT) - 1u,
                                         ZR_TEST_SPARSE_LOOKUP_ITERATIONS,
                                         &hitCount,
                                         &sparseSamples[sample]),
                    "clock() failed for sparse lookup sample");
            TEST_ASSERT_TRUE_MESSAGE(
                    measure_lookup_ticks(&denseFunction,
                                         ZR_TEST_LAYOUT_COUNT - 1u,
                                         ZR_TEST_DENSE_LOOKUP_ITERATIONS,
                                         &hitCount,
                                         &denseSamples[sample]),
                    "clock() failed for dense lookup sample");
        }
        printf("frame-slot lookup sample %u: dense=%lld ticks/%u iterations, "
               "sparse=%lld ticks/%u iterations\n",
               (unsigned int)(sample + 1u),
               (long long)denseSamples[sample],
               (unsigned int)ZR_TEST_DENSE_LOOKUP_ITERATIONS,
               (long long)sparseSamples[sample],
               (unsigned int)ZR_TEST_SPARSE_LOOKUP_ITERATIONS);
    }

    sort_lookup_ticks(denseSamples);
    sort_lookup_ticks(sparseSamples);
    denseTicks = denseSamples[ZR_TEST_TIMING_SAMPLE_COUNT / 2u];
    sparseTicks = sparseSamples[ZR_TEST_TIMING_SAMPLE_COUNT / 2u];
    printf("frame-slot lookup median: dense=%lld ticks/%u iterations, "
           "sparse=%lld ticks/%u iterations\n",
           (long long)denseTicks,
           (unsigned int)ZR_TEST_DENSE_LOOKUP_ITERATIONS,
           (long long)sparseTicks,
           (unsigned int)ZR_TEST_SPARSE_LOOKUP_ITERATIONS);

    TEST_ASSERT_EQUAL_UINT64(
            ((uint64_t)ZR_TEST_DENSE_LOOKUP_ITERATIONS +
             (uint64_t)ZR_TEST_SPARSE_LOOKUP_ITERATIONS) *
                    ((uint64_t)ZR_TEST_TIMING_SAMPLE_COUNT + 1u),
            hitCount);
    TEST_ASSERT_TRUE_MESSAGE(denseTicks > 0,
                             "dense lookup median must be measurable");
    TEST_ASSERT_TRUE_MESSAGE(sparseTicks > 0,
                             "sparse lookup median must be measurable");
    TEST_ASSERT_TRUE_MESSAGE(
            (uint64_t)denseTicks <=
                    UINT64_MAX / (uint64_t)ZR_TEST_SPARSE_LOOKUP_ITERATIONS / 4u,
            "dense normalized timing comparison would overflow");
    TEST_ASSERT_TRUE_MESSAGE(
            (uint64_t)sparseTicks <=
                    UINT64_MAX / (uint64_t)ZR_TEST_DENSE_LOOKUP_ITERATIONS,
            "sparse normalized timing comparison would overflow");
    denseScaledTicks = (uint64_t)denseTicks *
                       (uint64_t)ZR_TEST_SPARSE_LOOKUP_ITERATIONS * 4u;
    sparseScaledTicks = (uint64_t)sparseTicks *
                        (uint64_t)ZR_TEST_DENSE_LOOKUP_ITERATIONS;
    TEST_ASSERT_TRUE_MESSAGE(
            denseScaledTicks < sparseScaledTicks,
            "dense frame-slot lookup must avoid the linear sparse-layout scan");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dense_lookup_returns_exact_canonical_entry);
    RUN_TEST(test_reordered_in_range_lookup_returns_exact_fallback_entry);
    RUN_TEST(test_sparse_out_of_range_lookup_returns_exact_fallback_entry);
    RUN_TEST(test_out_of_range_lookup_miss_returns_null);
    RUN_TEST(test_null_function_and_null_layouts_return_null);
    RUN_TEST(test_direct_value_slot_uses_verified_byte_offset);
    RUN_TEST(test_direct_value_frame_place_preserves_checked_boundaries);
    RUN_TEST(test_direct_value_slot_finalization_rejects_unsafe_layouts);
    RUN_TEST(test_direct_value_slot_rejects_untrusted_derived_flags);
    RUN_TEST(test_finalized_direct_value_slot_survives_frame_initialization);
    RUN_TEST(test_direct_value_slot_skips_inline_member_probe);
    RUN_TEST(test_frame_value_slot_profile_helper_names_are_append_only);
    RUN_TEST(test_frame_value_parameter_copy_profile_helper_names_are_append_only);
    RUN_TEST(test_frame_value_drop_profile_helper_names_are_append_only);
    RUN_TEST(test_frame_value_initialization_profile_helper_names_are_append_only);
    RUN_TEST(test_frame_value_copy_probe_profile_helper_name_is_append_only);
    RUN_TEST(test_direct_value_parameter_summary_clears_stale_metadata);
    RUN_TEST(test_direct_value_frame_drop_summary_requires_all_direct_value_slots);
    RUN_TEST(test_direct_value_frame_summary_rejects_gapped_mirror);
    RUN_TEST(test_packed_direct_value_slot_uses_fixed_stride_address);
    RUN_TEST(test_io_loader_rebuilds_direct_value_parameter_summary);
    RUN_TEST(test_value_parameter_copy_profiles_empty_argument_fast_path);
    RUN_TEST(test_value_parameter_copy_visits_only_required_layout_prefix);
    RUN_TEST(test_direct_value_slot_profile_counts_distinguish_direct_and_checked);
    RUN_TEST(test_value_parameter_copy_profile_counts_direct_and_checked_paths);
    RUN_TEST(test_value_parameter_copy_from_frame_requires_direct_source);
    RUN_TEST(test_frame_value_drop_profile_counts_direct_and_checked_paths);
    RUN_TEST(test_frame_value_initialization_preserves_direct_and_checked_arguments);
    RUN_TEST(test_direct_value_frame_drop_preflights_complete_frame_span);
    RUN_TEST(test_packed_direct_value_frame_drop_keeps_batched_owner_release);
    RUN_TEST(test_direct_value_parameter_copy_preflights_complete_frame_span);
    RUN_TEST(test_direct_value_parameter_copy_keeps_dense_source_in_place);
    RUN_TEST(test_dispatch_frame_value_slot_inline_signals_checked_fallback);
    RUN_TEST(test_direct_value_copy_probe_skips_speculative_getters);
    RUN_TEST(test_dispatch_direct_value_copy_bypasses_inline_probe_helper);
    RUN_TEST(test_strict_direct_value_summary_skips_precall_inline_parameter_scan);
    RUN_TEST(test_dense_frame_slot_lookup_is_constant_time);
    return UNITY_END();
}
