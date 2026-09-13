#include "zr_vm_parser/artifact_exec_ir.h"

TZrBool ZrParser_ExecIr_Read(SZrExecIrReader *reader,
                             SZrArtifactExecIrView *module,
                             SZrArtifactExecIrDiagnostic *diagnostic) {
    if (reader == ZR_NULL || module == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)(ZrCore_ArtifactExecIr_Read(reader->bytes, reader->length,
                                                 module, diagnostic) ==
                     ZR_ARTIFACT_EXEC_IR_OK);
}

TZrBool ZrParser_ExecIr_Write(const SZrArtifactExecIrDocument *module,
                              SZrExecIrWriter *writer,
                              SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrUInt32 written = 0u;
    if (module == ZR_NULL || writer == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrCore_ArtifactExecIr_Write(module, writer->bytes, writer->capacity,
                                    &written, diagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK) {
        writer->written = 0u;
        return ZR_FALSE;
    }
    writer->written = written;
    return ZR_TRUE;
}
