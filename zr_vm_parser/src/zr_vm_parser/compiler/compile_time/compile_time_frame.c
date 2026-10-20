#include "compiler/compile_time/compile_time_executor_internal.h"
#include "compiler/compile_time/comptime_runtime_contract.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"

#include <string.h>

typedef struct SZrCompileTimeFrameInput {
    SZrRawObject *objects[2];
    SZrTypeValue value;
    SZrAotGcRootFrame rootFrame;
    TZrBool mutationLocked;
} SZrCompileTimeFrameInput;

static const SZrAotGcRootSlot g_frame_input_slots[] = {
        {0U, 0U, 0U, 0U, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0U, 0U},
        {0U, sizeof(SZrRawObject *), 0U, 0U,
         ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0U, 0U},
};
static const SZrAotGcRootMap g_frame_input_map = {
        sizeof(g_frame_input_slots) / sizeof(g_frame_input_slots[0]),
        g_frame_input_slots,
};

/* Protect inputs across candidate allocation and mutation-boundary polls.
 * Shared Core root-table calls retain their own allocator reentry limits. */
static TZrBool ct_frame_input_begin(
        SZrCompilerState *cs, SZrString *name, const SZrTypeValue *value,
        SZrCompileTimeFrameInput *input) {
    memset(input, 0, sizeof(*input));
    input->value = *value;
    input->objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(name);
    if (ct_value_needs_root(value)) {
        input->objects[1] = value->value.object;
    }
    for (TZrSize index = 0U; index < 2U; index++) {
        if (input->objects[index] != ZR_NULL &&
            !ZrCore_GcDomain_ObjectBelongsToState(cs->state, input->objects[index])) {
            return ZR_FALSE;
        }
    }
    if (!ZrCore_Gc_AotRootFramePush(
                cs->state, &input->rootFrame,
                (TZrStackValuePointer)input->objects, &g_frame_input_map)) {
        return ZR_FALSE;
    }
    input->mutationLocked = ZrCore_GcDomain_SharedMutationBegin(cs->state);
    if (!input->mutationLocked) {
        (void)ZrCore_Gc_AotRootFramePop(cs->state, &input->rootFrame);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void ct_frame_input_end(
        SZrCompilerState *cs, SZrCompileTimeFrameInput *input) {
    ZrCore_GcDomain_MutationEnd(cs->state, input->mutationLocked);
    (void)ZrCore_Gc_AotRootFramePop(cs->state, &input->rootFrame);
}

static TZrBool ct_frame_create_root(
        SZrCompilerState *cs, SZrRawObject **object, SZrGcRootHandle *root) {
    SZrRawObject *resolved = ZR_NULL;

    if (!ZrCore_GcRootHandle_Create(cs->state, *object, root)) {
        return ZR_FALSE;
    }
    /* A fresh, unshared handle has a nonallocating Update. Reload the mapped
     * cell before publication without taking the copy-on-write path. */
    if (!ZrCore_GcRootHandle_Update(cs->state, root, *object) ||
        !ZrCore_GcRootHandle_Resolve(cs->state, root, &resolved)) {
        ZrCore_GcRootHandle_Release(cs->state, root);
        return ZR_FALSE;
    }
    *object = resolved;
    return ZR_TRUE;
}

static TZrBool ct_frame_capacity_request(
        const SZrArray *array, TZrSize elementSize,
        TZrSize *capacity, TZrUInt64 *bytes) {
    TZrSize maximum = ZR_MAX_SIZE / elementSize;
    TZrSize percent = ZR_MATH_MAX(ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT, 100U);
    TZrSize whole = percent / 100U;
    TZrSize fraction = percent % 100U;
    TZrSize extra;

    *capacity = 0U;
    *bytes = 0U;
    if (array->length < array->capacity) {
        return ZR_TRUE;
    }
    if (array->length == maximum) {
        return ZR_FALSE;
    }
    if (array->capacity == 0U) {
        *capacity = ZR_MATH_MIN(ZR_PARSER_INITIAL_CAPACITY_TINY, maximum);
    } else if (array->capacity > (maximum - 1U) / whole) {
        *capacity = maximum;
    } else {
        *capacity = array->capacity * whole;
        extra = (array->capacity / 100U) * fraction +
                (array->capacity % 100U) * fraction / 100U;
        *capacity = extra >= maximum - *capacity
                ? maximum : *capacity + extra + 1U;
    }
    if (*capacity < array->length + 1U) {
        *capacity = array->length + 1U;
    }
    *bytes = (TZrUInt64)(*capacity * elementSize);
    return ZR_TRUE;
}

static void ct_frame_abort_reservation(
        SZrCompilerState *cs, SZrParserComptimeBudgetTransaction *transaction,
        TZrUInt64 candidateBytes) {
    EZrParserComptimeBudgetResource failed = transaction->exceededResource;
    ZrParser_ComptimeBudget_AbortTransaction(transaction);
    if (failed != ZR_PARSER_COMPTIME_BUDGET_NONE) {
        (void)ZrParser_ComptimeRuntime_Consume(
                cs, failed,
                failed == ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES ? candidateBytes : 1U,
                (SZrFileRange){0});
    }
}

static TZrBool ct_frame_reserve(
        SZrCompilerState *cs, SZrParserComptimeBudgetTransaction *transaction,
        TZrUInt64 candidateBytes) {
    ZrParser_ComptimeBudget_BeginTransaction(&cs->comptimeBudget, transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, candidateBytes) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT, 1U)) {
        ct_frame_abort_reservation(cs, transaction, candidateBytes);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool ct_frame_stage_capacity(
        SZrCompilerState *cs, const SZrArray *array, TZrSize elementSize,
        TZrSize capacity, SZrArray *candidate) {
    ZrCore_Array_Construct(candidate);
    if (capacity == 0U) {
        return ZR_TRUE;
    }
    if (!ZrCore_Array_TryInit(cs->state, candidate, elementSize, capacity)) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "Cannot allocate compile-time frame storage", (SZrFileRange){0});
        return ZR_FALSE;
    }
    if (array->length != 0U) {
        memcpy(candidate->head, array->head, array->length * elementSize);
    }
    candidate->length = array->length;
    return ZR_TRUE;
}

