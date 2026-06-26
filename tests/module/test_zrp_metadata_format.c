#include <string.h>

#include "unity.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

void setUp(void) {}

void tearDown(void) {}

static void assert_section_equal(const SZrZrpMetadataSection *expected,
                                 const SZrZrpMetadataSection *actual) {
    TEST_ASSERT_NOT_NULL(expected);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_UINT32(expected->offset, actual->offset);
    TEST_ASSERT_EQUAL_UINT32(expected->byteLength, actual->byteLength);
    TEST_ASSERT_EQUAL_UINT32(expected->count, actual->count);
    TEST_ASSERT_EQUAL_UINT32(expected->elementSize, actual->elementSize);
}

static void set_counted_section(SZrZrpMetadataSection *section,
                                TZrUInt32 *nextOffset,
                                TZrUInt32 count,
                                TZrUInt32 elementSize) {
    TEST_ASSERT_NOT_NULL(section);
    TEST_ASSERT_NOT_NULL(nextOffset);

    section->offset = *nextOffset;
    section->count = count;
    section->elementSize = elementSize;
    section->byteLength = count * elementSize;
    *nextOffset += section->byteLength;
}

static void test_zrp_metadata_header_roundtrips_tables_and_pools(void) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 4096u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    TEST_ASSERT_EQUAL_UINT32(ZR_ZRP_METADATA_MAGIC, header.magic);
    TEST_ASSERT_EQUAL_UINT16(ZR_ZRP_METADATA_VERSION, header.version);
    TEST_ASSERT_EQUAL_UINT16(ZR_ZRP_METADATA_HEADER_SIZE, header.headerSize);
    TEST_ASSERT_EQUAL_UINT32(ZR_ZRP_METADATA_SECTION_COUNT, header.sectionCount);
    TEST_ASSERT_EQUAL_UINT32(12u, ZR_ZRP_METADATA_SECTION_COUNT);
    TEST_ASSERT_EQUAL_UINT32(208u, ZR_ZRP_METADATA_HEADER_SIZE);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_TYPE_DEFS, 1);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_METHOD_DEFS, 2);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_FIELD_DEFS, 3);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS, 4);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS, 5);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_TYPE_SPECS, 6);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_METHOD_SPECS, 7);
    TEST_ASSERT_EQUAL_INT(ZR_ZRP_METADATA_SECTION_MODULE_REFS, 8);

    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.tokenRecords,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrMetadataTokenRecord));
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.methodDefs,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    set_counted_section(&header.fieldDefs,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    set_counted_section(&header.genericParams,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow));
    set_counted_section(&header.genericParamConstraints,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow));
    set_counted_section(&header.typeSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_counted_section(&header.methodSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow));
    set_counted_section(&header.moduleRefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow));
    set_counted_section(&header.stringPool, &nextOffset, 32u, 1u);
    set_counted_section(&header.signatureBlobPool, &nextOffset, 16u, 1u);
    set_counted_section(&header.constantPool, &nextOffset, 16u, 1u);

    TEST_ASSERT_LESS_OR_EQUAL_UINT32((TZrUInt32)sizeof(bytes), nextOffset);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    memset(&decoded, 0, sizeof(decoded));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateHeader(&decoded, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(header.magic, decoded.magic);
    TEST_ASSERT_EQUAL_UINT16(header.version, decoded.version);
    TEST_ASSERT_EQUAL_UINT16(header.headerSize, decoded.headerSize);
    TEST_ASSERT_EQUAL_UINT32(header.flags, decoded.flags);
    TEST_ASSERT_EQUAL_UINT32(header.sectionCount, decoded.sectionCount);
    assert_section_equal(&header.tokenRecords, &decoded.tokenRecords);
    assert_section_equal(&header.typeDefs, &decoded.typeDefs);
    assert_section_equal(&header.methodDefs, &decoded.methodDefs);
    assert_section_equal(&header.fieldDefs, &decoded.fieldDefs);
    assert_section_equal(&header.genericParams, &decoded.genericParams);
    assert_section_equal(&header.genericParamConstraints, &decoded.genericParamConstraints);
    assert_section_equal(&header.typeSpecs, &decoded.typeSpecs);
    assert_section_equal(&header.methodSpecs, &decoded.methodSpecs);
    assert_section_equal(&header.moduleRefs, &decoded.moduleRefs);
    assert_section_equal(&header.stringPool, &decoded.stringPool);
    assert_section_equal(&header.signatureBlobPool, &decoded.signatureBlobPool);
    assert_section_equal(&header.constantPool, &decoded.constantPool);
}

