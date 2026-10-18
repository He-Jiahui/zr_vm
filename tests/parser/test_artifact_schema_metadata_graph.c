#include "unity.h"

#include <string.h>

#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_library/native_binding.h"
#include "artifact/artifact_projection.h"

// 同一组类型身份与哈希贯穿状态、记录、布局图和二进制节，便于定位单字段损坏。
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

// 把元数据状态、记录、blob 和布局图作为相互引用的一组栈上产物行。
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

// 建立同一类型身份下的完整元数据图，哈希、保留计数和节范围互相匹配。
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

    // 所有图节均带必需标志；宏只在初始化夹具时追加借用的行数据。
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

// 对完整图先核所需容量再编码，供正反两类读取测试共用。
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

// ZRI/ZRO 读回状态与记录节后，确认 generation 和哈希未被编码改写。
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

// 相同元数据图在中间产物与目标产物中均可往返。
void test_artifact_metadata_graph_roundtrips_zri_and_zro(void) {
    graph_assert_roundtrip(ZR_ARTIFACT_KIND_ZRI);
    graph_assert_roundtrip(ZR_ARTIFACT_KIND_ZRO);
}

// 裁剪声明、类别、owner、哈希、blob 和布局图任一失配都不得被接受。
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

// 源码与 native descriptor 投影须生成相同状态行，二进制再读一致；类别错配须拒绝。
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

/**
 * @struct SZrArtifactMetadataGraphFixture
 * ## Summary
 * 为metadata graph场景把行、载荷和section视图放在同一栈对象内，维持编解码期间可追踪的借用关系。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::SZrArtifactMetadataGraphFixture
 *
 * ## Fields
 * - TZrByte signature[5]; — 手写签名缓冲；基础夹具只使用signatureLength前缀，graph夹具使用固定5字节。
 * - TZrByte metadataBlob[4]; — metadata记录所借用的4字节载荷。
 * - TZrByte layoutMap[20]; — 20字节布局映射载荷；由layout的ownershipMapOffset/ownershipMapLength引用，校验版本、分段计数和槽偏移边界。
 * - TZrByte executablePayload[1]; — ZRI/ZRO载荷的1字节夹具，不执行。
 * - SZrArtifactTypeDefRow typeDef; — 类型定义token及canonical ID基线。
 * - SZrArtifactTypeIdentityRow typeRef; — type-ref身份行，与公开identity哈希配对。
 * - SZrArtifactTypeIdentityRow typeSpec; — type-spec身份行，与公开identity哈希配对。
 * - SZrArtifactMemberDefRow member; — metadata owner/member token关联。
 * - SZrArtifactContractRow contract; — 公开contract身份及版本基线。
 * - SZrArtifactLayoutRow layout; — 布局身份、版本与大小关系。
 * - SZrArtifactMetadataStateRow metadataState; — generation、preservation及聚合hash状态行。
 * - SZrArtifactMetadataRecordRow metadataRecord; — 记录owner、kind、retention、generation及metadataBlob内payload范围；recordHash绑定记录字段和有效载荷。
 * - SZrArtifactSectionInput sections[12]; — 节输入视图数组；实际count由document记录，data借用本对象字段或静态载荷。
 * - SZrArtifactDocument document; — 编码器入口文档，sections指针及公开identity须与各行一致。
 *
 * ## Value model
 * 调用者持有完整值；初始化辅助器把document.sections和section.data指向本对象或只读静态载荷，不转移任何指针所有权。
 *
 * ## Invariants and lifecycle
 * 初始化后对象地址与被借用字段须保持有效直至编码完成；普通按值复制不会重定位内部指针。未建立跨线程共享或ABI持久化约定，写磁盘的是schema编码而非此C内存布局。
 *
 * ## References
 * - tests/parser/test_artifact_schema_metadata_graph.c:53 — type use (SZrArtifactMetadataGraphFixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:186 — type use (SZrArtifactMetadataGraphFixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:211 — type use (SZrArtifactMetadataGraphFixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:27 — physical definition or protocol span (SZrArtifactMetadataGraphFixture)
 */

