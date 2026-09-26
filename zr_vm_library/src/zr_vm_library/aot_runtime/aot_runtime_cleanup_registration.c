#include "aot_runtime_cleanup_registration.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

static TZrBool aot_runtime_cleanup_registration_is_owner(const SZrTypeValue *value) {
    return (TZrBool)(value != ZR_NULL &&
                     (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED));
}

static TZrStackValuePointer aot_runtime_cleanup_registration_dense_slot(
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL || frame->function == ZR_NULL ||
        logicalSlot >= frame->generatedFrameSlotCount) {
        return ZR_NULL;
    }
    return frame->slotBase + logicalSlot;
}

/* 仅当物理 VALUE 槽与稠密槽分离时返回物理地址，供作用域清理维护唯一 owner。 */
static SZrTypeValue *aot_runtime_cleanup_registration_physical_value(
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot,
        TZrStackValuePointer *outDense) {
    TZrStackValuePointer dense = aot_runtime_cleanup_registration_dense_slot(frame, logicalSlot);
    SZrTypeValue *physical;

    if (outDense != ZR_NULL) {
        *outDense = dense;
    }
    if (dense == ZR_NULL) {
        return ZR_NULL;
    }
    physical = ZrCore_Function_TryGetDirectFrameValueSlot(frame->function, frame->slotBase, logicalSlot);
    if (physical == ZR_NULL || physical == ZrCore_Stack_GetValueNoProfile(dense)) {
        return ZR_NULL;
    }
    return physical;
}

/* 只有仍挂在当前 state 的 toBeClosed 链上的物理槽才能被清除或刷新。 */
static TZrBool aot_runtime_cleanup_registration_is_active(
        const SZrState *state,
        TZrStackValuePointer registration) {
    TZrStackValuePointer cursor;

    if (state == ZR_NULL || registration == ZR_NULL || state->stackBase.valuePointer == ZR_NULL ||
        state->toBeClosedValueList.valuePointer == ZR_NULL ||
        registration <= state->stackBase.valuePointer ||
        registration > state->toBeClosedValueList.valuePointer) {
        return ZR_FALSE;
    }

    cursor = state->toBeClosedValueList.valuePointer;
    while (cursor > state->stackBase.valuePointer) {
        if (cursor == registration) {
            return ZR_TRUE;
        }
        if (cursor->toBeClosedValueOffset == 0u) {
            return ZR_FALSE;
        }
        cursor -= cursor->toBeClosedValueOffset;
    }
    return ZR_FALSE;
}

/* MarkToBeClosed 的注册前置步骤：把稠密槽 owner 安放到可由 core 关闭链追踪的物理槽。 */
TZrBool aot_runtime_cleanup_registration_prepare(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot,
        TZrStackValuePointer *outRegistration) {
    TZrStackValuePointer dense;
    TZrStackValuePointer physicalPointer;
    SZrTypeValue *physical;

    if (outRegistration == ZR_NULL) {
        return ZR_FALSE;
    }
    *outRegistration = ZR_NULL;
    physical = aot_runtime_cleanup_registration_physical_value(frame, logicalSlot, &dense);
    if (dense == ZR_NULL) {
        return ZR_FALSE;
    }
    if (physical == ZR_NULL) {
        *outRegistration = dense;
        return ZR_TRUE;
    }
    physicalPointer = ZR_CAST_STACK_VALUE(physical);
    if (state == ZR_NULL ||
        (state->toBeClosedValueList.valuePointer != ZR_NULL &&
         physicalPointer <= state->toBeClosedValueList.valuePointer)) {
        return ZR_FALSE;
    }

    ZrCore_Value_CopyNoProfile(state, physical, ZrCore_Stack_GetValueNoProfile(dense));
    *outRegistration = ZR_CAST_STACK_VALUE(physical);
    return ZR_TRUE;
}

/* 物理槽仍在关闭链中时释放旧 owner，阻止稠密槽被覆盖后再次 drop 旧值。 */
void aot_runtime_cleanup_registration_clear(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot) {
    TZrStackValuePointer dense;
    SZrTypeValue *physical = aot_runtime_cleanup_registration_physical_value(frame, logicalSlot, &dense);

    if (state == ZR_NULL || dense == ZR_NULL || physical == ZR_NULL ||
        !aot_runtime_cleanup_registration_is_active(state, ZR_CAST_STACK_VALUE(physical))) {
        return;
    }
    if (aot_runtime_cleanup_registration_is_owner(physical) &&
        physical->ownershipControl != ZR_NULL) {
        ZrCore_Ownership_ReleaseValue(state, physical);
    } else {
        ZrCore_Value_ResetAsNullNoProfile(physical);
    }
}

/* 稠密槽更新后同步当前 owner；只有活动的双槽注册需要写回。 */
void aot_runtime_cleanup_registration_refresh(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot) {
    TZrStackValuePointer dense;
    SZrTypeValue *physical = aot_runtime_cleanup_registration_physical_value(frame, logicalSlot, &dense);
    const SZrTypeValue *denseValue;

    if (state == ZR_NULL || dense == ZR_NULL || physical == ZR_NULL ||
        !aot_runtime_cleanup_registration_is_active(state, ZR_CAST_STACK_VALUE(physical))) {
        return;
    }
    denseValue = ZrCore_Stack_GetValueNoProfile(dense);
    if (aot_runtime_cleanup_registration_is_owner(denseValue)) {
        if (denseValue->ownershipControl != ZR_NULL) {
            ZrCore_Value_CopyNoProfile(state, physical, denseValue);
        } else {
            *physical = *denseValue;
        }
    } else {
        ZrCore_Value_ResetAsNullNoProfile(physical);
    }
}
