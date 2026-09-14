#ifndef ZR_VM_CORE_BATCH_CONTRACT_H
#define ZR_VM_CORE_BATCH_CONTRACT_H

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Scalar metadata shared by the library baseline and future vector passes. */
typedef enum EZrBatchDiagnosticCode {
    ZR_BATCH_DIAGNOSTIC_NONE = 0,
    ZR_BATCH_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_BATCH_DIAGNOSTIC_OVERFLOW,
    ZR_BATCH_DIAGNOSTIC_BOUNDS,
    ZR_BATCH_DIAGNOSTIC_STRIDE,
    ZR_BATCH_DIAGNOSTIC_SHAPE_MISMATCH,
    ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE,
    ZR_BATCH_DIAGNOSTIC_CALLBACK_ERROR,
    ZR_BATCH_DIAGNOSTIC_EFFECT_UNSAFE,
    ZR_BATCH_DIAGNOSTIC_UNSUPPORTED
} EZrBatchDiagnosticCode;

typedef struct SZrBatchDiagnostic {
    EZrBatchDiagnosticCode code;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrBatchDiagnostic;

typedef struct SZrBatchShape {
    TZrSize rows;
    TZrSize columns;
} SZrBatchShape;

/* data points at logical element zero.  A negative stride walks backwards. */
typedef struct SZrBatchBuffer {
    TZrPtr data;
    TZrSize byteLength;
    TZrMemoryOffset stride;
    TZrSize elementSize;
} SZrBatchBuffer;

ZR_CORE_API void ZrCore_BatchDiagnostic_Clear(SZrBatchDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_BatchDiagnostic_Name(
        EZrBatchDiagnosticCode code);
ZR_CORE_API TZrBool ZrCore_BatchShape_ElementCount(
        SZrBatchShape shape, TZrSize *count, SZrBatchDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_BatchBuffer_Validate(
        const SZrBatchBuffer *buffer, TZrSize count,
        SZrBatchDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_BatchBuffer_Offset(
        const SZrBatchBuffer *buffer, TZrSize count, TZrSize index,
        TZrMemoryOffset *offset, SZrBatchDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_BatchBuffers_MayAlias(
        const SZrBatchBuffer *left, TZrSize leftCount,
        const SZrBatchBuffer *right, TZrSize rightCount,
        TZrBool *mayAlias, SZrBatchDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_BatchBuffers_ExactLayout(
        const SZrBatchBuffer *left, const SZrBatchBuffer *right);

#ifdef __cplusplus
}
#endif

#endif