/* Commit precedes this nonallocating append. A replacement is independent
 * of the live array until every fallible step and budget merge has passed. */
static void ct_frame_publish(
        SZrCompilerState *cs, SZrArray *array, SZrArray *candidate,
        const void *element) {
    SZrArray *target = candidate->isValid ? candidate : array;
    memcpy(target->head + target->length * target->elementSize,
           element, target->elementSize);
    target->length++;
    if (candidate->isValid) {
        SZrArray previous = *array;
        *array = *candidate;
        ZrCore_Array_Construct(candidate);
        ZrCore_Array_Free(cs->state, &previous);
    }
}

static TZrBool ct_root_resolves_to(
        SZrState *state,
        const SZrGcRootHandle *root,
        SZrRawObject *object) {
    SZrRawObject *resolved = ZR_NULL;

    return state != ZR_NULL && root != ZR_NULL && object != ZR_NULL &&
           ZrCore_GcRootHandle_Resolve(state, root, &resolved) &&
           resolved == object;
}

static TZrBool ct_frame_value_is_rooted(
        SZrCompileTimeFrame *frame,
        SZrRawObject *object) {
    if (frame == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }
    for (SZrCompileTimeFrame *cursor = frame;
         cursor != ZR_NULL;
         cursor = cursor->parent) {
        for (TZrSize index = cursor->bindings.length; index > 0U; index--) {
            SZrCompileTimeBinding *binding =
                    (SZrCompileTimeBinding *)ZrCore_Array_Get(
                            &cursor->bindings, index - 1U);
            if (binding != ZR_NULL && binding->hasValueRoot &&
                ct_root_resolves_to(cursor->state, &binding->valueRoot, object)) {
                return ZR_TRUE;
            }
        }
        for (TZrSize index = 0U;
             index < cursor->temporaryRoots.length;
             index++) {
            SZrGcRootHandle *root =
                    (SZrGcRootHandle *)ZrCore_Array_Get(
                            &cursor->temporaryRoots, index);
            if (ct_root_resolves_to(cursor->state, root, object)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool ct_binding_resolve_name(
        SZrState *state,
        SZrCompileTimeBinding *binding) {
    SZrRawObject *rooted;

    if (binding == ZR_NULL || !binding->hasNameRoot) {
        return binding != ZR_NULL;
    }
    rooted = ZR_NULL;
    if (state == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(
                state, &binding->nameRoot, &rooted) || rooted == ZR_NULL) {
        return ZR_FALSE;
    }
    binding->name = (SZrString *)rooted;
    return ZR_TRUE;
}

static TZrBool ct_binding_resolve_value(
        SZrState *state,
        SZrCompileTimeBinding *binding) {
    SZrRawObject *rooted;

    if (binding == ZR_NULL || !binding->hasValueRoot) {
        return binding != ZR_NULL;
    }
    rooted = ZR_NULL;
    if (state == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(
                state, &binding->valueRoot, &rooted) || rooted == ZR_NULL) {
        return ZR_FALSE;
    }
    binding->value.value.object = rooted;
    return ZR_TRUE;
}

static TZrBool ct_binding_set_value(
        SZrCompilerState *cs, SZrCompileTimeBinding *binding,
        const SZrTypeValue *value) {
    SZrCompileTimeFrameInput input;
    SZrGcRootHandle root = {0};
    SZrGcRootHandle previousRoot;
    TZrBool hasRoot;
    TZrBool hadRoot;

    if (!ct_frame_input_begin(cs, ZR_NULL, value, &input)) {
        return ZR_FALSE;
    }
    hasRoot = ct_value_needs_root(&input.value);
    if (hasRoot && !ct_frame_create_root(cs, &input.objects[1], &root)) {
        ct_frame_input_end(cs, &input);
        return ZR_FALSE;
    }
    previousRoot = binding->valueRoot;
    hadRoot = binding->hasValueRoot;
    if (hasRoot) {
        input.value.value.object = input.objects[1];
    }
    binding->value = input.value;
    binding->valueRoot = root;
    binding->hasValueRoot = hasRoot;
    if (hadRoot) {
        ZrCore_GcRootHandle_Release(cs->state, &previousRoot);
    }
    ct_frame_input_end(cs, &input);
    return ZR_TRUE;
}

void ct_frame_init(
        SZrCompilerState *cs, SZrCompileTimeFrame *frame,
        SZrCompileTimeFrame *parent) {
    if (cs == ZR_NULL || frame == ZR_NULL) {
        return;
    }
    frame->parent = parent;
    frame->state = cs->state;
    ZrCore_Array_Construct(&frame->bindings);
    ZrCore_Array_Construct(&frame->temporaryRoots);
}

void ct_frame_free(SZrCompilerState *cs, SZrCompileTimeFrame *frame) {
    SZrState *state;
    if (frame == ZR_NULL) {
        return;
    }
    state = cs != ZR_NULL ? cs->state : frame->state;
    if (state == ZR_NULL) {
        return;
    }
    for (TZrSize index = frame->bindings.length; index > 0U; index--) {
        SZrCompileTimeBinding *binding =
                (SZrCompileTimeBinding *)ZrCore_Array_Get(&frame->bindings, index - 1U);
        if (binding->hasNameRoot) {
            ZrCore_GcRootHandle_Release(state, &binding->nameRoot);
        }
        if (binding->hasValueRoot) {
            ZrCore_GcRootHandle_Release(state, &binding->valueRoot);
        }
    }
    for (TZrSize index = frame->temporaryRoots.length; index > 0U; index--) {
        ZrCore_GcRootHandle_Release(state,
                (SZrGcRootHandle *)ZrCore_Array_Get(&frame->temporaryRoots, index - 1U));
    }
    ZrCore_Array_Free(state, &frame->bindings);
    ZrCore_Array_Free(state, &frame->temporaryRoots);
}

TZrBool ct_frame_get_with_context(
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        SZrTypeValue *result) {
    if (frame == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize i = frame->bindings.length; i > 0; i--) {
        SZrCompileTimeBinding *binding = (SZrCompileTimeBinding *)ZrCore_Array_Get(&frame->bindings, i - 1);
        if (binding != ZR_NULL && ct_binding_resolve_name(frame->state, binding) &&
            binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name) &&
            ct_contexts_equal(binding->syntaxContext,
                              syntaxContext,
                              ZR_SYNTAX_NAMESPACE_LOCAL)) {
            if (!ct_binding_resolve_value(frame->state, binding)) {
                return ZR_FALSE;
            }
            *result = binding->value;
            return ZR_TRUE;
        }
    }
    return frame->parent != ZR_NULL
               ? ct_frame_get_with_context(
                         frame->parent, name, syntaxContext, result)
               : ZR_FALSE;
}

TZrBool ct_frame_get(
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrTypeValue *result) {
    return ct_frame_get_with_context(
            frame, name, ZrParser_SyntaxContext_Root(), result);
}

TZrBool ct_frame_set(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name, const SZrTypeValue *value) {
    return ct_frame_declare(cs, frame, name, value, ZR_FALSE);
}

TZrBool ct_frame_declare_with_context(
        SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name,
        SZrSyntaxContext syntaxContext, const SZrTypeValue *value, TZrBool isConst) {
    SZrCompileTimeBinding binding;
    SZrCompileTimeFrameInput input;
    SZrParserComptimeBudgetTransaction transaction;
    SZrArray candidate;
    TZrSize capacity;
    TZrUInt64 candidateBytes;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || frame == ZR_NULL ||
        name == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = frame->bindings.length; index > 0U; index--) {
        SZrCompileTimeBinding *existing =
                (SZrCompileTimeBinding *)ZrCore_Array_Get(&frame->bindings, index - 1U);
        if (ct_binding_resolve_name(cs->state, existing) &&
            existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, name) &&
            ct_contexts_equal(existing->syntaxContext, syntaxContext,
                              ZR_SYNTAX_NAMESPACE_LOCAL)) {
            if (!ct_binding_set_value(cs, existing, value)) {
                return ZR_FALSE;
            }
            existing->isConst = (TZrBool)(existing->isConst || isConst);
            return ZR_TRUE;
        }
    }
    if (!ct_frame_capacity_request(
                &frame->bindings, sizeof(binding), &capacity, &candidateBytes)) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "Compile-time frame capacity is not representable", (SZrFileRange){0});
        return ZR_FALSE;
    }
    if (!ct_frame_reserve(cs, &transaction, candidateBytes)) {
        return ZR_FALSE;
    }
    if (!ct_frame_input_begin(cs, name, value, &input)) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    memset(&binding, 0, sizeof(binding));
    ZrCore_Array_Construct(&candidate);
    if (!ct_frame_create_root(cs, &input.objects[0], &binding.nameRoot)) {
        goto cleanup;
    }
    binding.hasNameRoot = ZR_TRUE;
    if (ct_value_needs_root(&input.value)) {
        if (!ct_frame_create_root(cs, &input.objects[1], &binding.valueRoot)) {
            goto cleanup;
        }
        binding.hasValueRoot = ZR_TRUE;
    }
    if (!ct_frame_stage_capacity(
                cs, &frame->bindings, sizeof(binding), capacity, &candidate)) {
        goto cleanup;
    }
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ct_frame_abort_reservation(cs, &transaction, candidateBytes);
        goto cleanup;
    }
    binding.name = (SZrString *)input.objects[0];
    binding.value = input.value;
    if (binding.hasValueRoot) {
        binding.value.value.object = input.objects[1];
    }
    binding.syntaxContext = syntaxContext;
    binding.isConst = isConst;
    ct_frame_publish(cs, &frame->bindings, &candidate, &binding);
    success = ZR_TRUE;

