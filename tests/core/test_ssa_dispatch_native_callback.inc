/* Native callback fixture for stack growth followed by a minor collection. */
#include <stdint.h>

typedef struct ZrDispatchNativeBoundaryCapture {
    TZrInstruction *expectedInstructions;
    uintptr_t stackBaseBefore;
    uintptr_t stackBaseAfterGrowth;
    uintptr_t callerFrameBaseBefore;
    uintptr_t callerFrameBaseAfterGrowth;
    uintptr_t rootObjectAfterGc;
    TZrSize stackCapacityBefore;
    TZrSize stackCapacityAfterGrowth;
    TZrUInt32 rootRegionBeforeGc;
    TZrUInt32 rootStorageBeforeGc;
    TZrUInt32 callbackCount;
    TZrUInt32 callerProgramCounterBefore;
    TZrUInt32 callerProgramCounterAfterGc;
    TZrUInt32 rootRegionAfterGc;
    TZrUInt32 rootStorageAfterGc;
    TZrUInt64 minorCollectionCountBefore;
    TZrUInt64 minorCollectionCountAfter;
    EZrGarbageCollectCollectionKind collectionKindAfterGc;
    TZrBool stackGrowthSucceeded;
    TZrBool callerFrameWasReloadedAfterGrowth;
    TZrBool callerFrameWasReloadedAfterGc;
    TZrBool rootValueSurvived;
} ZrDispatchNativeBoundaryCapture;

static ZrDispatchNativeBoundaryCapture *gDispatchNativeBoundaryCapture = ZR_NULL;

