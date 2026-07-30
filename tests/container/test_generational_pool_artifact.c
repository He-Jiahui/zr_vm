#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_lib_container/generational_pool.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_parser/artifact_projection.h"

#define POOL_TYPE_ID ((TZrUInt32)91u)
#define POOL_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define POOL_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define POOL_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define POOL_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define POOL_MEMBER_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define POOL_TYPE_REF_HASH UINT64_C(0x9100000000000001)
#define POOL_TYPE_SPEC_HASH UINT64_C(0x9100000000000002)
#define POOL_LAYOUT_HASH UINT64_C(0x9100000000000004)
#define POOL_CALLABLE_HASH UINT64_C(0x9100000000000005)
#define POOL_MODULE_HASH UINT64_C(0x9100000000000006)

typedef struct SPoolArtifactFixture {
    TZrByte signature[5];
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactSectionInput sections[8];
    SZrArtifactDocument document;
} SPoolArtifactFixture;

static void write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static const ZrLibTypeDescriptor *find_type(
        const ZrLibModuleDescriptor *module,
        const TZrChar *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < module->typeCount; index++) {
        if (strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return ZR_NULL;
}

static const ZrLibConstantDescriptor *find_constant(
        const ZrLibModuleDescriptor *module,
        const TZrChar *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < module->constantCount; index++) {
        if (strcmp(module->constants[index].name, name) == 0) {
            return &module->constants[index];
        }
    }
    return ZR_NULL;
}

static void init_fixture(
        SPoolArtifactFixture *fixture,
        const ZrLibTypeDescriptor *poolType,
        TZrUInt64 stableSlotContractHash) {
    static const TZrByte strings[] = {0u};
    static const TZrByte code[] = {0u};
    SZrArtifactPublicIdentity *identity;
    TZrUInt64 signatureHash;

    memset(fixture, 0, sizeof(*fixture));
    fixture->signature[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE;
    write_u32(&fixture->signature[1], 4u);
    signatureHash = ZrCore_Artifact_HashBytes(
            fixture->signature, sizeof(fixture->signature));

    fixture->typeDef.token = POOL_TYPE_DEF_TOKEN;
    fixture->typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_GC;
    fixture->typeDef.canonicalTypeId = POOL_TYPE_ID;
    fixture->typeDef.typeSignatureHash = signatureHash;
    fixture->typeDef.constructorContractHash = POOL_CALLABLE_HASH;

    fixture->typeRef.token = POOL_TYPE_REF_TOKEN;
    fixture->typeRef.signatureToken = POOL_SIGNATURE_TOKEN;
    fixture->typeRef.canonicalTypeId = POOL_TYPE_ID;
    fixture->typeRef.signatureLength = sizeof(fixture->signature);
    fixture->typeRef.signatureHash = POOL_TYPE_REF_HASH;
    fixture->typeRef.layoutVersion = 2u;
    fixture->typeRef.layoutHash = POOL_LAYOUT_HASH;
    fixture->typeSpec = fixture->typeRef;
    fixture->typeSpec.token = POOL_TYPE_SPEC_TOKEN;
    fixture->typeSpec.signatureHash = POOL_TYPE_SPEC_HASH;

    fixture->contract.memberToken = POOL_MEMBER_TOKEN;
    fixture->contract.signatureToken = POOL_SIGNATURE_TOKEN;
    fixture->contract.receiverEffect = ZR_ARTIFACT_RECEIVER_MUTABLE;
    fixture->contract.contractHash = POOL_CALLABLE_HASH;

    fixture->layout.typeToken = POOL_TYPE_DEF_TOKEN;
    fixture->layout.version = 2u;
    fixture->layout.byteSize = (TZrUInt32)sizeof(void *);
    fixture->layout.byteAlignment = (TZrUInt32)_Alignof(void *);
    fixture->layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_MAPPED;
    fixture->layout.layoutHash = POOL_LAYOUT_HASH;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactLayout_ApplyNativeCapabilities(
                    poolType,
                    stableSlotContractHash,
                    &fixture->layout,
                    ZR_NULL));

    fixture->sections[0] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_STRING_HEAP,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            sizeof(strings),
            strings};
    fixture->sections[1] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &fixture->typeDef};
    fixture->sections[2] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &fixture->typeRef};
    fixture->sections[3] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &fixture->typeSpec};
    fixture->sections[4] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            sizeof(fixture->signature),
            fixture->signature};
    fixture->sections[5] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &fixture->contract};
    fixture->sections[6] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &fixture->layout};
    fixture->sections[7] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CODE_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            sizeof(code),
            code};

    fixture->document.kind = ZR_ARTIFACT_KIND_ZRO;
    fixture->document.sectionCount = 8u;
    fixture->document.sections = fixture->sections;
    identity = &fixture->document.identity;
    identity->canonicalTypeId = POOL_TYPE_ID;
    identity->typeRefToken = POOL_TYPE_REF_TOKEN;
    identity->typeSpecToken = POOL_TYPE_SPEC_TOKEN;
    identity->signatureToken = POOL_SIGNATURE_TOKEN;
    identity->typeRefHash = POOL_TYPE_REF_HASH;
    identity->typeSpecHash = POOL_TYPE_SPEC_HASH;
    identity->signatureHash = signatureHash;
    identity->layoutVersion = 2u;
    identity->layoutHash = POOL_LAYOUT_HASH;
    identity->callableContractHash = POOL_CALLABLE_HASH;
    identity->moduleHash = POOL_MODULE_HASH;
}

