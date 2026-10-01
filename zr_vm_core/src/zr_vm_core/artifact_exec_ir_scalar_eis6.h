#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS6_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS6_H

#include "zr_vm_core/artifact_exec_ir_scalar.h"

/* Private standalone EIS6 codec entry points. */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic);

#endif
