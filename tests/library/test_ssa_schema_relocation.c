#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_parser/artifact_exec_ir.h"

#include <assert.h>
#include <string.h>

static TZrBool resolve_ok(TZrUInt32 token, TZrUInt32 kind, TZrUInt64 hash,
                           TZrUInt64 contract, TZrUInt32 *out, TZrPtr user) {
    (void)kind; (void)user;
    if (token != 7u || hash != 11u || contract != 13u || out == ZR_NULL) {
        return ZR_FALSE;
    }
    *out = 99u;
    return ZR_TRUE;
}

static TZrBool resolve_fail(TZrUInt32 token, TZrUInt32 kind, TZrUInt64 hash,
                            TZrUInt64 contract, TZrUInt32 *out, TZrPtr user) {
    (void)token; (void)kind; (void)hash; (void)contract; (void)out; (void)user;
    return ZR_FALSE;
}

int main(void) {
    TZrByte relocation[ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE] = {0};
    TZrByte ir[] = {1u, 2u, 3u, 4u};
    SZrArtifactExecIrSectionInput sections[2];
    SZrArtifactExecIrDocument document;
    SZrArtifactExecIrDiagnostic diagnostic;
    TZrByte bytes[512];
    SZrArtifactExecIrView view;
    TZrUInt32 resolved[1] = {123u};
    SZrExecIrReader reader;
    SZrExecIrWriter writer;

    relocation[0] = 7u;
    relocation[8] = 1u;
    relocation[16] = 11u;
    relocation[24] = 13u;
    memset(sections, 0, sizeof(sections));
    sections[0].kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR;
    sections[0].elementCount = 1u;
    sections[0].elementSize = sizeof(ir);
    sections[0].data = ir;
    sections[0].byteLength = sizeof(ir);
    sections[1].kind = ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS;
    sections[1].elementCount = 1u;
    sections[1].elementSize = ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE;
    sections[1].data = relocation;
    sections[1].byteLength = sizeof(relocation);
    document.abiVersion = 16u;
    document.flags = 0u;
    document.moduleHash = 21u;
    document.execIrHash = ZrCore_ArtifactExecIr_HashBytes(ir, sizeof(ir));
    document.execBcHash = 0u;
    document.sectionCount = 2u;
    document.sections = sections;

    writer.bytes = bytes;
    writer.capacity = sizeof(bytes);
    writer.written = 0u;
    assert(ZrParser_ExecIr_Write(&document, &writer, &diagnostic));
    assert(writer.written > ZR_ARTIFACT_EXEC_IR_HEADER_SIZE);
    reader.bytes = bytes;
    reader.length = writer.written;
    assert(ZrParser_ExecIr_Read(&reader, &view, &diagnostic));
    assert(view.abiVersion == 16u && view.sectionCount == 2u);
    assert(ZrCore_ArtifactExecIr_ValidateRelocations(
                   &view, resolve_ok, resolved, 1u, ZR_NULL, &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_OK);
    assert(resolved[0] == 99u);

    resolved[0] = 123u;
    assert(ZrCore_ArtifactExecIr_ValidateRelocations(
                   &view, resolve_fail, resolved, 1u, ZR_NULL, &diagnostic) ==
           ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED);
    assert(resolved[0] == 123u);

    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        corrupt[44] = (TZrByte)(writer.written - 1u);
        assert(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                          &diagnostic) !=
               ZR_ARTIFACT_EXEC_IR_OK);
    }
    {
        TZrByte corrupt[512];
        memcpy(corrupt, bytes, writer.written);
        /* Relocation directory entry offset overlaps the first section. */
        corrupt[ZR_ARTIFACT_EXEC_IR_HEADER_SIZE + ZR_ARTIFACT_EXEC_IR_SECTION_SIZE + 16u] =
                corrupt[ZR_ARTIFACT_EXEC_IR_HEADER_SIZE + 16u];
        assert(ZrCore_ArtifactExecIr_Read(corrupt, writer.written, &view,
                                          &diagnostic) !=
               ZR_ARTIFACT_EXEC_IR_OK);
    }
    return 0;
}
