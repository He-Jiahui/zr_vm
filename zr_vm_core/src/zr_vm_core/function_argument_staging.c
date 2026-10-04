#include "function_argument_staging.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/profile.h"
#include <stdlib.h>
#include <string.h>

enum { FUNCTION_ARGUMENT_LOCAL_ROWS = 8 };

static EZrExecutionTransferStatus argument_result(
        SZrExecutionArgumentStagingDiagnostic *diagnostic,
        EZrExecutionTransferStatus status, TZrUInt32 parameter, TZrUInt32 source) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->parameterIndex = parameter;
        diagnostic->sourceStackSlot = source;
        diagnostic->relatedParameterIndex = UINT32_MAX;
    }
    return status;
}

TZrBool ZrCore_Function_ValueArgumentWorkspaceSize(const SZrFunction *callee,
                                                 TZrSize *outSize) {
    if (callee == ZR_NULL || outSize == ZR_NULL ||
        (TZrSize)callee->frameSlotLayoutLength > SIZE_MAX / sizeof(SZrFunctionArgumentStage)) {
        return ZR_FALSE;
    }
    *outSize = (TZrSize)callee->frameSlotLayoutLength * sizeof(SZrFunctionArgumentStage);
    return ZR_TRUE;
}

static TZrBool argument_plain_value(const SZrTypeValue *value) {
    return (TZrBool)(value->isNative == ZR_TRUE &&
            value->isGarbageCollectable == ZR_FALSE &&
            value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
            value->ownershipControl == ZR_NULL && value->ownershipWeakRef == ZR_NULL &&
            (ZR_VALUE_IS_TYPE_NULL(value->type) || ZR_VALUE_IS_TYPE_BOOL(value->type) ||
             ZR_VALUE_IS_TYPE_INT(value->type) || ZR_VALUE_IS_TYPE_FLOAT(value->type)));
}

static TZrBool argument_ranges_overlap(uintptr_t a, size_t an, uintptr_t b, size_t bn) {
    if (an > UINTPTR_MAX - a || bn > UINTPTR_MAX - b) return ZR_TRUE;
    return (TZrBool)(an != 0u && bn != 0u && a < b + bn && b < a + an);
}

