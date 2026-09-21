#include "zr_vm_parser/exec_ir_frame_layout.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void frame_diag(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
                       TZrMetadataToken functionToken, TZrUInt32 valueId) {
    if (d != ZR_NULL) {
        memset(d, 0, sizeof(*d));
        d->code = code;
        d->functionToken = functionToken;
        d->actualVersion = valueId;
    }
}

static TZrBool checked_add(TZrUInt32 a, TZrUInt32 b, TZrUInt32 *out) {
    if (out == ZR_NULL || b > UINT32_MAX - a) return ZR_FALSE;
    *out = a + b;
    return ZR_TRUE;
}

static TZrBool checked_align(TZrUInt32 value, TZrUInt32 align, TZrUInt32 *out) {
    TZrUInt32 mask;
    if (out == ZR_NULL || align == 0u || (align & (align - 1u)) != 0u) return ZR_FALSE;
    mask = align - 1u;
    if (value > UINT32_MAX - mask) return ZR_FALSE;
    *out = (value + mask) & ~mask;
    return ZR_TRUE;
}

static TZrBool overlap(const SZrExecIrPackedValue *a, const SZrExecIrPackedValue *b) {
    return (TZrBool)(a->liveStart < b->liveEnd && b->liveStart < a->liveEnd);
}

static TZrUInt64 hash_mix(TZrUInt64 h, TZrUInt64 x) {
    h ^= x;
    h *= UINT64_C(1099511628211);
    return h;
}

static TZrBool frame_count_fits(TZrUInt32 count, size_t elementSize) {
#if SIZE_MAX < UINT32_MAX
    return (TZrBool)(elementSize != 0u &&
                     count <= (TZrUInt32)(SIZE_MAX / elementSize));
#else
    (void)count;
    (void)elementSize;
    return ZR_TRUE;
#endif
}

void ZrParser_ExecIr_PackedFrameLayoutInit(SZrExecIrPackedFrameLayout *layout) {
    if (layout != ZR_NULL) memset(layout, 0, sizeof(*layout));
}

void ZrParser_ExecIr_PackedFrameLayoutFree(SZrExecIrPackedFrameLayout *layout) {
    if (layout == ZR_NULL) return;
    ZrCore_ExecIr_FrameLayoutFree(&layout->frame);
    free(layout->logicalToPhysical);
    free(layout->slotClasses);
    memset(layout, 0, sizeof(*layout));
}

