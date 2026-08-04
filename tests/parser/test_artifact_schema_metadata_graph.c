#include "unity.h"

#include <string.h>

#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/artifact_projection.h"

#define GRAPH_TYPE_ID ((TZrUInt32)0x2042u)
#define GRAPH_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define GRAPH_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define GRAPH_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define GRAPH_MEMBER_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define GRAPH_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define GRAPH_TYPE_HASH ((TZrUInt64)0x1011222233334444ULL)
#define GRAPH_LAYOUT_HASH ((TZrUInt64)0x2022333344445555ULL)
#define GRAPH_CONTRACT_HASH ((TZrUInt64)0x3033444455556666ULL)
#define GRAPH_MODULE_HASH ((TZrUInt64)0x5055666677778888ULL)

void test_artifact_metadata_graph_roundtrips_zri_and_zro(void);
void test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs(void);
void test_artifact_metadata_projection_matches_source_native_and_binary(void);

typedef struct SZrArtifactMetadataGraphFixture {
    TZrByte signature[5];
    TZrByte metadataBlob[4];
    TZrByte layoutMap[20];
    TZrByte executablePayload[1];
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactMemberDefRow member;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactMetadataStateRow metadataState;
    SZrArtifactMetadataRecordRow metadataRecord;
    SZrArtifactSectionInput sections[12];
    SZrArtifactDocument document;
} SZrArtifactMetadataGraphFixture;

