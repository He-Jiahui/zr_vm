#ifndef ZR_VM_CORE_CANONICAL_CONSUMER_H
#define ZR_VM_CORE_CANONICAL_CONSUMER_H

#include "zr_vm_core/artifact_schema.h"

typedef struct SZrCanonicalTypeProjection {
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken typeToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 capabilityFlags;
    const TZrByte *signatureData;
    TZrUInt32 signatureLength;
    TZrUInt64 signatureHash;
    TZrBool hasLayout;
    SZrArtifactLayoutRow layout;
    TZrBool hasContract;
    SZrArtifactContractRow contract;
    TZrBool hasDomainTransfer;
    SZrArtifactDomainTransferRow domainTransfer;
} SZrCanonicalTypeProjection;

typedef struct SZrCanonicalConsumerProjection {
    SZrArtifactView artifact;
    SZrArtifactSectionView typeDefs;
    SZrArtifactSectionView typeRefs;
    SZrArtifactSectionView typeSpecs;
    SZrArtifactSectionView signatures;
    SZrArtifactSectionView contracts;
    SZrArtifactSectionView layouts;
    SZrArtifactSectionView domainTransfers;
    SZrArtifactSectionView schedulerContracts;
    SZrArtifactSectionView callBindings;
    SZrCanonicalTypeProjection rootType;
} SZrCanonicalConsumerProjection;

typedef struct SZrCanonicalSchedulerContractExpectation {
    TZrMetadataToken schedulerTypeToken;
    TZrMetadataToken taskTypeToken;
    TZrMetadataToken jobTypeToken;
    TZrUInt32 abiVersion;
    TZrUInt32 policy;
    TZrUInt32 requirementFlags;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
} SZrCanonicalSchedulerContractExpectation;

typedef struct SZrCanonicalPublicRefLikeAbiExpectation {
    TZrMetadataToken typeToken;
    TZrMetadataToken callableSignatureToken;
    TZrUInt64 typeRefHash;
    TZrUInt32 typeFlags;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    TZrUInt32 callableEscapeFlags;
    EZrArtifactAbiLoweringKind abiLoweringKind;
} SZrCanonicalPublicRefLikeAbiExpectation;

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_Open(
        const TZrByte *buffer,
        TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrCanonicalConsumerProjection *outProjection,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveTypeToken(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveTypeId(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 canonicalTypeId,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveLayout(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactLayoutRow *outLayout,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveDomainTransfer(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactDomainTransferRow *outContract,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveSchedulerContract(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken schedulerTypeToken,
        SZrArtifactSchedulerContractRow *outContract,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ValidateSchedulerContract(
        const SZrCanonicalConsumerProjection *projection,
        const SZrCanonicalSchedulerContractExpectation *expected,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
        const SZrCanonicalConsumerProjection *projection,
        const SZrCanonicalPublicRefLikeAbiExpectation *expected,
        SZrArtifactDiagnostic *diagnostic);

#endif /* ZR_VM_CORE_CANONICAL_CONSUMER_H */
