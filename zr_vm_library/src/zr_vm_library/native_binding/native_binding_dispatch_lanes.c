//
// Extracted native binding dispatch-lane and stable-value helpers.
//

#include "native_binding/native_binding_dispatch_lanes.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/ownership.h"

/*
 * These helpers are pure runtime plumbing on hot paths; keep benchmark
 * execution off the helper-profile branches for stack slot reads, value
 * copy/reset.
 */
#define ZrCore_Stack_GetValue ZrCore_Stack_GetValueNoProfile
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile

TZrBool native_binding_prepare_stable_value(SZrState *state,
                                            ZrLibStableValueCopy *copy,
                                            const SZrTypeValue *source) {
    return native_binding_prepare_stable_value_inline(state, copy, source);
}

void native_binding_release_stable_value(SZrState *state, ZrLibStableValueCopy *copy) {
    native_binding_release_stable_value_inline(state, copy);
}

#define native_binding_pin_value_object native_binding_pin_value_object_inline
#define native_binding_unpin_value_object native_binding_unpin_value_object_inline
#define native_binding_copy_stable_value native_binding_copy_stable_value_inline
#define native_binding_prepare_stable_value_raw native_binding_prepare_stable_value_raw_inline
#define native_binding_release_stable_value_raw native_binding_release_stable_value_raw_inline
#define native_binding_prepare_stable_value native_binding_prepare_stable_value_inline
#define native_binding_release_stable_value native_binding_release_stable_value_inline
#define native_binding_pin_stable_value_if_needed native_binding_pin_stable_value_if_needed_inline
#define native_binding_entry_callback native_binding_entry_callback_inline
#define native_binding_context_enable_stack_layout_anchor native_binding_context_enable_stack_layout_anchor_inline
#define native_binding_context_adopt_stack_layout_anchor native_binding_context_adopt_stack_layout_anchor_inline
#define native_binding_sync_self_to_stack_slot native_binding_sync_self_to_stack_slot_inline

