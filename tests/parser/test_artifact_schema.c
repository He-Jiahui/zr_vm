#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_parser/artifact_projection.h"
#include "zr_vm_parser/semantic.h"

#define TEST_TYPE_ID ((TZrUInt32)17u)
#define TEST_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define TEST_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define TEST_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_CONSTRUCTOR_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u)
#define TEST_MEMBER_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u)

#define TEST_TYPE_REF_HASH ((TZrUInt64)0x1111222233334444ULL)
#define TEST_TYPE_SPEC_HASH ((TZrUInt64)0x2222333344445555ULL)
#define TEST_SIGNATURE_HASH ((TZrUInt64)0x3333444455556666ULL)
#define TEST_LAYOUT_HASH ((TZrUInt64)0x4444555566667777ULL)
#define TEST_CONTRACT_HASH ((TZrUInt64)0x5555666677778888ULL)
#define TEST_MODULE_HASH ((TZrUInt64)0x6666777788889999ULL)

typedef struct SZrArtifactTestFixture {
    TZrByte signature[16];
    TZrUInt32 signatureLength;
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactSectionInput sections[8];
    SZrArtifactDocument document;
} SZrArtifactTestFixture;

void setUp(void) {}

void tearDown(void) {}

void test_real_source_compile_and_binary_signature_import_are_identical(void);

static void write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static SZrArtifactPublicIdentity make_identity(void) {
    SZrArtifactPublicIdentity identity;

    memset(&identity, 0, sizeof(identity));
    identity.canonicalTypeId = TEST_TYPE_ID;
    identity.typeRefToken = TEST_TYPE_REF_TOKEN;
    identity.typeSpecToken = TEST_TYPE_SPEC_TOKEN;
    identity.signatureToken = TEST_SIGNATURE_TOKEN;
    identity.typeRefHash = TEST_TYPE_REF_HASH;
    identity.typeSpecHash = TEST_TYPE_SPEC_HASH;
    identity.signatureHash = TEST_SIGNATURE_HASH;
    identity.layoutVersion = 7u;
    identity.layoutHash = TEST_LAYOUT_HASH;
    identity.callableContractHash = TEST_CONTRACT_HASH;
    identity.moduleHash = TEST_MODULE_HASH;
    return identity;
}

