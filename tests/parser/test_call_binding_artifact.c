#include "unity.h"

#include <string.h>

#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_parser/artifact_projection.h"

void setUp(void) {}
void tearDown(void) {}

static SZrArtifactCallBindingRow make_row(void) {
    SZrArtifactCallBindingRow row;
    memset(&row, 0, sizeof(row));
    row.schemaVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    row.functionIndex = 2u;
    row.cacheIndex = 1u;
    row.instructionIndex = 7u;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    row.contract.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 4u);
    row.contract.signatureHash = 0x1020304050607080ULL;
    row.contract.moduleSignatureHash = 0x2030405060708090ULL;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_CONSTANT;
    row.location.targetIndex = 5u;
    return row;
}

static SZrArtifactSectionView row_section(const TZrByte *bytes, TZrUInt32 count) {
    SZrArtifactSectionView section;
    memset(&section, 0, sizeof(section));
    section.kind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE;
    section.elementSize = ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE;
    section.elementCount = count;
    section.byteLength = section.elementSize * count;
    section.data = bytes;
    return section;
}

static void test_binding_row_roundtrip_is_fixed_width_and_pointer_free(void) {
    SZrArtifactCallBindingRow expected = make_row(), actual;
    SZrArtifactDiagnostic diagnostic;
    TZrByte bytes[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
    SZrArtifactSectionView section = row_section(bytes, 1u);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_WriteCallBindingRow(&expected, bytes, sizeof(bytes), &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(96u, sizeof(bytes));
    TEST_ASSERT_EQUAL_UINT8(ZR_CALL_BINDING_SCHEMA_VERSION, bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(2u, bytes[4]);
    TEST_ASSERT_EQUAL_UINT8(1u, bytes[8]);
    TEST_ASSERT_EQUAL_UINT8(7u, bytes[12]);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_ReadCallBindingRow(&section, 0u, &actual, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&expected, &actual, sizeof(expected));
}

static void test_binding_row_rejects_invalid_contract_relocation_and_version(void) {
    SZrArtifactCallBindingRow row = make_row(), decoded;
    SZrArtifactDiagnostic diagnostic;
    TZrByte bytes[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
    SZrArtifactSectionView section = row_section(bytes, 1u);

    row.contract.signatureToken = row.contract.targetMetadataToken;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_WriteCallBindingRow(&row, bytes, sizeof(bytes), &diagnostic));
    row = make_row();
    row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_Artifact_WriteCallBindingRow(&row, bytes, sizeof(bytes), &diagnostic));
    row = make_row();
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_WriteCallBindingRow(&row, bytes, sizeof(bytes), &diagnostic));
    bytes[0]++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION,
            ZrCore_Artifact_ReadCallBindingRow(&section, 0u, &decoded, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_SCHEMA_VERSION, diagnostic.expectedVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_SCHEMA_VERSION + 1u, diagnostic.actualVersion);
    bytes[0]--;
    bytes[68] = 1u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_Artifact_ReadCallBindingRow(&section, 0u, &decoded, &diagnostic));
}

static void test_binding_row_rejects_truncation_and_wrong_section(void) {
    SZrArtifactCallBindingRow row = make_row(), decoded;
    TZrByte bytes[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
    SZrArtifactSectionView section = row_section(bytes, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
            ZrCore_Artifact_WriteCallBindingRow(&row, bytes, sizeof(bytes) - 1u, ZR_NULL));
    section.byteLength--;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TRUNCATED,
            ZrCore_Artifact_ReadCallBindingRow(&section, 0u, &decoded, ZR_NULL));
    section = row_section(bytes, 1u);
    section.kind = ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
            ZrCore_Artifact_ReadCallBindingRow(&section, 0u, &decoded, ZR_NULL));
}