TZrBool native_binding_dispatch_stack_root_callback_lane(SZrState *state,
                                                         FZrLibBoundCallback callback,
                                                         ZrLibCallContext *context,
                                                         const SZrFunctionStackAnchor *functionBaseAnchor,
                                                         SZrTypeValue *result) {
    SZrTypeValue *selfValueBefore;
    TZrStackValuePointer stackBaseBefore;
    TZrStackValuePointer stackTailBefore;
    TZrBool success;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(callback != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(functionBaseAnchor != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    native_binding_context_adopt_stack_layout_anchor(context, functionBaseAnchor);
    selfValueBefore = context->selfValue;
    stackBaseBefore = state->stackBase.valuePointer;
    stackTailBefore = state->stackTail.valuePointer;
    success = callback(context, result);
    if (success && (context->selfValue != selfValueBefore || state->stackBase.valuePointer != stackBaseBefore)) {
        native_binding_sync_self_to_stack_slot(state, functionBaseAnchor, context, stackBaseBefore, stackTailBefore);
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

TZrBool native_binding_dispatch_stack_root_lane(SZrState *state,
                                                const ZrLibBindingEntry *entry,
                                                ZrLibCallContext *context,
                                                const SZrFunctionStackAnchor *functionBaseAnchor,
                                                SZrTypeValue *result) {
    FZrLibBoundCallback callback;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(functionBaseAnchor != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    callback = native_binding_entry_callback(entry);
    ZR_ASSERT(callback != ZR_NULL);
    return native_binding_dispatch_stack_root_callback_lane(state, callback, context, functionBaseAnchor, result);
}

TZrBool native_binding_dispatch_fast_lane(SZrState *state,
                                          const ZrLibBindingEntry *entry,
                                          ZrLibCallContext *context,
                                          const SZrFunctionStackAnchor *functionBaseAnchor,
                                          SZrTypeValue *result) {
    FZrLibBoundCallback callback;
    SZrTypeValue stableSelfCopy;
    SZrTypeValue stableArgumentCopies[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    TZrSize index;
    TZrBool success = ZR_FALSE;
    TZrStackValuePointer stackBaseBefore;
    TZrStackValuePointer stackTailBefore;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(functionBaseAnchor != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    callback = native_binding_entry_callback(entry);
    ZR_ASSERT(callback != ZR_NULL);

    if (context->selfValue != ZR_NULL) {
        stableSelfCopy = *context->selfValue;
        context->selfValue = &stableSelfCopy;
    }

    for (index = 0; index < context->argumentCount; index++) {
        stableArgumentCopies[index] = *ZrCore_Stack_GetValue(context->argumentBase + index);
    }
    context->argumentValues = context->argumentCount > 0 ? stableArgumentCopies : ZR_NULL;
    context->argumentValuePointers = ZR_NULL;

    stackBaseBefore = state->stackBase.valuePointer;
    stackTailBefore = state->stackTail.valuePointer;
    success = callback(context, result);
    if (success) {
        native_binding_sync_self_to_stack_slot(state, functionBaseAnchor, context, stackBaseBefore, stackTailBefore);
    }

    return success;
}

static TZrBool native_binding_dispatch_inline_pinned_lane_one_argument(
        SZrState *state,
        const ZrLibBindingEntry *entry,
        ZrLibCallContext *context,
        const SZrFunctionStackAnchor *functionBaseAnchor,
        SZrTypeValue *result) {
    FZrLibBoundCallback callback;
    ZrLibStableValueCopy stableSelfCopy = {0};
    SZrTypeValue stableArgumentCopy;
    TZrBool argumentPinAdded = ZR_FALSE;
    TZrBool argumentNeedsRelease = ZR_FALSE;
    TZrBool hasStableSelf = ZR_FALSE;
    TZrBool selfPinAdded = ZR_FALSE;
    TZrBool success = ZR_FALSE;
    TZrStackValuePointer stackBaseBefore = ZR_NULL;
    TZrStackValuePointer stackTailBefore = ZR_NULL;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(functionBaseAnchor != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(context->argumentCount == 1);

    callback = native_binding_entry_callback(entry);
    ZR_ASSERT(callback != ZR_NULL);

    if (context->selfValue != ZR_NULL) {
        if (!native_binding_prepare_stable_value(state, &stableSelfCopy, context->selfValue)) {
            return ZR_FALSE;
        }
        hasStableSelf = ZR_TRUE;
        context->selfValue = &stableSelfCopy.value;
    }

    if (!native_binding_prepare_stable_value_raw(state,
                                                 &stableArgumentCopy,
                                                 &argumentNeedsRelease,
                                                 ZrCore_Stack_GetValue(context->argumentBase))) {
        goto cleanup;
    }
    context->argumentValues = &stableArgumentCopy;
    context->argumentValuePointers = ZR_NULL;

    if (!native_binding_pin_stable_value_if_needed(state,
                                                   hasStableSelf ? &stableSelfCopy.value : ZR_NULL,
                                                   hasStableSelf ? stableSelfCopy.needsRelease : ZR_FALSE,
                                                   &selfPinAdded) ||
        !native_binding_pin_stable_value_if_needed(state,
                                                   &stableArgumentCopy,
                                                   argumentNeedsRelease,
                                                   &argumentPinAdded)) {
        goto cleanup;
    }

    stackBaseBefore = state->stackBase.valuePointer;
    stackTailBefore = state->stackTail.valuePointer;
    success = callback(context, result);
    if (success) {
        native_binding_sync_self_to_stack_slot(state, functionBaseAnchor, context, stackBaseBefore, stackTailBefore);
    }

cleanup:
    native_binding_unpin_value_object(state->global, &stableArgumentCopy, argumentPinAdded);
    native_binding_unpin_value_object(state->global, hasStableSelf ? &stableSelfCopy.value : ZR_NULL, selfPinAdded);
    native_binding_release_stable_value_raw(state, &stableArgumentCopy, &argumentNeedsRelease);
    if (hasStableSelf) {
        native_binding_release_stable_value(state, &stableSelfCopy);
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static TZrBool native_binding_dispatch_inline_pinned_lane_two_arguments(
        SZrState *state,
        const ZrLibBindingEntry *entry,
        ZrLibCallContext *context,
        const SZrFunctionStackAnchor *functionBaseAnchor,
        SZrTypeValue *result) {
    FZrLibBoundCallback callback;
    ZrLibStableValueCopy stableSelfCopy = {0};
    SZrTypeValue stableArgumentCopies[2];
    TZrBool argumentPinAdded[2] = {ZR_FALSE, ZR_FALSE};
    TZrBool argumentNeedsRelease[2] = {ZR_FALSE, ZR_FALSE};
    TZrBool hasStableSelf = ZR_FALSE;
    TZrBool selfPinAdded = ZR_FALSE;
    TZrBool success = ZR_FALSE;
    TZrStackValuePointer stackBaseBefore = ZR_NULL;
    TZrStackValuePointer stackTailBefore = ZR_NULL;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(functionBaseAnchor != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(context->argumentCount == 2);

    callback = native_binding_entry_callback(entry);
    ZR_ASSERT(callback != ZR_NULL);

    if (context->selfValue != ZR_NULL) {
        if (!native_binding_prepare_stable_value(state, &stableSelfCopy, context->selfValue)) {
            return ZR_FALSE;
        }
        hasStableSelf = ZR_TRUE;
        context->selfValue = &stableSelfCopy.value;
    }

    if (!native_binding_prepare_stable_value_raw(state,
                                                 &stableArgumentCopies[0],
                                                 &argumentNeedsRelease[0],
                                                 ZrCore_Stack_GetValue(context->argumentBase)) ||
        !native_binding_prepare_stable_value_raw(state,
                                                 &stableArgumentCopies[1],
                                                 &argumentNeedsRelease[1],
                                                 ZrCore_Stack_GetValue(context->argumentBase + 1))) {
        goto cleanup;
    }
    context->argumentValues = stableArgumentCopies;
    context->argumentValuePointers = ZR_NULL;

    if (!native_binding_pin_stable_value_if_needed(state,
                                                   hasStableSelf ? &stableSelfCopy.value : ZR_NULL,
                                                   hasStableSelf ? stableSelfCopy.needsRelease : ZR_FALSE,
                                                   &selfPinAdded) ||
        !native_binding_pin_stable_value_if_needed(state,
                                                   &stableArgumentCopies[0],
                                                   argumentNeedsRelease[0],
                                                   &argumentPinAdded[0]) ||
        !native_binding_pin_stable_value_if_needed(state,
                                                   &stableArgumentCopies[1],
                                                   argumentNeedsRelease[1],
                                                   &argumentPinAdded[1])) {
        goto cleanup;
    }

    stackBaseBefore = state->stackBase.valuePointer;
    stackTailBefore = state->stackTail.valuePointer;
    success = callback(context, result);
    if (success) {
        native_binding_sync_self_to_stack_slot(state, functionBaseAnchor, context, stackBaseBefore, stackTailBefore);
    }

cleanup:
    native_binding_unpin_value_object(state->global, &stableArgumentCopies[1], argumentPinAdded[1]);
    native_binding_unpin_value_object(state->global, &stableArgumentCopies[0], argumentPinAdded[0]);
    native_binding_unpin_value_object(state->global, hasStableSelf ? &stableSelfCopy.value : ZR_NULL, selfPinAdded);
    native_binding_release_stable_value_raw(state, &stableArgumentCopies[1], &argumentNeedsRelease[1]);
    native_binding_release_stable_value_raw(state, &stableArgumentCopies[0], &argumentNeedsRelease[0]);
    if (hasStableSelf) {
        native_binding_release_stable_value(state, &stableSelfCopy);
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static TZrBool native_binding_dispatch_inline_pinned_lane_generic(SZrState *state,
                                                                  const ZrLibBindingEntry *entry,
                                                                  ZrLibCallContext *context,
                                                                  const SZrFunctionStackAnchor *functionBaseAnchor,
                                                                  SZrTypeValue *result) {
    FZrLibBoundCallback callback;
    ZrLibStableValueCopy stableSelfCopy = {0};
    SZrTypeValue stableArgumentCopies[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    TZrBool argumentPinAdded[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    TZrBool argumentNeedsRelease[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    TZrSize argumentCount;
    TZrSize copiedArgumentCount;
    TZrBool hasStableSelf;
    TZrBool selfPinAdded;
    TZrSize index;
    TZrBool success = ZR_FALSE;
    TZrStackValuePointer stackBaseBefore = ZR_NULL;
    TZrStackValuePointer stackTailBefore = ZR_NULL;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(functionBaseAnchor != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    callback = native_binding_entry_callback(entry);
    ZR_ASSERT(callback != ZR_NULL);
    argumentCount = context->argumentCount;
    if (argumentCount > ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY) {
        return ZR_FALSE;
    }

    memset(argumentPinAdded, 0, sizeof(argumentPinAdded));
    memset(argumentNeedsRelease, 0, sizeof(argumentNeedsRelease));
    copiedArgumentCount = 0;
    hasStableSelf = ZR_FALSE;
    selfPinAdded = ZR_FALSE;

    if (context->selfValue != ZR_NULL) {
        if (!native_binding_prepare_stable_value(state, &stableSelfCopy, context->selfValue)) {
            return ZR_FALSE;
        }
        hasStableSelf = ZR_TRUE;
        context->selfValue = &stableSelfCopy.value;
    }

    for (index = 0; index < argumentCount; index++) {
        if (!native_binding_prepare_stable_value_raw(state,
                                                     &stableArgumentCopies[index],
                                                     &argumentNeedsRelease[index],
                                                     ZrCore_Stack_GetValue(context->argumentBase + index))) {
            goto cleanup;
        }
        copiedArgumentCount++;
    }
    context->argumentValues = argumentCount > 0 ? stableArgumentCopies : ZR_NULL;
    context->argumentValuePointers = ZR_NULL;

    if (!native_binding_pin_stable_value_if_needed(state,
                                                   hasStableSelf ? &stableSelfCopy.value : ZR_NULL,
                                                   hasStableSelf ? stableSelfCopy.needsRelease : ZR_FALSE,
                                                   &selfPinAdded)) {
        goto cleanup;
    }
    for (index = 0; index < argumentCount; index++) {
        if (!native_binding_pin_stable_value_if_needed(state,
                                                       &stableArgumentCopies[index],
                                                       argumentNeedsRelease[index],
                                                       &argumentPinAdded[index])) {
            goto cleanup;
        }
    }

    stackBaseBefore = state->stackBase.valuePointer;
    stackTailBefore = state->stackTail.valuePointer;
    success = callback(context, result);
    if (success) {
        native_binding_sync_self_to_stack_slot(state, functionBaseAnchor, context, stackBaseBefore, stackTailBefore);
    }

cleanup:
    for (index = copiedArgumentCount; index > 0; index--) {
        native_binding_unpin_value_object(state->global,
                                          &stableArgumentCopies[index - 1],
                                          argumentPinAdded[index - 1]);
    }
    native_binding_unpin_value_object(state->global,
                                      hasStableSelf ? &stableSelfCopy.value : ZR_NULL,
                                      selfPinAdded);
    for (index = copiedArgumentCount; index > 0; index--) {
        native_binding_release_stable_value_raw(state,
                                                &stableArgumentCopies[index - 1],
                                                &argumentNeedsRelease[index - 1]);
    }
    if (hasStableSelf) {
        native_binding_release_stable_value(state, &stableSelfCopy);
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

TZrBool native_binding_dispatch_inline_pinned_lane(SZrState *state,
                                                   const ZrLibBindingEntry *entry,
                                                   ZrLibCallContext *context,
                                                   const SZrFunctionStackAnchor *functionBaseAnchor,
                                                   SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->selfValue != ZR_NULL) {
        if (context->argumentCount == 1) {
            return native_binding_dispatch_inline_pinned_lane_one_argument(state,
                                                                           entry,
                                                                           context,
                                                                           functionBaseAnchor,
                                                                           result);
        }
        if (context->argumentCount == 2) {
            return native_binding_dispatch_inline_pinned_lane_two_arguments(state,
                                                                            entry,
                                                                            context,
                                                                            functionBaseAnchor,
                                                                            result);
        }
    }

    return native_binding_dispatch_inline_pinned_lane_generic(state,
                                                              entry,
                                                              context,
                                                              functionBaseAnchor,
                                                              result);
}