static TZrInt64 dispatch_test_native_grow_stack_and_collect(SZrState *state) {
    ZrDispatchNativeBoundaryCapture *capture = gDispatchNativeBoundaryCapture;
    SZrCallInfo *nativeCallInfo;
    SZrCallInfo *callerCallInfo;
    SZrGarbageCollector *collector;
    TZrStackValuePointer callerFrameBase;
    TZrMemoryOffset callerFrameOffset;
    TZrSize requestedStackSize;
    SZrTypeValue *rootValue;

    if (capture == ZR_NULL || capture->expectedInstructions == ZR_NULL || state == ZR_NULL ||
        state->callInfoList == ZR_NULL) {
        return 0;
    }
    ++capture->callbackCount;
    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo->previous;
    if (callerCallInfo == ZR_NULL || callerCallInfo->functionBase.valuePointer == ZR_NULL ||
        callerCallInfo->context.context.programCounter == ZR_NULL) {
        return 0;
    }

    callerFrameBase = callerCallInfo->functionBase.valuePointer;
    callerFrameOffset = ZrCore_Stack_SavePointerAsOffset(state, callerFrameBase);
    capture->callerFrameBaseBefore = (uintptr_t)(void *)callerFrameBase;
    capture->callerProgramCounterBefore =
            callerCallInfo->context.context.programCounter == capture->expectedInstructions + 1u
                    ? 1u
                    : (TZrUInt32)-1;
    rootValue = ZrCore_Stack_GetValue(callerFrameBase + 2u);
    if (rootValue == ZR_NULL || rootValue->type != ZR_VALUE_TYPE_OBJECT ||
        rootValue->value.object == ZR_NULL) {
        return 0;
    }
    capture->rootRegionBeforeGc = rootValue->value.object->garbageCollectMark.regionKind;
    capture->rootStorageBeforeGc = rootValue->value.object->garbageCollectMark.storageKind;
    capture->stackBaseBefore = (uintptr_t)(void *)state->stackBase.valuePointer;
    capture->stackCapacityBefore =
            (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer);

    requestedStackSize = capture->stackCapacityBefore * 2u;
    capture->stackGrowthSucceeded = ZrCore_Stack_GrowTo(state, requestedStackSize, ZR_TRUE);
    capture->stackBaseAfterGrowth = (uintptr_t)(void *)state->stackBase.valuePointer;
    capture->stackCapacityAfterGrowth =
            (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer);
    callerCallInfo = state->callInfoList != ZR_NULL ? state->callInfoList->previous : ZR_NULL;
    if (callerCallInfo != ZR_NULL && callerCallInfo->functionBase.valuePointer != ZR_NULL) {
        callerFrameBase = ZrCore_Stack_LoadOffsetToPointer(state, callerFrameOffset);
        capture->callerFrameBaseAfterGrowth =
                (uintptr_t)(void *)callerCallInfo->functionBase.valuePointer;
        capture->callerFrameWasReloadedAfterGrowth =
                (TZrBool)(callerCallInfo->functionBase.valuePointer == callerFrameBase);
    }

    collector = state->global != ZR_NULL ? state->global->garbageCollector : ZR_NULL;
    if (collector == ZR_NULL) {
        return 0;
    }
    capture->minorCollectionCountBefore = collector->statsSnapshot.minorCollectionCount;
    collector->gcDebtSize = 4096;
    collector->gcLastStepWork = 0;
    ZrCore_GarbageCollector_GcStep(state);
    capture->minorCollectionCountAfter = collector->statsSnapshot.minorCollectionCount;
    capture->collectionKindAfterGc = collector->statsSnapshot.lastCollectionKind;

    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo != ZR_NULL ? nativeCallInfo->previous : ZR_NULL;
    if (callerCallInfo != ZR_NULL && callerCallInfo->functionBase.valuePointer != ZR_NULL &&
        callerCallInfo->context.context.programCounter != ZR_NULL) {
        callerFrameBase = ZrCore_Stack_LoadOffsetToPointer(state, callerFrameOffset);
        capture->callerFrameBaseAfterGrowth =
                (uintptr_t)(void *)callerCallInfo->functionBase.valuePointer;
        capture->callerFrameWasReloadedAfterGc =
                (TZrBool)(callerCallInfo->functionBase.valuePointer == callerFrameBase);
        capture->callerProgramCounterAfterGc =
                callerCallInfo->context.context.programCounter == capture->expectedInstructions + 1u
                        ? 1u
                        : (TZrUInt32)-1;
        rootValue = ZrCore_Stack_GetValue(callerFrameBase + 2u);
        if (rootValue != ZR_NULL && rootValue->type == ZR_VALUE_TYPE_OBJECT &&
            rootValue->isGarbageCollectable && rootValue->value.object != ZR_NULL) {
            capture->rootObjectAfterGc = (uintptr_t)(void *)rootValue->value.object;
            capture->rootRegionAfterGc = rootValue->value.object->garbageCollectMark.regionKind;
            capture->rootStorageAfterGc = rootValue->value.object->garbageCollectMark.storageKind;
            capture->rootValueSurvived = ZR_TRUE;
        }
    }
    return 0;
}