static void graph_write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static void graph_init_fixture(
        SZrArtifactMetadataGraphFixture *fixture,
        EZrArtifactKind kind) {
    TZrUInt32 sectionCount = 0u;

    memset(fixture, 0, sizeof(*fixture));
    fixture->signature[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF;
    graph_write_u32(fixture->signature + 1u, GRAPH_TYPE_DEF_TOKEN);
    fixture->metadataBlob[0] = 1u;
    fixture->metadataBlob[1] = 2u;
    fixture->metadataBlob[2] = 3u;
    fixture->metadataBlob[3] = 4u;
    graph_write_u32(fixture->layoutMap + 0u, ZR_ARTIFACT_LAYOUT_MAP_VERSION);
    graph_write_u32(fixture->layoutMap + 4u, 1u);
    graph_write_u32(fixture->layoutMap + 8u, 0u);
    graph_write_u32(fixture->layoutMap + 12u, 0u);
    graph_write_u32(fixture->layoutMap + 16u, 8u);

    fixture->typeDef.token = GRAPH_TYPE_DEF_TOKEN;
    fixture->typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_VALUE |
                             ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE;
    fixture->typeDef.canonicalTypeId = GRAPH_TYPE_ID;
    fixture->typeDef.constructorToken = GRAPH_MEMBER_TOKEN;
    fixture->typeDef.constructorSignatureToken = GRAPH_SIGNATURE_TOKEN;
    fixture->typeDef.typeSignatureHash = GRAPH_TYPE_HASH;
    fixture->typeDef.constructorContractHash = GRAPH_CONTRACT_HASH;

    fixture->typeRef.token = GRAPH_TYPE_REF_TOKEN;
    fixture->typeRef.signatureToken = GRAPH_SIGNATURE_TOKEN;
    fixture->typeRef.canonicalTypeId = GRAPH_TYPE_ID;
    fixture->typeRef.signatureLength = sizeof(fixture->signature);
    fixture->typeRef.signatureHash = GRAPH_TYPE_HASH;
    fixture->typeRef.layoutVersion = 1u;
    fixture->typeRef.layoutHash = GRAPH_LAYOUT_HASH;
    fixture->typeSpec = fixture->typeRef;
    fixture->typeSpec.token = GRAPH_TYPE_SPEC_TOKEN;

    fixture->member.token = GRAPH_MEMBER_TOKEN;
    fixture->member.ownerTypeToken = GRAPH_TYPE_DEF_TOKEN;
    fixture->member.signatureToken = GRAPH_SIGNATURE_TOKEN;
    fixture->member.signatureHash = GRAPH_TYPE_HASH;
    fixture->member.contractHash = GRAPH_CONTRACT_HASH;

    fixture->contract.memberToken = GRAPH_MEMBER_TOKEN;
    fixture->contract.signatureToken = GRAPH_SIGNATURE_TOKEN;
    fixture->contract.contractHash = GRAPH_CONTRACT_HASH;

    fixture->layout.typeToken = GRAPH_TYPE_DEF_TOKEN;
    fixture->layout.version = 1u;
    fixture->layout.byteSize = 16u;
    fixture->layout.byteAlignment = 8u;
    fixture->layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_MAPPED;
    fixture->layout.ownershipMapOffset = 0u;
    fixture->layout.ownershipMapLength = sizeof(fixture->layoutMap);
    fixture->layout.layoutHash = GRAPH_LAYOUT_HASH;

    fixture->metadataState.typeToken = GRAPH_TYPE_DEF_TOKEN;
    fixture->metadataState.preservationState = ZR_ARTIFACT_METADATA_PRESERVATION_FULL;
    fixture->metadataState.category = ZR_ARTIFACT_REFLECTION_CATEGORY_STRUCT;
    fixture->metadataState.metadataGeneration = 3u;
    fixture->metadataState.retainedMemberCount = 1u;
    fixture->metadataState.retainedMetaRecordCount = 1u;
    fixture->metadataState.typeSignatureHash = GRAPH_TYPE_HASH;
    fixture->metadataState.layoutHash = GRAPH_LAYOUT_HASH;
    fixture->metadataState.callableContractHash = GRAPH_CONTRACT_HASH;
    fixture->metadataState.metadataHash =
            ZrCore_Artifact_ComputeMetadataStateHash(&fixture->metadataState);

    fixture->metadataRecord.ownerToken = GRAPH_MEMBER_TOKEN;
    fixture->metadataRecord.kind = ZR_ARTIFACT_METADATA_RECORD_ATTRIBUTE_DATA;
    fixture->metadataRecord.retention = ZR_ARTIFACT_METADATA_RETENTION_RUNTIME;
    fixture->metadataRecord.payloadLength = sizeof(fixture->metadataBlob);
    fixture->metadataRecord.metadataGeneration = 3u;
    fixture->metadataRecord.recordHash = ZrCore_Artifact_ComputeMetadataRecordHash(
            &fixture->metadataRecord,
            fixture->metadataBlob,
            sizeof(fixture->metadataBlob));

    fixture->document.kind = kind;
    fixture->document.identity.canonicalTypeId = GRAPH_TYPE_ID;
    fixture->document.identity.typeRefToken = GRAPH_TYPE_REF_TOKEN;
    fixture->document.identity.typeSpecToken = GRAPH_TYPE_SPEC_TOKEN;
    fixture->document.identity.signatureToken = GRAPH_SIGNATURE_TOKEN;
    fixture->document.identity.typeRefHash = GRAPH_TYPE_HASH;
    fixture->document.identity.typeSpecHash = GRAPH_TYPE_HASH;
    fixture->document.identity.signatureHash = GRAPH_TYPE_HASH;
    fixture->document.identity.layoutVersion = 1u;
    fixture->document.identity.layoutHash = GRAPH_LAYOUT_HASH;
    fixture->document.identity.callableContractHash = GRAPH_CONTRACT_HASH;
    fixture->document.identity.moduleHash = GRAPH_MODULE_HASH;

#define GRAPH_ADD_SECTION(KIND, COUNT, DATA) \
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){ \
            (KIND), ZR_ARTIFACT_SECTION_FLAG_MANDATORY, (COUNT), (DATA)}
    GRAPH_ADD_SECTION(ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, 1u, &fixture->typeDef);
    GRAPH_ADD_SECTION(ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, 1u, &fixture->typeRef);
    GRAPH_ADD_SECTION(ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, 1u, &fixture->typeSpec);
    GRAPH_ADD_SECTION(
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
            sizeof(fixture->signature),
            fixture->signature);
    GRAPH_ADD_SECTION(ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE, 1u, &fixture->member);
    GRAPH_ADD_SECTION(ZR_ARTIFACT_SECTION_CONTRACT_TABLE, 1u, &fixture->contract);
    GRAPH_ADD_SECTION(ZR_ARTIFACT_SECTION_LAYOUT_TABLE, 1u, &fixture->layout);
    GRAPH_ADD_SECTION(
            ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
            1u,
            &fixture->metadataState);
    GRAPH_ADD_SECTION(
            ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
            1u,
            &fixture->metadataRecord);
    GRAPH_ADD_SECTION(
            ZR_ARTIFACT_SECTION_METADATA_BLOB_HEAP,
            sizeof(fixture->metadataBlob),
            fixture->metadataBlob);
    GRAPH_ADD_SECTION(
            ZR_ARTIFACT_SECTION_LAYOUT_MAP_HEAP,
            sizeof(fixture->layoutMap),
            fixture->layoutMap);
    GRAPH_ADD_SECTION(
            kind == ZR_ARTIFACT_KIND_ZRI
                    ? ZR_ARTIFACT_SECTION_SEMANTIC_IR
                    : ZR_ARTIFACT_SECTION_CODE_TABLE,
            sizeof(fixture->executablePayload),
            fixture->executablePayload);
#undef GRAPH_ADD_SECTION
    fixture->document.sectionCount = sectionCount;
    fixture->document.sections = fixture->sections;
}

