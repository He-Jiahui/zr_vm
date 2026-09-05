#ifndef ZR_TEST_OWNERSHIP_AOT_PENDING_CASES_H
#define ZR_TEST_OWNERSHIP_AOT_PENDING_CASES_H

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_library/aot_runtime.h"

static TZrUInt32 g_aot_pending_drop_count;
static TZrBool g_aot_pending_stack_relocated;
static TZrBool g_aot_pending_mutate_payload;
static SZrCallInfo *g_aot_pending_caller;
static FZrAllocator g_aot_pending_allocator;

static TZrPtr aot_pending_moving_stack_allocator(TZrPtr userData, TZrPtr pointer,
                                                 TZrSize originalSize, TZrSize newSize,
                                                 TZrInt64 flag) {
    if (flag == ZR_MEMORY_NATIVE_TYPE_STACK && pointer != ZR_NULL && newSize > originalSize) {
        TZrPtr replacement = g_aot_pending_allocator(userData, ZR_NULL, 0u, newSize, flag);
        if (replacement != ZR_NULL) {
            memcpy(replacement, pointer, originalSize);
            g_aot_pending_allocator(userData, pointer, originalSize, 0u, flag);
        }
        return replacement;
    }
    return g_aot_pending_allocator(userData, pointer, originalSize, newSize, flag);
}

static TZrInt64 aot_pending_relocating_drop(SZrState *state) {
    TZrStackValuePointer previousBase = state->stackBase.valuePointer;
    TZrSize previousSize = (TZrSize)(state->stackTail.valuePointer - previousBase);
    ++g_aot_pending_drop_count;
    if (g_aot_pending_mutate_payload) {
        ZrCore_Value_InitAsInt(state,
                ZrCore_Stack_GetValue(g_aot_pending_caller->functionBase.valuePointer + 1), 99);
    }
    /* Keep the old stack allocated until its replacement exists. */
    g_aot_pending_allocator = state->global->allocator;
    state->global->allocator = aot_pending_moving_stack_allocator;
    (void)ZrCore_Function_CheckStack(state, previousSize + 4096u, state->stackTop.valuePointer);
    state->global->allocator = g_aot_pending_allocator;
    g_aot_pending_stack_relocated = (TZrBool)(state->stackBase.valuePointer != previousBase);
    return 0;
}

static void aot_pending_prepare_source_frame(ZrAotGeneratedFrame *frame, const char *source) {
    SZrFunction *function = compile_source(source);
    SZrCallInfo *callInfo = g_state->callInfoList;
    TZrStackValuePointer functionBase;
    TEST_ASSERT_NOT_NULL(function);
    functionBase = ZrCore_Function_CheckStack(g_state, 65u, callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(functionBase);
    for (TZrUInt32 index = 0u; index < 65u; ++index) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase + index));
    }
    ZrCore_Value_InitAsRawObject(g_state, ZrCore_Stack_GetValue(functionBase),
                                ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callInfo->metadataFunction = function;
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = functionBase + 65u;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->context.context.programCounter = function->instructionsList;
    g_state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    memset(frame, 0, sizeof(*frame));
    frame->function = function;
    frame->callInfo = callInfo;
    frame->slotBase = functionBase + 1;
    frame->generatedFrameSlotCount = 64u;
    g_aot_pending_caller = callInfo;
}

static void aot_pending_prepare_frame(ZrAotGeneratedFrame *frame) {
    aot_pending_prepare_source_frame(frame, "try { return 0; } catch (error) { return 1; }\n");
}

