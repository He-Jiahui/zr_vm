#include <string.h>

#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/zrp_metadata.h"

#define TEST_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_TYPE_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define TEST_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u)

void setUp(void) {}
void tearDown(void) {}

static void set_counted_section(SZrZrpMetadataSection *section,
                                TZrUInt32 *nextOffset,
                                TZrUInt32 count,
                                TZrUInt32 elementSize) {
    section->offset = *nextOffset;
    section->count = count;
    section->elementSize = elementSize;
    section->byteLength = count * elementSize;
    *nextOffset += section->byteLength;
}

static void populate_typedef_typespec_records(SZrFunction *metadataFunction,
                                              SZrMetadataTokenRecord *records,
                                              TZrUInt64 typeSpecSignatureHash,
                                              TZrUInt32 typeSpecSignatureLength,
                                              TZrUInt32 baseSignatureOffset,
                                              TZrUInt32 baseSignatureLength) {
    records[0].token = TEST_TYPE_SPEC_TOKEN;
    records[0].relatedToken = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    records[0].signatureHash = typeSpecSignatureHash;
    records[1].token = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_TYPE_SPEC_TOKEN;
    records[1].ownerToken = TEST_TYPE_SPEC_TOKEN;
    records[1].signatureBlobOffset = 0u;
    records[1].signatureBlobLength = typeSpecSignatureLength;
    records[1].signatureHash = records[0].signatureHash;
    records[2].token = TEST_TYPE_DEF_TOKEN;
    records[2].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    records[2].signatureHash = 0x2233445566778899ULL;
    records[3].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    records[3].relatedToken = TEST_TYPE_DEF_TOKEN;
    records[3].ownerToken = TEST_TYPE_DEF_TOKEN;
    records[3].signatureBlobOffset = baseSignatureOffset;
    records[3].signatureBlobLength = baseSignatureLength;
    records[3].signatureHash = records[2].signatureHash;

    metadataFunction->metadataTokenRecords = records;
    metadataFunction->metadataTokenRecordLength = 4u;
}

static void write_typespec_layout_metadata(TZrByte *bytes,
                                           TZrSize byteLength,
                                           const TZrByte *signaturePayload,
                                           TZrUInt32 typeSpecSignatureLength,
                                           TZrUInt32 signaturePayloadLength,
                                           TZrUInt64 signatureHash,
                                           TZrUInt32 typeLayoutId) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeSpecRow *typeSpecRows;
    TZrUInt32 nextOffset;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_counted_section(&header.signatureBlobPool, &nextOffset, signaturePayloadLength, 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, byteLength, &header));

    typeSpecRows = (SZrZrpMetadataTypeSpecRow *)(void *)(bytes + header.typeSpecs.offset);
    typeSpecRows[0].token = TEST_TYPE_SPEC_TOKEN;
    typeSpecRows[0].signatureBlobOffset = 0u;
    typeSpecRows[0].signatureBlobLength = typeSpecSignatureLength;
    typeSpecRows[0].typeLayoutId = typeLayoutId;
    typeSpecRows[0].signatureHash = signatureHash;

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        byteLength,
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        signaturePayloadLength));
}

static void write_typedef_layout_metadata(TZrByte *bytes,
                                          TZrSize byteLength,
                                          TZrUInt32 typeLayoutId) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefRows;
    TZrUInt32 nextOffset;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, byteLength, &header));

    typeDefRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    typeDefRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeDefRows[0].typeLayoutId = typeLayoutId;
}

static void write_typedef_typespec_layout_metadata(TZrByte *bytes,
                                                   TZrSize byteLength,
                                                   const TZrByte *signaturePayload,
                                                   TZrUInt32 typeSpecSignatureLength,
                                                   TZrUInt32 signaturePayloadLength,
                                                   TZrUInt64 signatureHash,
                                                   TZrUInt32 typeDefLayoutId,
                                                   TZrUInt32 typeSpecLayoutId) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefRows;
    SZrZrpMetadataTypeSpecRow *typeSpecRows;
    TZrUInt32 nextOffset;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.typeSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_counted_section(&header.signatureBlobPool, &nextOffset, signaturePayloadLength, 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, byteLength, &header));

    typeDefRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    typeDefRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeDefRows[0].typeLayoutId = typeDefLayoutId;

    typeSpecRows = (SZrZrpMetadataTypeSpecRow *)(void *)(bytes + header.typeSpecs.offset);
    typeSpecRows[0].token = TEST_TYPE_SPEC_TOKEN;
    typeSpecRows[0].signatureBlobOffset = 0u;
    typeSpecRows[0].signatureBlobLength = typeSpecSignatureLength;
    typeSpecRows[0].typeLayoutId = typeSpecLayoutId;
    typeSpecRows[0].signatureHash = signatureHash;

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        byteLength,
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        signaturePayloadLength));
}

