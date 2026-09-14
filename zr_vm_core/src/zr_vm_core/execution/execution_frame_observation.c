#include "zr_vm_core/execution_frame_layout.h"

#include <string.h>

static void execution_frame_observation_diag(
        SZrExecutionFrameDiagnostic *diagnostic,
        EZrExecutionFrameDiagnosticCode code,
        TZrUInt32 index,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->index = index;
    diagnostic->relatedIndex = 0u;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

static TZrBool execution_frame_observation_add_u32(TZrUInt32 left,
                                                   TZrUInt32 right,
                                                   TZrUInt32 *out) {
    if (out == ZR_NULL || right > UINT32_MAX - left) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool execution_frame_observation_has_physical(
        const TZrUInt32 *slots,
        TZrUInt32 count,
        TZrUInt32 physical) {
    TZrUInt32 index;
    for (index = 0u; index < count; ++index) {
        if (slots[index] == physical) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrCore_Execution_ObserveFrame(
        const SZrFrameObservationRequest *request,
        SZrExecutionDiagnostic *diagnostic) {
    const SZrExecutionFrameLayout *layout;
    TZrUInt32 frameByteSize;
    TZrUInt32 index;
    TZrUInt32 uniquePhysicalCount = 0u;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (request == ZR_NULL || (layout = request->layout) == ZR_NULL ||
        request->frameBase == ZR_NULL ||
        (request->flags & ~ZR_EXECUTION_FRAME_OBSERVE_KNOWN_MASK) != 0u ||
        !ZrCore_ExecutionFrameLayout_Validate(layout, diagnostic)) {
        execution_frame_observation_diag(
                diagnostic, ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                0u, 1u, 0u);
        return ZR_FALSE;
    }
    frameByteSize = request->frameByteSize != 0u
                        ? request->frameByteSize
                        : layout->frameByteSize;
    if (frameByteSize < layout->frameByteSize ||
        ((request->flags & ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS) != 0u &&
         (request->scalarValues == ZR_NULL ||
          request->scalarValueCount < layout->slotCount)) ||
        (request->writebackValues != ZR_NULL &&
         request->writebackCapacity < layout->slotCount)) {
        execution_frame_observation_diag(
                diagnostic, ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                0u, layout->frameByteSize, frameByteSize);
        return ZR_FALSE;
    }

    /* Check every destination before modifying frame memory.  This gives
     * debugger/deopt callers an all-or-nothing observation boundary. */
    for (index = 0u; index < layout->slotCount; ++index) {
        const SZrExecutionFrameSlot *slot = &layout->slots[index];
        TZrUInt32 end;
        TZrBool seenPhysical = ZR_FALSE;
        TZrUInt32 prior;
        if (!execution_frame_observation_add_u32(slot->byteOffset,
                                                 slot->byteSize, &end) ||
            end > frameByteSize ||
            ((request->flags & ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS) != 0u &&
             slot->slotClass == ZR_EXECUTION_FRAME_SLOT_SCALAR &&
             slot->byteSize > sizeof(TZrUInt64))) {
            execution_frame_observation_diag(
                    diagnostic, ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
                    index, frameByteSize, slot->byteOffset);
            return ZR_FALSE;
        }
        for (prior = 0u; prior < index; ++prior) {
            if (layout->slots[prior].physicalSlot == slot->physicalSlot) {
                seenPhysical = ZR_TRUE;
                break;
            }
        }
        if ((request->flags & ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE) != 0u &&
            !seenPhysical) {
            ++uniquePhysicalCount;
        }
    }
    if ((request->flags & ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE) != 0u) {
        if (request->invalidatedCount == ZR_NULL ||
            (uniquePhysicalCount != 0u &&
             request->invalidatedPhysicalSlots == ZR_NULL) ||
            request->invalidatedCapacity < uniquePhysicalCount) {
            execution_frame_observation_diag(
                    diagnostic, ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                    0u, uniquePhysicalCount,
                    request->invalidatedCapacity);
            return ZR_FALSE;
        }
    }

    uniquePhysicalCount = 0u;
    for (index = 0u; index < layout->slotCount; ++index) {
        const SZrExecutionFrameSlot *slot = &layout->slots[index];
        TZrUInt32 copySize = slot->byteSize < sizeof(TZrUInt64)
                                 ? slot->byteSize
                                 : (TZrUInt32)sizeof(TZrUInt64);
        TZrByte *address = request->frameBase + slot->byteOffset;
        if ((request->flags & ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS) != 0u &&
            slot->slotClass == ZR_EXECUTION_FRAME_SLOT_SCALAR) {
            memcpy(address, &request->scalarValues[index], copySize);
        }
        if (request->writebackValues != ZR_NULL) {
            request->writebackValues[index] = 0u;
            memcpy(&request->writebackValues[index], address, copySize);
        }
        if ((request->flags & ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE) != 0u &&
            !execution_frame_observation_has_physical(
                    request->invalidatedPhysicalSlots, uniquePhysicalCount,
                    slot->physicalSlot)) {
            request->invalidatedPhysicalSlots[uniquePhysicalCount++] =
                    slot->physicalSlot;
        }
    }
    if (request->invalidatedCount != ZR_NULL) {
        *request->invalidatedCount = uniquePhysicalCount;
    }
    return ZR_TRUE;
}
