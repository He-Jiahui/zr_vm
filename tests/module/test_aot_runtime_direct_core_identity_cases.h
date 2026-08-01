#ifndef ZR_VM_TEST_AOT_RUNTIME_DIRECT_CORE_IDENTITY_CASES_H
#define ZR_VM_TEST_AOT_RUNTIME_DIRECT_CORE_IDENTITY_CASES_H

#include <string.h>

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/type_layout.h"

typedef struct TestAotDirectCoreIdentityFixture {
    SZrState *state;
    SZrFunction *caller;
    SZrFunction *callee;
    SZrFunction *replacement;
    SZrFunction *functionTable[2];
    FZrAotEntryThunk functionThunks[2];
    SZrTypeLayout returnLayout;
    const SZrTypeLayout *typeLayouts[1];
    SZrAotCodeRegistration codeRegistration;
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer callerFunctionBase;
    TZrStackValuePointer callerFrameBase;
    SZrRawObject *calleeClosureObject;
    ZrAotGeneratedFrame frame;
} TestAotDirectCoreIdentityFixture;

static TZrUInt32 g_directCoreIdentityThunkCallCount;

static TZrInt64 direct_core_identity_snapshot_thunk(struct SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 0;
}

static TZrInt64 direct_core_identity_observed_thunk(struct SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    g_directCoreIdentityThunkCallCount++;
    return 0;
}

static void initialize_direct_core_identity_fixture(TestAotDirectCoreIdentityFixture *fixture) {
    SZrClosureNative *calleeClosure;
    TZrUInt32 slotIndex;

    TEST_ASSERT_NOT_NULL(fixture);
    memset(fixture, 0, sizeof(*fixture));
    fixture->state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(fixture->state);

    fixture->caller = ZrCore_Function_New(fixture->state);
    fixture->callee = ZrCore_Function_New(fixture->state);
    fixture->replacement = ZrCore_Function_New(fixture->state);
    TEST_ASSERT_NOT_NULL(fixture->caller);
    TEST_ASSERT_NOT_NULL(fixture->callee);
    TEST_ASSERT_NOT_NULL(fixture->replacement);
    fixture->caller->stackSize = 2u;
    fixture->callee->stackSize = 0u;
    fixture->callee->parameterCount = 0u;

    ZrCore_TypeLayout_InitStruct(&fixture->returnLayout,
                                 (TZrUInt32)sizeof(TZrUInt64),
                                 (TZrUInt32)_Alignof(TZrUInt64),
                                 ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 ZR_NULL,
                                 0u);
    fixture->typeLayouts[0] = &fixture->returnLayout;
    fixture->codeRegistration.typeLayouts = fixture->typeLayouts;
    fixture->codeRegistration.typeLayoutCount = 1u;
    fixture->caller->metadataCodeRegistration = &fixture->codeRegistration;
    fixture->caller->metadataTypeLayoutCount = 1u;

    fixture->callerFunctionBase = ZrCore_Function_CheckStackAndGc(
            fixture->state, 16u, fixture->state->stackTop.valuePointer);
    TEST_ASSERT_NOT_NULL(fixture->callerFunctionBase);
    for (slotIndex = 0u; slotIndex < 16u; slotIndex++) {
        ZrCore_Value_ResetAsNull(
                ZrCore_Stack_GetValue(fixture->callerFunctionBase + slotIndex));
    }

    fixture->callerFrameBase = fixture->callerFunctionBase + 1u;
    calleeClosure = ZrCore_ClosureNative_New(fixture->state, 0u);
    TEST_ASSERT_NOT_NULL(calleeClosure);
    calleeClosure->aotShimFunction = fixture->callee;
    fixture->calleeClosureObject = ZR_CAST_RAW_OBJECT_AS_SUPER(calleeClosure);
    ZrCore_Value_InitAsRawObject(
            fixture->state,
            ZrCore_Stack_GetValue(fixture->callerFrameBase),
            fixture->calleeClosureObject);
    ZrCore_Stack_GetValue(fixture->callerFrameBase)->type = ZR_VALUE_TYPE_CLOSURE;
    ZrCore_Stack_GetValue(fixture->callerFrameBase)->isNative = ZR_TRUE;
    ZrCore_Stack_GetValue(fixture->callerFrameBase)->isGarbageCollectable = ZR_TRUE;
    ZrCore_Value_InitAsInt(
            fixture->state,
            ZrCore_Stack_GetValue(fixture->callerFrameBase + 1u),
            (TZrInt64)0x12345678);

    fixture->callerCallInfo = &fixture->state->baseCallInfo;
    memset(fixture->callerCallInfo, 0, sizeof(*fixture->callerCallInfo));
    fixture->callerCallInfo->metadataFunction = fixture->caller;
    fixture->callerCallInfo->functionBase.valuePointer = fixture->callerFunctionBase;
    fixture->callerCallInfo->functionTop.valuePointer = fixture->callerFrameBase + 2u;
    fixture->state->callInfoList = fixture->callerCallInfo;
    fixture->state->stackTop.valuePointer = fixture->callerCallInfo->functionTop.valuePointer;

    fixture->functionTable[0] = fixture->caller;
    fixture->functionTable[1] = fixture->callee;
    fixture->functionThunks[0] = direct_core_identity_snapshot_thunk;
    fixture->functionThunks[1] = direct_core_identity_snapshot_thunk;
    fixture->frame.function = fixture->caller;
    fixture->frame.callInfo = fixture->callerCallInfo;
    fixture->frame.slotBase = fixture->callerFrameBase;
    fixture->frame.functionIndex = 0u;
    fixture->frame.generatedFrameSlotCount = 2u;
    fixture->frame.functionTable = fixture->functionTable;
    fixture->frame.functionCount = 2u;
    fixture->frame.functionThunks = fixture->functionThunks;
    fixture->frame.functionThunkCount = 2u;
}