static void test_native_callback_growth_and_minor_gc_reloads_frame_and_pc(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrClosureNative *nativeClosure = ZR_NULL;
    SZrObject *rootObject = ZR_NULL;
    TZrInstruction *instructions = ZR_NULL;
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrTypeValue callableValue;
    SZrTypeValue *frameValue;
    SZrCallInfo *callInfo = ZR_NULL;
    SZrGarbageCollector *collector = ZR_NULL;
    ZrDispatchNativeBoundaryCapture capture;
    EZrThreadStatus finalThreadStatus = ZR_THREAD_STATUS_FINE;
    TZrUInt32 finalResumeOffset = (TZrUInt32)-1;
    uintptr_t returnedObjectAddress = 0u;

    ZrCore_Memory_RawSet(&capture, 0, sizeof(capture));
    TEST_ASSERT_NOT_NULL(state);
    collector = state->global->garbageCollector;
    TEST_ASSERT_NOT_NULL(collector);
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(instructions);
    ZrCore_Memory_RawSet(instructions, 0, sizeof(TZrInstruction) * 3u);
    instructions[0].instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL);
    instructions[0].instruction.operandExtra = 2u;
    instructions[0].instruction.operand.operand1[0] = 0u;
    instructions[0].instruction.operand.operand1[1] = 0u;
    instructions[1].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_STACK);
    instructions[1].instruction.operandExtra = 3u;
    instructions[1].instruction.operand.operand2[0] = 1;
    instructions[2].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instructions[2].instruction.operandExtra = 1u;
    instructions[2].instruction.operand.operand1[0] = 3u;
    function->instructionsList = instructions;
    function->instructionsLength = 3u;
    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0u;
    function->stackSize = 4u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;

    nativeClosure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = dispatch_test_native_grow_stack_and_collect;
    rootObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(rootObject);
    ZrCore_Object_Init(state, rootObject);

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(
            state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;
    functionBase = ZrCore_Function_CheckStackAndGc(
            state, (TZrSize)(1u + function->stackSize), state->stackTop.valuePointer);
    TEST_ASSERT_NOT_NULL(functionBase);
    frameValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(frameValue);
    ZrCore_Value_Copy(state, frameValue, &callableValue);
    frameValue = ZrCore_Stack_GetValue(functionBase + 1u);
    TEST_ASSERT_NOT_NULL(frameValue);
    ZrCore_Value_InitAsRawObject(
            state, frameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, frameValue->type);
    TEST_ASSERT_TRUE(frameValue->isNative);
    frameValue = ZrCore_Stack_GetValue(functionBase + 2u);
    TEST_ASSERT_NOT_NULL(frameValue);
    ZrCore_Value_InitAsRawObject(
            state, frameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(rootObject));
    frameValue->type = ZR_VALUE_TYPE_OBJECT;
    frameValue->isGarbageCollectable = ZR_TRUE;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase + 3u));
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase + 4u));
    state->stackTop.valuePointer = functionBase + 1u + function->stackSize;

    callInfo = ZrCore_CallInfo_Extend(state);
    TEST_ASSERT_NOT_NULL(callInfo);
    ZrCore_CallInfo_EntryNativeInit(
            state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = functionBase + 1u + function->stackSize;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1u;
    state->callInfoList = callInfo;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    capture.expectedInstructions = function->instructionsList;
    rootObject = ZR_NULL;
    gDispatchNativeBoundaryCapture = &capture;

    ZrCore_Execute(state, callInfo);

    gDispatchNativeBoundaryCapture = ZR_NULL;
    finalThreadStatus = state->threadStatus;
    if (callInfo != ZR_NULL && capture.expectedInstructions != ZR_NULL) {
        finalResumeOffset = (TZrUInt32)(callInfo->context.context.programCounter -
                                        capture.expectedInstructions);
    }
    frameValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (frameValue != ZR_NULL && frameValue->type == ZR_VALUE_TYPE_OBJECT) {
        returnedObjectAddress = (uintptr_t)(void *)frameValue->value.object;
    }
    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_EQUAL_UINT32(1u, capture.callbackCount);
    TEST_ASSERT_TRUE(capture.stackGrowthSucceeded);
    TEST_ASSERT_GREATER_THAN_UINT64(
            capture.stackCapacityBefore, capture.stackCapacityAfterGrowth);
    if (capture.stackBaseBefore != capture.stackBaseAfterGrowth) {
        TEST_ASSERT_NOT_EQUAL(
                capture.callerFrameBaseBefore, capture.callerFrameBaseAfterGrowth);
    }
    TEST_ASSERT_EQUAL_UINT32(1u, capture.callerProgramCounterBefore);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.callerProgramCounterAfterGc);
    TEST_ASSERT_TRUE(capture.callerFrameWasReloadedAfterGrowth);
    TEST_ASSERT_TRUE(capture.callerFrameWasReloadedAfterGc);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_REGION_KIND_EDEN, capture.rootRegionBeforeGc);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE, capture.rootStorageBeforeGc);
    TEST_ASSERT_EQUAL_UINT64(
            capture.minorCollectionCountBefore + 1u, capture.minorCollectionCountAfter);
    TEST_ASSERT_EQUAL_INT(
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR, capture.collectionKindAfterGc);
    TEST_ASSERT_TRUE(capture.rootValueSurvived);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR, capture.rootRegionAfterGc);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE, capture.rootStorageAfterGc);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, finalThreadStatus);
    TEST_ASSERT_EQUAL_UINT32(2u, finalResumeOffset);
    TEST_ASSERT_EQUAL_UINT64(capture.rootObjectAfterGc, returnedObjectAddress);
}