/* An invalid diagnostic alias cannot itself receive a diagnostic write. */
static TZrBool argument_diagnostic_disjoint(SZrState *state, const SZrFunction *callee,
        const SZrFunction *source, const SZrExecutionArgumentStagingDiagnostic *diagnostic) {
    uintptr_t address = (uintptr_t)diagnostic;
    size_t size = sizeof(*diagnostic);
    const SZrFunction *functions[2] = {callee, source};
    SZrProfileRuntime *current = ZrCore_Profile_Current();
    if (diagnostic == ZR_NULL) return ZR_TRUE;
    if (size > UINTPTR_MAX-address) return ZR_FALSE;
    if (state != ZR_NULL) {
        uintptr_t first=(uintptr_t)state->stackBase.valuePointer;
        uintptr_t last=(uintptr_t)state->stackTail.valuePointer;
        if (argument_ranges_overlap(address,size,(uintptr_t)state,sizeof(*state)) ||
            (last >= first && argument_ranges_overlap(address,size,first,last-first))) return ZR_FALSE;
        if (state->global != ZR_NULL &&
            (argument_ranges_overlap(address,size,(uintptr_t)state->global,sizeof(*state->global)) ||
             (state->global->profileRuntime != ZR_NULL && argument_ranges_overlap(address,size,
                (uintptr_t)state->global->profileRuntime,sizeof(*state->global->profileRuntime))))) return ZR_FALSE;
    }
    if (current != ZR_NULL && argument_ranges_overlap(address,size,(uintptr_t)current,sizeof(*current))) return ZR_FALSE;
    for (unsigned i=0u;i<2u;++i) {
        const SZrFunction *function=functions[i];
        if (function != ZR_NULL &&
            (argument_ranges_overlap(address,size,(uintptr_t)function,sizeof(*function)) ||
             (TZrSize)function->frameSlotLayoutLength > SIZE_MAX/sizeof(*function->frameSlotLayouts) ||
             argument_ranges_overlap(address,size,(uintptr_t)function->frameSlotLayouts,
                (size_t)function->frameSlotLayoutLength*sizeof(*function->frameSlotLayouts)))) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool argument_unique_key(const SZrFunction *function, TZrUInt32 key) {
    TZrUInt32 matches = 0u;
    for (TZrUInt32 i = 0u; i < function->frameSlotLayoutLength; ++i) {
        if (function->frameSlotLayouts[i].stackSlot == key && ++matches > 1u) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool argument_concrete_layout(const SZrFunctionFrameSlotLayout *layout) {
    /* Only the existing direct-cache bit has a representation proof for A. */
    return (TZrBool)((layout->reserved0 &
            (TZrUInt16)~ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) == 0u);
}

static TZrBool argument_selection(const SZrFunction *callee, TZrUInt32 startSlot,
        TZrSize count, TZrBool *selected, TZrUInt32 *visits) {
    TZrBool summary;
    TZrUInt32 scan, ordinal=0u;
    *selected=ZR_FALSE; *visits=0u;
    if (callee->frameSlotLayoutLength != 0u && callee->frameSlotLayouts == ZR_NULL) return ZR_FALSE;
    summary=function_has_direct_value_parameter_summary(callee);
    scan=summary ? callee->directValueParameterScanLength : callee->frameSlotLayoutLength;
    for (TZrUInt32 i=0u;i<scan;++i) {
        const SZrFunctionFrameSlotLayout *layout=callee->frameSlotLayouts+i;
        TZrUInt32 parameter;
        ++*visits;
        if (!layout->isParameter) continue;
        parameter=summary ? ordinal++ : function_frame_parameter_index_for_stack_slot(callee,layout->stackSlot);
        if (parameter >= count || startSlot > UINT32_MAX-parameter) {
            if (summary && parameter >= count) break;
            continue;
        }
        if (layout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_VALUE || layout->byteSize < sizeof(SZrTypeValue)) continue;
        *selected=ZR_TRUE;
        if (summary && ordinal >= count) break;
    }
    return ZR_TRUE;
}

static TZrBool argument_frame_range(SZrState *state, const SZrFunction *function,
                                   TZrStackValuePointer base) {
    uintptr_t first = (uintptr_t)state->stackBase.valuePointer;
    uintptr_t last = (uintptr_t)state->stackTail.valuePointer;
    uintptr_t address = (uintptr_t)base;
    size_t denseSize, extent;
    if ((TZrSize)function->stackSize > SIZE_MAX / sizeof(SZrTypeValueOnStack)) return ZR_FALSE;
    denseSize = (size_t)function->stackSize * sizeof(SZrTypeValueOnStack);
    extent = denseSize > function->frameByteSize ? denseSize : function->frameByteSize;
    return (TZrBool)(address >= first && address <= last &&
            extent <= last - address &&
            address % _Alignof(SZrTypeValueOnStack) == 0u);
}

static TZrBool argument_dense_range(const SZrFunction *function, TZrUInt32 key) {
    if (key >= function->stackSize || (TZrSize)key > SIZE_MAX / sizeof(SZrTypeValueOnStack)) return ZR_FALSE;
    return ZR_TRUE; /* key is inside the separately validated logical dense frame. */
}

static TZrBool argument_resolve_place(SZrState *state, const SZrFunction *function,
        TZrStackValuePointer base, const SZrFunctionFrameSlotLayout *layout,
        SZrStackFramePlace *place, TZrBool *direct) {
    if (layout->byteOffset > function->frameByteSize ||
        layout->byteSize > function->frameByteSize - layout->byteOffset ||
        layout->byteAlign == 0u || (layout->byteAlign & (layout->byteAlign - 1u)) != 0u ||
        layout->byteOffset % layout->byteAlign != 0u ||
        !function_make_value_parameter_place(state, function, base, layout, place, direct)) return ZR_FALSE;
    return (TZrBool)((uintptr_t)place->address % _Alignof(SZrTypeValue) == 0u &&
            (uintptr_t)place->address % layout->byteAlign == 0u);
}

static TZrBool argument_payloads_conflict(const SZrTypeValue *a, const SZrTypeValue *av,
                                        const SZrTypeValue *b, const SZrTypeValue *bv) {
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    uintptr_t first, last;
    if (a == ZR_NULL || b == ZR_NULL ||
        !argument_ranges_overlap(ap, sizeof(*a), bp, sizeof(*b))) return ZR_FALSE;
    first = ap > bp ? ap : bp;
    last = ap + sizeof(*a) < bp + sizeof(*b) ? ap + sizeof(*a) : bp + sizeof(*b);
    return (TZrBool)(memcmp((const unsigned char *)av + (first - ap),
            (const unsigned char *)bv + (first - bp), last - first) != 0);
}

static void argument_record_write(void) {
    ZrCore_Profile_RecordHelperCurrent(ZR_PROFILE_HELPER_VALUE_RESET_NULL);
    ZrCore_Profile_RecordHelperCurrent(ZR_PROFILE_HELPER_VALUE_CONSTRUCT);
    ZrCore_Profile_RecordValueCopyCurrent(sizeof(SZrTypeValue));
}

EZrExecutionTransferStatus ZrCore_Function_StageValueFrameParametersWithWorkspace(
        SZrState *state, const SZrFunction *callee, TZrStackValuePointer destinationBase,
        const SZrFunction *source, TZrStackValuePointer sourceBase,
        TZrUInt32 startSlot, TZrSize count, void *workspace, TZrSize capacity,
        SZrExecutionArgumentStagingDiagnostic *diagnostic) {
    TZrSize required, rows = 0u;
    TZrUInt32 visits = 0u, summaryIndex = 0u, scan;
    TZrUInt32 badParameter = 0u, badSource = 0u;
    TZrBool summary, unsupported = ZR_FALSE;
    TZrBool selected;
    EZrExecutionTransferStatus invalid = ZR_EXECUTION_TRANSFER_OK;
    SZrFunctionArgumentStage *stages = (SZrFunctionArgumentStage *)workspace;
    SZrProfileRuntime *profile;
    SZrProfileRuntime *currentProfile = ZrCore_Profile_Current();
    uintptr_t scratch;
    if (!argument_diagnostic_disjoint(state,callee,source,diagnostic)) return ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT;
    if (state == ZR_NULL || callee == ZR_NULL || source == ZR_NULL ||
        destinationBase == ZR_NULL || sourceBase == ZR_NULL) {
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT, 0u, 0u);
    }
    profile = state->global != ZR_NULL ? state->global->profileRuntime : ZR_NULL;
    if (count == 0u) {
        if (profile != ZR_NULL && profile->recordHelpers) {
            profile->helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_EMPTY]++;
        }
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_OK, 0u, 0u);
    }
    if (!argument_selection(callee,startSlot,count,&selected,&visits)) {
        return argument_result(diagnostic,ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT,0u,0u);
    }
    if (!selected) {
        if (profile != ZR_NULL && profile->recordHelpers) profile->helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT] += visits;
        return argument_result(diagnostic,ZR_EXECUTION_TRANSFER_OK,0u,0u);
    }
    visits=0u;
    if (!ZrCore_Function_ValueArgumentWorkspaceSize(callee, &required) ||
        (callee->frameSlotLayoutLength != 0u && callee->frameSlotLayouts == ZR_NULL) ||
        (source->frameSlotLayoutLength != 0u && source->frameSlotLayouts == ZR_NULL) ||
        (TZrSize)source->frameSlotLayoutLength > SIZE_MAX / sizeof(*source->frameSlotLayouts) ||
        (TZrSize)callee->frameSlotLayoutLength > SIZE_MAX / sizeof(*callee->frameSlotLayouts) ||
        state->stackBase.valuePointer == ZR_NULL || state->stackTail.valuePointer == ZR_NULL ||
        (uintptr_t)state->stackTail.valuePointer < (uintptr_t)state->stackBase.valuePointer ||
        !argument_frame_range(state, callee, destinationBase) ||
        !argument_frame_range(state, source, sourceBase)) {
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT, 0u, 0u);
    }
    if (capacity < required || (required != 0u && workspace == ZR_NULL)) {
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_SCRATCH_TOO_SMALL, 0u, 0u);
    }
    scratch = (uintptr_t)workspace;
    if (required != 0u && (scratch % _Alignof(SZrFunctionArgumentStage) != 0u ||
        required > UINTPTR_MAX - scratch ||
        argument_ranges_overlap(scratch, required, (uintptr_t)state->stackBase.valuePointer,
            (uintptr_t)state->stackTail.valuePointer - (uintptr_t)state->stackBase.valuePointer) ||
        argument_ranges_overlap(scratch, required, (uintptr_t)state, sizeof(*state)) ||
        argument_ranges_overlap(scratch, required, (uintptr_t)callee, sizeof(*callee)) ||
        argument_ranges_overlap(scratch, required, (uintptr_t)source, sizeof(*source)) ||
        argument_ranges_overlap(scratch, required, (uintptr_t)callee->frameSlotLayouts,
            (size_t)callee->frameSlotLayoutLength * sizeof(*callee->frameSlotLayouts)) ||
        argument_ranges_overlap(scratch, required, (uintptr_t)source->frameSlotLayouts,
            (size_t)source->frameSlotLayoutLength * sizeof(*source->frameSlotLayouts)) ||
        (state->global != ZR_NULL && argument_ranges_overlap(scratch, required,
            (uintptr_t)state->global, sizeof(*state->global))) ||
        (profile != ZR_NULL && argument_ranges_overlap(scratch, required,
            (uintptr_t)profile, sizeof(*profile))) ||
        (currentProfile != ZR_NULL && argument_ranges_overlap(scratch, required,
            (uintptr_t)currentProfile, sizeof(*currentProfile))) ||
        (diagnostic != ZR_NULL && argument_ranges_overlap(scratch, required,
            (uintptr_t)diagnostic, sizeof(*diagnostic))))) {
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT, 0u, 0u);
    }
    summary = function_has_direct_value_parameter_summary(callee);
    scan = summary ? callee->directValueParameterScanLength : callee->frameSlotLayoutLength;
    for (TZrUInt32 i = 0u; i < scan; ++i) {
        const SZrFunctionFrameSlotLayout *destination = callee->frameSlotLayouts + i;
        const SZrFunctionFrameSlotLayout *sourceLayout;
        const SZrTypeValue *value, *dense;
        SZrStackFramePlace destinationPlace, sourcePlace;
        TZrUInt32 parameter, sourceKey;
        TZrBool direct, sourceDirect;
        SZrFunctionArgumentStage *row;
        ++visits;
        if (!destination->isParameter) continue;
        parameter = summary ? summaryIndex++ :
                function_frame_parameter_index_for_stack_slot(callee, destination->stackSlot);
        if (parameter >= count || startSlot > UINT32_MAX - parameter) {
            if (summary && parameter >= count) break;
            continue;
        }
        if (destination->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            destination->byteSize < sizeof(SZrTypeValue)) continue;
        sourceKey = startSlot + parameter;
        sourceLayout = ZrCore_Function_FindFrameSlotLayout(source, sourceKey);
        if (!argument_concrete_layout(destination) ||
            (sourceLayout != ZR_NULL && !argument_concrete_layout(sourceLayout))) {
            unsupported = ZR_TRUE;
            goto selected_done;
        }
        if (!argument_unique_key(callee, destination->stackSlot) ||
            !argument_unique_key(source, sourceKey) ||
            !argument_dense_range(callee, destination->stackSlot) ||
            !argument_dense_range(source, sourceKey) ||
            !argument_resolve_place(state, callee, destinationBase, destination, &destinationPlace, &direct)) {
            invalid = ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT;
            badParameter = parameter; badSource = sourceKey;
            goto selected_done;
        }
        dense = &sourceBase[sourceKey].value;
        value = dense;
        if (sourceLayout != ZR_NULL) {
            if (sourceLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
                sourceLayout->byteSize < sizeof(SZrTypeValue) ||
                !argument_resolve_place(state, source, sourceBase, sourceLayout, &sourcePlace, &sourceDirect)) {
                invalid = ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT;
                badParameter = parameter; badSource = sourceKey;
                goto selected_done;
            }
            direct = (TZrBool)(direct && sourceDirect);
            value = (const SZrTypeValue *)sourcePlace.address;
            if (ZR_VALUE_IS_TYPE_NULL(value->type) && !ZR_VALUE_IS_TYPE_NULL(dense->type)) value = dense;
        }
        row = stages + rows;
        row->destination = (SZrTypeValue *)destinationPlace.address;
        row->mirror = &destinationBase[destination->stackSlot].value;
        if (row->mirror == row->destination ||
            ZrCore_Value_SlotsOverlapNoProfile(row->destination, row->mirror)) row->mirror = ZR_NULL;
        if (!argument_plain_value(value) || !argument_plain_value(row->destination) ||
            (row->mirror != ZR_NULL && !argument_plain_value(row->mirror))) {
            unsupported = ZR_TRUE;
            goto selected_done;
        }
        memcpy(&row->value, value, sizeof(row->value));
        row->direct = direct;
        row->parameterIndex = parameter;
        row->sourceStackSlot = sourceKey;
        ++rows;
selected_done:
        if (summary && summaryIndex >= count) break;
    }
    if (unsupported) return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_UNSUPPORTED, 0u, 0u);
    if (profile != ZR_NULL && profile->recordHelpers) {
        profile->helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT] += visits;
    }
    if (invalid != ZR_EXECUTION_TRANSFER_OK) return argument_result(diagnostic, invalid, badParameter, badSource);
    for (TZrSize i = 0u; i < rows; ++i) {
        for (TZrSize j = 0u; j < i; ++j) {
            if (argument_payloads_conflict(stages[i].destination, &stages[i].value, stages[j].destination, &stages[j].value) ||
                argument_payloads_conflict(stages[i].destination, &stages[i].value, stages[j].mirror, &stages[j].value) ||
                argument_payloads_conflict(stages[i].mirror, &stages[i].value, stages[j].destination, &stages[j].value) ||
                argument_payloads_conflict(stages[i].mirror, &stages[i].value, stages[j].mirror, &stages[j].value)) {
                argument_result(diagnostic, ZR_EXECUTION_TRANSFER_ALIAS_CONFLICT,
                        stages[i].parameterIndex, stages[i].sourceStackSlot);
                if (diagnostic != ZR_NULL) diagnostic->relatedParameterIndex = stages[j].parameterIndex;
                return ZR_EXECUTION_TRANSFER_ALIAS_CONFLICT;
            }
        }
    }
    for (TZrSize i = 0u; i < rows; ++i) {
        if (profile != ZR_NULL && profile->recordHelpers) {
            profile->helperCounts[stages[i].direct ?
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT :
                    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED]++;
        }
        ZrCore_Profile_RecordHelperCurrent(ZR_PROFILE_HELPER_STACK_GET_VALUE);
        ZrCore_Profile_RecordHelperCurrent(ZR_PROFILE_HELPER_STACK_GET_VALUE);
        argument_record_write();
        memcpy(stages[i].destination, &stages[i].value, sizeof(stages[i].value));
        if (stages[i].mirror != ZR_NULL) {
            argument_record_write();
            memcpy(stages[i].mirror, &stages[i].value, sizeof(stages[i].value));
        }
    }
    return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_OK, 0u, 0u);
}