static void populate_typedef_record(SZrFunction *metadataFunction, SZrMetadataTokenRecord *record) {
    record->token = TEST_TYPE_DEF_TOKEN;
    record->layoutVersion = 7u;
    record->layoutHash = 0x445566778899AABBULL;
    metadataFunction->metadataTokenRecords = record;
    metadataFunction->metadataTokenRecordLength = 1u;
}

static void test_metadata_runtime_reads_typespec_layout_binding_view(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeSpecLayoutBindingView view;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    typeSpecLayout.cTypeId = 42u;
    typeSpecLayout.byteSize = 80u;
    registeredLayouts[42] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    write_typespec_layout_metadata(bytes,
                                   sizeof(bytes),
                                   signaturePayload,
                                   (TZrUInt32)sizeof(genericInstanceSignature),
                                   (TZrUInt32)sizeof(signaturePayload),
                                   records[0].signatureHash,
                                   42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
            ZR_NULL, TEST_TYPE_SPEC_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
            runtime, TEST_TYPE_SPEC_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
            runtime, TEST_TYPE_DEF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
            runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
            runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, view.typeSpecToken);
    TEST_ASSERT_EQUAL_PTR(&records[0], view.typeRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, view.typeSpecRow->token);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, view.genericBindingView.baseToken);
    TEST_ASSERT_EQUAL_PTR(&records[2], view.genericBindingView.baseRecord);
    TEST_ASSERT_EQUAL_UINT32(42u, view.typeLayoutId);
    TEST_ASSERT_EQUAL_UINT32(42u, view.cTypeId);
    TEST_ASSERT_EQUAL_UINT64(records[0].signatureHash, view.signatureHash);
    TEST_ASSERT_EQUAL_PTR(&typeSpecLayout, view.typeLayout);
    TEST_ASSERT_EQUAL_UINT32(80u, view.typeLayout->byteSize);
}

static void test_metadata_runtime_typespec_layout_binding_does_not_fallback_to_prototype_cache(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeSpecLayoutBindingView view;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 128u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;
    write_typespec_layout_metadata(bytes,
                                   sizeof(bytes),
                                   signaturePayload,
                                   (TZrUInt32)sizeof(genericInstanceSignature),
                                   (TZrUInt32)sizeof(signaturePayload),
                                   records[0].signatureHash,
                                   42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
            runtime, TEST_TYPE_SPEC_TOKEN, &view));
}

static void test_metadata_runtime_resolves_typedef_token_layout_with_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord record = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    const SZrTypeLayout *resolvedLayout;
    SZrMetadataRuntime *runtime;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    populate_typedef_record(&metadataFunction, &record);
    typeDefLayout.cTypeId = 42u;
    typeDefLayout.byteSize = 32u;
    registeredLayouts[42] = &typeDefLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    write_typedef_layout_metadata(bytes, sizeof(bytes), 42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeTokenLayout(ZR_NULL,
                                                                   TEST_TYPE_DEF_TOKEN,
                                                                   &typeLayoutId));
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, typeLayoutId);
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime,
                                                                   TEST_TYPE_SPEC_SIGNATURE_TOKEN,
                                                                   &typeLayoutId));
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, typeLayoutId);

    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_DEF_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeDefLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
    TEST_ASSERT_EQUAL_UINT32(32u, resolvedLayout->byteSize);

    registeredLayouts[42] = ZR_NULL;
    typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_DEF_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeDefLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
}

