#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"

/* 回调记录原 local 的双表示是否已清空；保存相对栈基址的偏移，避免保留陈旧栈地址。 */
static TZrMemoryOffset gDenseOffset;
static TZrMemoryOffset gPhysicalOffset;
static TZrUInt32 gCloseCalls;
static TZrBool gBothClearedAtCallback;

void setUp(void) {
    gDenseOffset = 0u;
    gPhysicalOffset = 0u;
    gCloseCalls = 0u;
    gBothClearedAtCallback = ZR_FALSE;
}

void tearDown(void) {}

/* 对象的 @close 回调只观察 dense source 与 physical source，不读取 proxy，也不主动触发 GC、栈扩容或重入。 */
static TZrInt64 close_proxy_aot_probe(SZrState *state) {
    const SZrTypeValue *dense = ZrCore_Stack_GetValueNoProfile(
            ZrCore_Stack_LoadOffsetToPointer(state, gDenseOffset));
    const SZrTypeValue *physical = (const SZrTypeValue *)ZrCore_Stack_LoadByteOffsetToAddress(
            state, gPhysicalOffset);

    ++gCloseCalls;
    gBothClearedAtCallback =
            ZR_VALUE_IS_TYPE_NULL(dense->type) && ZR_VALUE_IS_TYPE_NULL(physical->type);
    return 0;
}

/* 手工构造双表示 VALUE 帧，验证高物理 proxy 关闭同一普通对象一次；直接调用 helper，不执行生成函数入口，也不提供强制 GC 的存活证据。 */
static void test_aot_helper_marks_high_physical_proxy_and_closes_original_once(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *closer;
    SZrString *name;
    SZrObjectPrototype *prototype;
    SZrObject *object;
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layouts[2] = {{0}};
    ZrAotGeneratedFrame generatedFrame = {0};
    TZrStackValuePointer denseBase;
    TZrStackValuePointer physicalSource;
    TZrStackValuePointer physicalProxy;
    const TZrUInt32 sourceByteOffset = (TZrUInt32)(2u * sizeof(SZrTypeValueOnStack));
    const TZrUInt32 proxyByteOffset = (TZrUInt32)(3u * sizeof(SZrTypeValueOnStack));

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state, 128u, ZR_TRUE));
    closer = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closer);
    closer->nativeFunction = close_proxy_aot_probe;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closer));
    name = ZrCore_String_CreateFromNative(state, "AotCloseProxyProbe");
    TEST_ASSERT_NOT_NULL(name);
    prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    ZrCore_ObjectPrototype_AddMeta(state, prototype, ZR_META_CLOSE,
                                   ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closer)));
    object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Object_Init(state, object);

    /* dense source/proxy 是逻辑槽 0/1，独立物理 VALUE 存储位于偏移 2/3 个栈槽；模拟活动 VM 帧覆盖这些位置，使 core 可由 dense source 找到 physical mirror。 */
    denseBase = state->stackBase.valuePointer + 8u;
    state->stackTop.valuePointer = denseBase + 24u;
    physicalSource = (TZrStackValuePointer)((TZrByte *)denseBase + sourceByteOffset);
    physicalProxy = (TZrStackValuePointer)((TZrByte *)denseBase + proxyByteOffset);
    for (TZrUInt32 slot = 0u; slot < 2u; ++slot) {
        layouts[slot].stackSlot = slot;
        layouts[slot].byteOffset = slot == 0u ? sourceByteOffset : proxyByteOffset;
        layouts[slot].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[slot].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[slot].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        layouts[slot].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[slot].reserved0 = ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE;
    }
    function.stackSize = 2u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    function.frameByteSize = proxyByteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.functionBase.valuePointer = denseBase - 1u;
    state->baseCallInfo.functionTop.valuePointer = denseBase + 4u;
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    state->callInfoList = &state->baseCallInfo;
    generatedFrame.function = &function;
    generatedFrame.slotBase = denseBase;
    generatedFrame.generatedFrameSlotCount = 2u;

    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(denseBase),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Stack_GetValue(denseBase)->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNullNoProfile(ZrCore_Stack_GetValueNoProfile(denseBase + 1u));
    gDenseOffset = ZrCore_Stack_SavePointerAsOffset(state, denseBase);
    gPhysicalOffset = ZrCore_Stack_SaveByteAddressAsOffset(state, physicalSource);

    /* 外层登记落在 physical source；更高的 physical proxy 通过 token 指向 dense source。 */
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_MarkToBeClosed(state, &generatedFrame, 0u));
    TEST_ASSERT_EQUAL_PTR(physicalSource, state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_TRUE(physicalProxy > physicalSource);
    TEST_ASSERT_TRUE(physicalProxy < state->stackTop.valuePointer);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_MarkCloseProxy(state, &generatedFrame, 1u, 0u));
    TEST_ASSERT_EQUAL_PTR(physicalProxy, state->toBeClosedValueList.valuePointer);

    /* 第一次只摘除 proxy，回调进入前原 local 的两个表示必须为 null；physical source 登记仍在链上，第二次摘除它时不再调用 @close。 */
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CloseScope(state, &generatedFrame, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, gCloseCalls);
    TEST_ASSERT_TRUE(gBothClearedAtCallback);
    TEST_ASSERT_EQUAL_PTR(physicalSource, state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CloseScope(state, &generatedFrame, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, gCloseCalls);
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                          state->toBeClosedValueList.valuePointer);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_helper_marks_high_physical_proxy_and_closes_original_once);
    return UNITY_END();
}