static void test_zrp_metadata_header_rejects_wrong_definition_table_element_size(void) {
    SZrZrpMetadataHeader header;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 256u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    header.typeDefs.offset = ZR_ZRP_METADATA_HEADER_SIZE;
    header.typeDefs.count = 1u;
    header.typeDefs.elementSize = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow) - 1u;
    header.typeDefs.byteLength = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));

    ZrCore_ZrpMetadata_InitHeader(&header);
    header.methodDefs.offset = ZR_ZRP_METADATA_HEADER_SIZE;
    header.methodDefs.count = 1u;
    header.methodDefs.elementSize = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    header.methodDefs.byteLength = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow) - 1u;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));
}

static void test_zrp_metadata_section_view_resolves_mmap_payloads(void) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataSectionView view;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 512u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.stringPool, &nextOffset, 16u, 1u);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    bytes[header.typeDefs.offset] = 0xA5u;
    bytes[header.stringPool.offset] = 'z';
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));

    memset(&view, 0, sizeof(view));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                       sizeof(bytes),
                                                       &decoded,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                       &view));
    TEST_ASSERT_EQUAL_PTR(&decoded.typeDefs, view.section);
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.typeDefs.offset, view.data);
    TEST_ASSERT_EQUAL_UINT32(decoded.typeDefs.byteLength, (TZrUInt32)view.byteLength);
    TEST_ASSERT_EQUAL_UINT32(decoded.typeDefs.count, view.count);
    TEST_ASSERT_EQUAL_UINT32(decoded.typeDefs.elementSize, view.elementSize);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, view.data[0]);

    memset(&view, 0, sizeof(view));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                       sizeof(bytes),
                                                       &decoded,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &view));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.stringPool.offset, view.data);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)'z', view.data[0]);

    memset(&view, 0xFF, sizeof(view));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                       sizeof(bytes),
                                                       &decoded,
                                                       ZR_ZRP_METADATA_SECTION_CONSTANT_POOL,
                                                       &view));
    TEST_ASSERT_EQUAL_PTR(&decoded.constantPool, view.section);
    TEST_ASSERT_NULL(view.data);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)view.byteLength);
    TEST_ASSERT_EQUAL_UINT32(0u, view.count);
    TEST_ASSERT_EQUAL_UINT32(0u, view.elementSize);

    memset(&view, 0xFF, sizeof(view));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                        decoded.stringPool.offset + 1u,
                                                        &decoded,
                                                        ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                        &view));
    TEST_ASSERT_NULL(view.data);

    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                        sizeof(bytes),
                                                        &decoded,
                                                        (EZrZrpMetadataSectionKind)99,
                                                        &view));
}

