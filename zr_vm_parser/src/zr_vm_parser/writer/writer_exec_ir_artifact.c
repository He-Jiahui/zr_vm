#include "zr_vm_parser/artifact_exec_ir.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_common/zr_aot_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef ZR_PLATFORM_WIN
#include <windows.h>
#endif

#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
static TZrBool execIrArtifactForceWriteFailure = ZR_FALSE;
void ZrParser_ExecIrArtifact_TestInjectWriteFailure(TZrBool enabled);
void ZrParser_ExecIrArtifact_TestInjectWriteFailure(TZrBool enabled) {
    execIrArtifactForceWriteFailure = enabled;
}
#endif

static EZrArtifactStatus exec_ir_writer_fail(
        SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status,
        TZrUInt32 sectionKind) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = sectionKind;
    }
    return status;
}

static TZrBool exec_ir_writer_metadata_is_supported(
        const SZrArtifactDocument *metadata, const SZrExecIrModule *module) {
    static const EZrArtifactSectionKind requiredSections[] = {
        ZR_ARTIFACT_SECTION_STRING_HEAP,
        ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
        ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
        ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
        ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
        ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
        ZR_ARTIFACT_SECTION_LAYOUT_TABLE
    };
    const SZrArtifactContractRow *contract = ZR_NULL;
    if (metadata->kind != ZR_ARTIFACT_KIND_ZRO || metadata->flags != 0u ||
        metadata->sectionCount != 7u || metadata->sections == ZR_NULL)
        return ZR_FALSE;
    for (TZrUInt32 required = 0u; required < 7u; ++required) {
        TZrBool found = ZR_FALSE;
        for (TZrUInt32 index = 0u; index < metadata->sectionCount; ++index) {
            const SZrArtifactSectionInput *section = &metadata->sections[index];
            if (section->kind == requiredSections[required] &&
                section->flags == ZR_ARTIFACT_SECTION_FLAG_MANDATORY) {
                if (found) return ZR_FALSE;
                found = ZR_TRUE;
                if (section->kind == ZR_ARTIFACT_SECTION_CONTRACT_TABLE) {
                    if (section->elementCount != 1u || section->data == ZR_NULL)
                        return ZR_FALSE;
                    contract = (const SZrArtifactContractRow *)section->data;
                }
            }
        }
        if (!found) return ZR_FALSE;
    }
    if (contract == ZR_NULL ||
        metadata->identity.moduleHash != module->moduleHash ||
        metadata->identity.moduleHash != module->contract.moduleHash ||
        metadata->identity.moduleHash !=
                module->functions[0].contract.moduleHash ||
        metadata->identity.signatureHash != module->functions[0].signatureHash ||
        metadata->identity.signatureHash != module->contract.signatureHash ||
        metadata->identity.signatureHash !=
                module->functions[0].contract.signatureHash ||
        metadata->identity.layoutHash != module->contract.layoutHash ||
        metadata->identity.layoutHash !=
                module->functions[0].contract.layoutHash ||
        contract->memberToken != module->functions[0].functionToken ||
        contract->signatureToken != metadata->identity.signatureToken ||
        contract->contractHash != metadata->identity.callableContractHash ||
        contract->parameterCount != 0u || contract->flags != 0u ||
        contract->receiverEffect != ZR_ARTIFACT_RECEIVER_NONE ||
        contract->refExportEffect != ZR_ARTIFACT_REF_EXPORT_NONE ||
        contract->escapeFlags != 0u ||
        contract->abiLoweringKind != ZR_ARTIFACT_ABI_LOWERING_NONE)
        return ZR_FALSE;
    return ZR_TRUE;
}

/* Persist one verified canonical scalar graph in a ZRAF ZRO. All encoders run
 * before fopen, so validation failures cannot replace an existing artifact. */
