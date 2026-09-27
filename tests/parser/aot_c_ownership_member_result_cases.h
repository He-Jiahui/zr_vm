#ifndef ZR_TEST_AOT_C_OWNERSHIP_MEMBER_RESULT_CASES_H
#define ZR_TEST_AOT_C_OWNERSHIP_MEMBER_RESULT_CASES_H

#include "zr_vm_library/aot_runtime.h"

/* 模拟生成帧的四个直接 value 槽位，后半存储承载帧布局。 */
enum { AOT_MEMBER_FRAME_SLOT_COUNT = 4u };

/* 记录注册为待关闭的接收者析构次数，由每个夹具初始化时清零。 */
static TZrUInt32 g_aot_member_owner_drops;
/* 记录成员结果析构次数，以验证它可独立于接收者存活。 */
static TZrUInt32 g_aot_member_leaf_drops;

/* 资源对象的 native 析构回调，供接收者关闭次数断言。 */
static TZrInt64 aot_member_owner_drop(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    ++g_aot_member_owner_drops;
    return 0;
}

/* 成员资源的 native 析构回调，供返回值释放次数断言。 */
static TZrInt64 aot_member_leaf_drop(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    ++g_aot_member_leaf_drops;
    return 0;
}

/* 给新资源安装析构回调，再把唯一所有权转为调用方持有的 shared 值。 */
static void aot_member_create_shared(SZrTypeValue *shared, TZrInt64 (*drop)(SZrState *)) {
    SZrTypeValue unique;
    SZrObject *object = create_resource_object();
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0u);
    TEST_ASSERT_NOT_NULL(destructor);
    destructor->nativeFunction = drop;
    ZrCore_RawObject_MarkAsPermanent(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_ObjectPrototype_AddMeta(g_state, object->prototype, ZR_META_DESTRUCTOR,
                                  (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    init_direct_unique(object, &unique);
    ZrCore_Value_ResetAsNull(shared);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, shared, &unique));
}

/* 构造带直接 value 槽的生成帧与调用信息，并重置析构计数。 */
static void aot_member_prepare_frame(ZrAotGeneratedFrame *frame) {
    SZrFunction *function = ZrCore_Function_New(g_state);
    SZrCallInfo *callInfo = g_state->callInfoList;
    TZrStackValuePointer functionBase;
    const TZrUInt32 storageCount = AOT_MEMBER_FRAME_SLOT_COUNT * 2u;
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = AOT_MEMBER_FRAME_SLOT_COUNT;
    function->frameSlotLayoutLength = AOT_MEMBER_FRAME_SLOT_COUNT;
    function->frameSlotLayouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            g_state->global, AOT_MEMBER_FRAME_SLOT_COUNT * sizeof(*function->frameSlotLayouts),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->frameSlotLayouts);
    memset(function->frameSlotLayouts, 0,
           AOT_MEMBER_FRAME_SLOT_COUNT * sizeof(*function->frameSlotLayouts));
    function->frameByteSize = storageCount * sizeof(SZrTypeValueOnStack);
    function->frameByteAlign = _Alignof(SZrTypeValueOnStack);
    for (TZrUInt32 index = 0u; index < AOT_MEMBER_FRAME_SLOT_COUNT; ++index) {
        SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        layout->stackSlot = index;
        layout->byteOffset = (AOT_MEMBER_FRAME_SLOT_COUNT + index) * sizeof(SZrTypeValueOnStack);
        layout->byteSize = sizeof(SZrTypeValue);
        layout->byteAlign = _Alignof(SZrTypeValue);
        layout->slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    ZrCore_Function_FinalizeDirectFrameValueSlots(function);
    TEST_ASSERT_TRUE(ZrCore_Function_HasDirectValueFrameSlotSummary(function));
    functionBase = ZrCore_Function_CheckStack(g_state, storageCount + 1u,
                                               callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(functionBase);
    memset(functionBase, 0, (storageCount + 1u) * sizeof(*functionBase));
    for (TZrUInt32 index = 0u; index <= storageCount; ++index) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase + index));
    }
    ZrCore_Value_InitAsRawObject(g_state, ZrCore_Stack_GetValue(functionBase),
                                ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callInfo->metadataFunction = function;
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = functionBase + storageCount + 1u;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    g_state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    memset(frame, 0, sizeof(*frame));
    frame->function = function;
    frame->callInfo = callInfo;
    frame->slotBase = functionBase + 1;
    frame->generatedFrameSlotCount = AOT_MEMBER_FRAME_SLOT_COUNT;
    g_aot_member_owner_drops = 0u;
    g_aot_member_leaf_drops = 0u;
}