static void test_zrp_metadata_pool_slice_resolves_bounded_payload_ranges(void) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataPoolSliceView slice;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 128u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.stringPool, &nextOffset, 16u, 1u);
    set_counted_section(&header.signatureBlobPool, &nextOffset, 8u, 1u);
    set_counted_section(&header.constantPool, &nextOffset, 8u, 1u);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    bytes[header.stringPool.offset + 2u] = 'r';
    bytes[header.signatureBlobPool.offset + 1u] = 0xC1u;
    bytes[header.constantPool.offset + 4u] = 0x7Eu;
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));

    memset(&slice, 0, sizeof(slice));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                     sizeof(bytes),
                                                     &decoded,
                                                     ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                     2u,
                                                     4u,
                                                     &slice));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.stringPool.offset + 2u, slice.data);
    TEST_ASSERT_EQUAL_UINT32(4u, (TZrUInt32)slice.byteLength);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)'r', slice.data[0]);

    memset(&slice, 0, sizeof(slice));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                     sizeof(bytes),
                                                     &decoded,
                                                     ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                     1u,
                                                     2u,
                                                     &slice));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.signatureBlobPool.offset + 1u, slice.data);
    TEST_ASSERT_EQUAL_UINT8(0xC1u, slice.data[0]);

    memset(&slice, 0, sizeof(slice));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                     sizeof(bytes),
                                                     &decoded,
                                                     ZR_ZRP_METADATA_SECTION_CONSTANT_POOL,
                                                     4u,
                                                     1u,
                                                     &slice));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.constantPool.offset + 4u, slice.data);
    TEST_ASSERT_EQUAL_UINT8(0x7Eu, slice.data[0]);

    memset(&slice, 0xFF, sizeof(slice));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                     sizeof(bytes),
                                                     &decoded,
                                                     ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                     16u,
                                                     0u,
                                                     &slice));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.stringPool.offset + 16u, slice.data);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)slice.byteLength);

    memset(&slice, 0xFF, sizeof(slice));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                      sizeof(bytes),
                                                      &decoded,
                                                      ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                      0u,
                                                      1u,
                                                      &slice));
    TEST_ASSERT_NULL(slice.data);

    memset(&slice, 0xFF, sizeof(slice));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                      sizeof(bytes),
                                                      &decoded,
                                                      ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                      15u,
                                                      2u,
                                                      &slice));
    TEST_ASSERT_NULL(slice.data);
}

static void test_zrp_metadata_writes_pool_payloads_into_buffer(void) {
    static const TZrByte stringPayload[] = {'m', 'o', 'd', 0u};
    static const TZrByte signaturePayload[] = {0x11u, 0x22u, 0x33u};
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataPoolSliceView slice;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 64u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.stringPool,
                        &nextOffset,
                        (TZrUInt32)sizeof(stringPayload),
                        1u);
    set_counted_section(&header.signatureBlobPool,
                        &nextOffset,
                        (TZrUInt32)sizeof(signaturePayload),
                        1u);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                         sizeof(bytes),
                                                         &header,
                                                         ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                         stringPayload,
                                                         (TZrUInt32)sizeof(stringPayload)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                         sizeof(bytes),
                                                         &header,
                                                         ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                         signaturePayload,
                                                         (TZrUInt32)sizeof(signaturePayload)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                         sizeof(bytes),
                                                         &header,
                                                         ZR_ZRP_METADATA_SECTION_CONSTANT_POOL,
                                                         ZR_NULL,
                                                         0u));

    TEST_ASSERT_EQUAL_INT(0,
                          memcmp(stringPayload,
                                 bytes + header.stringPool.offset,
                                 sizeof(stringPayload)));
    TEST_ASSERT_EQUAL_INT(0,
                          memcmp(signaturePayload,
                                 bytes + header.signatureBlobPool.offset,
                                 sizeof(signaturePayload)));

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));
    memset(&slice, 0, sizeof(slice));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetPoolSlice(bytes,
                                                     sizeof(bytes),
                                                     &decoded,
                                                     ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                     0u,
                                                     (TZrUInt32)sizeof(stringPayload),
                                                     &slice));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.stringPool.offset, slice.data);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(stringPayload), (TZrUInt32)slice.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(stringPayload, slice.data, sizeof(stringPayload)));

    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                          sizeof(bytes),
                                                          &decoded,
                                                          ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                          stringPayload,
                                                          (TZrUInt32)sizeof(stringPayload)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                          sizeof(bytes),
                                                          &decoded,
                                                          ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                          ZR_NULL,
                                                          (TZrUInt32)sizeof(stringPayload)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                          sizeof(bytes),
                                                          &decoded,
                                                          ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                          stringPayload,
                                                          (TZrUInt32)sizeof(stringPayload) - 1u));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                          decoded.signatureBlobPool.offset + 1u,
                                                          &decoded,
                                                          ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                          signaturePayload,
                                                          (TZrUInt32)sizeof(signaturePayload)));
}

