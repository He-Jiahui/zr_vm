//
// Internal execution helpers shared across split translation units.
//

#ifndef ZR_VM_CORE_EXECUTION_INTERNAL_H
#define ZR_VM_CORE_EXECUTION_INTERNAL_H

#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/execution.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_common/zr_string_conf.h"

#include "gc/gc_internal.h"
#include "object/object_internal.h"

typedef enum EZrExecutionNumericFallbackOp {
    ZR_EXEC_NUMERIC_FALLBACK_ADD = 0,
    ZR_EXEC_NUMERIC_FALLBACK_SUB,
    ZR_EXEC_NUMERIC_FALLBACK_MUL,
    ZR_EXEC_NUMERIC_FALLBACK_DIV,
    ZR_EXEC_NUMERIC_FALLBACK_MOD,
    ZR_EXEC_NUMERIC_FALLBACK_POW
} EZrExecutionNumericFallbackOp;

typedef enum EZrExecutionNumericCompareOp {
    ZR_EXEC_NUMERIC_COMPARE_GREATER = 0,
    ZR_EXEC_NUMERIC_COMPARE_LESS,
    ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
    ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL
} EZrExecutionNumericCompareOp;

static ZR_FORCE_INLINE TZrBool execution_callsite_sanitize_enabled(void) {
    static TZrInt8 cachedState = -1;
    TZrInt8 state = cachedState;

    if (ZR_LIKELY(state >= 0)) {
        return (TZrBool)(state != 0);
    }

    state = getenv("ZR_VM_TRACE_GC_CALLSITE_SANITIZE") != ZR_NULL ? 1 : 0;
    cachedState = state;
    return (TZrBool)(state != 0);
}

static ZR_FORCE_INLINE SZrProfileRuntime *execution_member_dispatch_exact_receiver_pair_profile_runtime(
        SZrState *state) {
    return (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;
}

static ZR_FORCE_INLINE void execution_member_dispatch_refresh_forwarded_assigned_value(
        SZrTypeValue *value) {
    SZrRawObject *forwardedObject;

    if (value == ZR_NULL || !value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return;
    }

    forwardedObject = (SZrRawObject *)value->value.object->garbageCollectMark.forwardingAddress;
    if (forwardedObject == ZR_NULL || forwardedObject == value->value.object) {
        return;
    }

    value->value.object = forwardedObject;
    value->type = (EZrValueType)forwardedObject->type;
    value->isNative = forwardedObject->isNative;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_dispatch_exact_receiver_pair_get_hot_fast(
        SZrState *state,
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        const SZrTypeValue *receiver,
        SZrTypeValue *result) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunctionCallSitePicSlot *slot;
    const SZrTypeValue *sourceValue;
    SZrProfileRuntime *runtime;
    TZrBool recordHelpers;

    if (state == ZR_NULL || function == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength ||
            ZR_UNLIKELY(execution_callsite_sanitize_enabled())) {
        return ZR_FALSE;
    }

    entry = &function->callSiteCaches[cacheIndex];
    if ((EZrFunctionCallSiteCacheKind)entry->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET ||
        entry->picSlotCount != 1u) {
        return ZR_FALSE;
    }

    slot = &entry->picSlots[0];
    if ((receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedReceiverObject) ||
        slot->cachedReceiverPair == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceValue = &slot->cachedReceiverPair->value;
    runtime = execution_member_dispatch_exact_receiver_pair_profile_runtime(state);
    recordHelpers = runtime != ZR_NULL && runtime->recordHelpers;
    if (ZR_UNLIKELY(recordHelpers)) {
        runtime->helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]++;
        runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]++;
    }

    if (ZR_LIKELY(ZrCore_Value_CanFastCopyPlainValue(result, sourceValue))) {
        *result = *sourceValue;
    } else {
        ZrCore_Value_CopyNoProfile(state, result, sourceValue);
    }

    entry->runtimeHitCount++;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_dispatch_exact_receiver_pair_set_hot_fast(
        SZrState *state,
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        const SZrTypeValue *receiver,
        const SZrTypeValue *assignedValue) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunctionCallSitePicSlot *slot;
    SZrObject *receiverObject;
    SZrProfileRuntime *runtime;
    TZrBool recordHelpers;
    SZrTypeValue stableAssignedValue;
    const SZrTypeValue *effectiveAssignedValue = assignedValue;

    if (state == ZR_NULL || function == ZR_NULL || receiver == ZR_NULL || assignedValue == ZR_NULL ||
        function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength ||
            ZR_UNLIKELY(execution_callsite_sanitize_enabled())) {
        return ZR_FALSE;
    }

    entry = &function->callSiteCaches[cacheIndex];
    if ((EZrFunctionCallSiteCacheKind)entry->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET ||
        entry->picSlotCount != 1u) {
        return ZR_FALSE;
    }

    slot = &entry->picSlots[0];
    if ((receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedReceiverObject) ||
        slot->cachedReceiverPair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (assignedValue->isGarbageCollectable && assignedValue->value.object != ZR_NULL) {
        stableAssignedValue = *assignedValue;
        execution_member_dispatch_refresh_forwarded_assigned_value(&stableAssignedValue);
        effectiveAssignedValue = &stableAssignedValue;
    }

    receiverObject = (SZrObject *)receiver->value.object;
    runtime = execution_member_dispatch_exact_receiver_pair_profile_runtime(state);
    recordHelpers = runtime != ZR_NULL && runtime->recordHelpers;
    if (ZR_UNLIKELY(recordHelpers)) {
        runtime->helperCounts[ZR_PROFILE_HELPER_SET_MEMBER]++;
    }

    if ((slot->cachedIsStatic & ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET) != 0u &&
        object_try_set_existing_string_pair_plain_value_assume_non_hidden_unchecked(
                state, receiverObject, slot->cachedReceiverPair, effectiveAssignedValue)) {
        if (ZR_UNLIKELY(recordHelpers)) {
            runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]++;
        }
        entry->runtimeHitCount++;
        return ZR_TRUE;
    }
    if (object_try_set_existing_pair_plain_value_fast_unchecked(
                state, receiverObject, slot->cachedReceiverPair, effectiveAssignedValue)) {
        if (ZR_UNLIKELY(recordHelpers)) {
            runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]++;
        }
        entry->runtimeHitCount++;
        return ZR_TRUE;
    }

    ZrCore_Object_SetExistingPairValueUnchecked(state, receiverObject, slot->cachedReceiverPair, effectiveAssignedValue);
    entry->runtimeHitCount++;
    return ZR_TRUE;
}

