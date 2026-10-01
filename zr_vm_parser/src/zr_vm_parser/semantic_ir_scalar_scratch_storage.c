#include "semantic_ir_scalar_scratch_internal.h"

#include <stdint.h>
#include <string.h>

#include "zr_vm_core/memory.h"

TZrBool zr_parser_semantic_ir_scalar_scratch_append_proof(
        SZrSemanticIrFunction *function,
        const SZrSemanticIrScalarScratchProof *proof) {
    SZrArray *proofs;
    TZrSize index;

    if (function == ZR_NULL || function->state == ZR_NULL || proof == ZR_NULL ||
        proof->placeId == ZR_PLACE_ID_INVALID ||
        proof->constantValueId == ZR_VALUE_ID_INVALID ||
        (proof->valueType != ZR_VALUE_TYPE_BOOL &&
         proof->valueType != ZR_VALUE_TYPE_INT64) ||
        !zr_parser_semantic_ir_scalar_scratch_proofs_well_formed(function) ||
        !zr_parser_semantic_ir_scalar_scratch_matches_direct_initializer(
                function, proof)) {
        return ZR_FALSE;
    }

    proofs = &function->scalarScratchProofs;
    for (index = 0U; index < proofs->length; ++index) {
        const SZrSemanticIrScalarScratchProof *previous =
                (const SZrSemanticIrScalarScratchProof *)ZrCore_Array_Get(
                        proofs, index);
        if (previous == ZR_NULL || previous->placeId == proof->placeId ||
            previous->constantValueId == proof->constantValueId) {
            return ZR_FALSE;
        }
    }

    if (proofs->length < proofs->capacity) {
        memcpy(proofs->head + proofs->length * proofs->elementSize,
               proof, sizeof(*proof));
        ++proofs->length;
        return ZR_TRUE;
    }

    {
        TZrSize oldCapacity = proofs->capacity;
        TZrSize maximum = (TZrSize)SIZE_MAX;
        TZrSize newCapacity;
        TZrSize oldBytes;
        TZrSize newBytes;
        TZrBytePtr newHead;
        TZrBytePtr oldHead;

        if (function->state->global == ZR_NULL ||
            proofs->elementSize != sizeof(*proof) || oldCapacity == 0U ||
            oldCapacity > maximum - oldCapacity / 2U - 1U) {
            return ZR_FALSE;
        }
        newCapacity = oldCapacity + oldCapacity / 2U + 1U;
        if (newCapacity > maximum / proofs->elementSize ||
            oldCapacity > maximum / proofs->elementSize) {
            return ZR_FALSE;
        }
        oldBytes = oldCapacity * proofs->elementSize;
        newBytes = newCapacity * proofs->elementSize;
        newHead = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(
                function->state->global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (newHead == ZR_NULL) {
            return ZR_FALSE;
        }
        memcpy(newHead, proofs->head, oldBytes);
        memcpy(newHead + proofs->length * proofs->elementSize,
               proof, sizeof(*proof));

        oldHead = proofs->head;
        proofs->head = newHead;
        proofs->capacity = newCapacity;
        ++proofs->length;
        ZrCore_Memory_RawFreeWithType(
                function->state->global, oldHead, oldBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_TRUE;
}