static void test_zrp_metadata_writes_definition_table_payloads_into_buffer(void) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataSectionView view;
    SZrZrpMetadataTypeDefRow typeRows[1] = {{0}};
    SZrZrpMetadataMethodDefRow methodRows[1] = {{0}};
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 512u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.methodDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));

    typeRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    typeRows[0].firstMethodDefIndex = 0u;
    typeRows[0].methodDefCount = 1u;
    methodRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    methodRows[0].ownerTypeToken = typeRows[0].token;

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &header,
            ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
            typeRows,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &header,
            ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
            methodRows,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)));

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));
    memset(&view, 0, sizeof(view));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                       sizeof(bytes),
                                                       &decoded,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                       &view));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.typeDefs.offset, view.data);
    TEST_ASSERT_EQUAL_INT(0, memcmp(typeRows, view.data, sizeof(typeRows)));

    memset(&view, 0, sizeof(view));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(bytes,
                                                       sizeof(bytes),
                                                       &decoded,
                                                       ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
                                                       &view));
    TEST_ASSERT_EQUAL_PTR(bytes + decoded.methodDefs.offset, view.data);
    TEST_ASSERT_EQUAL_INT(0, memcmp(methodRows, view.data, sizeof(methodRows)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));

    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &decoded,
            ZR_ZRP_METADATA_SECTION_STRING_POOL,
            typeRows,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &decoded,
            ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
            ZR_NULL,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &decoded,
            ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
            typeRows,
            2u,
            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &decoded,
            ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
            typeRows,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow) - 1u));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            decoded.methodDefs.offset + 1u,
            &decoded,
            ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
            methodRows,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)));

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteDefinitionTablePayload(
            bytes,
            sizeof(bytes),
            &decoded,
            ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
            ZR_NULL,
            0u,
            (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow)));
}

static void test_zrp_metadata_resolves_string_pool_entries(void) {
    static const TZrByte stringPayload[] = {'Z', 'r', 0u, 'V', 'M', 0u, 0u};
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataStringView stringView;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 64u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.stringPool,
                        &nextOffset,
                        (TZrUInt32)sizeof(stringPayload),
                        1u);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                         sizeof(bytes),
                                                         &header,
                                                         ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                         stringPayload,
                                                         (TZrUInt32)sizeof(stringPayload)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));

    memset(&stringView, 0, sizeof(stringView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetString(bytes, sizeof(bytes), &decoded, 0u, &stringView));
    TEST_ASSERT_EQUAL_PTR((const char *)(const void *)(bytes + decoded.stringPool.offset), stringView.data);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)stringView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp("Zr", stringView.data, stringView.byteLength));

    memset(&stringView, 0, sizeof(stringView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetString(bytes, sizeof(bytes), &decoded, 3u, &stringView));
    TEST_ASSERT_EQUAL_PTR((const char *)(const void *)(bytes + decoded.stringPool.offset + 3u), stringView.data);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)stringView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp("VM", stringView.data, stringView.byteLength));

    memset(&stringView, 0, sizeof(stringView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetString(bytes, sizeof(bytes), &decoded, 6u, &stringView));
    TEST_ASSERT_EQUAL_PTR((const char *)(const void *)(bytes + decoded.stringPool.offset + 6u), stringView.data);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)stringView.byteLength);

    memset(&stringView, 0xFF, sizeof(stringView));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_GetString(bytes,
                                                   sizeof(bytes),
                                                   &decoded,
                                                   (TZrUInt32)sizeof(stringPayload),
                                                   &stringView));
    TEST_ASSERT_NULL(stringView.data);

    bytes[decoded.stringPool.offset + 6u] = 'x';
    memset(&stringView, 0xFF, sizeof(stringView));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_GetString(bytes, sizeof(bytes), &decoded, 6u, &stringView));
    TEST_ASSERT_NULL(stringView.data);
}

