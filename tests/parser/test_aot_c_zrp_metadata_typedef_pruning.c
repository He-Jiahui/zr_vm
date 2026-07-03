#include "unity.h"

#include "backend_aot_c_zrp_metadata_prune.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void set_section(SZrZrpMetadataSection *section,
                        TZrUInt32 *offset,
                        TZrUInt32 byteLength,
                        TZrUInt32 count,
                        TZrUInt32 elementSize) {
    if (byteLength == 0u) {
        memset(section, 0, sizeof(*section));
        return;
    }

    section->offset = *offset;
    section->byteLength = byteLength;
    section->count = count;
    section->elementSize = elementSize;
    *offset += byteLength;
}

static void write_u32_le(TZrByte *target, TZrUInt32 value) {
    target[0] = (TZrByte)(value & 0xFFu);
    target[1] = (TZrByte)((value >> 8u) & 0xFFu);
    target[2] = (TZrByte)((value >> 16u) & 0xFFu);
    target[3] = (TZrByte)((value >> 24u) & 0xFFu);
}

static TZrUInt32 read_u32_le(const TZrByte *source) {
    return ((TZrUInt32)source[0]) |
           ((TZrUInt32)source[1] << 8u) |
           ((TZrUInt32)source[2] << 16u) |
           ((TZrUInt32)source[3] << 24u);
}