static void test_metadata_runtime_type_token_layout_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord record = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    TZrUInt32 typeLayoutId = 42u;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    populate_typedef_record(&metadataFunction, &record);
    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 96u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;
    write_typedef_layout_metadata(bytes, sizeof(bytes), 42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_DEF_TOKEN, &typeLayoutId));
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, typeLayoutId);
}

static void test_metadata_runtime_resolves_typedef_layout_id_token_with_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord record = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    populate_typedef_record(&metadataFunction, &record);
    typeDefLayout.cTypeId = 42u;
    registeredLayouts[42] = &typeDefLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    write_typedef_layout_metadata(bytes, sizeof(bytes), 42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveTypeLayoutToken(ZR_NULL, 42u));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveTypeLayoutToken(
            runtime,
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));

    registeredLayouts[42] = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));
}

static void test_metadata_runtime_layout_id_token_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord record = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    populate_typedef_record(&metadataFunction, &record);
    stalePrototypeLayouts[42].cTypeId = 42u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;
    write_typedef_layout_metadata(bytes, sizeof(bytes), 42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));
}

static void test_metadata_runtime_resolves_typespec_token_layout_with_cache(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    const SZrTypeLayout *resolvedLayout;
    SZrMetadataRuntime *runtime;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    typeSpecLayout.cTypeId = 42u;
    typeSpecLayout.byteSize = 80u;
    registeredLayouts[42] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    write_typespec_layout_metadata(bytes,
                                   sizeof(bytes),
                                   signaturePayload,
                                   (TZrUInt32)sizeof(genericInstanceSignature),
                                   (TZrUInt32)sizeof(signaturePayload),
                                   records[0].signatureHash,
                                   42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_SPEC_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeSpecLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
    TEST_ASSERT_EQUAL_UINT32(80u, resolvedLayout->byteSize);

    registeredLayouts[42] = ZR_NULL;
    typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_SPEC_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeSpecLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
}

static void test_metadata_runtime_resolves_typespec_layout_id_token_with_cache(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    typeSpecLayout.cTypeId = 42u;
    registeredLayouts[42] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    write_typespec_layout_metadata(bytes,
                                   sizeof(bytes),
                                   signaturePayload,
                                   (TZrUInt32)sizeof(genericInstanceSignature),
                                   (TZrUInt32)sizeof(signaturePayload),
                                   records[0].signatureHash,
                                   42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));

    registeredLayouts[42] = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));
}

static void test_metadata_runtime_type_layout_cache_keeps_multiple_token_entries(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    const SZrTypeLayout *resolvedLayout;
    SZrMetadataRuntime *runtime;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeDefRow) +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    typeDefLayout.cTypeId = 42u;
    typeDefLayout.byteSize = 32u;
    typeSpecLayout.cTypeId = 43u;
    typeSpecLayout.byteSize = 80u;
    registeredLayouts[42] = &typeDefLayout;
    registeredLayouts[43] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;
    write_typedef_typespec_layout_metadata(bytes,
                                           sizeof(bytes),
                                           signaturePayload,
                                           (TZrUInt32)sizeof(genericInstanceSignature),
                                           (TZrUInt32)sizeof(signaturePayload),
                                           records[0].signatureHash,
                                           42u,
                                           43u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_DEF_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeDefLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_SPEC_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeSpecLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(43u, typeLayoutId);

    registeredLayouts[42] = ZR_NULL;
    registeredLayouts[43] = ZR_NULL;
    typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_DEF_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeDefLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, TEST_TYPE_SPEC_TOKEN, &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeSpecLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(43u, typeLayoutId);
}

static void test_metadata_runtime_type_layout_cache_keeps_multiple_reverse_entries(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    SZrMetadataRuntime *runtime;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeDefRow) +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    typeDefLayout.cTypeId = 42u;
    typeSpecLayout.cTypeId = 43u;
    registeredLayouts[42] = &typeDefLayout;
    registeredLayouts[43] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;
    write_typedef_typespec_layout_metadata(bytes,
                                           sizeof(bytes),
                                           signaturePayload,
                                           (TZrUInt32)sizeof(genericInstanceSignature),
                                           (TZrUInt32)sizeof(signaturePayload),
                                           records[0].signatureHash,
                                           42u,
                                           43u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 43u));

    registeredLayouts[42] = ZR_NULL;
    registeredLayouts[43] = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 42u));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN,
                             ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, 43u));
}

