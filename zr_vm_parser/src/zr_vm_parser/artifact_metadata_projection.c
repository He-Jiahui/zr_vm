#include "zr_vm_parser/artifact_projection.h"

#include <string.h>

#include "zr_vm_core/reflection.h"
#include "zr_vm_library/native_binding.h"

static EZrArtifactStatus artifact_metadata_projection_fail(
        SZrArtifactDiagnostic *diagnostic,
        EZrArtifactStatus status,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE;
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return status;
}

static TZrBool artifact_metadata_native_category_matches(
        EZrReflectionTypeCategory category,
        EZrObjectPrototypeType prototypeType) {
    if (category == ZR_REFLECTION_TYPE_CATEGORY_ERASED) return ZR_TRUE;
    switch (prototypeType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_CLASS ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS);
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_INTERFACE);
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_STRUCT ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT);
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_ENUM);
        case ZR_OBJECT_PROTOTYPE_TYPE_NATIVE:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS);
        default:
            return ZR_FALSE;
    }
}

static TZrBool artifact_metadata_native_shape_is_valid(
        const ZrLibTypeDescriptor *descriptor) {
    return (TZrBool)((descriptor->fieldCount == 0u || descriptor->fields != ZR_NULL) &&
                     (descriptor->methodCount == 0u || descriptor->methods != ZR_NULL) &&
                     (descriptor->metaMethodCount == 0u ||
                      descriptor->metaMethods != ZR_NULL) &&
                     (descriptor->enumMemberCount == 0u ||
                      descriptor->enumMembers != ZR_NULL));
}

static TZrSize artifact_metadata_native_property_count(
        const ZrLibTypeDescriptor *descriptor) {
    TZrSize propertyCount = 0u;
    TZrSize index;

    for (index = 0u; index < descriptor->methodCount; ++index) {
        const TZrChar *propertyName = descriptor->methods[index].propertyName;
        TZrSize previousIndex;
        TZrBool seen = ZR_FALSE;

        if (propertyName == ZR_NULL || propertyName[0] == '\0') continue;
        for (previousIndex = 0u; previousIndex < index; ++previousIndex) {
            const TZrChar *previousName =
                    descriptor->methods[previousIndex].propertyName;
            if (previousName != ZR_NULL && strcmp(previousName, propertyName) == 0) {
                seen = ZR_TRUE;
                break;
            }
        }
        if (!seen) ++propertyCount;
    }
    return propertyCount;
}

static EZrArtifactStatus artifact_metadata_native_counts(
        const ZrLibTypeDescriptor *descriptor,
        TZrSize *outMemberCount,
        TZrSize *outPropertyCount,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrSize maxCount = (TZrSize)((TZrUInt32)~0u);
    const TZrSize counts[] = {
            descriptor->fieldCount,
            descriptor->methodCount,
            descriptor->metaMethodCount,
            descriptor->enumMemberCount};
    TZrSize memberCount = 0u;
    TZrSize index;

    if (!artifact_metadata_native_shape_is_valid(descriptor)) {
        return artifact_metadata_projection_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    for (index = 0u; index < sizeof(counts) / sizeof(counts[0]); ++index) {
        if (counts[index] > maxCount || memberCount > maxCount - counts[index]) {
            return artifact_metadata_projection_fail(
                    diagnostic, ZR_ARTIFACT_STATUS_COUNT_LIMIT, maxCount, counts[index]);
        }
        memberCount += counts[index];
    }
    *outMemberCount = memberCount;
    *outPropertyCount = artifact_metadata_native_property_count(descriptor);
    return ZR_ARTIFACT_STATUS_OK;
}

EZrArtifactStatus ZrParser_ArtifactMetadata_BuildState(
        const SZrReflectionTypeIdentity *identity,
        const ZrLibTypeDescriptor *nativeTypeDescriptor,
        EZrArtifactMetadataPreservationState preservationState,
        TZrUInt32 retainedMemberCount,
        TZrUInt32 retainedPropertyCount,
        TZrUInt32 retainedMetaRecordCount,
        TZrUInt64 layoutHash,
        TZrUInt64 callableContractHash,
        SZrArtifactMetadataStateRow *outState,
        SZrArtifactDiagnostic *diagnostic) {
    TZrSize nativeMemberCount = 0u;
    TZrSize nativePropertyCount = 0u;
    EZrArtifactStatus status;

    if (outState != ZR_NULL) memset(outState, 0, sizeof(*outState));
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (identity == ZR_NULL || outState == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(identity->typeToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        ZR_METADATA_TOKEN_RID(identity->typeToken) == 0u ||
        identity->signatureHash == 0u || identity->metadataGeneration == 0u ||
        identity->category < ZR_REFLECTION_TYPE_CATEGORY_ERASED ||
        identity->category > ZR_REFLECTION_TYPE_CATEGORY_ENUM ||
        preservationState < ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY ||
        preservationState > ZR_ARTIFACT_METADATA_PRESERVATION_FULL ||
        layoutHash == 0u || callableContractHash == 0u) {
        return artifact_metadata_projection_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    if ((preservationState == ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
         (retainedMemberCount != 0u || retainedPropertyCount != 0u ||
          retainedMetaRecordCount != 0u)) ||
        (preservationState == ZR_ARTIFACT_METADATA_PRESERVATION_MEMBERS &&
         retainedMetaRecordCount != 0u) ||
        (preservationState != ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
         identity->category == ZR_REFLECTION_TYPE_CATEGORY_ERASED)) {
        return artifact_metadata_projection_fail(
                diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN, 0u, preservationState);
    }
    if (nativeTypeDescriptor != ZR_NULL) {
        if (!artifact_metadata_native_category_matches(
                    identity->category, nativeTypeDescriptor->prototypeType)) {
            return artifact_metadata_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    identity->category,
                    nativeTypeDescriptor->prototypeType);
        }
        status = artifact_metadata_native_counts(
                nativeTypeDescriptor,
                &nativeMemberCount,
                &nativePropertyCount,
                diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        if (preservationState != ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
            (nativeMemberCount != retainedMemberCount ||
             nativePropertyCount != retainedPropertyCount)) {
            return artifact_metadata_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    nativeMemberCount,
                    retainedMemberCount);
        }
    }

    outState->typeToken = identity->typeToken;
    outState->preservationState = preservationState;
    outState->category = (EZrArtifactReflectionCategory)identity->category;
    outState->metadataGeneration = identity->metadataGeneration;
    outState->retainedMemberCount = retainedMemberCount;
    outState->retainedPropertyCount = retainedPropertyCount;
    outState->retainedMetaRecordCount = retainedMetaRecordCount;
    outState->typeSignatureHash = identity->signatureHash;
    outState->layoutHash = layoutHash;
    outState->callableContractHash = callableContractHash;
    outState->metadataHash = ZrCore_Artifact_ComputeMetadataStateHash(outState);
    return ZR_ARTIFACT_STATUS_OK;
}
