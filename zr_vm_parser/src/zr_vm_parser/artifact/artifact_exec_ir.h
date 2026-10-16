#ifndef ZR_VM_PARSER_ARTIFACT_EXEC_IR_H
#define ZR_VM_PARSER_ARTIFACT_EXEC_IR_H

#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

typedef struct SZrExecIrReader {
    const TZrByte *bytes;
    TZrUInt32 length;
} SZrExecIrReader;

typedef struct SZrExecIrWriter {
    TZrByte *bytes;
    TZrUInt32 capacity;
    TZrUInt32 written;
} SZrExecIrWriter;

ZR_PARSER_API TZrBool ZrParser_ExecIr_Read(
        SZrExecIrReader *reader, SZrArtifactExecIrView *module,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_Write(
        const SZrArtifactExecIrDocument *module, SZrExecIrWriter *writer,
        SZrArtifactExecIrDiagnostic *diagnostic);

/* Persist the first canonical ExecIR scalar subset as a ZRAF v6 ZRO file.
 * metadata must contain the seven validated identity sections, and filename
 * is opened only after all graph and artifact encoding succeeds. */
ZR_PARSER_API EZrArtifactStatus ZrParser_ExecIr_WriteCanonicalZroFile(
        const SZrArtifactDocument *metadata, const SZrExecIrModule *module,
        const char *filename, SZrArtifactDiagnostic *diagnostic);

#endif