static void test_function_projection_copies_only_persistent_binding_fields(void) {
    SZrFunction function;
    SZrFunctionCallSiteCacheEntry caches[3];
    SZrArtifactCallBindingRow rows[2], expected = make_row();
    TZrUInt32 count = 0u;
    memset(&function, 0, sizeof(function));
    memset(caches, 0, sizeof(caches));
    function.callSiteCaches = caches;
    function.callSiteCacheLength = 3u;
    function.instructionsLength = 9u;
    function.constantValueLength = 6u;
    caches[1].instructionIndex = expected.instructionIndex;
    caches[1].binding.contract = expected.contract;
    caches[1].bindingLocation = expected.location;
    caches[1].binding.generation = 0x33445566778899aaULL;
    caches[1].binding.target.targetKind = ZR_CALL_BINDING_TARGET_VM;
    caches[1].binding.target.vm.function = &function;

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactCallBinding_BuildRows(&function, 2u, ZR_NULL, 0u, &count, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT32(1u, count);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactCallBinding_BuildRows(&function, 2u, rows, 2u, &count, ZR_NULL));
    TEST_ASSERT_EQUAL_MEMORY(&expected, &rows[0], sizeof(expected));
    caches[1].instructionIndex = function.instructionsLength;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrParser_ArtifactCallBinding_BuildRows(&function, 2u, rows, 2u, &count, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT32(0u, count);
}

typedef struct BindingArtifactFixture {
    SZrArtifactDocument document;
    SZrArtifactSectionInput sections[7];
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow callable;
    SZrArtifactLayoutRow layout;
    SZrArtifactCallBindingRow bindings[2];
} BindingArtifactFixture;

static void init_artifact_fixture(BindingArtifactFixture *fixture) {
    static const TZrByte signature[] = {ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE, 4u, 0u, 0u, 0u};
    SZrArtifactPublicIdentity *identity;
    memset(fixture, 0, sizeof(*fixture));
    fixture->document.kind = ZR_ARTIFACT_KIND_ZRO;
    fixture->document.sections = fixture->sections;
    fixture->document.sectionCount = 7u;
    identity = &fixture->document.identity;
    identity->canonicalTypeId = 1u;
    identity->typeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    identity->typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    identity->signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    identity->typeRefHash = identity->typeSpecHash = identity->signatureHash =
            ZrCore_Artifact_HashBytes(signature, sizeof(signature));
    identity->layoutVersion = 1u;
    identity->layoutHash = 12u;
    identity->callableContractHash = 13u;
    identity->moduleHash = 14u;
    fixture->typeDef.token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    fixture->typeDef.canonicalTypeId = 1u;
    fixture->typeDef.typeSignatureHash = identity->signatureHash;
    fixture->typeRef.token = identity->typeRefToken;
    fixture->typeRef.signatureToken = identity->signatureToken;
    fixture->typeRef.canonicalTypeId = 1u;
    fixture->typeRef.signatureLength = sizeof(signature);
    fixture->typeRef.signatureHash = identity->typeRefHash;
    fixture->typeRef.layoutVersion = 1u;
    fixture->typeRef.layoutHash = identity->layoutHash;
    fixture->typeSpec = fixture->typeRef;
    fixture->typeSpec.token = identity->typeSpecToken;
    fixture->callable.memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    fixture->callable.signatureToken = identity->signatureToken;
    fixture->callable.contractHash = identity->callableContractHash;
    fixture->layout.typeToken = fixture->typeDef.token;
    fixture->layout.version = 1u;
    fixture->layout.byteSize = fixture->layout.byteAlignment = 8u;
    fixture->layout.layoutHash = identity->layoutHash;
    fixture->bindings[0] = make_row();
    fixture->bindings[1] = make_row();
    fixture->bindings[1].cacheIndex++;
    fixture->bindings[1].instructionIndex++;
    fixture->sections[0] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, 0u, 1u, &fixture->typeDef};
    fixture->sections[1] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, 0u, 1u, &fixture->typeRef};
    fixture->sections[2] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, 0u, 1u, &fixture->typeSpec};
    fixture->sections[3] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, 0u, sizeof(signature), signature};
    fixture->sections[4] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_CONTRACT_TABLE, 0u, 1u, &fixture->callable};
    fixture->sections[5] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_LAYOUT_TABLE, 0u, 1u, &fixture->layout};
    fixture->sections[6] = (SZrArtifactSectionInput){ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, 0u, 2u, fixture->bindings};
}

static void test_generic_artifact_roundtrip_and_consumer_keep_binding_section(void) {
    BindingArtifactFixture fixture;
    SZrArtifactView view;
    SZrCanonicalConsumerProjection projection;
    SZrArtifactCallBindingRow decoded;
    SZrArtifactDiagnostic diagnostic;
    TZrByte bytes[2048], copy[2048];
    TZrSize written = 0u;
    init_artifact_fixture(&fixture);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Write(&fixture.document, bytes, sizeof(bytes), &written, &diagnostic));
    memcpy(copy, bytes, written);
    memset(bytes, 0xff, written);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Read(copy, written, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(copy, written, &fixture.document.identity, &projection, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(2u, projection.callBindings.elementCount);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_ReadCallBindingRow(&projection.callBindings, 1u, &decoded, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&fixture.bindings[1], &decoded, sizeof(decoded));
    TEST_ASSERT_EQUAL_STRING("call-binding-table", ZrCore_Artifact_SectionName(projection.callBindings.kind));
}

static void test_generic_artifact_rejects_duplicate_sites_and_corrupt_bindings(void) {
    BindingArtifactFixture fixture;
    SZrArtifactView view;
    SZrArtifactSectionView section;
    SZrArtifactDiagnostic diagnostic;
    TZrByte bytes[2048];
    TZrSize written = 0u;
    init_artifact_fixture(&fixture);
    fixture.bindings[1].cacheIndex = fixture.bindings[0].cacheIndex;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_Artifact_Write(&fixture.document, bytes, sizeof(bytes), &written, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.rowIndex);
    fixture.bindings[1].cacheIndex++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Write(&fixture.document, bytes, sizeof(bytes), &written, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK, ZrCore_Artifact_Read(bytes, written, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(&view, ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, &section, &diagnostic));
    bytes[section.byteOffset + section.elementSize + 8u] = (TZrByte)fixture.bindings[0].cacheIndex;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_Artifact_Read(bytes, written, &view, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.rowIndex);
    bytes[section.byteOffset + section.elementSize + 8u]++;
    bytes[section.byteOffset + 80u] = 0u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_Artifact_Read(bytes, written, &view, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE, diagnostic.sectionKind);
    fixture.document.kind = ZR_ARTIFACT_KIND_ZRI;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_FORBIDDEN_SECTION,
            ZrCore_Artifact_Write(&fixture.document, bytes, sizeof(bytes), &written, &diagnostic));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binding_row_roundtrip_is_fixed_width_and_pointer_free);
    RUN_TEST(test_binding_row_rejects_invalid_contract_relocation_and_version);
    RUN_TEST(test_binding_row_rejects_truncation_and_wrong_section);
    RUN_TEST(test_function_projection_copies_only_persistent_binding_fields);
    RUN_TEST(test_generic_artifact_roundtrip_and_consumer_keep_binding_section);
    RUN_TEST(test_generic_artifact_rejects_duplicate_sites_and_corrupt_bindings);
    return UNITY_END();
}