static void init_fixture(SZrArtifactTestFixture *fixture, EZrArtifactKind kind) {
    static const TZrByte strings[] = {'a', 'p', 'p', 0u};
    static const TZrByte syntaxTree[] = "FunctionDefinition range=1:1-1:28\n";
    static const TZrByte semanticIr[] = "type %17 = shared readonly ref int\n";
    static const TZrByte code[] = {0x41u, 0x01u, 0x00u};
    TZrUInt32 sectionCount = 0u;

    memset(fixture, 0, sizeof(*fixture));
    fixture->signature[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_OWNER;
    fixture->signature[1] = (TZrByte)ZR_ARTIFACT_OWNER_SHARED;
    fixture->signature[2] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW;
    fixture->signature[3] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_REF;
    fixture->signature[4] = (TZrByte)ZR_ARTIFACT_REF_READONLY;
    fixture->signature[5] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE;
    write_u32(&fixture->signature[6], 4u);
    fixture->signatureLength = 10u;

    fixture->typeDef.token = TEST_TYPE_DEF_TOKEN;
    fixture->typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_VALUE |
                             ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE |
                             ZR_ARTIFACT_TYPE_FLAG_READONLY;
    fixture->typeDef.canonicalTypeId = TEST_TYPE_ID;
    fixture->typeDef.constructorToken = TEST_CONSTRUCTOR_TOKEN;
    fixture->typeDef.constructorSignatureToken = TEST_SIGNATURE_TOKEN;
    fixture->typeDef.typeSignatureHash = TEST_SIGNATURE_HASH;
    fixture->typeDef.constructorContractHash = TEST_CONTRACT_HASH;

    fixture->typeRef.token = TEST_TYPE_REF_TOKEN;
    fixture->typeRef.signatureToken = TEST_SIGNATURE_TOKEN;
    fixture->typeRef.canonicalTypeId = TEST_TYPE_ID;
    fixture->typeRef.signatureOffset = 0u;
    fixture->typeRef.signatureLength = fixture->signatureLength;
    fixture->typeRef.signatureHash = TEST_TYPE_REF_HASH;
    fixture->typeRef.layoutVersion = 7u;
    fixture->typeRef.layoutHash = TEST_LAYOUT_HASH;

    fixture->typeSpec = fixture->typeRef;
    fixture->typeSpec.token = TEST_TYPE_SPEC_TOKEN;
    fixture->typeSpec.signatureHash = TEST_TYPE_SPEC_HASH;

    fixture->contract.memberToken = TEST_MEMBER_TOKEN;
    fixture->contract.signatureToken = TEST_SIGNATURE_TOKEN;
    fixture->contract.parameterCount = 1u;
    fixture->contract.flags = ZR_ARTIFACT_CONTRACT_FLAG_SCOPED;
    fixture->contract.receiverEffect = ZR_ARTIFACT_RECEIVER_READONLY;
    fixture->contract.refExportEffect = ZR_ARTIFACT_REF_EXPORT_READONLY;
    fixture->contract.contractHash = TEST_CONTRACT_HASH;

    fixture->layout.typeToken = TEST_TYPE_DEF_TOKEN;
    fixture->layout.version = 7u;
    fixture->layout.byteSize = 16u;
    fixture->layout.byteAlignment = 8u;
    fixture->layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_MAPPED;
    fixture->layout.capabilityFlags = ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE;
    fixture->layout.layoutHash = TEST_LAYOUT_HASH;
    fixture->layout.stableSlotContractHash = 0xABCDEFu;

    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_STRING_HEAP, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(strings), strings};

    if (kind != ZR_ARTIFACT_KIND_ZRS) {
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                1u, &fixture->typeDef};
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                1u, &fixture->typeRef};
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                1u, &fixture->typeSpec};
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                fixture->signatureLength, fixture->signature};
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                1u, &fixture->contract};
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                1u, &fixture->layout};
    }

    if (kind == ZR_ARTIFACT_KIND_ZRS) {
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_SYNTAX_TREE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                (TZrUInt32)(sizeof(syntaxTree) - 1u), syntaxTree};
    } else if (kind == ZR_ARTIFACT_KIND_ZRI) {
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_SEMANTIC_IR, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                (TZrUInt32)(sizeof(semanticIr) - 1u), semanticIr};
    } else {
        fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
                ZR_ARTIFACT_SECTION_CODE_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
                (TZrUInt32)sizeof(code), code};
    }

    fixture->document.kind = kind;
    fixture->document.identity = kind == ZR_ARTIFACT_KIND_ZRS
                                         ? (SZrArtifactPublicIdentity){0}
                                         : make_identity();
    fixture->document.sectionCount = sectionCount;
    fixture->document.sections = fixture->sections;
}

static TZrSize write_fixture(const SZrArtifactTestFixture *fixture,
                             TZrByte *buffer,
                             TZrSize capacity) {
    SZrArtifactDiagnostic diagnostic;
    TZrSize requiredSize = 0u;
    TZrSize writtenSize = 0u;

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_GetEncodedSize(&fixture->document, &requiredSize, &diagnostic));
    TEST_ASSERT_LESS_OR_EQUAL_UINT64((TZrUInt64)capacity, (TZrUInt64)requiredSize);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Write(&fixture->document,
                                                buffer,
                                                capacity,
                                                &writtenSize,
                                                &diagnostic));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)requiredSize, (TZrUInt64)writtenSize);
    return writtenSize;
}

static void test_zro_roundtrips_fixed_width_public_contract_sections(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactView view;
    SZrArtifactSectionView section;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[2048];
    TZrSize length;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    length = write_fixture(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)'Z', buffer[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)'R', buffer[1]);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)'A', buffer[2]);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)'F', buffer[3]);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)(ZR_ARTIFACT_SCHEMA_VERSION & 0xffu), buffer[4]);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_KIND_ZRO, view.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_ID, view.identity.canonicalTypeId);
    TEST_ASSERT_EQUAL_UINT64(TEST_SIGNATURE_HASH, view.identity.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_LAYOUT_HASH, view.identity.layoutHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_CONTRACT_HASH, view.identity.callableContractHash);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
                                                      &section,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, section.elementCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE, section.elementSize);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ReadTypeIdentityRow(&section, 0u, &typeRef, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_REF_TOKEN, typeRef.token);
    TEST_ASSERT_EQUAL_UINT64(TEST_TYPE_REF_HASH, typeRef.signatureHash);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
                                                      &section,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ReadTypeDefRow(&section, 0u, &typeDef, &diagnostic));
    TEST_ASSERT_BITS_HIGH(ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE, typeDef.flags);
    TEST_ASSERT_BITS_HIGH(ZR_ARTIFACT_TYPE_FLAG_READONLY, typeDef.flags);
    TEST_ASSERT_EQUAL_UINT32(TEST_CONSTRUCTOR_TOKEN, typeDef.constructorToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_CONTRACT_HASH, typeDef.constructorContractHash);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                                                      &section,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ReadContractRow(&section, 0u, &contract, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_RECEIVER_READONLY, contract.receiverEffect);
    TEST_ASSERT_EQUAL_UINT64(TEST_CONTRACT_HASH, contract.contractHash);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                                                      &section,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ReadLayoutRow(&section, 0u, &layout, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(7u, layout.version);
    TEST_ASSERT_EQUAL_UINT64(TEST_LAYOUT_HASH, layout.layoutHash);
}

