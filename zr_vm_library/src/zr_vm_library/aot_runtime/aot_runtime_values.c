#include "zr_vm_library/aot_runtime.h"

#include "aot_runtime_internal.h"

#include <math.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

static SZrLibraryAotRuntimeState *aot_runtime_value_runtime_state(SZrState *state) {
    return state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
}

static TZrStackValuePointer aot_runtime_value_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL || frame->function == ZR_NULL ||
        slotIndex >= frame->generatedFrameSlotCount) {
        return ZR_NULL;
    }

    return frame->slotBase + slotIndex;
}

/* lowering_values 在需要物化转移时选择 CopyStack；inline struct 使用物理布局复制，
 * 普通 VALUE 则委托 core 的 materialized ownership 赋值。 */
/* TODO: 源为 inline struct、目标为非同布局的普通 VALUE 时会落到普通值赋值；
 * 需核对生成 IR 是否保证该组合不可达，并补错配布局夹具。 */
TZrBool ZrLibrary_AotRuntime_CopyStack(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    const SZrFunctionFrameSlotLayout *destinationLayout;
    const SZrFunctionFrameSlotLayout *sourceLayout;
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || destinationPointer == ZR_NULL ||
        sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "COPY_STACK: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "COPY_STACK: missing value");
        return ZR_FALSE;
    }

    destinationLayout = ZrCore_Function_FindFrameSlotLayout(frame->function, destinationSlot);
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
    if (destinationLayout != ZR_NULL && sourceLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
        sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
        destinationLayout->typeLayoutId == sourceLayout->typeLayoutId) {
        const SZrTypeLayout *inlineLayout =
                ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, destinationLayout->typeLayoutId);
        if (inlineLayout == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "COPY_STACK: missing inline layout");
            return ZR_FALSE;
        }
        if (!ZrCore_Function_CopyFrameSlotInline(
                    state, inlineLayout, frame->function, frame->slotBase, destinationSlot, frame->function,
                    frame->slotBase, sourceSlot)) {
            aot_runtime_fail(state, runtimeState, "COPY_STACK: failed inline frame copy");
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (destinationLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
        if (!ZrCore_Function_CopyObjectValueToFrameSlotInline(
                    state, frame->function, frame->slotBase, destinationSlot, sourceValue)) {
            aot_runtime_fail(state,
                             runtimeState,
                             "COPY_STACK: failed inline object copy destination=%u source=%u sourceType=%u",
                             (unsigned)destinationSlot,
                             (unsigned)sourceSlot,
                             (unsigned)sourceValue->type);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (destinationLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
        destinationLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
        ZrCore_Value_AssignMaterializedStackValue(state, destinationValue, sourceValue);
        return ZR_TRUE;
    }

    ZrCore_Value_AssignMaterializedStackValue(state, destinationValue, sourceValue);
    return ZR_TRUE;
}

/* lowering_values 要保留源值时选择 GetStack；非 inline 槽经 Value_Copy 保持引用计数。 */
/* TODO: 源为 inline struct、目标为非 inline 槽时会把稠密槽当普通值复制；
 * 需核对生成器的槽布局门禁及此组合是否可达。 */
TZrBool ZrLibrary_AotRuntime_GetStack(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    const SZrFunctionFrameSlotLayout *destinationLayout;
    const SZrFunctionFrameSlotLayout *sourceLayout;
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_STACK: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_STACK: missing value");
        return ZR_FALSE;
    }

    destinationLayout = ZrCore_Function_FindFrameSlotLayout(frame->function, destinationSlot);
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
    if (destinationLayout != ZR_NULL && sourceLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
        sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
        destinationLayout->typeLayoutId == sourceLayout->typeLayoutId) {
        const SZrTypeLayout *inlineLayout =
                ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, destinationLayout->typeLayoutId);
        if (inlineLayout == ZR_NULL ||
            !ZrCore_Function_CopyFrameSlotInline(
                    state, inlineLayout, frame->function, frame->slotBase, destinationSlot, frame->function,
                    frame->slotBase, sourceSlot)) {
            aot_runtime_fail(state, runtimeState, "GET_STACK: failed inline frame copy");
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (destinationLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
        if (!ZrCore_Function_CopyObjectValueToFrameSlotInline(
                    state, frame->function, frame->slotBase, destinationSlot, sourceValue)) {
            aot_runtime_fail(state, runtimeState, "GET_STACK: failed inline object copy");
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    ZrCore_Value_Copy(state, destinationValue, sourceValue);
    return ZR_TRUE;
}

/* 生成器明确结束值槽寿命时先让 core 处理目标原所有权，再置空。 */
TZrBool ZrLibrary_AotRuntime_ResetStackNull(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL: missing destination value");
        return ZR_FALSE;
    }

    ZrCore_Value_PrepareDestinationForOverwriteNoProfile(state, destinationValue);
    ZrCore_Value_ResetAsNullNoProfile(destinationValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ResetStackNull2(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 firstSlot,
                                             TZrUInt32 secondSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer firstPointer = aot_runtime_value_frame_slot(frame, firstSlot);
    TZrStackValuePointer secondPointer = aot_runtime_value_frame_slot(frame, secondSlot);
    SZrTypeValue *firstValue;
    SZrTypeValue *secondValue;

    if (state == ZR_NULL || firstPointer == ZR_NULL || secondPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL2: invalid stack slot");
        return ZR_FALSE;
    }

    firstValue = ZrCore_Stack_GetValue(firstPointer);
    secondValue = ZrCore_Stack_GetValue(secondPointer);
    if (firstValue == ZR_NULL || secondValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL2: missing value");
        return ZR_FALSE;
    }

    ZrCore_Value_PrepareDestinationForOverwriteNoProfile(state, firstValue);
    ZrCore_Value_ResetAsNullNoProfile(firstValue);
    ZrCore_Value_PrepareDestinationForOverwriteNoProfile(state, secondValue);
    ZrCore_Value_ResetAsNullNoProfile(secondValue);
    return ZR_TRUE;
}

/* 泛型转换共用的槽门禁；仅返回当前帧中的借用指针，调用者不可在扩栈后继续使用。 */
/* TODO: 各 ConvertGeneric 入口以 ZR_VALUE_FAST_SET 覆盖目标槽；需核对生成器是否
 * 保证目标原值已清理或无 owner，并用 Shared/Unique 旧值覆盖做定向测试。 */
static TZrBool aot_runtime_generic_conversion_values(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     SZrLibraryAotRuntimeState **runtimeStateOut,
                                                     SZrTypeValue **destinationValueOut,
                                                     const SZrTypeValue **sourceValueOut) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    *destinationValueOut = destinationValue;
    *sourceValueOut = sourceValue;
    return ZR_TRUE;
}

/* TODO: 泛型 TO_BOOL 仅接受 null/bool/数值；需核对未知类型的字符串、对象
 * 或元方法真值转换是否可能流入此入口，以及生成器退路是否完备。 */
TZrBool ZrLibrary_AotRuntime_ConvertGenericToBool(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          sourceValue->value.nativeObject.nativeInt64 != 0,
                          ZR_VALUE_TYPE_BOOL);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          sourceValue->value.nativeObject.nativeUInt64 != 0,
                          ZR_VALUE_TYPE_BOOL);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          sourceValue->value.nativeObject.nativeDouble != 0.0,
                          ZR_VALUE_TYPE_BOOL);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 生成器的泛型标量转换：源类型在运行期才确定，结果写入同一生成帧。 */
/* BUG: float 转 int64 未检查 NaN、无穷和目标范围；越界 C 转换未定义，
 * lowering_generic_conversion 可将运行时浮点输入导向此路径，需加边界回归。 */
TZrBool ZrLibrary_AotRuntime_ConvertGenericToInt(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          (TZrInt64)sourceValue->value.nativeObject.nativeUInt64,
                          ZR_VALUE_TYPE_INT64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          (TZrInt64)sourceValue->value.nativeObject.nativeDouble,
                          ZR_VALUE_TYPE_INT64);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          sourceValue->value.nativeObject.nativeBool ? 1 : 0,
                          ZR_VALUE_TYPE_INT64);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 与 ToInt 同源的泛型无符号转换；其值域由运行时输入决定。 */
/* BUG: float 转 uint64 未检查 NaN、负数、无穷或上界；越界 C 转换未定义，
 * 需和 core 的数值转换契约核对并加入边界测试。 */
TZrBool ZrLibrary_AotRuntime_ConvertGenericToUInt(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeUInt64,
                          (TZrUInt64)sourceValue->value.nativeObject.nativeInt64,
                          ZR_VALUE_TYPE_UINT64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeUInt64,
                          (TZrUInt64)sourceValue->value.nativeObject.nativeDouble,
                          ZR_VALUE_TYPE_UINT64);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeUInt64,
                          sourceValue->value.nativeObject.nativeBool ? (TZrUInt64)1u : (TZrUInt64)0u,
                          ZR_VALUE_TYPE_UINT64);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ConvertGenericToFloat(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          (TZrFloat64)sourceValue->value.nativeObject.nativeInt64,
                          ZR_VALUE_TYPE_DOUBLE);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          (TZrFloat64)sourceValue->value.nativeObject.nativeUInt64,
                          ZR_VALUE_TYPE_DOUBLE);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          sourceValue->value.nativeObject.nativeBool ? (TZrFloat64)1.0 : (TZrFloat64)0.0,
                          ZR_VALUE_TYPE_DOUBLE);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