/**
 * @fn static void graph_init_fixture( SZrArtifactMetadataGraphFixture *fixture, EZrArtifactKind kind)
 * ## Summary
 * 把metadata state、record、blob和layout-map组合成同一类型身份下的完整ZRI/ZRO栈夹具，供正常往返及单点篡改使用。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::graph_init_fixture
 *
 * ## Parameters
 * @param[out] fixture 非NULL可写完整图夹具，清零后建立行、blob和document自借用。
 * @param[in] kind 当前选择ZRI语义载荷或ZRO代码载荷。
 *
 * ## Returns
 * 无返回值；将document.sections指向fixture.sections，借用有效到编码结束。
 *
 * ## Constraints and lifecycle
 * 所有12个节均借用fixture内部字段且带mandatory标志；先构造依赖字段再计算状态/记录哈希。kind由当前调用限ZRI/ZRO；不提供NULL/容量防护，浅拷贝不重定位指针。
 *
 * ## References
 * - tests/parser/test_artifact_schema_metadata_graph.c:220 — direct call expression (graph_init_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:272 — direct call expression (graph_init_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:438 — direct call expression (graph_init_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:44 — paired helper definition (graph_write_u32)
 * - tests/parser/test_artifact_schema_metadata_graph.c:52 — physical definition or protocol span (graph_init_fixture)
 */

/**
 * @def GRAPH_ADD_SECTION(KIND, COUNT, DATA)
 * ## Summary
 * 在graph_init_fixture内追加一个mandatory节视图，集中保持12节的写入次序和sectionCount递增。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::GRAPH_ADD_SECTION(KIND, COUNT, DATA)
 *
 * ## Parameters
 * @param[in] KIND 写入当前节的schema种类常量。
 * @param[in] COUNT 当前节行数；只写入目录元数据，不核验DATA容量。
 * @param[in] DATA 借用fixture内行/载荷的指针，不转移所有权。
 *
 * ## Returns
 * 无返回值；写fixture->sections[sectionCount]并推进sectionCount。
 *
 * ## Constraints and lifecycle
 * 宏捕获当前函数的fixture与sectionCount，调用者维持sections[12]边界与行大小关系；当前12次调用为固定输入。宏不提供普通错误返回，末尾#undef限制后续预处理作用域。
 *
 * ## References
 * - tests/parser/test_artifact_schema_metadata_graph.c:147 — macro expansion (GRAPH_ADD_SECTION)
 * - tests/parser/test_artifact_schema_metadata_graph.c:148 — macro expansion (GRAPH_ADD_SECTION)
 * - tests/parser/test_artifact_schema_metadata_graph.c:149 — macro expansion (GRAPH_ADD_SECTION)
 * - tests/parser/test_artifact_schema_metadata_graph.c:144 — physical definition or protocol span (GRAPH_ADD_SECTION(KIND, COUNT, DATA))
 */

/**
 * @fn static TZrSize graph_write_fixture( SZrArtifactMetadataGraphFixture *fixture, TZrByte *buffer, TZrSize capacity)
 * ## Summary
 * 在metadata graph测试中先核所需容量再调用编码器，把合法图写入调用者缓冲。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::graph_write_fixture
 *
 * ## Parameters
 * @param[in] fixture 已建立的图文档；本辅助器不修改其行。
 * @param[out] buffer 至少capacity字节的编码输出。
 * @param[in] capacity buffer可写字节范围。
 *
 * ## Returns
 * @return 编码断言通过后返回实际written字节数；缓冲所有权不转移。
 *
 * ## Constraints and lifecycle
 * required<=capacity由Unity断言检查；编码后只断言status==OK并返回written，没有另行断言written==required。所有输入行/输出缓冲在调用期有效，断言失败不作为普通错误返回。
 *
 * ## References
 * - tests/parser/test_artifact_schema_metadata_graph.c:221 — direct call expression (graph_write_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:330 — direct call expression (graph_write_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:440 — direct call expression (graph_write_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:185 — physical definition or protocol span (graph_write_fixture)
 */