cleanup:
    if (!success) {
        ZrCore_Array_Free(cs->state, &candidate);
        if (binding.hasNameRoot) {
            ZrCore_GcRootHandle_Release(cs->state, &binding.nameRoot);
        }
        if (binding.hasValueRoot) {
            ZrCore_GcRootHandle_Release(cs->state, &binding.valueRoot);
        }
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    }
    ct_frame_input_end(cs, &input);
    return success;
}

TZrBool ct_frame_declare(
        SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name,
        const SZrTypeValue *value, TZrBool isConst) {
    return ct_frame_declare_with_context(
            cs, frame, name, ZrParser_SyntaxContext_Root(), value, isConst);
}

/* Assignment updates only the nearest existing frame binding.  Declarations
 * use ct_frame_set and therefore still shadow in the current frame. */
TZrBool ct_frame_update_with_context(
        SZrCompilerState *cs,
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        const SZrTypeValue *value,
        SZrFileRange location) {
    if (cs == ZR_NULL || frame == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize i = frame->bindings.length; i > 0; i--) {
        SZrCompileTimeBinding *binding =
                (SZrCompileTimeBinding *)ZrCore_Array_Get(
                        &frame->bindings, i - 1);
        if (binding != ZR_NULL && ct_binding_resolve_name(cs->state, binding) &&
            binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name) &&
            ct_contexts_equal(binding->syntaxContext,
                              syntaxContext,
                              ZR_SYNTAX_NAMESPACE_LOCAL)) {
            if (binding->isConst) {
                ZrParser_CompileTime_Error(
                        cs,
                        ZR_COMPILE_TIME_ERROR_ERROR,
                        "Cannot assign to immutable compile-time binding",
                        location);
                return ZR_FALSE;
            }
            return ct_binding_set_value(cs, binding, value);
        }
    }
    return ZR_FALSE;
}

