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

static void copy_literal(TZrByte *target, TZrUInt32 offset, const char *literal, TZrUInt32 byteLength) {
    memcpy(target + offset, literal, byteLength);
}

static TZrSize build_method_def_string_pool_pruning_fixture(TZrByte *buffer,
                                                            TZrSize bufferLength,
                                                            TZrSize *outExpectedPrunedLength,
                                                            TZrUInt32 *outExpectedStringPoolBytes,
                                                            TZrUInt32 *outExpectedKeptMethodNameOffset) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 removedNameBytes = (TZrUInt32)sizeof("Removed");
    const TZrUInt32 keptNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 unusedNameBytes = (TZrUInt32)sizeof("Unused");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 removedNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 keptNameOffset = removedNameOffset + removedNameBytes;
    const TZrUInt32 unusedNameOffset = keptNameOffset + keptNameBytes;
    const TZrUInt32 sourceStringPoolBytes = unusedNameOffset + unusedNameBytes;
    const TZrUInt32 retainedStringPoolBytes = typeNameBytes + namespaceBytes + keptNameBytes;
    const TZrUInt32 expectedKeptMethodNameOffset = typeNameBytes + namespaceBytes;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrByte *stringPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           sourceStringPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, sourceStringPoolBytes, sourceStringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
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

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = keptNameOffset;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].nameStringOffset = removedNameOffset;
    methodDefs[1].functionIndex = 2u;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, removedNameOffset, "Removed", removedNameBytes);
    copy_literal(stringPool, keptNameOffset, "Kept", keptNameBytes);
    copy_literal(stringPool, unusedNameOffset, "Unused", unusedNameBytes);

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedKeptMethodNameOffset = expectedKeptMethodNameOffset;
    *outExpectedPrunedLength =
            (TZrSize)(offset - tokenRecordBytes - methodDefBytes - (sourceStringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_method_def_constant_pool_pruning_fixture(TZrByte *buffer,
                                                              TZrSize bufferLength,
                                                              TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 constantPoolBytes = 5u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrByte *constantPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           constantPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, constantPoolBytes, constantPoolBytes, 1u);

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

    constantPool = buffer + header.constantPool.offset;
    constantPool[0] = 0x10u;
    constantPool[1] = 0x20u;
    constantPool[2] = 0x30u;
    constantPool[3] = 0x40u;
    constantPool[4] = 0x50u;

    *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes - constantPoolBytes);
    return offset;
}