static void assert_direct_core_identity_failure_preserves_caller(
        TestAotDirectCoreIdentityFixture *fixture,
        TZrBool callResult,
        SZrFunction *expectedGeneratedFunction,
        FZrAotEntryThunk expectedGeneratedThunk) {
    SZrTypeValue *callableValue;
    SZrTypeValue *destinationValue;

    TEST_ASSERT_NOT_NULL(fixture);
    callableValue = ZrCore_Stack_GetValue(fixture->callerFrameBase);
    destinationValue = ZrCore_Stack_GetValue(fixture->callerFrameBase + 1u);
    TEST_ASSERT_FALSE(callResult);
    TEST_ASSERT_EQUAL_UINT32(0u, g_directCoreIdentityThunkCallCount);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, fixture->state->threadStatus);
    TEST_ASSERT_EQUAL_PTR(fixture->callerCallInfo, fixture->state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(fixture->callerFunctionBase,
                          fixture->callerCallInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(fixture->callerFrameBase + 2u,
                          fixture->callerCallInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(fixture->callerFrameBase + 2u,
                          fixture->state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(fixture->callerCallInfo, fixture->frame.callInfo);
    TEST_ASSERT_EQUAL_PTR(fixture->callerFrameBase, fixture->frame.slotBase);
    TEST_ASSERT_EQUAL_PTR(expectedGeneratedFunction, fixture->frame.functionTable[1]);
    TEST_ASSERT_EQUAL_PTR(expectedGeneratedThunk, fixture->frame.functionThunks[1]);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, callableValue->type);
    TEST_ASSERT_TRUE(callableValue->isNative);
    TEST_ASSERT_EQUAL_PTR(fixture->calleeClosureObject, callableValue->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, destinationValue->type);
    TEST_ASSERT_EQUAL_INT64((TZrInt64)0x12345678,
                            destinationValue->value.nativeObject.nativeInt64);
}

static void run_direct_core_identity_drift_case(TZrBool inlineStruct, TZrBool metadataDrift) {
    TestAotDirectCoreIdentityFixture fixture;
    TZrBool callResult;

    initialize_direct_core_identity_fixture(&fixture);
    g_directCoreIdentityThunkCallCount = 0u;
    if (metadataDrift) {
        fixture.functionTable[1] = fixture.replacement;
        fixture.functionThunks[1] = direct_core_identity_observed_thunk;
    } else {
        fixture.functionThunks[1] = direct_core_identity_snapshot_thunk;
    }

    if (inlineStruct) {
        callResult = ZrLibrary_AotRuntime_CallInlineStruct(
                fixture.state,
                &fixture.frame,
                1u,
                0u,
                0u,
                1u,
                0u,
                (TZrUInt32)sizeof(SZrTypeValueOnStack),
                (TZrUInt32)sizeof(TZrUInt64),
                direct_core_identity_observed_thunk);
    } else {
        callResult = ZrLibrary_AotRuntime_CallStaticDirect(
                fixture.state,
                &fixture.frame,
                1u,
                0u,
                0u,
                1u,
                direct_core_identity_observed_thunk);
    }

    assert_direct_core_identity_failure_preserves_caller(
            &fixture,
            callResult,
            metadataDrift ? fixture.replacement : fixture.callee,
            metadataDrift ? direct_core_identity_observed_thunk
                          : direct_core_identity_snapshot_thunk);
    ZrTests_Runtime_State_Destroy(fixture.state);
}

static void test_static_direct_core_rejects_metadata_generation_drift_before_thunk(void) {
    run_direct_core_identity_drift_case(ZR_FALSE, ZR_TRUE);
}

static void test_static_direct_core_rejects_thunk_generation_drift_before_thunk(void) {
    run_direct_core_identity_drift_case(ZR_FALSE, ZR_FALSE);
}

static void test_inline_struct_direct_core_rejects_metadata_generation_drift_before_thunk(void) {
    run_direct_core_identity_drift_case(ZR_TRUE, ZR_TRUE);
}

static void test_inline_struct_direct_core_rejects_thunk_generation_drift_before_thunk(void) {
    run_direct_core_identity_drift_case(ZR_TRUE, ZR_FALSE);
}

#endif