static void test_zrp_metadata_validates_signature_blob_structure(void) {
    static const TZrByte methodSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
            1u,
            0u,
            0u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte fieldSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_BOOL, 0u, 0u, 0u,
    };
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte methodSignatureWithTrailingByte[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_BOOL, 0u, 0u, 0u,
            0xEEu,
    };
    static const TZrByte truncatedSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
            1u,
            0u,
            0u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64,
    };
    static const TZrByte invalidNodeSignature[] = {0xFFu};

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(methodSignature, sizeof(methodSignature)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(fieldSignature, sizeof(fieldSignature)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(genericInstanceSignature,
                                                              sizeof(genericInstanceSignature)));

    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateSignatureBlob(ZR_NULL, sizeof(methodSignature)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateSignatureBlob(methodSignature, 0u));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateSignatureBlob(methodSignatureWithTrailingByte,
                                                               sizeof(methodSignatureWithTrailingByte)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateSignatureBlob(truncatedSignature, sizeof(truncatedSignature)));
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateSignatureBlob(invalidNodeSignature, sizeof(invalidNodeSignature)));
}

static void test_zrp_metadata_definition_table_rows_validate_token_tags(void) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataMethodDefRow *methodRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrZrpMetadataGenericParamRow *genericParamRows;
    SZrZrpMetadataGenericParamConstraintRow *constraintRows;
    SZrZrpMetadataTypeSpecRow *typeSpecRows;
    SZrZrpMetadataMethodSpecRow *methodSpecRows;
    SZrZrpMetadataModuleRefRow *moduleRefRows;
    TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 1024u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.methodDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    set_counted_section(&header.fieldDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    set_counted_section(&header.genericParams,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow));
    set_counted_section(&header.genericParamConstraints,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow));
    set_counted_section(&header.typeSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_counted_section(&header.methodSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow));
    set_counted_section(&header.moduleRefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow));

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    methodRows = (SZrZrpMetadataMethodDefRow *)(void *)(bytes + header.methodDefs.offset);
    fieldRows = (SZrZrpMetadataFieldDefRow *)(void *)(bytes + header.fieldDefs.offset);
    genericParamRows = (SZrZrpMetadataGenericParamRow *)(void *)(bytes + header.genericParams.offset);
    constraintRows =
            (SZrZrpMetadataGenericParamConstraintRow *)(void *)(bytes + header.genericParamConstraints.offset);
    typeSpecRows = (SZrZrpMetadataTypeSpecRow *)(void *)(bytes + header.typeSpecs.offset);
    methodSpecRows = (SZrZrpMetadataMethodSpecRow *)(void *)(bytes + header.methodSpecs.offset);
    moduleRefRows = (SZrZrpMetadataModuleRefRow *)(void *)(bytes + header.moduleRefs.offset);

    typeRows[0].token = typeToken;
    methodRows[0].token = methodToken;
    methodRows[0].ownerTypeToken = typeToken;
    fieldRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    fieldRows[0].ownerTypeToken = typeToken;
    genericParamRows[0].ownerToken = typeToken;
    constraintRows[0].genericParamIndex = 0u;
    constraintRows[0].constraintTypeToken = typeToken;
    typeSpecRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    methodSpecRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    methodSpecRows[0].methodToken = methodToken;
    moduleRefRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));

    typeRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));
    typeRows[0].token = typeToken;

    methodRows[0].ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));
    methodRows[0].ownerTypeToken = typeToken;

    methodSpecRows[0].methodToken = typeToken;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));
}