#define EXEC_DONE(N) ZR_INSTRUCTION_DONE(instruction, programCounter, N)
#define EXEC_E(INSTRUCTION) ((INSTRUCTION).instruction.operandExtra)
#define EXEC_A0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[0])
#define EXEC_B0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[1])
#define EXEC_C0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[2])
#define EXEC_D0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[3])
#define EXEC_A1(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand1[0])
#define EXEC_B1(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand1[1])
#define EXEC_A2(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand2[0])

TZrBool execution_try_materialize_global_prototypes(SZrState *state,
                                                    SZrClosure *currentClosure,
                                                    SZrCallInfo *currentCallInfo,
                                                    const SZrTypeValue *tableValue,
                                                    const SZrTypeValue *keyValue);

TZrInt64 value_to_int64(const SZrTypeValue *value);
TZrUInt64 value_to_uint64(const SZrTypeValue *value);
TZrDouble value_to_double(const SZrTypeValue *value);

TZrBool concat_values_to_destination(SZrState *state,
                                     SZrTypeValue *outResult,
                                     const SZrTypeValue *opA,
                                     const SZrTypeValue *opB,
                                     TZrBool safeMode);
TZrBool try_builtin_add(SZrState *state,
                        SZrTypeValue *outResult,
                        const SZrTypeValue *opA,
                        const SZrTypeValue *opB);