typedef enum EZrAotRuntimeGenericNumericBinaryOp {
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD
} EZrAotRuntimeGenericNumericBinaryOp;

static TZrBool aot_runtime_generic_numeric_values(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  SZrLibraryAotRuntimeState **runtimeStateOut,
                                                  SZrTypeValue **destinationValueOut,
                                                  const SZrTypeValue **leftValueOut,
                                                  const SZrTypeValue **rightValueOut) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_value_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_value_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (destinationValueOut == ZR_NULL || leftValueOut == ZR_NULL || rightValueOut == ZR_NULL ||
        state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    *destinationValueOut = destinationValue;
    *leftValueOut = leftValue;
    *rightValueOut = rightValue;
    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_numeric_extract_float64(SZrState *state,
                                                           SZrLibraryAotRuntimeState *runtimeState,
                                                           const SZrTypeValue *value,
                                                           TZrFloat64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = value->value.nativeObject.nativeDouble;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* TODO: unsigned 大于 INT64_MAX 时转 signed 的结果为实现定义；需核对语言规范
 * 与解释器的混合数值路径，补极值对照测试。 */
static TZrBool aot_runtime_generic_numeric_extract_int64(SZrState *state,
                                                         SZrLibraryAotRuntimeState *runtimeState,
                                                         const SZrTypeValue *value,
                                                         TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 泛型数值退路在运行时区分 float、unsigned 和 signed；由五种生成算术操作复用。 */
/* BUG: signed ADD/SUB/MUL 溢出及 INT64_MIN 除/模 -1 触发 C 未定义行为，
 * 当前仅检查除数为零；需明确语言溢出语义并以极值/UBSan 覆盖此生成退路。 */
static TZrBool aot_runtime_generic_numeric_binary(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  EZrAotRuntimeGenericNumericBinaryOp operation) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;

    if (!aot_runtime_generic_numeric_values(state,
                                            frame,
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            &runtimeState,
                                            &destinationValue,
                                            &leftValue,
                                            &rightValue)) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_NUMBER(leftValue->type) || !ZR_VALUE_IS_TYPE_NUMBER(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) || ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        TZrFloat64 leftFloat;
        TZrFloat64 rightFloat;
        TZrFloat64 resultFloat;

        if (!aot_runtime_generic_numeric_extract_float64(state, runtimeState, leftValue, &leftFloat) ||
            !aot_runtime_generic_numeric_extract_float64(state, runtimeState, rightValue, &rightFloat)) {
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV && rightFloat == 0.0) {
            aot_runtime_fail(state, runtimeState, "divide by zero");
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD && rightFloat == 0.0) {
            aot_runtime_fail(state, runtimeState, "modulo by zero");
            return ZR_FALSE;
        }

        switch (operation) {
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD:
                resultFloat = leftFloat + rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB:
                resultFloat = leftFloat - rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL:
                resultFloat = leftFloat * rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV:
                resultFloat = leftFloat / rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD:
                resultFloat = fmod(leftFloat, rightFloat);
                break;
            default:
                aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
                return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destinationValue, nativeDouble, resultFloat, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        TZrUInt64 leftUInt = leftValue->value.nativeObject.nativeUInt64;
        TZrUInt64 rightUInt = rightValue->value.nativeObject.nativeUInt64;
        TZrUInt64 resultUInt;

        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV && rightUInt == 0u) {
            aot_runtime_fail(state, runtimeState, "divide by zero");
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD && rightUInt == 0u) {
            aot_runtime_fail(state, runtimeState, "modulo by zero");
            return ZR_FALSE;
        }

        switch (operation) {
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD:
                resultUInt = leftUInt + rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB:
                resultUInt = leftUInt - rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL:
                resultUInt = leftUInt * rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV:
                resultUInt = leftUInt / rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD:
                resultUInt = leftUInt % rightUInt;
                break;
            default:
                aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
                return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destinationValue, nativeUInt64, resultUInt, ZR_VALUE_TYPE_UINT64);
        return ZR_TRUE;
    }

    {
        TZrInt64 leftInt;
        TZrInt64 rightInt;
        TZrInt64 resultInt;

        if (!aot_runtime_generic_numeric_extract_int64(state, runtimeState, leftValue, &leftInt) ||
            !aot_runtime_generic_numeric_extract_int64(state, runtimeState, rightValue, &rightInt)) {
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV && rightInt == 0) {
            aot_runtime_fail(state, runtimeState, "divide by zero");
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD && rightInt == 0) {
            aot_runtime_fail(state, runtimeState, "modulo by zero");
            return ZR_FALSE;
        }

        switch (operation) {
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD:
                resultInt = leftInt + rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB:
                resultInt = leftInt - rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL:
                resultInt = leftInt * rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV:
                resultInt = leftInt / rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD:
                resultInt = leftInt % rightInt;
                break;
            default:
                aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
                return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destinationValue, nativeInt64, resultInt, ZR_VALUE_TYPE_INT64);
        return ZR_TRUE;
    }
}

TZrBool ZrLibrary_AotRuntime_GenericNumericAdd(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericSub(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericMul(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericDiv(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericMod(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD);
}

/* 泛型负号在执行时依据源值类型选定目标值种类。 */
/* BUG: signed INT64_MIN 取负触发 C 有符号溢出，unsigned 高位值转 signed
 * 后再取负也可能越界；需加入极值回归并统一解释器/生成器契约。 */
TZrBool ZrLibrary_AotRuntime_GenericNumericNeg(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          -sourceValue->value.nativeObject.nativeInt64,
                          sourceValue->type);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          -(TZrInt64)sourceValue->value.nativeObject.nativeUInt64,
                          ZR_VALUE_TYPE_INT64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          -sourceValue->value.nativeObject.nativeDouble,
                          sourceValue->type);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 编译器的泛型 POW 退路应在运行时沿元方法处理自定义值。 */
/* BUG: 有 ZR_META_POW 时此处直接报 unsupported，而解释器 execution_dispatch.c
 * 会调用该元方法；自定义 POW 在 AOT 下失败。需补同一脚本的 AOT/解释器对照测试。 */
TZrBool ZrLibrary_AotRuntime_GenericPower(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_value_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_value_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL ||
        rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic power meta dispatch");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic power meta dispatch");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_POW);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    aot_runtime_fail(state, runtimeState, "unsupported AOT generic power meta dispatch");
    return ZR_FALSE;
}

static TZrBool aot_runtime_generic_logical_source_value(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 sourceSlot,
                                                        SZrLibraryAotRuntimeState **runtimeStateOut,
                                                        const SZrTypeValue **sourceValueOut,
                                                        const char *failureMessage) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    const SZrTypeValue *sourceValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (sourceValueOut == ZR_NULL || state == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    *sourceValueOut = sourceValue;
    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_logical_values(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  SZrLibraryAotRuntimeState **runtimeStateOut,
                                                  SZrTypeValue **destinationValueOut,
                                                  const SZrTypeValue **leftValueOut,
                                                  const SZrTypeValue **rightValueOut,
                                                  const char *failureMessage) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_value_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_value_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (destinationValueOut == ZR_NULL || leftValueOut == ZR_NULL || rightValueOut == ZR_NULL ||
        state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    *destinationValueOut = destinationValue;
    *leftValueOut = leftValue;
    *rightValueOut = rightValue;
    return ZR_TRUE;
}

/* TODO: 仅覆盖基本值真值语义；需核对未知类型是否可经 lowering_generic_logical
 * 进入本入口，并与动态对象/字符串真值路径做对照。 */
static TZrBool aot_runtime_generic_primitive_truthy(SZrState *state,
                                                   SZrLibraryAotRuntimeState *runtimeState,
                                                   const SZrTypeValue *sourceValue,
                                                   TZrBool *outTruthy) {
    if (sourceValue == ZR_NULL || outTruthy == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(sourceValue->type)) {
        *outTruthy = ZR_FALSE;
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeBool != 0u);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeUInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeDouble != 0.0);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 作为生成器基本值 EQ/NEQ 的共享比较边界；这里只支持 null、bool 和数字。 */
/* TODO: 未知类型的 EQ/NEQ 是否可能从生成器进入此原始值退路尚未核证；
 * 需测试字符串与对象比较同解释器的结果是否一致。 */
static TZrBool aot_runtime_generic_primitive_equal(SZrState *state,
                                                  SZrLibraryAotRuntimeState *runtimeState,
                                                  const SZrTypeValue *leftValue,
                                                  const SZrTypeValue *rightValue,
                                                  TZrBool *outEqual) {
    if (leftValue == ZR_NULL || rightValue == ZR_NULL || outEqual == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive equality");
        return ZR_FALSE;
    }

    if (leftValue->type != rightValue->type) {
        *outEqual = ZR_FALSE;
    } else if (ZR_VALUE_IS_TYPE_NULL(leftValue->type)) {
        *outEqual = ZR_TRUE;
    } else if (ZR_VALUE_IS_TYPE_BOOL(leftValue->type)) {
        *outEqual = (TZrBool)((leftValue->value.nativeObject.nativeBool != 0u) ==
                              (rightValue->value.nativeObject.nativeBool != 0u));
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        *outEqual = (TZrBool)(leftValue->value.nativeObject.nativeInt64 ==
                              rightValue->value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        *outEqual = (TZrBool)(leftValue->value.nativeObject.nativeUInt64 ==
                              rightValue->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue->type)) {
        *outEqual = (TZrBool)(leftValue->value.nativeObject.nativeDouble ==
                              rightValue->value.nativeObject.nativeDouble);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive equality");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 sourceSlot,
                                                      TZrBool *outTruthy) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrTypeValue *sourceValue;

    if (outTruthy != ZR_NULL) {
        *outTruthy = ZR_FALSE;
    }
    if (!aot_runtime_generic_logical_source_value(state,
                                                  frame,
                                                  sourceSlot,
                                                  &runtimeState,
                                                  &sourceValue,
                                                  "unsupported AOT generic primitive truthiness")) {
        return ZR_FALSE;
    }

    return aot_runtime_generic_primitive_truthy(state, runtimeState, sourceValue, outTruthy);
}

/* TODO: 此类逻辑结果以 FAST_SET 覆盖目标；需核对目标槽可能已有 Shared/Unique
 * owner 时是否由生成器预先释放，补同槽重复赋值与关闭作用域的所有权测试。 */
TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;
    TZrBool truthy = ZR_FALSE;

    if (!aot_runtime_generic_logical_source_value(state,
                                                  frame,
                                                  sourceSlot,
                                                  &runtimeState,
                                                  &sourceValue,
                                                  "unsupported AOT generic primitive truthiness")) {
        return ZR_FALSE;
    }
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }
    if (!aot_runtime_generic_primitive_truthy(state, runtimeState, sourceValue, &truthy)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, !truthy, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;
    TZrBool equal = ZR_FALSE;

    if (!aot_runtime_generic_logical_values(state,
                                            frame,
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            &runtimeState,
                                            &destinationValue,
                                            &leftValue,
                                            &rightValue,
                                            "unsupported AOT generic primitive equality") ||
        !aot_runtime_generic_primitive_equal(state, runtimeState, leftValue, rightValue, &equal)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, equal, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;
    TZrBool equal = ZR_FALSE;

    if (!aot_runtime_generic_logical_values(state,
                                            frame,
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            &runtimeState,
                                            &destinationValue,
                                            &leftValue,
                                            &rightValue,
                                            "unsupported AOT generic primitive equality") ||
        !aot_runtime_generic_primitive_equal(state, runtimeState, leftValue, rightValue, &equal)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, !equal, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}