EZrExecutionTransferStatus ZrCore_Function_StageValueFrameParameters(
        SZrState *state, const SZrFunction *callee, TZrStackValuePointer destinationBase,
        const SZrFunction *source, TZrStackValuePointer sourceBase,
        TZrUInt32 startSlot, TZrSize count, SZrExecutionArgumentStagingDiagnostic *diagnostic) {
    SZrFunctionArgumentStage local[FUNCTION_ARGUMENT_LOCAL_ROWS];
    void *workspace = local;
    TZrSize required;
    TZrUInt32 visits;
    TZrBool selected;
    EZrExecutionTransferStatus result;
    if (!argument_diagnostic_disjoint(state,callee,source,diagnostic)) return ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT;
    if (state == ZR_NULL || callee == ZR_NULL || source == ZR_NULL ||
        destinationBase == ZR_NULL || sourceBase == ZR_NULL) {
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT, 0u, 0u);
    }
    if (count == 0u) return ZrCore_Function_StageValueFrameParametersWithWorkspace(
            state, callee, destinationBase, source, sourceBase, startSlot, count, ZR_NULL, 0u, diagnostic);
    if (!argument_selection(callee,startSlot,count,&selected,&visits)) return argument_result(diagnostic,ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT,0u,0u);
    if (!selected) return ZrCore_Function_StageValueFrameParametersWithWorkspace(
            state,callee,destinationBase,source,sourceBase,startSlot,count,ZR_NULL,0u,diagnostic);
    if (!ZrCore_Function_ValueArgumentWorkspaceSize(callee, &required)) {
        return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT, 0u, 0u);
    }
    /* No borrowed frame place is resolved until ordinary CRT allocation finishes. */
    if (required > sizeof(local)) {
        workspace = malloc(required);
        if (workspace == ZR_NULL) return argument_result(diagnostic, ZR_EXECUTION_TRANSFER_NO_MEMORY, 0u, 0u);
    }
    result = ZrCore_Function_StageValueFrameParametersWithWorkspace(state, callee, destinationBase,
            source, sourceBase, startSlot, count, workspace, required, diagnostic);
    if (workspace != local) free(workspace);
    return result;
}
