#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SCHEDULER_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SCHEDULER_CONTRACT_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"

typedef struct SZrLspSchedulerContract {
    TZrUInt32 receiverTypeId;
    TZrMetadataToken schedulerTypeToken;
    TZrMetadataToken taskTypeToken;
    TZrMetadataToken jobTypeToken;
    TZrMetadataToken scheduleSignatureToken;
    TZrUInt32 abiVersion;
    TZrUInt32 policyMask;
    TZrUInt32 attachedRequirementFlags;
    TZrUInt32 isolatedRequirementFlags;
    TZrUInt32 ownerLayoutVersion;
    TZrUInt64 ownerLayoutHash;
    TZrUInt64 ownerModuleHash;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
} SZrLspSchedulerContract;

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSchedulerContract_ResolveArtifact(
        const SZrFunctionSchedulerSourceFact *sourceFact,
        const TZrByte *artifactBytes,
        TZrSize artifactLength,
        SZrLspSchedulerContract *outContract,
        SZrArtifactDiagnostic *outDiagnostic);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_SCHEDULER_CONTRACT_H */
