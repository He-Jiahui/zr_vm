#include "zr_vm_library/batch_protocol.h"

#include <stdint.h>

static TZrBool batch_prepare(const SZrBatchBuffer *input, SZrBatchBuffer *output,
                             TZrSize count, SZrBatchDiagnostic *diagnostic) {
    TZrBool alias = ZR_FALSE;
    if (output == ZR_NULL || !ZrCore_BatchBuffer_Validate(input, count, diagnostic) ||
        !ZrCore_BatchBuffer_Validate(output, count, diagnostic) ||
        !ZrCore_BatchBuffers_MayAlias(input, count, output, count, &alias, diagnostic)) return ZR_FALSE;
    if (alias && !ZrCore_BatchBuffers_ExactLayout(input, output)) {
        if (diagnostic != ZR_NULL) diagnostic->code = ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_Batch_Map(const SZrBatchBuffer *input, SZrBatchBuffer *output,
                            TZrSize count, FZrBatchUnary callback, TZrPtr userData,
                            SZrBatchDiagnostic *diagnostic) {
    TZrSize index;
    TZrMemoryOffset inOffset, outOffset;
    if (callback == ZR_NULL || !batch_prepare(input, output, count, diagnostic)) return ZR_FALSE;
    for (index = 0u; index < count; ++index) {
        if (!ZrCore_BatchBuffer_Offset(input, count, index, &inOffset, diagnostic) ||
            !ZrCore_BatchBuffer_Offset(output, count, index, &outOffset, diagnostic) ||
            !callback((const TZrByte *)input->data + inOffset,
                      (TZrByte *)output->data + outOffset, input->elementSize, userData)) {
            if (diagnostic != ZR_NULL) { diagnostic->code = ZR_BATCH_DIAGNOSTIC_CALLBACK_ERROR; diagnostic->index = (TZrUInt32)index; }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_Batch_Zip(const SZrBatchBuffer *left, const SZrBatchBuffer *right,
                            SZrBatchBuffer *output, TZrSize count, FZrBatchBinary callback,
                            TZrPtr userData, SZrBatchDiagnostic *diagnostic) {
    TZrBool aliasLeft, aliasRight;
    TZrSize index;
    TZrMemoryOffset leftOffset, rightOffset, outputOffset;
    if (callback == ZR_NULL || output == ZR_NULL ||
        !ZrCore_BatchBuffer_Validate(left, count, diagnostic) ||
        !ZrCore_BatchBuffer_Validate(right, count, diagnostic) ||
        !ZrCore_BatchBuffer_Validate(output, count, diagnostic) ||
        left->elementSize != right->elementSize || left->elementSize != output->elementSize ||
        !ZrCore_BatchBuffers_MayAlias(left, count, output, count, &aliasLeft, diagnostic) ||
        !ZrCore_BatchBuffers_MayAlias(right, count, output, count, &aliasRight, diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_BATCH_DIAGNOSTIC_NONE) diagnostic->code = ZR_BATCH_DIAGNOSTIC_SHAPE_MISMATCH;
        return ZR_FALSE;
    }
    if (aliasLeft || aliasRight) {
        if (diagnostic != ZR_NULL) diagnostic->code = ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE;
        return ZR_FALSE;
    }
    for (index = 0u; index < count; ++index) {
        if (!ZrCore_BatchBuffer_Offset(left, count, index, &leftOffset, diagnostic) ||
            !ZrCore_BatchBuffer_Offset(right, count, index, &rightOffset, diagnostic) ||
            !ZrCore_BatchBuffer_Offset(output, count, index, &outputOffset, diagnostic) ||
            !callback((const TZrByte *)left->data + leftOffset,
                      (const TZrByte *)right->data + rightOffset,
                      (TZrByte *)output->data + outputOffset, left->elementSize, userData)) {
            if (diagnostic != ZR_NULL) { diagnostic->code = ZR_BATCH_DIAGNOSTIC_CALLBACK_ERROR; diagnostic->index = (TZrUInt32)index; }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_Batch_MatrixMap2D(SZrBatchShape shape, const SZrBatchBuffer *left,
                                    const SZrBatchBuffer *right, SZrBatchBuffer *output,
                                    FZrBatchBinary callback, TZrPtr userData,
                                    SZrBatchDiagnostic *diagnostic) {
    TZrSize count;
    if (!ZrCore_BatchShape_ElementCount(shape, &count, diagnostic)) return ZR_FALSE;
    return ZrLibrary_Batch_Zip(left, right, output, count, callback, userData, diagnostic);
}