static void test_zrp_metadata_definition_table_rows_validate_cross_table_ranges(void) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataHeader decoded;
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataMethodDefRow *methodRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrZrpMetadataGenericParamRow *genericParamRows;
    SZrZrpMetadataGenericParamConstraintRow *constraintRows;
    TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + 512u] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.methodDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    set_counted_section(&header.fieldDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    set_counted_section(&header.genericParams,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow));
    set_counted_section(&header.genericParamConstraints,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow));

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    methodRows = (SZrZrpMetadataMethodDefRow *)(void *)(bytes + header.methodDefs.offset);
    fieldRows = (SZrZrpMetadataFieldDefRow *)(void *)(bytes + header.fieldDefs.offset);
    genericParamRows = (SZrZrpMetadataGenericParamRow *)(void *)(bytes + header.genericParams.offset);
    constraintRows =
            (SZrZrpMetadataGenericParamConstraintRow *)(void *)(bytes + header.genericParamConstraints.offset);

    typeRows[0].token = typeToken;
    typeRows[0].firstMethodDefIndex = 0u;
    typeRows[0].methodDefCount = 1u;
    typeRows[0].firstFieldDefIndex = 0u;
    typeRows[0].fieldDefCount = 1u;
    typeRows[0].firstGenericParamIndex = 0u;
    typeRows[0].genericParamCount = 1u;
    methodRows[0].token = methodToken;
    methodRows[0].ownerTypeToken = typeToken;
    fieldRows[0].token = fieldToken;
    fieldRows[0].ownerTypeToken = typeToken;
    genericParamRows[0].ownerToken = typeToken;
    constraintRows[0].genericParamIndex = 0u;
    constraintRows[0].constraintTypeToken = typeToken;

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &decoded));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));

    methodRows[0].ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));
    methodRows[0].ownerTypeToken = typeToken;

    typeRows[0].firstMethodDefIndex = 1u;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));
    typeRows[0].firstMethodDefIndex = 0u;

    constraintRows[0].genericParamIndex = 1u;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateDefinitionTables(bytes, sizeof(bytes), &decoded));
}

static void test_zrp_metadata_header_rejects_invalid_mmap_view(void) {
    SZrZrpMetadataHeader header;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    bytes[0] ^= 0xFFu;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ReadHeader(bytes, sizeof(bytes), &header));

    ZrCore_ZrpMetadata_InitHeader(&header);
    header.version = (TZrUInt16)(ZR_ZRP_METADATA_VERSION + 1u);
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));

    ZrCore_ZrpMetadata_InitHeader(&header);
    header.headerSize = (TZrUInt16)(ZR_ZRP_METADATA_HEADER_SIZE - 1u);
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));

    ZrCore_ZrpMetadata_InitHeader(&header);
    header.tokenRecords.offset = ZR_ZRP_METADATA_HEADER_SIZE - 1u;
    header.tokenRecords.byteLength = 1u;
    header.tokenRecords.count = 1u;
    header.tokenRecords.elementSize = 1u;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));

    ZrCore_ZrpMetadata_InitHeader(&header);
    header.tokenRecords.offset = ZR_ZRP_METADATA_HEADER_SIZE;
    header.tokenRecords.byteLength = 1u;
    header.tokenRecords.count = 1u;
    header.tokenRecords.elementSize = 0u;
    TEST_ASSERT_FALSE(ZrCore_ZrpMetadata_ValidateHeader(&header, sizeof(bytes)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zrp_metadata_header_roundtrips_tables_and_pools);
    RUN_TEST(test_zrp_metadata_header_rejects_wrong_definition_table_element_size);
    RUN_TEST(test_zrp_metadata_section_view_resolves_mmap_payloads);
    RUN_TEST(test_zrp_metadata_pool_slice_resolves_bounded_payload_ranges);
    RUN_TEST(test_zrp_metadata_writes_pool_payloads_into_buffer);
    RUN_TEST(test_zrp_metadata_writes_definition_table_payloads_into_buffer);
    RUN_TEST(test_zrp_metadata_resolves_string_pool_entries);
    RUN_TEST(test_zrp_metadata_validates_signature_blob_structure);
    RUN_TEST(test_zrp_metadata_definition_table_rows_validate_token_tags);
    RUN_TEST(test_zrp_metadata_definition_table_rows_validate_cross_table_ranges);
    RUN_TEST(test_zrp_metadata_header_rejects_invalid_mmap_view);
    return UNITY_END();
}