TZrBool ZrParser_ExecIr_LayoutPackedFrame(const SZrExecIrPackedFrameRequest *request,
                                          SZrExecIrPackedFrameLayout *layout,
                                          SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrPackedFrameLayout candidate;
    TZrUInt32 *order = ZR_NULL;
    TZrUInt32 physicalCount = 0u, cursor = 0u, maxAlign = 1u;
    TZrUInt32 i;
    TZrUInt64 hash = UINT64_C(1469598103934665603);

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (request == ZR_NULL || layout == ZR_NULL ||
        (request->valueCount != 0u && request->values == ZR_NULL) ||
        request->parameterPrefixCount > request->valueCount) {
        frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                   request != ZR_NULL ? request->functionToken : 0u, 0u);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_PackedFrameLayoutInit(&candidate);
    if (request->valueCount > 0u) {
        if (!frame_count_fits(request->valueCount, sizeof(TZrUInt32)) ||
            !frame_count_fits(request->valueCount, sizeof(EZrExecIrPackedSlotClass)) ||
            !frame_count_fits(request->valueCount, sizeof(SZrExecIrFrameSlot))) {
            frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       request->functionToken, 0u);
            return ZR_FALSE;
        }
        order = (TZrUInt32 *)malloc((size_t)request->valueCount * sizeof(*order));
        candidate.logicalToPhysical = (TZrUInt32 *)malloc((size_t)request->valueCount * sizeof(TZrUInt32));
        candidate.slotClasses = (EZrExecIrPackedSlotClass *)malloc((size_t)request->valueCount * sizeof(*candidate.slotClasses));
        if (order == ZR_NULL || candidate.logicalToPhysical == ZR_NULL || candidate.slotClasses == ZR_NULL) {
            frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, request->functionToken, 0u);
            free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
        }
        for (i = 0u; i < request->valueCount; ++i) {
            const SZrExecIrPackedValue *v = &request->values[i];
            if (v->valueId == ZR_EXEC_IR_VALUE_ID_INVALID || v->slotClass >= ZR_EXEC_IR_PACKED_SLOT_CLASS_COUNT ||
                v->byteSize == 0u || v->byteAlign == 0u || (v->byteAlign & (v->byteAlign - 1u)) != 0u ||
                v->liveEnd < v->liveStart) {
                frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, request->functionToken, i);
                free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
            }
            for (TZrUInt32 prior = 0u; prior < i; ++prior) {
                if (request->values[prior].valueId == v->valueId) {
                    frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               request->functionToken, i);
                    free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
                }
            }
            order[i] = i;
            candidate.logicalToPhysical[i] = ZR_EXEC_IR_VALUE_ID_INVALID;
            candidate.slotClasses[i] = v->slotClass;
        }
        /* Stable class ordering makes physical offsets deterministic. */
        for (i = 1u; i < request->valueCount; ++i) {
            TZrUInt32 key = order[i], j = i;
            while (j > 0u && request->values[order[j - 1u]].slotClass > request->values[key].slotClass) {
                order[j] = order[j - 1u]; --j;
            }
            order[j] = key;
        }
        candidate.frame.slots = (SZrExecIrFrameSlot *)calloc(request->valueCount, sizeof(*candidate.frame.slots));
        if (candidate.frame.slots == ZR_NULL) {
            frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, request->functionToken, 0u);
            free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
        }
        for (i = 0u; i < request->valueCount; ++i) {
            const SZrExecIrPackedValue *v = &request->values[order[i]];
            TZrUInt32 physical = UINT32_MAX, p;
            TZrBool pinned = (TZrBool)((v->flags & (ZR_EXEC_IR_PACKED_SLOT_ADDRESS_ESCAPED | ZR_EXEC_IR_PACKED_SLOT_PARAMETER)) != 0u ||
                                       order[i] < request->parameterPrefixCount);
            for (p = 0u; p < physicalCount && !pinned; ++p) {
                const SZrExecIrFrameSlot *slot = &candidate.frame.slots[p];
                TZrBool compatible = (TZrBool)(slot->byteSize >= v->byteSize &&
                                                slot->byteAlign >= v->byteAlign);
                /* A physical slot may have several non-overlapping logical
                 * occupants.  Check every prior occupant, not just the first
                 * value that created the slot; otherwise A/B/C lifetimes can
                 * incorrectly reuse A's slot while overlapping B. */
                for (TZrUInt32 prior = 0u; prior < i && compatible; ++prior) {
                    if (candidate.logicalToPhysical[order[prior]] == p) {
                        if (candidate.slotClasses[order[prior]] != v->slotClass ||
                            overlap(v, &request->values[order[prior]])) {
                            compatible = ZR_FALSE;
                        }
                    }
                }
                if (compatible) physical = p;
            }
            if (physical == UINT32_MAX) {
                TZrUInt32 aligned;
                if (!checked_align(cursor, v->byteAlign, &aligned) || !checked_add(aligned, v->byteSize, &cursor)) {
                    frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, request->functionToken, order[i]);
                    free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
                }
                physical = physicalCount++;
                candidate.frame.slots[physical].slotId = v->valueId;
                candidate.frame.slots[physical].byteOffset = aligned;
                candidate.frame.slots[physical].byteSize = v->byteSize;
                candidate.frame.slots[physical].byteAlign = v->byteAlign;
                candidate.frame.slots[physical].typeToken = v->typeToken;
                if (v->byteAlign > maxAlign) maxAlign = v->byteAlign;
            }
            candidate.logicalToPhysical[order[i]] = physical;
        }
    }
    if (request->returnBufferAlign != 0u &&
        ((request->returnBufferAlign & (request->returnBufferAlign - 1u)) != 0u)) {
        frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                   request->functionToken, 0u);
        free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
    }
    if (request->returnBufferAlign > maxAlign) maxAlign = request->returnBufferAlign;
    {
        TZrUInt32 frameEnd;
        if (!checked_align(cursor,
                           request->returnBufferAlign == 0u ? maxAlign : request->returnBufferAlign,
                           &candidate.frame.returnBufferOffset) ||
            !checked_add(candidate.frame.returnBufferOffset, request->returnBufferSize,
                         &frameEnd) ||
            !checked_align(frameEnd, maxAlign, &candidate.frame.frameByteSize) ||
            (request->frameByteLimit != 0u &&
             candidate.frame.frameByteSize > request->frameByteLimit)) {
            frame_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       request->functionToken, 0u);
            free(order); ZrParser_ExecIr_PackedFrameLayoutFree(&candidate); return ZR_FALSE;
        }
    }
    candidate.frame.storageSlotCount = physicalCount;
    candidate.frame.logicalSlotCount = request->valueCount;
    candidate.frame.parameterPrefixCount = request->parameterPrefixCount;
    candidate.frame.parameterCount = request->parameterPrefixCount;
    candidate.frame.localCount = request->valueCount - request->parameterPrefixCount;
    candidate.frame.frameByteAlign = maxAlign;
    candidate.frame.slotCount = physicalCount;
    candidate.frame.slotCapacity = request->valueCount;
    hash = hash_mix(hash, request->functionToken);
    hash = hash_mix(hash, candidate.frame.logicalSlotCount);
    hash = hash_mix(hash, candidate.frame.storageSlotCount);
    hash = hash_mix(hash, candidate.frame.parameterPrefixCount);
    hash = hash_mix(hash, candidate.frame.returnBufferOffset);
    hash = hash_mix(hash, candidate.frame.frameByteSize);
    hash = hash_mix(hash, candidate.frame.frameByteAlign);
    hash = hash_mix(hash, candidate.frame.parameterCount);
    hash = hash_mix(hash, candidate.frame.localCount);
    for (i = 0u; i < request->valueCount; ++i) {
        hash = hash_mix(hash, candidate.logicalToPhysical[i]);
        hash = hash_mix(hash, candidate.slotClasses[i]);
    }
    for (i = 0u; i < physicalCount; ++i) {
        const SZrExecIrFrameSlot *s = &candidate.frame.slots[i];
        hash = hash_mix(hash, ((TZrUInt64)s->slotId << 32u) | s->byteOffset);
        hash = hash_mix(hash, ((TZrUInt64)s->byteSize << 32u) | s->byteAlign);
        hash = hash_mix(hash, s->typeToken);
    }
    candidate.frame.layoutHash = hash;
    free(order);
    ZrParser_ExecIr_PackedFrameLayoutFree(layout);
    *layout = candidate;
    return ZR_TRUE;
}
