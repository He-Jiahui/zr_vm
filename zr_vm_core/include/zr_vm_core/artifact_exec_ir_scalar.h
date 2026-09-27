#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_H

#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/exec_ir.h"

/* EIS1 is the first canonical ExecIR graph payload inside an ERI1 EXEC_IR
 * section. It supports one no-argument i64 function with CONSTANT, RETURN.
 * The exact width is a wire width, never sizeof a runtime C record. */
#define ZR_ARTIFACT_EXEC_IR_SCALAR_MAGIC ((TZrUInt32)0x31534945u)
#define ZR_ARTIFACT_EXEC_IR_SCALAR_VERSION ((TZrUInt16)1u)
#define ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE ((TZrUInt32)412u)
#define ZR_ARTIFACT_EXEC_IR_SCALAR_CONSTANT_OPCODE_OFFSET ((TZrUInt32)236u)

/* Rejects every graph field or side table outside the scalar subset. On
 * failure the destination bytes are unchanged. */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic);

/* The caller passes an initialized empty output module. This decoder builds
 * and verifies a temporary graph, then transfers ownership only on success. */
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic);

#endif