static void test_metadata_runtime_resolves_ctype_id_tokens_with_multi_entry_cache(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord records[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    SZrMetadataRuntime *runtime;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeDefRow) +
                  sizeof(SZrZrpMetadataTypeSpecRow) +
                  sizeof(signaturePayload)] = {0};

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));
    populate_typedef_typespec_records(&metadataFunction,
                                      records,
                                      0x123456789ABCDEF0ULL,
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(genericInstanceSignature),
                                      (TZrUInt32)sizeof(baseTypeDefSignature));
    typeDefLayout.cTypeId = 42u;
    typeSpecLayout.cTypeId = 43u;
    registeredLayouts[42] = &typeDefLayout;
    registeredLayouts[43] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;
    write_typedef_typespec_layout_metadata(bytes,
                                           sizeof(bytes),
                                           signaturePayload,
                                           (TZrUInt32)sizeof(genericInstanceSignature),
                                           (TZrUInt32)sizeof(signaturePayload),
                                           records[0].signatureHash,
                                           42u,
                                           43u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveCTypeIdToken(ZR_NULL, 42u));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveCTypeIdToken(
            runtime,
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN,
                             ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 43u));

    registeredLayouts[42] = ZR_NULL;
    registeredLayouts[43] = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN,
                             ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 43u));
}

static void test_metadata_runtime_ctype_id_token_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord record = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    populate_typedef_record(&metadataFunction, &record);
    stalePrototypeLayouts[42].cTypeId = 42u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;
    write_typedef_layout_metadata(bytes, sizeof(bytes), 42u);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));
}

static void test_metadata_runtime_resolves_ctype_id_token_from_code_registration_table(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    TZrUInt32 typeLayoutTokens[43] = {0};
    SZrMetadataRuntime *runtime;
    const SZrTypeLayout *resolvedLayout;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;

    typeDefLayout.cTypeId = 42u;
    typeDefLayout.byteSize = 32u;
    registeredLayouts[42] = &typeDefLayout;
    typeLayoutTokens[42] = TEST_TYPE_DEF_TOKEN;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    registration.typeLayoutTokens = typeLayoutTokens;
    registration.typeLayoutTokenCount = 43u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_UINT32(43u, runtime->typeLayoutTokenCount);

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));
    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime,
                                                                  TEST_TYPE_DEF_TOKEN,
                                                                  &typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&typeDefLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);

    registeredLayouts[42] = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN,
                             ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));
}

static void test_metadata_runtime_ctype_id_token_table_requires_type_token_and_layout(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeDefLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    TZrUInt32 typeLayoutTokens[43] = {0};
    SZrMetadataRuntime *runtime;

    typeDefLayout.cTypeId = 42u;
    registeredLayouts[42] = &typeDefLayout;
    typeLayoutTokens[42] = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;
    registration.typeLayoutTokens = typeLayoutTokens;
    registration.typeLayoutTokenCount = 43u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));

    typeLayoutTokens[42] = TEST_TYPE_DEF_TOKEN;
    registeredLayouts[42] = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 42u));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, 43u));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_metadata_runtime_reads_typespec_layout_binding_view);
    RUN_TEST(test_metadata_runtime_typespec_layout_binding_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_resolves_typedef_token_layout_with_cache);
    RUN_TEST(test_metadata_runtime_type_token_layout_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_resolves_typedef_layout_id_token_with_cache);
    RUN_TEST(test_metadata_runtime_layout_id_token_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_resolves_typespec_token_layout_with_cache);
    RUN_TEST(test_metadata_runtime_resolves_typespec_layout_id_token_with_cache);
    RUN_TEST(test_metadata_runtime_type_layout_cache_keeps_multiple_token_entries);
    RUN_TEST(test_metadata_runtime_type_layout_cache_keeps_multiple_reverse_entries);
    RUN_TEST(test_metadata_runtime_resolves_ctype_id_tokens_with_multi_entry_cache);
    RUN_TEST(test_metadata_runtime_ctype_id_token_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_resolves_ctype_id_token_from_code_registration_table);
    RUN_TEST(test_metadata_runtime_ctype_id_token_table_requires_type_token_and_layout);
    return UNITY_END();
}