static TZrSize graph_write_fixture(
        SZrArtifactMetadataGraphFixture *fixture,
        TZrByte *buffer,
        TZrSize capacity) {
    SZrArtifactDiagnostic diagnostic;
    TZrSize required = 0u;
    TZrSize written = 0u;

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture->document, &required, &diagnostic));
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(capacity, required);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Write(
                    &fixture->document,
                    buffer,
                    capacity,
                    &written,
                    &diagnostic));
    return written;
}

static void graph_assert_roundtrip(EZrArtifactKind kind) {
    SZrArtifactMetadataGraphFixture fixture;
    SZrArtifactView view;
    SZrArtifactSectionView section;
    SZrArtifactMetadataStateRow state;
    SZrArtifactMetadataRecordRow record;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[4096];
    TZrSize length;

    graph_init_fixture(&fixture, kind);
    length = graph_write_fixture(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
                    &section,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_ReadMetadataStateRow(
                    &section, 0u, &state, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(fixture.metadataState.metadataGeneration,
                             state.metadataGeneration);
    TEST_ASSERT_EQUAL_UINT64(fixture.metadataState.metadataHash,
                             state.metadataHash);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
                    &section,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_ReadMetadataRecordRow(
                    &section, 0u, &record, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(fixture.metadataRecord.recordHash,
                             record.recordHash);
}

void test_artifact_metadata_graph_roundtrips_zri_and_zro(void) {
    graph_assert_roundtrip(ZR_ARTIFACT_KIND_ZRI);
    graph_assert_roundtrip(ZR_ARTIFACT_KIND_ZRO);
}

void test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs(void) {
    SZrArtifactMetadataGraphFixture fixture;
    SZrArtifactView view;
    SZrArtifactSectionView stateSection;
    SZrArtifactSectionView blobSection;
    SZrArtifactSectionView mapSection;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[4096];
    TZrSize length;

    graph_init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.metadataState.preservationState =
            ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));

    fixture.metadataState.preservationState =
            ZR_ARTIFACT_METADATA_PRESERVATION_FULL;
    fixture.metadataState.category = ZR_ARTIFACT_REFLECTION_CATEGORY_ERASED;
    fixture.metadataState.metadataHash =
            ZrCore_Artifact_ComputeMetadataStateHash(&fixture.metadataState);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));

    fixture.metadataState.category = ZR_ARTIFACT_REFLECTION_CATEGORY_CLASS;
    fixture.metadataState.metadataHash =
            ZrCore_Artifact_ComputeMetadataStateHash(&fixture.metadataState);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));

    fixture.metadataState.category = ZR_ARTIFACT_REFLECTION_CATEGORY_STRUCT;
    fixture.metadataState.metadataHash =
            ZrCore_Artifact_ComputeMetadataStateHash(&fixture.metadataState);
    fixture.metadataRecord.ownerToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 99u);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));

    fixture.metadataRecord.ownerToken = GRAPH_MEMBER_TOKEN;
    fixture.metadataState.metadataHash ^= 1u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));
    fixture.metadataState.metadataHash ^= 1u;

    fixture.metadataBlob[0] ^= 1u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));
    fixture.metadataBlob[0] ^= 1u;

    graph_write_u32(fixture.layoutMap + 16u, fixture.layout.byteSize);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &length, &diagnostic));

    graph_write_u32(fixture.layoutMap + 16u, 8u);
    length = graph_write_fixture(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
                    &stateSection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_METADATA_BLOB_HEAP,
                    &blobSection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_LAYOUT_MAP_HEAP,
                    &mapSection,
                    &diagnostic));

    graph_write_u32(
            buffer + stateSection.byteOffset + sizeof(TZrMetadataToken) +
                    sizeof(TZrUInt32),
            ZR_ARTIFACT_REFLECTION_CATEGORY_CLASS);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    graph_write_u32(
            buffer + stateSection.byteOffset + sizeof(TZrMetadataToken) +
                    sizeof(TZrUInt32),
            ZR_ARTIFACT_REFLECTION_CATEGORY_STRUCT);

    buffer[blobSection.byteOffset] ^= 1u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    buffer[blobSection.byteOffset] ^= 1u;

    graph_write_u32(buffer + mapSection.byteOffset + 16u, fixture.layout.byteSize);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
}