static void test_native_pool_contract_roundtrips_to_reflection_layout(void) {
    const ZrLibModuleDescriptor *module =
            ZrVmLibContainer_GetPoolingModuleDescriptor();
    const ZrLibTypeDescriptor *poolType = find_type(module, "Pool");
    const ZrLibConstantDescriptor *contractConstant = find_constant(
            module, "STABLE_SLOT_CONTRACT_HASH");
    SPoolArtifactFixture fixture;
    SZrArtifactDiagnostic diagnostic;
    SZrCanonicalConsumerProjection projection;
    SZrCanonicalTypeProjection reflected;
    TZrByte buffer[2048];
    TZrSize requiredSize;
    TZrSize writtenSize;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *entry;
    TZrInt64 sourceContractHash;

    TEST_ASSERT_NOT_NULL(poolType);
    TEST_ASSERT_NOT_NULL(contractConstant);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
            (TZrUInt64)contractConstant->intValue);
    init_fixture(
            &fixture,
            poolType,
            (TZrUInt64)contractConstant->intValue);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
            fixture.layout.stableSlotContractHash);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &requiredSize, &diagnostic));
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(sizeof(buffer), requiredSize);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Write(
                    &fixture.document,
                    buffer,
                    sizeof(buffer),
                    &writtenSize,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    buffer,
                    writtenSize,
                    &fixture.document.identity,
                    &projection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Reflection_ResolveArtifactType(
                    &projection,
                    POOL_TYPE_DEF_TOKEN,
                    &reflected,
                    &diagnostic));
    TEST_ASSERT_TRUE(reflected.hasLayout);
    TEST_ASSERT_BITS_HIGH(
            ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE,
            reflected.layout.capabilityFlags);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
            reflected.layout.stableSlotContractHash);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "pool_artifact_contract_source.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    entry = ZrParser_Source_Compile(
            state,
            "let {STABLE_SLOT_CONTRACT_HASH} = import(\"zr.pooling\");\n"
            "return STABLE_SLOT_CONTRACT_HASH;\n",
            strlen(
                    "let {STABLE_SLOT_CONTRACT_HASH} = import(\"zr.pooling\");\n"
                    "return STABLE_SLOT_CONTRACT_HASH;\n"),
            sourceName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, entry, &sourceContractHash));
    TEST_ASSERT_EQUAL_UINT64(
            ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
            (TZrUInt64)sourceContractHash);
    ZrCore_Function_Free(state, entry);
    ZrContainerTests_DestroyState(state);
}

static void test_native_capability_projection_rejects_missing_or_dangling_hash(void) {
    const ZrLibModuleDescriptor *module =
            ZrVmLibContainer_GetPoolingModuleDescriptor();
    const ZrLibTypeDescriptor *poolType = find_type(module, "Pool");
    ZrLibTypeDescriptor invalid;
    SZrArtifactLayoutRow layout;
    SZrArtifactDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(poolType);
    invalid = *poolType;
    memset(&layout, 0, sizeof(layout));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrParser_ArtifactLayout_ApplyNativeCapabilities(
                    &invalid, 0u, &layout, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE, diagnostic.sectionKind);

    invalid = *poolType;
    invalid.protocolMask &=
            ~ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_STABLE_SLOT_SOURCE);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrParser_ArtifactLayout_ApplyNativeCapabilities(
                    &invalid,
                    ZR_POOL_STABLE_SLOT_CONTRACT_HASH,
                    &layout,
                    &diagnostic));
}

static void test_pool_artifact_rejects_corrupt_and_unknown_layout_contracts(void) {
    const ZrLibTypeDescriptor *poolType = find_type(
            ZrVmLibContainer_GetPoolingModuleDescriptor(), "Pool");
    SPoolArtifactFixture fixture;
    SZrArtifactDiagnostic diagnostic;
    TZrSize requiredSize;

    TEST_ASSERT_NOT_NULL(poolType);
    init_fixture(&fixture, poolType, ZR_POOL_STABLE_SLOT_CONTRACT_HASH);
    fixture.layout.stableSlotContractHash = 0u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &requiredSize, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE, diagnostic.sectionKind);

    init_fixture(&fixture, poolType, ZR_POOL_STABLE_SLOT_CONTRACT_HASH);
    fixture.layout.capabilityFlags |= (TZrUInt32)1u << 31u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &requiredSize, &diagnostic));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_native_pool_contract_roundtrips_to_reflection_layout);
    RUN_TEST(test_native_capability_projection_rejects_missing_or_dangling_hash);
    RUN_TEST(test_pool_artifact_rejects_corrupt_and_unknown_layout_contracts);
    return UNITY_END();
}