/* 关闭已登记接收者后，成员读取结果仍持有独立 shared 引用，直至显式 drop。 */
static void test_aot_member_result_outlives_registered_receiver(void) {
    ZrAotGeneratedFrame frame;
    SZrTypeValue owner;
    SZrTypeValue leaf;
    SZrTypeValue key;
    SZrMemberDescriptor descriptor;
    SZrString *memberName;
    SZrTypeValue *result;
    aot_member_prepare_frame(&frame);
    aot_member_create_shared(&owner, aot_member_owner_drop);
    aot_member_create_shared(&leaf, aot_member_leaf_drop);
    memberName = ZrCore_String_CreateFromNative(g_state, "leaf");
    TEST_ASSERT_NOT_NULL(memberName);
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(
            g_state, ((SZrObject *)owner.value.object)->prototype, &descriptor));
    ZrCore_ObjectPrototype_AddManagedField(
            g_state, ((SZrObject *)owner.value.object)->prototype, memberName,
            0u, sizeof(SZrTypeValue), ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_FALSE, ZR_TRUE, 0u);
    ZrCore_Value_InitAsRawObject(g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    ZrCore_Object_SetValue(g_state, (SZrObject *)owner.value.object, &key, &leaf);
    ZrCore_Ownership_ReleaseValue(g_state, &leaf);
    frame.function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
            g_state->global, sizeof(*frame.function->memberEntries), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    frame.function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            g_state->global, sizeof(*frame.function->callSiteCaches), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(frame.function->memberEntries);
    TEST_ASSERT_NOT_NULL(frame.function->callSiteCaches);
    memset(frame.function->memberEntries, 0, sizeof(*frame.function->memberEntries));
    memset(frame.function->callSiteCaches, 0, sizeof(*frame.function->callSiteCaches));
    frame.function->memberEntryLength = 1u;
    frame.function->callSiteCacheLength = 1u;
    frame.function->memberEntries[0].symbol = memberName;
    frame.function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
    frame.function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    frame.function->callSiteCaches[0].memberEntryIndex = 0u;
    result = ZrCore_Stack_GetValue(frame.slotBase);
    ZrCore_Value_AssignMaterializedStackValue(g_state, result, &owner);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_MarkToBeClosed(g_state, &frame, 0u));
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetMemberSlot(g_state, &frame, 0u, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, g_aot_member_owner_drops);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CloseScope(g_state, &frame, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_member_owner_drops);
    TEST_ASSERT_EQUAL_UINT32(0u, g_aot_member_leaf_drops);
    result = ZrCore_Stack_GetValue(frame.slotBase);
    TEST_ASSERT_EQUAL_UINT8(ZR_OWNERSHIP_VALUE_KIND_SHARED, result->ownershipKind);
    TEST_ASSERT_EQUAL_UINT32(1u, result->ownershipControl->strongRefCount);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_OwnDrop(g_state, &frame, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_member_leaf_drops);
}

/* 关闭待清理接收者时，同时清空与其对应的帧槽，避免残留已析构值。 */
static void test_aot_registered_shared_owner_closes_all_matching_storage(void) {
    ZrAotGeneratedFrame frame;
    SZrTypeValue owner;
    aot_member_prepare_frame(&frame);
    aot_member_create_shared(&owner, aot_member_owner_drop);
    ZrCore_Value_AssignMaterializedStackValue(g_state, ZrCore_Stack_GetValue(frame.slotBase), &owner);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_MarkToBeClosed(g_state, &frame, 0u));
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CloseScope(g_state, &frame, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_member_owner_drops);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(ZrCore_Stack_GetValue(frame.slotBase)->type));
}

/* 显式 drop 已登记接收者后再关闭作用域，不应二次调用析构。 */
static void test_aot_registered_shared_owner_drops_immediately(void) {
    ZrAotGeneratedFrame frame;
    SZrTypeValue owner;
    aot_member_prepare_frame(&frame);
    aot_member_create_shared(&owner, aot_member_owner_drop);
    ZrCore_Value_AssignMaterializedStackValue(g_state, ZrCore_Stack_GetValue(frame.slotBase), &owner);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_MarkToBeClosed(g_state, &frame, 0u));
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_OwnDrop(g_state, &frame, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_member_owner_drops);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CloseScope(g_state, &frame, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_aot_member_owner_drops);
}

#endif
