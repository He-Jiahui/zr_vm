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

static TZrSize build_orphan_typespec_fixture(TZrByte *buffer,
                                             TZrSize bufferLength,
                                             TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 typeSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
    const TZrUInt32 typeSpecSignatureBytes = 5u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken orphanTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 4u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           typeSpecBytes +
                                           typeSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 4u, 4u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, typeSpecBytes, 1u, typeSpecBytes);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, typeSpecSignatureBytes, typeSpecSignatureBytes, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = removedMethodToken;
    tokenRecords[2].ownerToken = typeToken;
    tokenRecords[2].targetMetadataToken = removedMethodToken;
    tokenRecords[3].token = orphanTypeSpecToken;
    tokenRecords[3].relatedToken = removedMethodToken;
    tokenRecords[3].ownerToken = removedMethodToken;
    tokenRecords[3].targetMetadataToken = removedMethodToken;
    tokenRecords[3].signatureBlobOffset = 0u;
    tokenRecords[3].signatureBlobLength = typeSpecSignatureBytes;
    tokenRecords[3].signatureHash = 0x1111222233334444ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 2u;

    typeSpecs = (SZrZrpMetadataTypeSpecRow *)(void *)(buffer + header.typeSpecs.offset);
    typeSpecs[0].token = orphanTypeSpecToken;
    typeSpecs[0].signatureBlobOffset = 0u;
    typeSpecs[0].signatureBlobLength = typeSpecSignatureBytes;
    typeSpecs[0].typeLayoutId = 77u;
    typeSpecs[0].signatureHash = 0x1111222233334444ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 1u, 7u);

    *outExpectedPrunedLength =
            (TZrSize)(offset - (tokenRecordBytes * 2u) - methodDefBytes - typeSpecBytes - typeSpecSignatureBytes);
    return offset;
}

static TZrSize build_compacted_typespec_fixture(TZrByte *buffer,
                                                TZrSize bufferLength,
                                                TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 typeSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
    const TZrUInt32 typeSpecSignatureBytes = 5u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken removedTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    const TZrMetadataToken keptTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 5u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           (typeSpecBytes * 2u) +
                                           (typeSpecSignatureBytes * 2u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 5u, 5u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, typeSpecBytes * 2u, 2u, typeSpecBytes);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, typeSpecSignatureBytes * 2u, typeSpecSignatureBytes * 2u, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = removedMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = removedMethodToken;
    tokenRecords[2].token = keptMethodToken;
    tokenRecords[2].ownerToken = typeToken;
    tokenRecords[2].targetMetadataToken = keptMethodToken;
    tokenRecords[3].token = removedTypeSpecToken;
    tokenRecords[3].relatedToken = removedMethodToken;
    tokenRecords[3].ownerToken = removedMethodToken;
    tokenRecords[3].targetMetadataToken = removedTypeSpecToken;
    tokenRecords[3].signatureBlobOffset = 0u;
    tokenRecords[3].signatureBlobLength = typeSpecSignatureBytes;
    tokenRecords[3].signatureHash = 0x1111222233334444ull;
    tokenRecords[4].token = keptTypeSpecToken;
    tokenRecords[4].relatedToken = keptMethodToken;
    tokenRecords[4].ownerToken = keptMethodToken;
    tokenRecords[4].targetMetadataToken = keptTypeSpecToken;
    tokenRecords[4].signatureBlobOffset = typeSpecSignatureBytes;
    tokenRecords[4].signatureBlobLength = typeSpecSignatureBytes;
    tokenRecords[4].signatureHash = 0x5555666677778888ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 2u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 1u;

    typeSpecs = (SZrZrpMetadataTypeSpecRow *)(void *)(buffer + header.typeSpecs.offset);
    typeSpecs[0].token = removedTypeSpecToken;
    typeSpecs[0].signatureBlobOffset = 0u;
    typeSpecs[0].signatureBlobLength = typeSpecSignatureBytes;
    typeSpecs[0].typeLayoutId = 77u;
    typeSpecs[0].signatureHash = 0x1111222233334444ull;
    typeSpecs[1].token = keptTypeSpecToken;
    typeSpecs[1].signatureBlobOffset = typeSpecSignatureBytes;
    typeSpecs[1].signatureBlobLength = typeSpecSignatureBytes;
    typeSpecs[1].typeLayoutId = 88u;
    typeSpecs[1].signatureHash = 0x5555666677778888ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 1u, 7u);
    signatureBlobTarget[typeSpecSignatureBytes] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + typeSpecSignatureBytes + 1u, 9u);

    *outExpectedPrunedLength =
            (TZrSize)(offset - (tokenRecordBytes * 2u) - methodDefBytes - typeSpecBytes - typeSpecSignatureBytes);
    return offset;
}

static void test_aot_c_zrp_metadata_pruning_drops_orphan_typespec_rows_after_token_record_pruning(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    const SZrMetadataTokenRecord *tokenRecords;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_orphan_typespec_fixture(blob, sizeof(blob), &expectedPrunedLength);

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 3u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(2u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.typeSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.typeSpecs.byteLength);
    TEST_ASSERT_EQUAL_UINT32(0u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].targetMetadataToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_compacts_retained_typespec_tokens(void) {
    TZrByte blob[1280];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView typeSpecView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataTypeSpecRow *typeSpecs;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);

    originalLength = build_compacted_typespec_fixture(blob, sizeof(blob), &expectedPrunedLength);

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 3u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(3u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(5u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
                                                       &methodView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                                       &typeSpecView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    typeSpecs = (const SZrZrpMetadataTypeSpecRow *)(const void *)typeSpecView.data;
    signatureBlobPool = signatureBlobView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[2].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[2].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[2].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(5u, tokenRecords[2].signatureBlobLength);
    TEST_ASSERT_TRUE(tokenRecords[2].signatureHash != 0u);
    TEST_ASSERT_TRUE(tokenRecords[2].signatureHash != 0x5555666677778888ull);

    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].functionIndex);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, typeSpecs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeSpecs[0].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(5u, typeSpecs[0].signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT32(88u, typeSpecs[0].typeLayoutId);
    TEST_ASSERT_EQUAL_UINT64(tokenRecords[2].signatureHash, typeSpecs[0].signatureHash);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT32(9u,
                             ((TZrUInt32)signatureBlobPool[1]) |
                                     ((TZrUInt32)signatureBlobPool[2] << 8u) |
                                     ((TZrUInt32)signatureBlobPool[3] << 16u) |
                                     ((TZrUInt32)signatureBlobPool[4] << 24u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_orphan_typespec_rows_after_token_record_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_compacts_retained_typespec_tokens);
    return UNITY_END();
}
