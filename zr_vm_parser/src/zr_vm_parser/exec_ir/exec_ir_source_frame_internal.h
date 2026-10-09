#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_FRAME_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_FRAME_INTERNAL_H

#include "zr_vm_parser/exec_ir_frame_layout.h"
#include "zr_vm_parser/exec_ir_source_frame.h"

#include <stdint.h>

/* Four caller objects, seventeen function pools, three optional owners,
 * eight nested pools and the canonical pool. GC owns one header. */
enum { SOURCE_FRAME_SPAN_LIMIT = 33 };
typedef struct SZrSourceFrameSpan {
    uintptr_t start;
    uintptr_t end;
} SZrSourceFrameSpan;
typedef struct SZrSourceFrameStorage {
    SZrSourceFrameSpan spans[SOURCE_FRAME_SPAN_LIMIT];
    size_t count;
    EZrExecutionDiagnosticCode error;
} SZrSourceFrameStorage;

TZrBool source_frame_fail(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic,
                          EZrExecutionDiagnosticCode code, const SZrExecIrValue *value);
TZrBool source_frame_count_fits(size_t count, size_t width);
TZrBool source_frame_storage(const SZrExecIrFunction *function, const SZrSemanticContext *context,
                             const SZrExecIrLayout *layouts, TZrUInt32 layoutCount, SZrExecIrDiagnostic *diagnostic,
                             SZrSourceFrameStorage *storage);
TZrBool source_frame_body_metadata(const SZrExecIrFunction *function);
TZrBool source_frame_rows(const SZrExecIrLayout *layouts, TZrUInt32 count);
TZrBool source_frame_admit_values(const SZrExecIrFunction *function, const SZrSemanticContext *context,
                                  const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
                                  SZrExecIrDiagnostic *diagnostic);
TZrBool source_frame_prepare_request(const SZrExecIrFunction *function, const SZrExecIrLayout *layouts,
                                     TZrUInt32 layoutCount, TZrUInt32 frameByteLimit, SZrExecIrPackedValue **valuesOut,
                                     SZrExecIrPackedFrameRequest *requestOut, SZrExecIrDiagnostic *diagnostic);
TZrBool source_frame_geometry(const SZrExecIrFunction *function, const SZrExecIrPackedValue *values,
                              const SZrExecIrPackedFrameLayout *packed, TZrUInt32 frameByteLimit);

#endif