static void assert_text_roundtrip(EZrArtifactKind kind, const TZrChar *sectionName) {
    SZrArtifactTestFixture fixture;
    SZrArtifactView firstView;
    SZrArtifactView secondView;
    SZrArtifactDiagnostic diagnostic;
    TZrByte binary[2048];
    TZrByte decoded[2048];
    TZrChar text[8192];
    TZrSize binaryLength;
    TZrSize textLength = 0u;
    TZrSize decodedLength = 0u;

    init_fixture(&fixture, kind);
    binaryLength = write_fixture(&fixture, binary, sizeof(binary));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(binary, binaryLength, &firstView, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_WriteText(&firstView,
                                                    text,
                                                    sizeof(text),
                                                    &textLength,
                                                    &diagnostic));
    TEST_ASSERT_NOT_NULL(strstr(text, kind == ZR_ARTIFACT_KIND_ZRS ? "kind=zrs" : "kind=zri"));
    TEST_ASSERT_NOT_NULL(strstr(text, sectionName));
    TEST_ASSERT_NOT_NULL(strstr(text,
                                kind == ZR_ARTIFACT_KIND_ZRS
                                        ? "FunctionDefinition range=1:1-1:28"
                                        : "shared readonly ref int"));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ReadText(text,
                                                  textLength,
                                                  decoded,
                                                  sizeof(decoded),
                                                  &decodedLength,
                                                  &diagnostic));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)binaryLength, (TZrUInt64)decodedLength);
    TEST_ASSERT_EQUAL_MEMORY(binary, decoded, binaryLength);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(decoded, decodedLength, &secondView, &diagnostic));
    TEST_ASSERT_EQUAL_INT(kind, secondView.kind);
}

static void test_zrs_and_zri_have_readable_stable_roundtrips(void) {
    assert_text_roundtrip(ZR_ARTIFACT_KIND_ZRS, "syntax-tree");
    assert_text_roundtrip(ZR_ARTIFACT_KIND_ZRI, "semantic-ir");
}

static void test_public_identity_mismatches_are_precise(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactView view;
    SZrArtifactDiagnostic diagnostic;
    SZrArtifactPublicIdentity expected;
    TZrByte buffer[2048];
    TZrSize length;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    length = write_fixture(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));

    expected = make_identity();
    expected.typeRefHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(expected.typeRefHash, diagnostic.expectedHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_TYPE_REF_HASH, diagnostic.actualHash);

    expected = make_identity();
    expected.typeSpecHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));

    expected = make_identity();
    expected.signatureHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));

    expected = make_identity();
    expected.layoutVersion++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));

    expected = make_identity();
    expected.layoutHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));

    expected = make_identity();
    expected.callableContractHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));

    expected = make_identity();
    expected.moduleHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH,
                          ZrCore_Artifact_ValidatePublicIdentity(&view, &expected, &diagnostic));
}

static void test_reader_rejects_unknown_mandatory_but_skips_unknown_optional_section(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactView view;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[2048];
    TZrSize length;
    const TZrSize firstDirectoryKindOffset = ZR_ARTIFACT_HEADER_ENCODED_SIZE;
    const TZrSize firstDirectoryFlagsOffset = firstDirectoryKindOffset + 4u;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    length = write_fixture(&fixture, buffer, sizeof(buffer));
    write_u32(&buffer[firstDirectoryKindOffset], 0x7fffffffu);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_UNKNOWN_MANDATORY_SECTION,
                          ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0x7fffffffu, diagnostic.sectionKind);

    write_u32(&buffer[firstDirectoryFlagsOffset], ZR_ARTIFACT_SECTION_FLAG_OPTIONAL);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));
}