static ZR_FORCE_INLINE TZrSize execution_concat_pair_cache_bucket_index(const SZrString *left, const SZrString *right) {
    TZrUInt64 leftHash;
    TZrUInt64 rightHash;
    TZrUInt64 mixedHash;

    if (left == ZR_NULL || right == ZR_NULL) {
        return 0u;
    }

    leftHash = left->super.hash;
    rightHash = right->super.hash;
    mixedHash = (leftHash * 1315423911u) ^ (rightHash + (leftHash << 7u) + (rightHash >> 3u));
    return (TZrSize)(mixedHash % ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_COUNT);
}
static ZR_FORCE_INLINE SZrString *execution_try_get_concat_pair_cache_hit(SZrState *state,
                                                                          SZrString *left,
                                                                          SZrString *right) {
    ZrStringConcatPairCacheEntry *bucket;

    if (state == ZR_NULL || state->global == ZR_NULL || left == ZR_NULL || right == ZR_NULL) {
        return ZR_NULL;
    }

    bucket = state->global->stringConcatPairCache[execution_concat_pair_cache_bucket_index(left, right)];
    if (bucket[0].left == left && bucket[0].right == right) {
        return bucket[0].result;
    }
#if ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH == 2U
    if (bucket[1].left == left && bucket[1].right == right) {
        ZrStringConcatPairCacheEntry hitEntry = bucket[1];

        bucket[1] = bucket[0];
        bucket[0] = hitEntry;
        return hitEntry.result;
    }
#else
    for (TZrSize depthIndex = 1; depthIndex < ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH; depthIndex++) {
        if (bucket[depthIndex].left == left && bucket[depthIndex].right == right) {
            ZrStringConcatPairCacheEntry hitEntry = bucket[depthIndex];

            for (; depthIndex > 0; depthIndex--) {
                bucket[depthIndex] = bucket[depthIndex - 1u];
            }
            bucket[0] = hitEntry;
            return hitEntry.result;
        }
    }
#endif

    return ZR_NULL;
}
static ZR_FORCE_INLINE TZrBool execution_try_concat_exact_strings(SZrState *state,
                                                                  SZrTypeValue *outResult,
                                                                  const SZrTypeValue *opA,
                                                                  const SZrTypeValue *opB) {
    SZrString *leftString;
    SZrString *rightString;
    SZrString *result;

    if (state == ZR_NULL || outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_STRING(opA->type) || !ZR_VALUE_IS_TYPE_STRING(opB->type)) {
        return ZR_FALSE;
    }

    leftString = ZR_CAST_STRING(state, opA->value.object);
    rightString = ZR_CAST_STRING(state, opB->value.object);
    result = execution_try_get_concat_pair_cache_hit(state, leftString, rightString);
    if (result == ZR_NULL) {
        result = ZrCore_String_ConcatPair(state, leftString, rightString);
    }
    if (result == ZR_NULL) {
        ZrCore_Value_ResetAsNull(outResult);
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsRawObject(state, outResult, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    outResult->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}
static ZR_FORCE_INLINE TZrBool execution_value_supports_fast_safe_string_concat(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(ZR_VALUE_IS_TYPE_NULL(value->type) ||
                     ZR_VALUE_IS_TYPE_BOOL(value->type) ||
                     ZR_VALUE_IS_TYPE_NUMBER(value->type) ||
                     ZR_VALUE_IS_TYPE_NATIVE(value->type));
}
static ZR_FORCE_INLINE TZrBool execution_builtin_add_has_fast_safe_string_concat_pair(const SZrTypeValue *opA,
                                                                                       const SZrTypeValue *opB) {
    if (opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_STRING(opA->type)) {
        return execution_value_supports_fast_safe_string_concat(opB);
    }
    if (ZR_VALUE_IS_TYPE_STRING(opB->type)) {
        return execution_value_supports_fast_safe_string_concat(opA);
    }
    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool execution_try_builtin_add_exact_numeric_fast(SZrTypeValue *outResult,
                                                                            const SZrTypeValue *opA,
                                                                            const SZrTypeValue *opB) {
    EZrValueType typeA;
    EZrValueType typeB;

    if (outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    typeA = opA->type;
    typeB = opB->type;

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_SIGNED_INT(typeA) && ZR_VALUE_IS_TYPE_SIGNED_INT(typeB))) {
        TZrInt64 leftValue = opA->value.nativeObject.nativeInt64;
        TZrInt64 rightValue = opB->value.nativeObject.nativeInt64;
        ZR_VALUE_FAST_SET(outResult, nativeInt64, leftValue + rightValue, ZR_VALUE_TYPE_INT64);
        return ZR_TRUE;
    }

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeA) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeB))) {
        TZrUInt64 leftValue = opA->value.nativeObject.nativeUInt64;
        TZrUInt64 rightValue = opB->value.nativeObject.nativeUInt64;
        ZR_VALUE_FAST_SET(outResult, nativeUInt64, leftValue + rightValue, ZR_VALUE_TYPE_UINT64);
        return ZR_TRUE;
    }

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_FLOAT(typeA) && ZR_VALUE_IS_TYPE_FLOAT(typeB))) {
        TZrFloat64 leftValue = opA->value.nativeObject.nativeDouble;
        TZrFloat64 rightValue = opB->value.nativeObject.nativeDouble;
        ZR_VALUE_FAST_SET(outResult, nativeDouble, leftValue + rightValue, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool execution_try_builtin_mul_exact_numeric_fast(SZrTypeValue *outResult,
                                                                            const SZrTypeValue *opA,
                                                                            const SZrTypeValue *opB) {
    EZrValueType typeA;
    EZrValueType typeB;

    if (outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    typeA = opA->type;
    typeB = opB->type;

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_SIGNED_INT(typeA) && ZR_VALUE_IS_TYPE_SIGNED_INT(typeB))) {
        TZrInt64 leftValue = opA->value.nativeObject.nativeInt64;
        TZrInt64 rightValue = opB->value.nativeObject.nativeInt64;
        ZR_VALUE_FAST_SET(outResult, nativeInt64, leftValue * rightValue, ZR_VALUE_TYPE_INT64);
        return ZR_TRUE;
    }

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeA) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeB))) {
        TZrUInt64 leftValue = opA->value.nativeObject.nativeUInt64;
        TZrUInt64 rightValue = opB->value.nativeObject.nativeUInt64;
        ZR_VALUE_FAST_SET(outResult, nativeUInt64, leftValue * rightValue, ZR_VALUE_TYPE_UINT64);
        return ZR_TRUE;
    }

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_FLOAT(typeA) && ZR_VALUE_IS_TYPE_FLOAT(typeB))) {
        TZrFloat64 leftValue = opA->value.nativeObject.nativeDouble;
        TZrFloat64 rightValue = opB->value.nativeObject.nativeDouble;
        ZR_VALUE_FAST_SET(outResult, nativeDouble, leftValue * rightValue, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(typeA) && ZR_VALUE_IS_TYPE_BOOL(typeB)) {
        TZrBool leftValue = opA->value.nativeObject.nativeBool != ZR_FALSE;
        TZrBool rightValue = opB->value.nativeObject.nativeBool != ZR_FALSE;
        ZR_VALUE_FAST_SET(outResult, nativeBool, leftValue != rightValue, ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool execution_try_builtin_mod_exact_integer_fast(SZrTypeValue *outResult,
                                                                            const SZrTypeValue *opA,
                                                                            const SZrTypeValue *opB,
                                                                            TZrBool *outDivisorWasZero) {
    EZrValueType typeA;
    EZrValueType typeB;

    if (outDivisorWasZero != ZR_NULL) {
        *outDivisorWasZero = ZR_FALSE;
    }

    if (outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    typeA = opA->type;
    typeB = opB->type;

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeA) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeB))) {
        TZrUInt64 divisor = opB->value.nativeObject.nativeUInt64;

        if (divisor == 0u) {
            if (outDivisorWasZero != ZR_NULL) {
                *outDivisorWasZero = ZR_TRUE;
            }
            return ZR_TRUE;
        }

        ZR_VALUE_FAST_SET(outResult,
                          nativeUInt64,
                          opA->value.nativeObject.nativeUInt64 % divisor,
                          ZR_VALUE_TYPE_UINT64);
        return ZR_TRUE;
    }

    if (ZR_LIKELY(ZR_VALUE_IS_TYPE_SIGNED_INT(typeA) && ZR_VALUE_IS_TYPE_SIGNED_INT(typeB))) {
        TZrInt64 divisor = opB->value.nativeObject.nativeInt64;

        if (divisor == 0) {
            if (outDivisorWasZero != ZR_NULL) {
                *outDivisorWasZero = ZR_TRUE;
            }
            return ZR_TRUE;
        }

        if (divisor < 0) {
            divisor = -divisor;
        }

        ZR_VALUE_FAST_SET(outResult,
                          nativeInt64,
                          opA->value.nativeObject.nativeInt64 % divisor,
                          ZR_VALUE_TYPE_INT64);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool execution_callinfo_has_pending_close_work(const SZrState *state,
                                                                         const SZrCallInfo *callInfo) {
    TZrStackValuePointer frameBase;

    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    frameBase = callInfo->functionBase.valuePointer + 1;
    if (state->stackClosureValueList != ZR_NULL &&
        state->stackClosureValueList->value.valuePointer >= frameBase) {
        return ZR_TRUE;
    }

    return state->toBeClosedValueList.valuePointer >= frameBase;
}

static ZR_FORCE_INLINE TZrBool execution_builtin_add_requires_temporary_result(const SZrTypeValue *opA,
                                                                               const SZrTypeValue *opB) {
    if (opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }
    if (execution_builtin_add_has_fast_safe_string_concat_pair(opA, opB)) {
        return ZR_FALSE;
    }

    return (TZrBool)((ZR_VALUE_IS_TYPE_STRING(opA->type) || ZR_VALUE_IS_TYPE_STRING(opB->type)) &&
                     !(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type)));
}
TZrBool execution_try_builtin_sub(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB);
TZrBool execution_try_builtin_div(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB);

void execution_apply_binary_numeric_float_or_raise(SZrState *state,
                                                   EZrExecutionNumericFallbackOp operation,
                                                   SZrTypeValue *destination,
                                                   const SZrTypeValue *opA,
                                                   const SZrTypeValue *opB,
                                                   const TZrChar *instructionName);
void execution_apply_binary_numeric_compare_or_raise(SZrState *state,
                                                     EZrExecutionNumericCompareOp operation,
                                                     SZrTypeValue *destination,
                                                     const SZrTypeValue *opA,
                                                     const SZrTypeValue *opB,
                                                     const TZrChar *instructionName);
void execution_try_binary_numeric_float_fallback_or_raise(SZrState *state,
                                                          EZrExecutionNumericFallbackOp operation,
                                                          SZrTypeValue *destination,
                                                          const SZrTypeValue *opA,
                                                          const SZrTypeValue *opB,
                                                          const TZrChar *instructionName);

SZrObjectPrototype *find_type_prototype(SZrState *state,
                                        SZrString *typeName,
                                        EZrObjectPrototypeType expectedType);
TZrBool convert_to_struct(SZrState *state,
                          SZrTypeValue *source,
                          SZrObjectPrototype *targetPrototype,
                          SZrTypeValue *destination);
TZrBool convert_to_class(SZrState *state,
                         SZrTypeValue *source,
                         SZrObjectPrototype *targetPrototype,
                         SZrTypeValue *destination);
TZrBool convert_to_enum(SZrState *state,
                        SZrTypeValue *source,
                        SZrObjectPrototype *targetPrototype,
                        SZrTypeValue *destination);

TZrSize close_scope_cleanup_registrations(SZrState *state, TZrSize cleanupCount);
TZrBool execution_prepare_meta_call_target(SZrState *state, TZrStackValuePointer stackPointer);
TZrBool execution_meta_get_member(SZrState *state,
                                  SZrTypeValue *receiver,
                                  SZrString *memberName,
                                  SZrTypeValue *result);
const SZrFunctionCallSiteCacheEntry *execution_get_callsite_cache_entry(SZrFunction *function,
                                                                        TZrUInt16 cacheIndex,
                                                                        EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_prepare_meta_call_target_cached(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrUInt16 cacheIndex,
                                                  TZrStackValuePointer stackPointer,
                                                  EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_try_prepare_dyn_call_target_cached(SZrState *state,
                                                     SZrFunction *function,
                                                     TZrUInt16 cacheIndex,
                                                     TZrStackValuePointer stackPointer,
                                                     EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_meta_get_cached_member(SZrState *state,
                                         SZrFunction *function,
                                         TZrUInt16 cacheIndex,
                                         SZrTypeValue *receiver,
                                         SZrTypeValue *result);
TZrBool execution_meta_get_cached_static_member(SZrState *state,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiver,
                                                SZrTypeValue *result);
TZrBool execution_meta_set_member(SZrState *state,
                                  SZrTypeValue *receiverAndResult,
                                  SZrString *memberName,
                                  const SZrTypeValue *assignedValue);
TZrBool execution_meta_set_cached_member(SZrState *state,
                                         SZrFunction *function,
                                         TZrUInt16 cacheIndex,
                                         SZrTypeValue *receiverAndResult,
                                         const SZrTypeValue *assignedValue);
TZrBool execution_meta_set_cached_static_member(SZrState *state,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiverAndResult,
                                                const SZrTypeValue *assignedValue);
ZR_CORE_API TZrBool execution_member_get_by_name(SZrState *state,
                                                 const TZrInstruction *programCounter,
                                                 SZrTypeValue *receiver,
                                                 SZrString *memberName,
                                                 SZrTypeValue *result);
ZR_CORE_API TZrBool execution_member_set_by_name(SZrState *state,
                                                 const TZrInstruction *programCounter,
                                                 SZrTypeValue *receiverAndResult,
                                                 SZrString *memberName,
                                                 const SZrTypeValue *assignedValue);
ZR_CORE_API TZrBool execution_member_get_cached(SZrState *state,
                                                const TZrInstruction *programCounter,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiver,
                                                SZrTypeValue *result);
SZrFunctionCallSiteCacheEntry *execution_member_get_cache_entry_fast(SZrFunction *function,
                                                                     TZrUInt16 cacheIndex,
                                                                     EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_member_try_resolve_cached_known_vm_function_entry(
        SZrState *state,
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        SZrFunctionCallSiteCacheEntry *entry,
        SZrTypeValue *receiver,
        SZrFunction **outFunction,
        TZrUInt32 *outArgumentCount);
ZR_CORE_API TZrBool execution_member_try_resolve_cached_known_vm_function(SZrState *state,
                                                                          SZrFunction *function,
                                                                          TZrUInt16 cacheIndex,
                                                                          SZrTypeValue *receiver,
                                                                          SZrFunction **outFunction,
                                                                          TZrUInt32 *outArgumentCount);
ZR_CORE_API TZrBool execution_member_set_cached(SZrState *state,
                                                const TZrInstruction *programCounter,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiverAndResult,
                                                const SZrTypeValue *assignedValue);
TZrBool execution_invoke_meta_call(SZrState *state,
                                   SZrCallInfo *savedCallInfo,
                                   TZrStackValuePointer savedStackTop,
                                   TZrStackValuePointer requestedScratchBase,
                                   SZrMeta *meta,
                                   const SZrTypeValue *arg0,
                                   const SZrTypeValue *arg1,
                                   TZrSize argumentCount,
                                   TZrStackValuePointer *outMetaBase,
                                   TZrStackValuePointer *outSavedStackTop);

static ZR_FORCE_INLINE void execution_discard_exception_handlers_for_callinfo_fast(
        SZrState *state,
        const SZrCallInfo *callInfo) {
    TZrUInt32 handlerStackLength;

    handlerStackLength = state->exceptionHandlerStackLength;
    while (handlerStackLength > 0u &&
           state->exceptionHandlerStack[handlerStackLength - 1u].callInfo == callInfo) {
        handlerStackLength--;
    }

    state->exceptionHandlerStackLength = handlerStackLength;
}

TZrBool execution_has_exception_handler_for_callinfo(SZrState *state, SZrCallInfo *callInfo);
const SZrFunctionExceptionHandlerInfo *execution_lookup_exception_handler_info(
        SZrState *state,
        const SZrVmExceptionHandlerState *handlerState,
        SZrFunction **outFunction);
TZrBool execution_try_reuse_tail_call_frame(SZrState *state,
                                            SZrCallInfo *callInfo,
                                            TZrStackValuePointer functionPointer);

#endif // ZR_VM_CORE_EXECUTION_INTERNAL_H