static TZrSize build_interior_orphan_type_def_fixture(TZrByte *buffer,
                                                      TZrSize bufferLength,
                                                      TZrSize *outExpectedPrunedLength,
                                                      TZrUInt32 *outExpectedStringPoolBytes) {
    static const TZrByte stringPool[] = "OrphanType\0Dead\0LiveType\0Example\0Kept";
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 stringPoolBytes = (TZrUInt32)sizeof(stringPool);
    const TZrUInt32 retainedStringPoolBytes = (TZrUInt32)sizeof("LiveType\0Example\0Kept");
    const TZrUInt32 orphanNameOffset = 0u;
    const TZrUInt32 orphanNamespaceOffset = (TZrUInt32)sizeof("OrphanType");
    const TZrUInt32 liveNameOffset = (TZrUInt32)sizeof("OrphanType\0Dead");
    const TZrUInt32 liveNamespaceOffset = liveNameOffset + (TZrUInt32)sizeof("LiveType");
    const TZrUInt32 keptMethodNameOffset = (TZrUInt32)sizeof("OrphanType\0Dead\0LiveType\0Example");
    const TZrMetadataToken orphanTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 2u) +
                                           (typeDefBytes * 2u) +
                                           methodDefBytes +
                                           stringPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 2u, 2u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes * 2u, 2u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));
    memcpy(buffer + header.stringPool.offset, stringPool, stringPoolBytes);

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = liveTypeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = liveTypeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = orphanTypeToken;
    typeDefs[0].nameStringOffset = orphanNameOffset;
    typeDefs[0].namespaceStringOffset = orphanNamespaceOffset;
    typeDefs[1].token = liveTypeToken;
    typeDefs[1].nameStringOffset = liveNameOffset;
    typeDefs[1].namespaceStringOffset = liveNamespaceOffset;
    typeDefs[1].firstMethodDefIndex = 0u;
    typeDefs[1].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].nameStringOffset = keptMethodNameOffset;
    methodDefs[0].functionIndex = 1u;

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedPrunedLength = (TZrSize)(offset - typeDefBytes - (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_signature_embedded_type_def_fixture(TZrByte *buffer,
                                                         TZrSize bufferLength,
                                                         TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 fieldSignatureBytes = 11u;
    const TZrMetadataToken orphanTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 7u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 4u) +
                                           (typeDefBytes * 2u) +
                                           methodDefBytes +
                                           fieldDefBytes +
                                           fieldSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 4u, 4u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes * 2u, 2u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes, 1u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, fieldSignatureBytes, fieldSignatureBytes, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = liveTypeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = liveTypeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = keptFieldToken;
    tokenRecords[2].ownerToken = liveTypeToken;
    tokenRecords[2].targetMetadataToken = keptFieldToken;
    tokenRecords[3].token = fieldSignatureToken;
    tokenRecords[3].relatedToken = keptFieldToken;
    tokenRecords[3].ownerToken = keptFieldToken;
    tokenRecords[3].targetMetadataToken = keptFieldToken;
    tokenRecords[3].signatureBlobOffset = 0u;
    tokenRecords[3].signatureBlobLength = fieldSignatureBytes;
    tokenRecords[3].signatureHash = 0x1111222233334444ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = orphanTypeToken;
    typeDefs[1].token = liveTypeToken;
    typeDefs[1].firstMethodDefIndex = 0u;
    typeDefs[1].methodDefCount = 1u;
    typeDefs[1].firstFieldDefIndex = 0u;
    typeDefs[1].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = keptFieldToken;
    fieldDefs[0].ownerTypeToken = liveTypeToken;
    fieldDefs[0].signatureBlobOffset = 0u;
    fieldDefs[0].signatureBlobLength = fieldSignatureBytes;
    fieldDefs[0].byteOffset = 16u;
    fieldDefs[0].typeLayoutId = 22u;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_FIELD_SIG;
    signatureBlobTarget[1] = 0u;
    signatureBlobTarget[2] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_TYPE_DEF;
    write_u32_le(signatureBlobTarget + 3u, 0u);
    write_u32_le(signatureBlobTarget + 7u, liveTypeToken);

    *outExpectedPrunedLength = (TZrSize)(offset - typeDefBytes);
    return offset;
}

static void test_aot_c_zrp_metadata_pruning_compacts_retained_type_def_tokens(void) {
    static const TZrByte expectedStringPool[] = "LiveType\0Example\0Kept";
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView stringView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const TZrMetadataToken compactedTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_interior_orphan_type_def_fixture(blob,
                                                            sizeof(blob),
                                                            &expectedPrunedLength,
                                                            &expectedStringPoolBytes);

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(2u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                       &typeView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
                                                       &methodView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].targetMetadataToken);

    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("LiveType"), typeDefs[0].namespaceStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);

    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, methodDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("LiveType\0Example"), methodDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].functionIndex);

    TEST_ASSERT_EQUAL_UINT32(sizeof(expectedStringPool), stringView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringView.data, sizeof(expectedStringPool)));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_rewrites_embedded_signature_type_def_tokens(void) {
    TZrByte blob[1280];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView fieldView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken compactedTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken compactedFieldSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);

    originalLength = build_signature_embedded_type_def_fixture(blob, sizeof(blob), &expectedPrunedLength);

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);
    TEST_ASSERT_EQUAL_UINT32(11u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                       &typeView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;
    signatureBlobPool = signatureBlobView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(keptFieldToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, tokenRecords[2].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(keptFieldToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldSignatureToken, tokenRecords[3].token);
    TEST_ASSERT_EQUAL_UINT32(keptFieldToken, tokenRecords[3].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(keptFieldToken, tokenRecords[3].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(keptFieldToken, tokenRecords[3].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[3].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(11u, tokenRecords[3].signatureBlobLength);
    TEST_ASSERT_TRUE(tokenRecords[3].signatureHash != 0u);
    TEST_ASSERT_TRUE(tokenRecords[3].signatureHash != 0x1111222233334444ull);

    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(keptFieldToken, fieldDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, fieldDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, fieldDefs[0].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(11u, fieldDefs[0].signatureBlobLength);

    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_FIELD_SIG, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8(0u, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_TYPE_DEF, signatureBlobPool[2]);
    TEST_ASSERT_EQUAL_UINT32(0u, read_u32_le(signatureBlobPool + 3u));
    TEST_ASSERT_EQUAL_UINT32(compactedTypeToken, read_u32_le(signatureBlobPool + 7u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_pruning_compacts_retained_type_def_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_rewrites_embedded_signature_type_def_tokens);
    return UNITY_END();
}