static TZrSize build_duplicate_string_pool_pruning_fixture(TZrByte *buffer,
                                                           TZrSize bufferLength,
                                                           TZrBool retainRemovedMethod,
                                                           TZrSize *outExpectedPrunedLength,
                                                           TZrUInt32 *outExpectedStringPoolBytes,
                                                           TZrUInt32 *outExpectedKeptMethodNameOffset) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 sharedNameBytes = (TZrUInt32)sizeof("Shared");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 keptNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 removedNameBytes = (TZrUInt32)sizeof("Removed");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + sharedNameBytes;
    const TZrUInt32 keptNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 removedNameOffset = keptNameOffset + keptNameBytes;
    const TZrUInt32 duplicateFieldNameOffset = removedNameOffset + removedNameBytes;
    const TZrUInt32 sourceStringPoolBytes = duplicateFieldNameOffset + sharedNameBytes;
    const TZrUInt32 retainedStringPoolBytes =
            sharedNameBytes + namespaceBytes + keptNameBytes + (retainRemovedMethod ? removedNameBytes : 0u);
    const TZrUInt32 expectedKeptMethodNameOffset = sharedNameBytes + namespaceBytes;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrByte *stringPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                            (tokenRecordBytes * 3u) +
                                            typeDefBytes +
                                            (methodDefBytes * 2u) +
                                            fieldDefBytes +
                                            sourceStringPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes, 1u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, sourceStringPoolBytes, sourceStringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
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

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = keptNameOffset;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].nameStringOffset = removedNameOffset;
    methodDefs[1].functionIndex = 2u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = fieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    fieldDefs[0].nameStringOffset = duplicateFieldNameOffset;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "Shared", sharedNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, keptNameOffset, "Kept", keptNameBytes);
    copy_literal(stringPool, removedNameOffset, "Removed", removedNameBytes);
    copy_literal(stringPool, duplicateFieldNameOffset, "Shared", sharedNameBytes);

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedKeptMethodNameOffset = expectedKeptMethodNameOffset;
    *outExpectedPrunedLength =
            (TZrSize)(offset -
                      (retainRemovedMethod ? 0u : tokenRecordBytes) -
                      (retainRemovedMethod ? 0u : methodDefBytes) -
                      (sourceStringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static void test_aot_c_zrp_metadata_pool_pruning_compacts_string_pool_after_method_pruning(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedKeptMethodNameOffset;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataStringView stringView;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const TZrByte expectedStringPool[] = "ExampleType\0Example\0Kept\0";

    originalLength = build_method_def_string_pool_pruning_fixture(blob,
                                                                  sizeof(blob),
                                                                  &expectedPrunedLength,
                                                                  &expectedStringPoolBytes,
                                                                  &expectedKeptMethodNameOffset);

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

    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.stringPool.elementSize);

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
                                                       &stringPoolView));

    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;

    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("ExampleType"), typeDefs[0].namespaceStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedKeptMethodNameOffset, methodDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringPoolView.data, expectedStringPoolBytes));

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetString(prunedMetadata.blob,
                                                  prunedMetadata.length,
                                                  &header,
                                                  methodDefs[0].nameStringOffset,
                                                  &stringView));
    TEST_ASSERT_EQUAL_UINT64((TZrSize)(sizeof("Kept") - 1u), stringView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp("Kept", stringView.data, stringView.byteLength));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pool_pruning_drops_orphan_constant_pool_after_method_pruning(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView constantPoolView;

    originalLength = build_method_def_constant_pool_pruning_fixture(blob,
                                                                    sizeof(blob),
                                                                    &expectedPrunedLength);

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
    TEST_ASSERT_EQUAL_UINT32(0u, header.constantPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(0u, header.constantPool.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.constantPool.elementSize);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_CONSTANT_POOL,
                                                       &constantPoolView));
    TEST_ASSERT_EQUAL_UINT64(0u, constantPoolView.byteLength);
    TEST_ASSERT_EQUAL_UINT32(0u, constantPoolView.count);
    TEST_ASSERT_EQUAL_UINT32(0u, constantPoolView.elementSize);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pool_pruning_deduplicates_retained_string_slices(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedKeptMethodNameOffset;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView fieldView;
    SZrZrpMetadataSectionView stringPoolView;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrByte expectedStringPool[] = "Shared\0Example\0Kept\0";

    originalLength = build_duplicate_string_pool_pruning_fixture(blob,
                                                                 sizeof(blob),
                                                                 ZR_FALSE,
                                                                 &expectedPrunedLength,
                                                                 &expectedStringPoolBytes,
                                                                 &expectedKeptMethodNameOffset);

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

    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.stringPool.elementSize);

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
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));

    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("Shared"), typeDefs[0].namespaceStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedKeptMethodNameOffset, methodDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(typeDefs[0].nameStringOffset, fieldDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringPoolView.data, expectedStringPoolBytes));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pool_pruning_compacts_duplicate_strings_without_method_pruning(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedKeptMethodNameOffset;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntries[2];
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView fieldView;
    SZrZrpMetadataSectionView stringPoolView;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrByte expectedStringPool[] = "Shared\0Example\0Kept\0Removed\0";

    originalLength = build_duplicate_string_pool_pruning_fixture(blob,
                                                                 sizeof(blob),
                                                                 ZR_TRUE,
                                                                 &expectedPrunedLength,
                                                                 &expectedStringPoolBytes,
                                                                 &expectedKeptMethodNameOffset);

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntries[0].function = ZR_NULL;
    retainedEntries[0].flatIndex = 1u;
    retainedEntries[1].function = ZR_NULL;
    retainedEntries[1].flatIndex = 2u;
    functionTable.entries = retainedEntries;
    functionTable.count = 2u;
    functionTable.capacity = 2u;
    functionTable.indexSpace = 3u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.stringPool.elementSize);
    TEST_ASSERT_EQUAL_UINT32(2u, header.methodDefs.count);

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
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));

    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("Shared"), typeDefs[0].namespaceStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedKeptMethodNameOffset, methodDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedKeptMethodNameOffset + (TZrUInt32)sizeof("Kept"),
                             methodDefs[1].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(typeDefs[0].nameStringOffset, fieldDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringPoolView.data, expectedStringPoolBytes));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_compacts_string_pool_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_drops_orphan_constant_pool_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_deduplicates_retained_string_slices);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_compacts_duplicate_strings_without_method_pruning);
    return UNITY_END();
}
