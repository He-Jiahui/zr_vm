#include "zr_vm_parser/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/metadata_token.h"

typedef struct SZrWriterSchedulerArtifactType {
    const SZrFunctionArtifactSourceTypeIdentity *provider;
    TZrMetadataToken serializedToken;
    TZrUInt32 canonicalTypeId;
} SZrWriterSchedulerArtifactType;

static EZrArtifactStatus writer_scheduler_artifact_fail(
        SZrArtifactDiagnostic *diagnostic,
        EZrArtifactStatus status) {
    if (diagnostic != ZR_NULL) {
        ZrCore_Memory_RawSet(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
    }
    return status;
}

static TZrBool writer_scheduler_artifact_provider_is_complete(
        const SZrFunctionArtifactSourceTypeIdentity *provider) {
    return provider != ZR_NULL && provider->metadataToken != 0u &&
           provider->signatureToken != 0u && provider->signatureHash != 0u &&
           provider->layoutVersion != 0u && provider->layoutHash != 0u &&
           provider->moduleSignatureHash != 0u &&
           ZR_METADATA_TOKEN_TABLE(provider->metadataToken) == ZR_METADATA_TABLE_TYPE_DEF &&
           ZR_METADATA_TOKEN_TABLE(provider->signatureToken) == ZR_METADATA_TABLE_SIGNATURE;
}

static void writer_scheduler_artifact_write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static void writer_scheduler_artifact_write_type_def_signature(
        TZrByte *bytes,
        TZrMetadataToken typeDefToken) {
    bytes[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF;
    writer_scheduler_artifact_write_u32(bytes + 1u, typeDefToken);
}

static void writer_scheduler_artifact_sort_types(
        SZrWriterSchedulerArtifactType *types,
        TZrUInt32 typeCount) {
    TZrUInt32 outer;

    for (outer = 0u; outer < typeCount; ++outer) {
        TZrUInt32 inner;
        for (inner = outer + 1u; inner < typeCount; ++inner) {
            if (types[inner].serializedToken < types[outer].serializedToken) {
                SZrWriterSchedulerArtifactType temporary = types[outer];
                types[outer] = types[inner];
                types[inner] = temporary;
            }
        }
    }
}

static TZrBool writer_scheduler_artifact_types_are_distinct(
        const SZrWriterSchedulerArtifactType *types,
        TZrUInt32 typeCount) {
    TZrUInt32 index;

    for (index = 1u; index < typeCount; ++index) {
        if (types[index - 1u].serializedToken == types[index].serializedToken) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

ZR_PARSER_API EZrArtifactStatus ZrParser_Writer_WriteSchedulerArtifactFile(
        SZrState *state,
        const SZrFunction *function,
        const TZrChar *filename,
        EZrArtifactKind kind,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrFunctionSchedulerSourceFact *fact;
    SZrWriterSchedulerArtifactType types[3];
    SZrArtifactTypeDefRow typeDefs[3];
    SZrArtifactTypeIdentityRow typeRefs[3];
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactDomainTransferRow transfer;
    SZrArtifactSchedulerContractRow schedulerContract;
    SZrArtifactSectionInput sections[8];
    SZrArtifactDocument document;
    TZrByte signatureHeap[20];
    TZrByte *encoded = ZR_NULL;
    TZrSize encodedSize = 0u;
    TZrSize writtenSize = 0u;
    FILE *file = ZR_NULL;
    TZrBool writeFailed = ZR_FALSE;
    EZrArtifactStatus status;
    TZrUInt32 index;
    TZrUInt32 schedulerSignatureOffset = 0u;
    TZrUInt64 schedulerSignatureHash;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL ||
        filename == ZR_NULL || (kind != ZR_ARTIFACT_KIND_ZRI && kind != ZR_ARTIFACT_KIND_ZRO)) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }
    fact = ZrCore_Function_FindSchedulerSourceFact(function, 0u);
    if (fact == ZR_NULL || fact->schedulerTypeId == 0u || fact->taskTypeId == 0u ||
        fact->jobTypeId == 0u || fact->scheduleMemberToken == 0u ||
        fact->scheduleSignatureToken == 0u || fact->scheduleSignatureHash == 0u ||
        fact->schedulerAbiVersion == 0u || fact->schedulerPolicyMask == 0u ||
        fact->transportContractHash == 0u || fact->schedulerContractHash == 0u ||
        !writer_scheduler_artifact_provider_is_complete(&fact->schedulerProvider) ||
        !writer_scheduler_artifact_provider_is_complete(&fact->taskProvider) ||
        !writer_scheduler_artifact_provider_is_complete(&fact->jobProvider)) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }

    ZrCore_Memory_RawSet(types, 0, sizeof(types));
    types[0].provider = &fact->schedulerProvider;
    types[0].canonicalTypeId = fact->schedulerTypeId;
    types[1].provider = &fact->taskProvider;
    types[1].canonicalTypeId = fact->taskTypeId;
    types[2].provider = &fact->jobProvider;
    types[2].canonicalTypeId = fact->jobTypeId;
    for (index = 0u; index < 3u; ++index) {
        types[index].serializedToken = ZR_METADATA_TOKEN_MAKE(
                ZR_METADATA_TABLE_TYPE_REF, index + 1u);
    }
    writer_scheduler_artifact_sort_types(types, 3u);
    if (!writer_scheduler_artifact_types_are_distinct(types, 3u)) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }

    ZrCore_Memory_RawSet(signatureHeap, 0, sizeof(signatureHeap));
    ZrCore_Memory_RawSet(typeDefs, 0, sizeof(typeDefs));
    ZrCore_Memory_RawSet(typeRefs, 0, sizeof(typeRefs));
    for (index = 0u; index < 3u; ++index) {
        TZrUInt32 offset = index * 5u;
        TZrUInt64 signatureHash;

        writer_scheduler_artifact_write_type_def_signature(
                signatureHeap + offset, types[index].provider->metadataToken);
        signatureHash = ZrCore_Artifact_HashBytes(signatureHeap + offset, 5u);
        typeDefs[index].token = types[index].provider->metadataToken;
        typeDefs[index].canonicalTypeId = types[index].canonicalTypeId;
        typeDefs[index].typeSignatureHash = signatureHash;
        typeRefs[index].token = types[index].serializedToken;
        typeRefs[index].signatureToken = types[index].provider == &fact->schedulerProvider
                                                 ? fact->scheduleSignatureToken
                                                 : types[index].provider->signatureToken;
        typeRefs[index].canonicalTypeId = types[index].canonicalTypeId;
        typeRefs[index].signatureOffset = offset;
        typeRefs[index].signatureLength = 5u;
        typeRefs[index].signatureHash = signatureHash;
        typeRefs[index].layoutVersion = types[index].provider->layoutVersion;
        typeRefs[index].layoutHash = types[index].provider->layoutHash;
        if (types[index].provider == &fact->schedulerProvider) {
            schedulerSignatureOffset = offset;
            schedulerSignatureHash = signatureHash;
        }
    }
    writer_scheduler_artifact_write_type_def_signature(
            signatureHeap + 15u, fact->schedulerProvider.metadataToken);
    ZrCore_Memory_RawSet(&typeSpec, 0, sizeof(typeSpec));
    typeSpec.token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    typeSpec.signatureToken = fact->scheduleSignatureToken;
    typeSpec.canonicalTypeId = fact->schedulerTypeId;
    typeSpec.signatureOffset = 15u;
    typeSpec.signatureLength = 5u;
    typeSpec.signatureHash = ZrCore_Artifact_HashBytes(signatureHeap + 15u, 5u);
    typeSpec.layoutVersion = fact->schedulerProvider.layoutVersion;
    typeSpec.layoutHash = fact->schedulerProvider.layoutHash;

    ZrCore_Memory_RawSet(&contract, 0, sizeof(contract));
    contract.memberToken = fact->scheduleMemberToken;
    contract.signatureToken = fact->scheduleSignatureToken;
    contract.parameterCount = 1u;
    contract.receiverEffect = ZR_ARTIFACT_RECEIVER_MUTABLE;
    contract.abiLoweringKind = ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT;
    contract.contractHash = fact->schedulerContractHash;

    ZrCore_Memory_RawSet(&layout, 0, sizeof(layout));
    layout.typeToken = types[0].provider == &fact->schedulerProvider
                               ? types[0].serializedToken
                               : 0u;
    for (index = 0u; index < 3u; ++index) {
        if (types[index].provider == &fact->schedulerProvider) {
            layout.typeToken = types[index].serializedToken;
            break;
        }
    }
    layout.version = fact->schedulerProvider.layoutVersion;
    layout.byteAlignment = 1u;
    layout.layoutHash = fact->schedulerProvider.layoutHash;

    ZrCore_Memory_RawSet(&transfer, 0, sizeof(transfer));
    for (index = 0u; index < 3u; ++index) {
        if (types[index].provider == &fact->jobProvider) {
            transfer.typeToken = types[index].serializedToken;
            break;
        }
    }
    transfer.kind = ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE;
    transfer.schemaVersion = 1u;
    transfer.flags = ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    transfer.providerToken = fact->scheduleMemberToken;
    transfer.schemaHash = fact->jobProvider.signatureHash;
    transfer.providerContractHash = fact->schedulerContractHash;

    ZrCore_Memory_RawSet(&schedulerContract, 0, sizeof(schedulerContract));
    for (index = 0u; index < 3u; ++index) {
        if (types[index].provider == &fact->schedulerProvider) {
            schedulerContract.schedulerTypeToken = types[index].serializedToken;
        } else if (types[index].provider == &fact->taskProvider) {
            schedulerContract.taskTypeToken = types[index].serializedToken;
        } else if (types[index].provider == &fact->jobProvider) {
            schedulerContract.jobTypeToken = types[index].serializedToken;
        }
    }
    schedulerContract.abiVersion = fact->schedulerAbiVersion;
    schedulerContract.policyMask = fact->schedulerPolicyMask;
    schedulerContract.attachedRequirementFlags = fact->attachedRequirementFlags;
    schedulerContract.isolatedRequirementFlags = fact->isolatedRequirementFlags;
    schedulerContract.transportContractHash = fact->transportContractHash;
    schedulerContract.schedulerContractHash = fact->schedulerContractHash;

    ZrCore_Memory_RawSet(&document, 0, sizeof(document));
    ZrCore_Memory_RawSet(sections, 0, sizeof(sections));
    document.kind = kind;
    document.identity.canonicalTypeId = fact->schedulerTypeId;
    document.identity.typeRefToken = layout.typeToken;
    document.identity.typeSpecToken = typeSpec.token;
    document.identity.signatureToken = fact->scheduleSignatureToken;
    document.identity.typeRefHash = schedulerSignatureHash;
    document.identity.typeSpecHash = typeSpec.signatureHash;
    document.identity.signatureHash = schedulerSignatureHash;
    document.identity.layoutVersion = fact->schedulerProvider.layoutVersion;
    document.identity.layoutHash = fact->schedulerProvider.layoutHash;
    document.identity.callableContractHash = fact->schedulerContractHash;
    document.identity.moduleHash = fact->schedulerProvider.moduleSignatureHash;
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 3u, typeDefs};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 3u, typeRefs};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &typeSpec};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(signatureHeap), signatureHeap};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &contract};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &layout};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &transfer};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &schedulerContract};
    document.sections = sections;

    status = ZrCore_Artifact_GetEncodedSize(&document, &encodedSize, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || encodedSize == 0u) {
        return status;
    }
    encoded = (TZrByte *)malloc(encodedSize);
    if (encoded == ZR_NULL) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL);
    }
    status = ZrCore_Artifact_Write(&document, encoded, encodedSize, &writtenSize, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || writtenSize != encodedSize) {
        free(encoded);
        return status != ZR_ARTIFACT_STATUS_OK
                       ? status
                       : writer_scheduler_artifact_fail(
                               diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    }
    file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        free(encoded);
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }
    if (fwrite(encoded, 1u, writtenSize, file) != writtenSize) {
        writeFailed = ZR_TRUE;
    }
    if (fclose(file) != 0) {
        writeFailed = ZR_TRUE;
    }
    if (writeFailed) {
        remove(filename);
        free(encoded);
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    }
    free(encoded);
    (void)schedulerSignatureOffset;
    return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_OK);
}