static void test_reader_rejects_truncation_count_limit_and_illegal_tokens(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactView view;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[2048];
    TZrSize length;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    length = write_fixture(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TRUNCATED,
                          ZrCore_Artifact_Read(buffer, length - 1u, &view, &diagnostic));

    write_u32(&buffer[ZR_ARTIFACT_HEADER_SECTION_COUNT_OFFSET], ZR_ARTIFACT_MAX_SECTION_COUNT + 1u);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_COUNT_LIMIT,
                          ZrCore_Artifact_Read(buffer, length, &view, &diagnostic));

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.typeRef.token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &length, &diagnostic));

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.typeRef.signatureLength = fixture.signatureLength + 1u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &length, &diagnostic));
}

static void test_signature_nodes_validate_ref_readonly_owner_and_callable_contracts(void) {
    TZrByte signature[128] = {0};
    SZrArtifactDiagnostic diagnostic;
    TZrSize offset = 0u;

    signature[offset++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_RECEIVER_MUTABLE;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_REF_EXPORT_WRITABLE;
    signature[offset++] = 0u;
    write_u32(&signature[offset], 1u);
    offset += 4u;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE;
    write_u32(&signature[offset], 4u);
    offset += 4u;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_PASSING_REF_READONLY;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_ESCAPE_CALLER;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_ENTRY_INITIALIZED;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_EXIT_UNCHANGED;
    signature[offset++] = 1u;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_CALL_SITE_REF;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_OWNER;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_OWNER_ATOMIC_SHARED;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_REF;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_REF_READONLY;
    signature[offset++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF;
    write_u32(&signature[offset], TEST_TYPE_DEF_TOKEN);
    offset += 4u;

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ValidateSignature(signature, offset, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(ZrCore_Artifact_HashBytes(signature, offset),
                             ZrCore_Artifact_HashBytes(signature, offset));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, ZrCore_Artifact_HashBytes(signature, offset));

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                          ZrCore_Artifact_ValidateSignature(signature, offset - 1u, &diagnostic));
    signature[0] = 0xffu;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                          ZrCore_Artifact_ValidateSignature(signature, offset, &diagnostic));
}

static void test_repeat_encoding_and_text_roundtrip_keep_hashes_stable(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactView view;
    SZrArtifactDiagnostic diagnostic;
    TZrByte first[2048];
    TZrByte second[2048];
    TZrSize firstLength;
    TZrSize secondLength;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    firstLength = write_fixture(&fixture, first, sizeof(first));
    secondLength = write_fixture(&fixture, second, sizeof(second));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)firstLength, (TZrUInt64)secondLength);
    TEST_ASSERT_EQUAL_MEMORY(first, second, firstLength);
    TEST_ASSERT_EQUAL_UINT64(ZrCore_Artifact_HashBytes(first, firstLength),
                             ZrCore_Artifact_HashBytes(second, secondLength));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(second, secondLength, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ValidatePublicIdentity(&view,
                                                                 &fixture.document.identity,
                                                                 &diagnostic));
}

static void test_internal_public_rows_report_the_exact_mismatch(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactDiagnostic diagnostic;
    TZrSize size = 0u;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.typeRef.signatureHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(TEST_TYPE_REF_HASH, diagnostic.expectedHash);

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.typeSpec.signatureHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.contract.contractHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.layout.version++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.layout.layoutHash++;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));
}

