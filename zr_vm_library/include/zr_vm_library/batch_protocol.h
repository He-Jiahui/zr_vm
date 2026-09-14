#ifndef ZR_VM_LIBRARY_BATCH_PROTOCOL_H
#define ZR_VM_LIBRARY_BATCH_PROTOCOL_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/batch_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef TZrBool (*FZrBatchUnary)(const TZrByte *input, TZrByte *output,
                                TZrSize elementSize, TZrPtr userData);
typedef TZrBool (*FZrBatchBinary)(const TZrByte *left, const TZrByte *right,
                                 TZrByte *output, TZrSize elementSize,
                                 TZrPtr userData);

/* Ordered scalar baselines.  A false callback result stops at that index. */
ZR_LIBRARY_API TZrBool ZrLibrary_Batch_Map(
        const SZrBatchBuffer *input, SZrBatchBuffer *output, TZrSize count,
        FZrBatchUnary callback, TZrPtr userData,
        SZrBatchDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_Batch_Zip(
        const SZrBatchBuffer *left, const SZrBatchBuffer *right,
        SZrBatchBuffer *output, TZrSize count, FZrBatchBinary callback,
        TZrPtr userData, SZrBatchDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_Batch_MatrixMap2D(
        SZrBatchShape shape, const SZrBatchBuffer *left,
        const SZrBatchBuffer *right, SZrBatchBuffer *output,
        FZrBatchBinary callback, TZrPtr userData,
        SZrBatchDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif
