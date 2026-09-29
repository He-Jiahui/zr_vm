#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS4_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS4_H

#include "zr_vm_core/artifact_exec_ir_scalar.h"

/* Private codec entry points used by the public scalar codec dispatcher. */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis4_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis4_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic);

#endif
