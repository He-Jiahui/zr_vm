#include "semantic/lsp_scheduler_contract.h"

#include <string.h>

#include "zr_vm_core/canonical_consumer.h"

TZrBool ZrLanguageServer_LspSchedulerContract_ResolveArtifact(
        const SZrFunctionSchedulerSourceFact *sourceFact,
        const TZrByte *artifactBytes,
        TZrSize artifactLength,
        SZrLspSchedulerContract *outContract,
        SZrArtifactDiagnostic *outDiagnostic) {
    SZrArtifactView artifact;
    SZrArtifactPublicIdentity expectedIdentity;
    SZrCanonicalConsumerProjection projection;
    SZrCanonicalTypeProjection schedulerType;
    SZrCanonicalTypeProjection schedulerReference;
    SZrCanonicalSchedulerContractExpectation expectation;
    SZrArtifactSchedulerContractRow schedulerContract;

    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (outDiagnostic != ZR_NULL) {
        memset(outDiagnostic, 0, sizeof(*outDiagnostic));
    }
    memset(&schedulerType, 0, sizeof(schedulerType));
    memset(&schedulerReference, 0, sizeof(schedulerReference));
    memset(&expectation, 0, sizeof(expectation));
    if (sourceFact == ZR_NULL || artifactBytes == ZR_NULL || artifactLength == 0u ||
        outContract == ZR_NULL ||
        sourceFact->contractRole != ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE ||
        sourceFact->schedulerTypeId == 0u ||
        sourceFact->schedulerProvider.metadataToken == 0u ||
        sourceFact->schedulerProvider.layoutVersion == 0u ||
        sourceFact->schedulerProvider.layoutHash == 0u ||
        sourceFact->schedulerProvider.moduleSignatureHash == 0u ||
        sourceFact->scheduleSignatureToken == 0u || sourceFact->scheduleSignatureHash == 0u ||
        sourceFact->schedulerAbiVersion == 0u || sourceFact->schedulerPolicyMask == 0u ||
        sourceFact->transportContractHash == 0u ||
        sourceFact->schedulerContractHash == 0u) {
        return ZR_FALSE;
    }
    if (ZrCore_Artifact_Read(artifactBytes, artifactLength, &artifact, outDiagnostic) !=
        ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }
    expectedIdentity = artifact.identity;
    expectedIdentity.canonicalTypeId = sourceFact->schedulerTypeId;
    expectedIdentity.signatureToken = sourceFact->scheduleSignatureToken;
    expectedIdentity.layoutVersion = sourceFact->schedulerProvider.layoutVersion;
    expectedIdentity.layoutHash = sourceFact->schedulerProvider.layoutHash;
    expectedIdentity.callableContractHash = sourceFact->schedulerContractHash;
    expectedIdentity.moduleHash = sourceFact->schedulerProvider.moduleSignatureHash;
    if (
        ZrCore_CanonicalConsumer_Open(
                artifactBytes, artifactLength, &expectedIdentity, &projection, outDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        ZrCore_CanonicalConsumer_ResolveTypeId(
                &projection,
                sourceFact->schedulerTypeId,
                &schedulerType,
                outDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        schedulerType.canonicalTypeId != sourceFact->schedulerTypeId ||
        artifact.identity.typeRefToken == 0u ||
        ZrCore_CanonicalConsumer_ResolveTypeToken(
                &projection,
                artifact.identity.typeRefToken,
                &schedulerReference,
                outDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        schedulerReference.canonicalTypeId != sourceFact->schedulerTypeId ||
        schedulerReference.signatureToken != sourceFact->scheduleSignatureToken ||
        ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                &projection,
                schedulerReference.typeToken,
                &schedulerContract,
                outDiagnostic) != ZR_ARTIFACT_STATUS_OK) {
        return ZR_FALSE;
    }

    expectation.schedulerTypeToken = schedulerReference.typeToken;
    expectation.taskTypeToken = schedulerContract.taskTypeToken;
    expectation.jobTypeToken = schedulerContract.jobTypeToken;
    expectation.abiVersion = sourceFact->schedulerAbiVersion;
    expectation.policy = sourceFact->schedulerPolicyMask;
    expectation.requirementFlags =
            sourceFact->schedulerPolicyMask == ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN
                    ? sourceFact->attachedRequirementFlags
                    : sourceFact->isolatedRequirementFlags;
    expectation.transportContractHash = sourceFact->transportContractHash;
    expectation.schedulerContractHash = sourceFact->schedulerContractHash;
    if (ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                &projection, &expectation, outDiagnostic) != ZR_ARTIFACT_STATUS_OK ||
        schedulerContract.schedulerTypeToken != schedulerReference.typeToken ||
        schedulerContract.abiVersion != sourceFact->schedulerAbiVersion ||
        schedulerContract.policyMask != sourceFact->schedulerPolicyMask ||
        schedulerContract.attachedRequirementFlags != sourceFact->attachedRequirementFlags ||
        schedulerContract.isolatedRequirementFlags != sourceFact->isolatedRequirementFlags ||
        schedulerContract.transportContractHash != sourceFact->transportContractHash ||
        schedulerContract.schedulerContractHash != sourceFact->schedulerContractHash) {
        return ZR_FALSE;
    }

    outContract->receiverTypeId = sourceFact->schedulerTypeId;
    outContract->schedulerTypeToken = schedulerReference.typeToken;
    outContract->taskTypeToken = schedulerContract.taskTypeToken;
    outContract->jobTypeToken = schedulerContract.jobTypeToken;
    outContract->scheduleSignatureToken = sourceFact->scheduleSignatureToken;
    outContract->abiVersion = schedulerContract.abiVersion;
    outContract->policyMask = schedulerContract.policyMask;
    outContract->attachedRequirementFlags = schedulerContract.attachedRequirementFlags;
    outContract->isolatedRequirementFlags = schedulerContract.isolatedRequirementFlags;
    outContract->ownerLayoutVersion = artifact.identity.layoutVersion;
    outContract->ownerLayoutHash = artifact.identity.layoutHash;
    outContract->ownerModuleHash = artifact.identity.moduleHash;
    outContract->transportContractHash = schedulerContract.transportContractHash;
    outContract->schedulerContractHash = schedulerContract.schedulerContractHash;
    return ZR_TRUE;
}