void test_artifact_metadata_projection_matches_source_native_and_binary(void) {
    ZrLibMethodDescriptor nativeMethod;
    ZrLibEnumMemberDescriptor nativeEnumMember;
    ZrLibTypeDescriptor nativeType;
    SZrReflectionTypeIdentity identity;
    SZrArtifactMetadataStateRow sourceState;
    SZrArtifactMetadataStateRow nativeState;
    SZrArtifactMetadataStateRow binaryState;
    SZrArtifactMetadataGraphFixture fixture;
    SZrArtifactView view;
    SZrArtifactSectionView section;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[4096];
    TZrSize length;

    memset(&nativeMethod, 0, sizeof(nativeMethod));
    memset(&nativeEnumMember, 0, sizeof(nativeEnumMember));
    memset(&nativeType, 0, sizeof(nativeType));
    nativeType.name = "Point";
    nativeType.prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    nativeType.methods = &nativeMethod;
    nativeType.methodCount = 1u;
    memset(&identity, 0, sizeof(identity));
    identity.canonicalTypeId = GRAPH_TYPE_ID;
    identity.typeToken = GRAPH_TYPE_DEF_TOKEN;
    identity.signatureHash = GRAPH_TYPE_HASH;
    identity.metadataGeneration = 3u;
    identity.category = ZR_REFLECTION_TYPE_CATEGORY_STRUCT;

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactMetadata_BuildState(
                    &identity,
                    ZR_NULL,
                    ZR_ARTIFACT_METADATA_PRESERVATION_FULL,
                    1u,
                    0u,
                    1u,
                    GRAPH_LAYOUT_HASH,
                    GRAPH_CONTRACT_HASH,
                    &sourceState,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactMetadata_BuildState(
                    &identity,
                    &nativeType,
                    ZR_ARTIFACT_METADATA_PRESERVATION_FULL,
                    1u,
                    0u,
                    1u,
                    GRAPH_LAYOUT_HASH,
                    GRAPH_CONTRACT_HASH,
                    &nativeState,
                    &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&sourceState, &nativeState, sizeof(sourceState));

    graph_init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.metadataState = nativeState;
    length = graph_write_fixture(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
                    &section,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_ReadMetadataStateRow(
                    &section, 0u, &binaryState, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&sourceState, &binaryState, sizeof(sourceState));

    nativeType.prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrParser_ArtifactMetadata_BuildState(
                    &identity,
                    &nativeType,
                    ZR_ARTIFACT_METADATA_PRESERVATION_FULL,
                    1u,
                    0u,
                    1u,
                    GRAPH_LAYOUT_HASH,
                    GRAPH_CONTRACT_HASH,
                    &nativeState,
                    &diagnostic));

    nativeType.prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    nativeType.methods = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
            ZrParser_ArtifactMetadata_BuildState(
                    &identity,
                    &nativeType,
                    ZR_ARTIFACT_METADATA_PRESERVATION_FULL,
                    1u,
                    0u,
                    1u,
                    GRAPH_LAYOUT_HASH,
                    GRAPH_CONTRACT_HASH,
                    &nativeState,
                    &diagnostic));

    nativeType.methods = &nativeMethod;
    nativeMethod.propertyName = "x";
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrParser_ArtifactMetadata_BuildState(
                    &identity,
                    &nativeType,
                    ZR_ARTIFACT_METADATA_PRESERVATION_FULL,
                    1u,
                    0u,
                    1u,
                    GRAPH_LAYOUT_HASH,
                    GRAPH_CONTRACT_HASH,
                    &nativeState,
                    &diagnostic));

    nativeMethod.propertyName = ZR_NULL;
    nativeType.prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
    nativeType.methods = ZR_NULL;
    nativeType.methodCount = 0u;
    nativeType.enumMembers = &nativeEnumMember;
    nativeType.enumMemberCount = 1u;
    identity.category = ZR_REFLECTION_TYPE_CATEGORY_ENUM;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactMetadata_BuildState(
                    &identity,
                    &nativeType,
                    ZR_ARTIFACT_METADATA_PRESERVATION_FULL,
                    1u,
                    0u,
                    1u,
                    GRAPH_LAYOUT_HASH,
                    GRAPH_CONTRACT_HASH,
                    &nativeState,
                    &diagnostic));
}