static void test_zero_many_and_duplicate_signature_rows_roundtrip_safely(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactSectionInput sections[9];
    SZrArtifactTypeIdentityRow typeRefs[2];
    SZrArtifactMemberDefRow members[256];
    SZrArtifactView view;
    SZrArtifactSectionView section;
    SZrArtifactMemberDefRow decoded;
    SZrArtifactDiagnostic diagnostic;
    TZrByte binary[32768];
    TZrSize length = 0u;
    TZrUInt32 index;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    memcpy(sections, fixture.sections, sizeof(fixture.sections));
    sections[8] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            0u,
            ZR_NULL};
    fixture.document.sections = sections;
    fixture.document.sectionCount = 9u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Write(&fixture.document,
                                                binary,
                                                sizeof(binary),
                                                &length,
                                                &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(binary, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
                                                      &section,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, section.elementCount);

    typeRefs[0] = fixture.typeRef;
    typeRefs[1] = fixture.typeRef;
    typeRefs[1].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 2u);
    sections[2].elementCount = 2u;
    sections[2].data = typeRefs;
    for (index = 0u; index < 256u; ++index) {
        memset(&members[index], 0, sizeof(members[index]));
        members[index].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, index + 32u);
        members[index].ownerTypeToken = TEST_TYPE_DEF_TOKEN;
        members[index].signatureToken = TEST_SIGNATURE_TOKEN;
        members[index].signatureHash = TEST_SIGNATURE_HASH + index;
        members[index].contractHash = TEST_CONTRACT_HASH + index;
    }
    sections[8].elementCount = 256u;
    sections[8].data = members;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Write(&fixture.document,
                                                binary,
                                                sizeof(binary),
                                                &length,
                                                &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(binary, length, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
                                                      &section,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(256u, section.elementCount);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ReadMemberDefRow(&section, 255u, &decoded, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(members[255].token, decoded.token);
    TEST_ASSERT_EQUAL_UINT64(members[255].contractHash, decoded.contractHash);
}

static void test_duplicate_forbidden_and_recursive_signature_inputs_are_rejected(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactSectionInput sections[9];
    SZrArtifactDiagnostic diagnostic;
    TZrByte deepSignature[ZR_ARTIFACT_SIGNATURE_MAX_DEPTH + 8u];
    TZrByte countSignature[5];
    TZrSize size = 0u;
    TZrUInt32 index;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    memcpy(sections, fixture.sections, sizeof(fixture.sections));
    sections[8] = fixture.sections[0];
    fixture.document.sections = sections;
    fixture.document.sectionCount = 9u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_DUPLICATE_SECTION,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    fixture.sections[7].kind = ZR_ARTIFACT_SECTION_SEMANTIC_IR;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_FORBIDDEN_SECTION,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &size, &diagnostic));

    for (index = 0u; index <= ZR_ARTIFACT_SIGNATURE_MAX_DEPTH; ++index)
        deepSignature[index] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE;
    deepSignature[index++] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE;
    write_u32(&deepSignature[index], 4u);
    index += 4u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                          ZrCore_Artifact_ValidateSignature(deepSignature, index, &diagnostic));

    countSignature[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_TUPLE;
    write_u32(&countSignature[1], ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT + 1u);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_COUNT_LIMIT,
                          ZrCore_Artifact_ValidateSignature(
                                  countSignature, sizeof(countSignature), &diagnostic));
}

static void test_member_property_and_relocation_contracts_validate_tokens_and_code_bounds(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactSectionInput sections[11];
    SZrArtifactMemberDefRow member;
    SZrArtifactPropertyDefRow property;
    SZrArtifactRelocationRow relocation;
    SZrArtifactDiagnostic diagnostic;
    TZrByte binary[4096];
    TZrSize length = 0u;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    memcpy(sections, fixture.sections, sizeof(fixture.sections));
    memset(&member, 0, sizeof(member));
    member.token = TEST_MEMBER_TOKEN;
    member.ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    member.signatureToken = TEST_SIGNATURE_TOKEN;
    member.signatureHash = TEST_SIGNATURE_HASH;
    member.contractHash = TEST_CONTRACT_HASH;
    memset(&property, 0, sizeof(property));
    property.token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 4u);
    property.ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    property.getterToken = TEST_MEMBER_TOKEN;
    property.signatureToken = TEST_SIGNATURE_TOKEN;
    property.signatureHash = TEST_SIGNATURE_HASH;
    property.contractHash = TEST_CONTRACT_HASH;
    memset(&relocation, 0, sizeof(relocation));
    relocation.codeOffset = 0u;
    relocation.kind = 1u;
    relocation.targetToken = TEST_MEMBER_TOKEN;
    relocation.targetSignatureToken = TEST_SIGNATURE_TOKEN;
    relocation.expectedSignatureHash = TEST_SIGNATURE_HASH;
    relocation.expectedContractHash = TEST_CONTRACT_HASH;
    relocation.expectedModuleHash = TEST_MODULE_HASH;
    sections[8] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &member};
    sections[9] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &property};
    sections[10] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &relocation};
    fixture.document.sections = sections;
    fixture.document.sectionCount = 11u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Write(&fixture.document,
                                                binary,
                                                sizeof(binary),
                                                &length,
                                                &diagnostic));

    property.getterToken = TEST_TYPE_DEF_TOKEN;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &length, &diagnostic));
    property.getterToken = TEST_MEMBER_TOKEN;
    relocation.codeOffset = 3u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                          ZrCore_Artifact_GetEncodedSize(&fixture.document, &length, &diagnostic));
}

