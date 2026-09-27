#include "zr_vm_core/module.h"
#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_common/zr_aot_abi.h"

#include <string.h>

static EZrArtifactStatus exec_ir_artifact_fail(
        SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status,
        TZrUInt32 sectionKind, TZrUInt32 byteOffset) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = sectionKind;
        diagnostic->byteOffset = byteOffset;
    }
    return status;
}

static EZrArtifactStatus exec_ir_artifact_version_fail(
        SZrArtifactDiagnostic *diagnostic, TZrUInt32 actual,
        TZrUInt32 byteOffset) {
    EZrArtifactStatus status = exec_ir_artifact_fail(
            diagnostic, ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION,
            ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE, byteOffset);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedVersion = ZR_VM_AOT_ABI_VERSION;
        diagnostic->actualVersion = actual;
    }
    return status;
}

static EZrArtifactStatus exec_ir_artifact_hash_fail(
        SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status,
        TZrUInt64 expected, TZrUInt64 actual) {
    exec_ir_artifact_fail(diagnostic, status,
                          ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE, 0u);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return status;
}

static EZrArtifactStatus exec_ir_artifact_validate_contract(
        const SZrArtifactView *outer, const SZrExecIrModule *graph,
        const SZrArtifactSectionView *contractSection,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactContractRow row;
    const SZrExecIrFunction *function = &graph->functions[0];
    EZrArtifactStatus status;
    if (contractSection->elementCount != 1u)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                contractSection->byteOffset);
    status = ZrCore_Artifact_ReadContractRow(contractSection, 0u,
                                             &row, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (row.memberToken != function->functionToken ||
        row.signatureToken != outer->identity.signatureToken ||
        row.contractHash != outer->identity.callableContractHash ||
        row.parameterCount != 0u || row.flags != 0u ||
        row.receiverEffect != ZR_ARTIFACT_RECEIVER_NONE ||
        row.refExportEffect != ZR_ARTIFACT_REF_EXPORT_NONE ||
        row.escapeFlags != 0u ||
        row.abiLoweringKind != ZR_ARTIFACT_ABI_LOWERING_NONE)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                contractSection->byteOffset);
    if (graph->moduleHash != outer->identity.moduleHash ||
        graph->contract.moduleHash != outer->identity.moduleHash ||
        function->contract.moduleHash != outer->identity.moduleHash)
        return exec_ir_artifact_hash_fail(diagnostic,
                ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH,
                outer->identity.moduleHash, graph->moduleHash);
    if (graph->contract.layoutHash != outer->identity.layoutHash ||
        function->contract.layoutHash != outer->identity.layoutHash)
        return exec_ir_artifact_hash_fail(diagnostic,
                ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
                outer->identity.layoutHash, function->contract.layoutHash);
    if (graph->contract.signatureHash != outer->identity.signatureHash ||
        function->signatureHash != outer->identity.signatureHash ||
        function->contract.signatureHash != outer->identity.signatureHash)
        return exec_ir_artifact_hash_fail(diagnostic,
                ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
                outer->identity.signatureHash, function->signatureHash);
    return ZR_ARTIFACT_STATUS_OK;
}

/* A dedicated ZRAF v6 entry. Legacy 01ZR ImportByPath remains a separate
 * loader path. The caller supplies the expected public identity and an empty
 * module; no graph is published until every layer and contract has verified. */