static void aot_pending_prepare_owner(void) {
    SZrTypeValue shared;
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0u);
    SZrObject *object;
    TEST_ASSERT_NOT_NULL(destructor);
    pending_create_shared(&shared);
    object = (SZrObject *)shared.value.object;
    destructor->nativeFunction = aot_pending_relocating_drop;
    ZrCore_RawObject_MarkAsPermanent(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_ObjectPrototype_AddMeta(g_state, object->prototype, ZR_META_DESTRUCTOR,
                                  (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    pending_set_return(&shared);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    g_aot_pending_drop_count = 0u;
    g_aot_pending_stack_relocated = ZR_FALSE;
    g_aot_pending_mutate_payload = ZR_FALSE;
}

static void test_aot_catch_refreshes_frame_after_pending_drop(void) {
    ZrAotGeneratedFrame frame;
    aot_pending_prepare_frame(&frame);
    aot_pending_prepare_owner();
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_Catch(g_state, &frame, 0u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_pending_drop_count);
    TEST_ASSERT_TRUE(g_aot_pending_stack_relocated);
    TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionBase.valuePointer + 1, frame.slotBase);
    TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionTop.valuePointer, g_state->stackTop.valuePointer);
}

static void test_aot_end_finally_refreshes_frame_after_discarded_pending_drop(void) {
    ZrAotGeneratedFrame frame;
    TZrUInt32 resumeIndex;
    aot_pending_prepare_frame(&frame);
    aot_pending_prepare_owner();
    g_state->pendingControl.kind = (EZrVmPendingControlKind)(ZR_VM_PENDING_CONTROL_CONTINUE + 1);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_EndFinally(g_state, &frame, 0u, &resumeIndex));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_pending_drop_count);
    TEST_ASSERT_TRUE(g_aot_pending_stack_relocated);
    TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionBase.valuePointer + 1, frame.slotBase);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_RUNTIME_RESUME_FALLTHROUGH, resumeIndex);
}

static void test_aot_throw_normalizes_payload_before_pending_drop(void) {
    ZrAotGeneratedFrame frame;
    SZrTypeValue key;
    const SZrTypeValue *payload;
    SZrString *field;
    TZrUInt32 resumeIndex;
    aot_pending_prepare_frame(&frame);
    aot_pending_prepare_owner();
    ZrCore_Value_InitAsInt(g_state, ZrCore_Stack_GetValue(frame.slotBase), 73);
    g_aot_pending_mutate_payload = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_Try(g_state, &frame, 0u));
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_Throw(g_state, &frame, 0u, &resumeIndex));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_pending_drop_count);
    TEST_ASSERT_TRUE(g_aot_pending_stack_relocated);
    TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionBase.valuePointer + 1, frame.slotBase);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    field = ZrCore_String_CreateFromNative(g_state, "exception");
    TEST_ASSERT_NOT_NULL(field);
    ZrCore_Value_InitAsRawObject(g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(field));
    payload = ZrCore_Object_GetValue(g_state, (SZrObject *)g_state->currentException.value.object, &key);
    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(payload->type));
    TEST_ASSERT_EQUAL_INT64(73, payload->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(frame.function->catchClauseList[0].targetInstructionOffset, resumeIndex);
}

static void test_aot_return_refreshes_frame_after_discarded_finally_drop(void) {
    ZrAotGeneratedFrame frame;
    SZrVmExceptionHandlerState *handler;
    aot_pending_prepare_frame(&frame);
    aot_pending_prepare_owner();
    ZrCore_Value_InitAsInt(g_state, ZrCore_Stack_GetValue(frame.slotBase), 73);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_Try(g_state, &frame, 0u));
    handler = execution_find_handler_state(g_state, frame.callInfo, 0u);
    TEST_ASSERT_NOT_NULL(handler);
    execution_enter_finally(g_state, handler);
    TEST_ASSERT_EQUAL_INT64(1, ZrLibrary_AotRuntime_Return(g_state, &frame, 0u, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_pending_drop_count);
    TEST_ASSERT_TRUE(g_aot_pending_stack_relocated);
    TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionBase.valuePointer + 1u, frame.slotBase);
    TEST_ASSERT_EQUAL_INT64(73, ZrCore_Stack_GetValue(
            frame.callInfo->functionBase.valuePointer)->value.nativeObject.nativeInt64);
}

#endif
