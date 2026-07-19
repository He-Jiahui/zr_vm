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
} SZrCanonicalTypeProjection;

typedef struct SZrCanonicalConsumerProjection {
    SZrArtifactView artifact;
    SZrArtifactSectionView typeDefs;
    SZrArtifactSectionView typeRefs;
    SZrArtifactSectionView typeSpecs;
    SZrArtifactSectionView signatures;
    SZrArtifactSectionView contracts;
    SZrArtifactSectionView layouts;
    SZrCanonicalTypeProjection rootType;
} SZrCanonicalConsumerProjection;

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

#endif /* ZR_VM_CORE_CANONICAL_CONSUMER_H */