EZrArtifactStatus ZrParser_ExecIr_WriteCanonicalZroFile(
        const SZrArtifactDocument *metadata, const SZrExecIrModule *module,
        const char *filename, SZrArtifactDiagnostic *diagnostic) {
    TZrByte payload[ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE];
    SZrArtifactExecIrSectionInput nestedSection;
    SZrArtifactExecIrDocument nestedDocument;
    SZrArtifactExecIrDiagnostic nestedDiagnostic;
    SZrArtifactSectionInput sections[8];
    SZrArtifactDocument document;
    TZrByte *nestedBytes = ZR_NULL;
    TZrByte *outerBytes = ZR_NULL;
    TZrUInt32 nestedSize = 0u;
    TZrUInt32 nestedWritten = 0u;
    TZrUInt32 payloadSize = 0u;
    TZrSize outerSize = 0u;
    TZrSize outerWritten = 0u;
    EZrArtifactStatus status;
    FILE *file = ZR_NULL;
    char *temporaryFilename = ZR_NULL;
    size_t filenameLength;
    TZrBool writeFailed = ZR_FALSE;
    if (metadata == ZR_NULL || module == ZR_NULL || filename == ZR_NULL ||
        filename[0] == '\0')
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    if (ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                module, &payloadSize, &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK)
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
    if (ZrCore_ArtifactExecIrScalar_Write(module, payload, sizeof(payload),
                                           &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK)
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
    if (!exec_ir_writer_metadata_is_supported(metadata, module))
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
    memset(&nestedSection, 0, sizeof(nestedSection));
    nestedSection.kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR;
    nestedSection.elementCount = payloadSize;
    nestedSection.elementSize = 1u;
    nestedSection.data = payload;
    nestedSection.byteLength = payloadSize;
    memset(&nestedDocument, 0, sizeof(nestedDocument));
    nestedDocument.abiVersion = ZR_VM_AOT_ABI_VERSION;
    nestedDocument.moduleHash = module->moduleHash;
    nestedDocument.execIrHash = ZrCore_ArtifactExecIr_HashBytes(
            payload, payloadSize);
    nestedDocument.sectionCount = 1u;
    nestedDocument.sections = &nestedSection;
    if (ZrCore_ArtifactExecIr_GetEncodedSize(&nestedDocument, &nestedSize,
                                             &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK || nestedSize == 0u)
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
    nestedBytes = (TZrByte *)malloc(nestedSize);
    if (nestedBytes == ZR_NULL)
        return exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
    if (ZrCore_ArtifactExecIr_Write(&nestedDocument, nestedBytes, nestedSize,
                                    &nestedWritten, &nestedDiagnostic) !=
        ZR_ARTIFACT_EXEC_IR_OK || nestedWritten != nestedSize) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE);
        goto done;
    }
    memcpy(sections, metadata->sections,
           metadata->sectionCount * sizeof(sections[0]));
    sections[7] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
        ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
        nestedSize, nestedBytes};
    document = *metadata;
    document.sectionCount = 8u;
    document.sections = sections;
    status = ZrCore_Artifact_GetEncodedSize(&document, &outerSize,
                                            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || outerSize == 0u) goto done;
    outerBytes = (TZrByte *)malloc(outerSize);
    if (outerBytes == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u);
        goto done;
    }
    status = ZrCore_Artifact_Write(&document, outerBytes, outerSize,
                                   &outerWritten, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || outerWritten != outerSize) {
        if (status == ZR_ARTIFACT_STATUS_OK)
            status = exec_ir_writer_fail(diagnostic,
                    ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
    filenameLength = strlen(filename);
    if (filenameLength > SIZE_MAX - 32u) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
        goto done;
    }
    temporaryFilename = (char *)malloc(filenameLength + 32u);
    if (temporaryFilename == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u);
        goto done;
    }
    for (unsigned attempt = 0u; attempt < 32u; ++attempt) {
        int length = snprintf(temporaryFilename, filenameLength + 32u,
                              "%s.zr-tmp-%u", filename, attempt);
        if (length < 0 || (size_t)length >= filenameLength + 32u) break;
        file = fopen(temporaryFilename, "wbx");
        if (file != ZR_NULL || errno != EEXIST) break;
    }
    if (file == ZR_NULL) {
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
        goto done;
    }
    if (fwrite(outerBytes, 1u, outerWritten, file) != outerWritten)
        writeFailed = ZR_TRUE;
#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
    if (execIrArtifactForceWriteFailure) writeFailed = ZR_TRUE;
#endif
    if (fclose(file) != 0) writeFailed = ZR_TRUE;
    if (writeFailed) {
        remove(temporaryFilename);
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
#ifdef ZR_PLATFORM_WIN
    if (!MoveFileExA(temporaryFilename, filename,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    if (rename(temporaryFilename, filename) != 0) {
#endif
        remove(temporaryFilename);
        status = exec_ir_writer_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u);
        goto done;
    }
    status = exec_ir_writer_fail(diagnostic, ZR_ARTIFACT_STATUS_OK, 0u);
done:
    free(temporaryFilename);
    free(outerBytes);
    free(nestedBytes);
    return status;
}