/**
 * @fn static void graph_assert_roundtrip(EZrArtifactKind kind)
 * ## Summary
 * 对一种ZRI/ZRO图夹具编码并读取状态、记录节，核对metadataGeneration及两个选定哈希。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::graph_assert_roundtrip
 *
 * ## Parameters
 * @param[in] kind 由调用者选定的ZRI或ZRO格式。
 *
 * ## Returns
 * 无返回值；失败经Unity结束当前用例。
 *
 * ## Constraints and lifecycle
 * 所有fixture、buffer、section和行都是栈上值或借用；只对当前3个字段进行读回断言，不代表全图每字段逐一相等。
 *
 * ## References
 * - tests/parser/test_artifact_schema_metadata_graph.c:257 — direct call expression (graph_assert_roundtrip)
 * - tests/parser/test_artifact_schema_metadata_graph.c:258 — direct call expression (graph_assert_roundtrip)
 * - tests/parser/test_artifact_schema_metadata_graph.c:52 — paired helper definition (graph_init_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:185 — paired helper definition (graph_write_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:210 — physical definition or protocol span (graph_assert_roundtrip)
 */

/**
 * @fn void test_artifact_metadata_graph_roundtrips_zri_and_zro(void)
 * ## Summary
 * 分别以ZRI和ZRO调用同一完整metadata graph夹具的往返核查器，要求generation和metadata/record哈希被保留。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::test_artifact_metadata_graph_roundtrips_zri_and_zro
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 两个kind共用一个Unity注册；首个场景失败会跳过第二个，不是完整reflection API验证。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1241 — Unity callback registration; not direct C call (test_artifact_metadata_graph_roundtrips_zri_and_zro)
 * - tests/parser/test_artifact_schema_metadata_graph.c:210 — paired helper definition (graph_assert_roundtrip)
 * - tests/parser/test_artifact_schema_metadata_graph.c:256 — physical definition or protocol span (test_artifact_metadata_graph_roundtrips_zri_and_zro)
 */

/**
 * @fn void test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs(void)
 * ## Summary
 * 依次破坏preservation/category/owner/state hash/blob/layout-map，再对合法编码的category/blob/map字节做篡改，核验写前和读取拒绝。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 夹具修复只针对下一段所需字段；只断言这些具体篡改对应ILLEGAL_TOKEN，不把顺序测试解读为独立故障隔离运行。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1242 — Unity callback registration; not direct C call (test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs)
 * - tests/parser/test_artifact_schema_metadata_graph.c:52 — paired helper definition (graph_init_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:44 — paired helper definition (graph_write_u32)
 * - tests/parser/test_artifact_schema_metadata_graph.c:185 — paired helper definition (graph_write_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:262 — physical definition or protocol span (test_artifact_metadata_graph_rejects_stripped_forged_and_corrupt_inputs)
 */

/**
 * @fn void test_artifact_metadata_projection_matches_source_native_and_binary(void)
 * ## Summary
 * 对同一reflection identity分别以无native descriptor和栈上native struct descriptor构建状态行，要求与二进制读回一致，并覆盖类别/空methods/propertyName拒绝及enum接受。
 *
 * ## Unit
 * test_artifact_schema_metadata_graph.c::test_artifact_metadata_projection_matches_source_native_and_binary
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * nativeMethod等descriptor仅为数据夹具，未派发native函数；方法名、全部反射字段或所有类别组合不在此测试保证内。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1243 — Unity callback registration; not direct C call (test_artifact_metadata_projection_matches_source_native_and_binary)
 * - tests/parser/test_artifact_schema_metadata_graph.c:52 — paired helper definition (graph_init_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:185 — paired helper definition (graph_write_fixture)
 * - tests/parser/test_artifact_schema_metadata_graph.c:381 — physical definition or protocol span (test_artifact_metadata_projection_matches_source_native_and_binary)
 */
