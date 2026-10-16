#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS5_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS5_H

#include "zr_vm_core/artifact_exec_ir_scalar.h"

/* Private dynamic counted-scalar codec entry points. */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic);

#endif