static void test_source_canonical_type_and_binary_import_share_type_id_and_public_contract(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrSemanticContext *context;
    SZrString *moduleIdentity;
    SZrCanonicalParameterContract parameter;
    SZrParserArtifactPublicContract publicContract;
    SZrArtifactPublicIdentity sourceIdentity;
    SZrArtifactPublicIdentity importedIdentity;
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contractRow;
    SZrArtifactLayoutRow layoutRow;
    SZrArtifactSectionInput sections[7];
    SZrArtifactDocument document;
    SZrArtifactView view;
    SZrArtifactSectionView signatureSection;
    SZrArtifactDiagnostic diagnostic;
    TZrByte signature[512];
    TZrByte importedSignature[512];
    TZrByte binary[4096];
    TZrSize signatureLength = 0u;
    TZrSize importedSignatureLength = 0u;
    TZrSize binaryLength = 0u;
    TZrSize requiredSize = 0u;
    TZrTypeId intType;
    TZrTypeId nominalType;
    TZrTypeId readonlyRef;
    TZrTypeId readonlyView;
    TZrTypeId sharedOwner;
    TZrTypeId sourceTypeId;
    TZrTypeId importedTypeId = 0u;
    static const TZrByte code[] = {0x01u};

    TEST_ASSERT_NOT_NULL(state);
    context = ZrParser_SemanticContext_New(state);
    TEST_ASSERT_NOT_NULL(context);
    moduleIdentity = ZrCore_String_Create(state, "app.model", 9u);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    nominalType = ZrParser_CanonicalType_InternNominal(
            context,
            moduleIdentity,
            ZrCore_String_Create(state, "Box", 3u),
            TEST_TYPE_DEF_TOKEN);
    readonlyRef = ZrParser_CanonicalType_InternRef(
            context, nominalType, ZR_CANONICAL_REF_READONLY);
    readonlyView = ZrParser_CanonicalType_InternReadonlyView(context, readonlyRef);
    sharedOwner = ZrParser_CanonicalType_InternOwner(
            context, readonlyView, ZR_CANONICAL_OWNER_SHARED);

    memset(&parameter, 0, sizeof(parameter));
    parameter.typeId = sharedOwner;
    parameter.passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    parameter.callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;
    sourceTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            &parameter,
            1u,
            intType,
            ZR_CANONICAL_RECEIVER_READONLY,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, sourceTypeId);

    memset(&publicContract, 0, sizeof(publicContract));
    publicContract.typeRefToken = TEST_TYPE_REF_TOKEN;
    publicContract.typeSpecToken = TEST_TYPE_SPEC_TOKEN;
    publicContract.signatureToken = TEST_SIGNATURE_TOKEN;
    publicContract.layoutVersion = 7u;
    publicContract.layoutHash = TEST_LAYOUT_HASH;
    publicContract.callableContractHash = TEST_CONTRACT_HASH;
    publicContract.moduleHash = TEST_MODULE_HASH;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_BuildPublicIdentity(
                    context,
                    sourceTypeId,
                    &publicContract,
                    signature,
                    sizeof(signature),
                    &signatureLength,
                    &sourceIdentity,
                    &diagnostic));

    memset(&typeDef, 0, sizeof(typeDef));
    typeDef.token = TEST_TYPE_DEF_TOKEN;
    typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_VALUE |
                    ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE;
    typeDef.canonicalTypeId = nominalType;
    typeDef.constructorToken = TEST_CONSTRUCTOR_TOKEN;
    typeDef.constructorSignatureToken = TEST_SIGNATURE_TOKEN;
    typeDef.typeSignatureHash = sourceIdentity.signatureHash;
    typeDef.constructorContractHash = TEST_CONTRACT_HASH;

    memset(&typeRef, 0, sizeof(typeRef));
    typeRef.token = TEST_TYPE_REF_TOKEN;
    typeRef.signatureToken = TEST_SIGNATURE_TOKEN;
    typeRef.canonicalTypeId = sourceTypeId;
    typeRef.signatureLength = (TZrUInt32)signatureLength;
    typeRef.signatureHash = sourceIdentity.typeRefHash;
    typeRef.layoutVersion = 7u;
    typeRef.layoutHash = TEST_LAYOUT_HASH;
    typeSpec = typeRef;
    typeSpec.token = TEST_TYPE_SPEC_TOKEN;
    typeSpec.signatureHash = sourceIdentity.typeSpecHash;

    memset(&contractRow, 0, sizeof(contractRow));
    contractRow.memberToken = TEST_MEMBER_TOKEN;
    contractRow.signatureToken = TEST_SIGNATURE_TOKEN;
    contractRow.parameterCount = 1u;
    contractRow.flags = ZR_ARTIFACT_CONTRACT_FLAG_THROWS;
    contractRow.receiverEffect = ZR_ARTIFACT_RECEIVER_READONLY;
    contractRow.contractHash = TEST_CONTRACT_HASH;

    memset(&layoutRow, 0, sizeof(layoutRow));
    layoutRow.typeToken = TEST_TYPE_DEF_TOKEN;
    layoutRow.version = 7u;
    layoutRow.byteSize = 16u;
    layoutRow.byteAlignment = 8u;
    layoutRow.gcScanKind = ZR_ARTIFACT_GC_SCAN_FREE;
    layoutRow.layoutHash = TEST_LAYOUT_HASH;

    sections[0] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &typeDef};
    sections[1] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &typeRef};
    sections[2] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &typeSpec};
    sections[3] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)signatureLength, signature};
    sections[4] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &contractRow};
    sections[5] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &layoutRow};
    sections[6] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CODE_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(code), code};
    memset(&document, 0, sizeof(document));
    document.kind = ZR_ARTIFACT_KIND_ZRO;
    document.identity = sourceIdentity;
    document.sectionCount = 7u;
    document.sections = sections;

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_GetEncodedSize(&document, &requiredSize, &diagnostic));
    TEST_ASSERT_LESS_OR_EQUAL_UINT64((TZrUInt64)sizeof(binary), (TZrUInt64)requiredSize);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Write(&document,
                                                binary,
                                                sizeof(binary),
                                                &binaryLength,
                                                &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Read(binary, binaryLength, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_FindSection(&view,
                                                      ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                                                      &signatureSection,
                                                      &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrParser_ArtifactType_InternSignature(
                                  context,
                                  moduleIdentity,
                                  signatureSection.data,
                                  signatureSection.byteLength,
                                  &importedTypeId,
                                  &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(sourceTypeId, importedTypeId);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_BuildPublicIdentity(
                    context,
                    importedTypeId,
                    &publicContract,
                    importedSignature,
                    sizeof(importedSignature),
                    &importedSignatureLength,
                    &importedIdentity,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)signatureLength, (TZrUInt64)importedSignatureLength);
    TEST_ASSERT_EQUAL_MEMORY(signature, importedSignature, signatureLength);
    TEST_ASSERT_EQUAL_MEMORY(&sourceIdentity, &importedIdentity, sizeof(sourceIdentity));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_ValidatePublicIdentity(
                                  &view, &importedIdentity, &diagnostic));

    ZrParser_SemanticContext_Free(context);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_domain_transfer_contract_roundtrips_as_independent_artifact_schema(void) {
    SZrArtifactTestFixture fixture;
    SZrArtifactDomainTransferRow rows[5];
    SZrArtifactDomainTransferRow decoded;
    SZrArtifactSectionInput sections[9];
    SZrArtifactView view;
    SZrArtifactSectionView section;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[4096];
    TZrSize writtenSize = 0u;

    init_fixture(&fixture, ZR_ARTIFACT_KIND_ZRO);
    memset(rows, 0, sizeof(rows));
    rows[0].typeToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF, 1u);
    rows[0].kind = ZR_ARTIFACT_DOMAIN_TRANSFER_FORBIDDEN;
    rows[1].typeToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF, 2u);
    rows[1].kind = ZR_ARTIFACT_DOMAIN_TRANSFER_VALUE_COPY;
    rows[1].schemaVersion = 2u;
    rows[1].schemaHash = UINT64_C(0x0102030405060708);
    rows[2].typeToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF, 3u);
    rows[2].kind = ZR_ARTIFACT_DOMAIN_TRANSFER_STRUCTURED_CLONE;
    rows[2].schemaVersion = 3u;
    rows[2].flags = ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    rows[2].schemaOffset = 0u;
    rows[2].schemaLength = fixture.signatureLength;
    rows[2].schemaHash = UINT64_C(0x1020304050607080);
    rows[3].typeToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF, 4u);
    rows[3].kind = ZR_ARTIFACT_DOMAIN_TRANSFER_IMMUTABLE_HANDLE;
    rows[3].schemaVersion = 1u;
    rows[3].schemaHash = UINT64_C(0x1111222233334444);
    rows[3].providerToken = TEST_MEMBER_TOKEN;
    rows[3].providerContractHash = UINT64_C(0x5555666677778888);
    rows[4].typeToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF, 5u);
    rows[4].kind = ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE;
    rows[4].schemaVersion = 1u;
    rows[4].flags = ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    rows[4].schemaHash = UINT64_C(0x9999aaaabbbbcccc);
    rows[4].providerToken = TEST_CONSTRUCTOR_TOKEN;
    rows[4].providerContractHash = UINT64_C(0xddddeeeeffff0001);

    memcpy(sections,
           fixture.document.sections,
           fixture.document.sectionCount * sizeof(sections[0]));
    sections[fixture.document.sectionCount] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)ZR_ARRAY_COUNT(rows),
            rows};
    fixture.document.sections = sections;
    fixture.document.sectionCount++;

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
            ZrCore_Artifact_Read(buffer, writtenSize, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_FindSection(
                    &view,
                    ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
                    &section,
                    &diagnostic));
    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(rows); index++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_ARTIFACT_STATUS_OK,
                ZrCore_Artifact_ReadDomainTransferRow(
                        &section, index, &decoded, &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(rows[index].typeToken, decoded.typeToken);
        TEST_ASSERT_EQUAL_INT(rows[index].kind, decoded.kind);
        TEST_ASSERT_EQUAL_UINT32(
                rows[index].schemaVersion, decoded.schemaVersion);
        TEST_ASSERT_EQUAL_UINT32(rows[index].flags, decoded.flags);
        TEST_ASSERT_EQUAL_UINT32(
                rows[index].schemaOffset, decoded.schemaOffset);
        TEST_ASSERT_EQUAL_UINT32(
                rows[index].schemaLength, decoded.schemaLength);
        TEST_ASSERT_EQUAL_UINT64(rows[index].schemaHash, decoded.schemaHash);
        TEST_ASSERT_EQUAL_UINT32(
                rows[index].providerToken, decoded.providerToken);
        TEST_ASSERT_EQUAL_UINT64(
                rows[index].providerContractHash,
                decoded.providerContractHash);
    }

    rows[4].typeToken = rows[3].typeToken;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &writtenSize, &diagnostic));
    rows[4].typeToken = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_TYPE_DEF, 5u);
    rows[4].providerToken = 0u;
    rows[4].providerContractHash = 0u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &writtenSize, &diagnostic));
    rows[4].kind = ZR_ARTIFACT_DOMAIN_TRANSFER_VALUE_COPY;
    rows[4].schemaVersion = 1u;
    rows[4].schemaHash = UINT64_C(0x9999aaaabbbbcccc);
    rows[4].schemaOffset = fixture.signatureLength + 1u;
    rows[4].schemaLength = 0u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
            ZrCore_Artifact_GetEncodedSize(
                    &fixture.document, &writtenSize, &diagnostic));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zro_roundtrips_fixed_width_public_contract_sections);
    RUN_TEST(test_zrs_and_zri_have_readable_stable_roundtrips);
    RUN_TEST(test_public_identity_mismatches_are_precise);
    RUN_TEST(test_reader_rejects_unknown_mandatory_but_skips_unknown_optional_section);
    RUN_TEST(test_reader_rejects_truncation_count_limit_and_illegal_tokens);
    RUN_TEST(test_signature_nodes_validate_ref_readonly_owner_and_callable_contracts);
    RUN_TEST(test_repeat_encoding_and_text_roundtrip_keep_hashes_stable);
    RUN_TEST(test_internal_public_rows_report_the_exact_mismatch);
    RUN_TEST(test_zero_many_and_duplicate_signature_rows_roundtrip_safely);
    RUN_TEST(test_duplicate_forbidden_and_recursive_signature_inputs_are_rejected);
    RUN_TEST(test_member_property_and_relocation_contracts_validate_tokens_and_code_bounds);
    RUN_TEST(test_source_canonical_type_and_binary_import_share_type_id_and_public_contract);
    RUN_TEST(test_real_source_compile_and_binary_signature_import_are_identical);
    RUN_TEST(test_domain_transfer_contract_roundtrips_as_independent_artifact_schema);
    return UNITY_END();
}