EZrArtifactStatus ZrCore_Module_OpenExecIrArtifact(
        const TZrByte *buffer, TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrExecIrModule *outModule, SZrArtifactDiagnostic *diagnostic) {
    static const EZrArtifactSectionKind requiredSections[] = {
        ZR_ARTIFACT_SECTION_STRING_HEAP,
        ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
        ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
        ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
        ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
        ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
        ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
        ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE
    };
    SZrArtifactView outer;
    SZrArtifactSectionView section;
    SZrArtifactSectionView bundle;
    SZrArtifactSectionView contract;
    SZrArtifactExecIrView nested;
    SZrArtifactExecIrDiagnostic nestedDiagnostic;
    SZrExecIrModule temporary;
    EZrArtifactStatus status;
    EZrArtifactExecIrStatus nestedStatus;
    if (buffer == ZR_NULL || expectedIdentity == ZR_NULL ||
        outModule == ZR_NULL || outModule->functionCount != 0u ||
        outModule->functions != ZR_NULL || outModule->constantCount != 0u ||
        outModule->constants != ZR_NULL || outModule->layoutCount != 0u ||
        outModule->layouts != ZR_NULL || outModule->sourceMapCount != 0u ||
        outModule->sourceMaps != ZR_NULL)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    status = ZrCore_Artifact_Read(buffer, bufferLength, &outer, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (outer.kind != ZR_ARTIFACT_KIND_ZRO)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_KIND, 0u, 8u);
    if (outer.bufferLength != bufferLength ||
        outer.flags != 0u)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u);
    status = ZrCore_Artifact_ValidatePublicIdentity(&outer, expectedIdentity,
                                                     diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(&bundle, 0, sizeof(bundle));
    memset(&contract, 0, sizeof(contract));
    for (TZrUInt32 index = 0u;
         index < sizeof(requiredSections) / sizeof(requiredSections[0]); ++index) {
        status = ZrCore_Artifact_FindSection(&outer, requiredSections[index],
                                             &section, diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        if (section.flags != ZR_ARTIFACT_SECTION_FLAG_MANDATORY)
            return exec_ir_artifact_fail(diagnostic,
                    ZR_ARTIFACT_STATUS_INVALID_SECTION,
                    section.kind, section.byteOffset);
        if (requiredSections[index] == ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE)
            bundle = section;
        if (requiredSections[index] == ZR_ARTIFACT_SECTION_CONTRACT_TABLE)
            contract = section;
    }
    if (outer.sectionCount !=
        sizeof(requiredSections) / sizeof(requiredSections[0]))
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u);
    if (bundle.elementSize != 1u || bundle.elementCount != bundle.byteLength ||
        bundle.byteLength == 0u)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                bundle.byteOffset);
    nestedStatus = ZrCore_ArtifactExecIr_Read(bundle.data, bundle.byteLength,
                                               &nested, &nestedDiagnostic);
    if (nestedStatus != ZR_ARTIFACT_EXEC_IR_OK)
        return exec_ir_artifact_fail(diagnostic,
                nestedStatus == ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION
                        ? ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION
                        : ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                bundle.byteOffset + nestedDiagnostic.byteOffset);
    if (nested.abiVersion != ZR_VM_AOT_ABI_VERSION)
        return exec_ir_artifact_version_fail(diagnostic, nested.abiVersion,
                                             bundle.byteOffset + 8u);
    if (nested.moduleHash != outer.identity.moduleHash)
        return exec_ir_artifact_hash_fail(diagnostic,
                ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH,
                outer.identity.moduleHash, nested.moduleHash);
    if (nested.flags != 0u || nested.execIrHash == 0u ||
        nested.execBcHash != 0u || nested.sectionCount != 1u ||
        nested.sections[0].kind != ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR ||
        nested.sections[0].flags != 0u || nested.sections[0].elementSize != 1u ||
        nested.sections[0].elementCount != nested.sections[0].byteLength ||
        nested.sections[0].byteLength !=
                ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE)
        return exec_ir_artifact_fail(diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                bundle.byteOffset);
    ZrCore_ExecIr_ModuleInit(&temporary);
    nestedStatus = ZrCore_ArtifactExecIrScalar_Read(
            nested.sections[0].data, nested.sections[0].byteLength,
            &temporary, &nestedDiagnostic);
    if (nestedStatus != ZR_ARTIFACT_EXEC_IR_OK) {
        ZrCore_ExecIr_FreeModule(&temporary);
        return exec_ir_artifact_fail(diagnostic,
                nestedStatus == ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION
                        ? ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION
                        : ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                bundle.byteOffset + nested.sections[0].byteOffset +
                nestedDiagnostic.byteOffset);
    }
    status = exec_ir_artifact_validate_contract(&outer, &temporary,
                                                &contract, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        ZrCore_ExecIr_FreeModule(&temporary);
        return status;
    }
    *outModule = temporary;
    return exec_ir_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_OK, 0u, 0u);
}