TZrBool ct_frame_update(
        SZrCompilerState *cs,
        SZrCompileTimeFrame *frame,
        SZrString *name,
        const SZrTypeValue *value,
        SZrFileRange location) {
    return ct_frame_update_with_context(
            cs,
            frame,
            name,
            ZrParser_SyntaxContext_Root(),
            value,
            location);
}

TZrBool ct_frame_has_binding_with_context(
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrSyntaxContext syntaxContext) {
    if (frame == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize i = frame->bindings.length; i > 0; i--) {
        SZrCompileTimeBinding *binding =
                (SZrCompileTimeBinding *)ZrCore_Array_Get(
                        &frame->bindings, i - 1);
        if (binding != ZR_NULL && ct_binding_resolve_name(frame->state, binding) &&
            binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name) &&
            ct_contexts_equal(binding->syntaxContext,
                              syntaxContext,
                              ZR_SYNTAX_NAMESPACE_LOCAL)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ct_frame_has_binding(
        SZrCompileTimeFrame *frame,
        SZrString *name) {
    return ct_frame_has_binding_with_context(
            frame, name, ZrParser_SyntaxContext_Root());
}

TZrBool ct_frame_root_value(
        SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrTypeValue *value) {
    SZrCompileTimeFrameInput input;
    SZrParserComptimeBudgetTransaction transaction;
    SZrGcRootHandle root = {0};
    SZrArray candidate;
    TZrSize capacity;
    TZrUInt64 candidateBytes;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    if (frame == ZR_NULL || !ct_value_needs_root(value)) {
        return ZR_TRUE;
    }
    if (ct_frame_value_is_rooted(frame, value->value.object)) {
        return ZR_TRUE;
    }
    if (!ct_frame_capacity_request(
                &frame->temporaryRoots, sizeof(root), &capacity, &candidateBytes)) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "Compile-time frame capacity is not representable", (SZrFileRange){0});
        return ZR_FALSE;
    }
    if (!ct_frame_reserve(cs, &transaction, candidateBytes)) {
        return ZR_FALSE;
    }
    if (!ct_frame_input_begin(cs, ZR_NULL, value, &input)) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    ZrCore_Array_Construct(&candidate);
    if (!ct_frame_create_root(cs, &input.objects[1], &root)) {
        goto cleanup;
    }
    if (!ct_frame_stage_capacity(
                cs, &frame->temporaryRoots, sizeof(root), capacity, &candidate)) {
        goto cleanup;
    }
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ct_frame_abort_reservation(cs, &transaction, candidateBytes);
        goto cleanup;
    }
    ct_frame_publish(cs, &frame->temporaryRoots, &candidate, &root);
    success = ZR_TRUE;

cleanup:
    if (!success) {
        ZrCore_Array_Free(cs->state, &candidate);
        ZrCore_GcRootHandle_Release(cs->state, &root);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    }
    ct_frame_input_end(cs, &input);
    if (success) {
        value->value.object = input.objects[1];
    }
    return success;
}
