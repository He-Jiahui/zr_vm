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

static TZrSize build_field_default_value_constant_pool_pruning_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrSize *outExpectedPrunedLength,
        TZrUInt32 *outExpectedConstantPoolBytes) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 sourceConstantPoolBytes = 9u;
    const TZrUInt32 retainedConstantPoolBytes = 5u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken firstFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken secondFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrByte *constantPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 4u) +
                                           typeDefBytes +
                                           methodDefBytes +
                                           (fieldDefBytes * 2u) +
                                           sourceConstantPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 4u, 4u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes * 2u, 2u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, sourceConstantPoolBytes, sourceConstantPoolBytes, 1u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = methodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = methodToken;
    tokenRecords[2].token = firstFieldToken;
    tokenRecords[2].ownerToken = typeToken;
    tokenRecords[2].targetMetadataToken = firstFieldToken;
    tokenRecords[3].token = secondFieldToken;
    tokenRecords[3].ownerToken = typeToken;
    tokenRecords[3].targetMetadataToken = secondFieldToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = methodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = firstFieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    fieldDefs[0].defaultValueConstantPoolOffset = 2u;
    fieldDefs[0].defaultValueConstantPoolLength = 3u;
    fieldDefs[1].token = secondFieldToken;
    fieldDefs[1].ownerTypeToken = typeToken;
    fieldDefs[1].defaultValueConstantPoolOffset = 7u;
    fieldDefs[1].defaultValueConstantPoolLength = 2u;

    constantPool = buffer + header.constantPool.offset;
    constantPool[0] = 0x10u;
    constantPool[1] = 0x20u;
    constantPool[2] = 0xA1u;
    constantPool[3] = 0xA2u;
    constantPool[4] = 0xA3u;
    constantPool[5] = 0x30u;
    constantPool[6] = 0x40u;
    constantPool[7] = 0xB1u;
    constantPool[8] = 0xB2u;

    *outExpectedConstantPoolBytes = retainedConstantPoolBytes;
    *outExpectedPrunedLength = (TZrSize)(offset - (sourceConstantPoolBytes - retainedConstantPoolBytes));
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

static TZrSize build_orphan_module_ref_pruning_fixture(TZrByte *buffer,
                                                       TZrSize bufferLength,
                                                       TZrSize *outExpectedPrunedLength,
                                                       TZrUInt32 *outExpectedStringPoolBytes,
                                                       TZrUInt32 *outExpectedLiveModuleNameOffset,
                                                       TZrUInt32 *outExpectedLiveModuleVersionOffset) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 moduleRefBytes = (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow);
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 methodNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 orphanModuleNameBytes = (TZrUInt32)sizeof("OrphanModule");
    const TZrUInt32 orphanModuleVersionBytes = (TZrUInt32)sizeof("0.0");
    const TZrUInt32 liveModuleNameBytes = (TZrUInt32)sizeof("LiveModule");
    const TZrUInt32 liveModuleVersionBytes = (TZrUInt32)sizeof("1.0");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 methodNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 orphanModuleNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 orphanModuleVersionOffset = orphanModuleNameOffset + orphanModuleNameBytes;
    const TZrUInt32 liveModuleNameOffset = orphanModuleVersionOffset + orphanModuleVersionBytes;
    const TZrUInt32 liveModuleVersionOffset = liveModuleNameOffset + liveModuleNameBytes;
    const TZrUInt32 sourceStringPoolBytes = liveModuleVersionOffset + liveModuleVersionBytes;
    const TZrUInt32 retainedStringPoolBytes =
            typeNameBytes + namespaceBytes + methodNameBytes + liveModuleNameBytes + liveModuleVersionBytes;
    const TZrUInt32 expectedLiveModuleNameOffset = typeNameBytes + namespaceBytes + methodNameBytes;
    const TZrUInt32 expectedLiveModuleVersionOffset = expectedLiveModuleNameOffset + liveModuleNameBytes;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken orphanAssemblyRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);
    const TZrMetadataToken liveAssemblyRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 2u);
    const TZrMetadataToken orphanAssemblySignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 21u);
    const TZrMetadataToken liveAssemblySignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 22u);
    const TZrMetadataToken importedTypeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    const TZrMetadataToken importedTypeRefSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 23u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataModuleRefRow *moduleRefs;
    TZrByte *stringPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 8u) +
                                           typeDefBytes +
                                           methodDefBytes +
                                           (moduleRefBytes * 2u) +
                                           sourceStringPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 8u, 8u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, moduleRefBytes * 2u, 2u, moduleRefBytes);
    set_section(&header.stringPool, &offset, sourceStringPoolBytes, sourceStringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = methodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = methodToken;
    tokenRecords[2].token = orphanAssemblyRefToken;
    tokenRecords[2].relatedToken = orphanAssemblySignatureToken;
    tokenRecords[3].token = orphanAssemblySignatureToken;
    tokenRecords[3].relatedToken = orphanAssemblyRefToken;
    tokenRecords[3].ownerToken = orphanAssemblyRefToken;
    tokenRecords[4].token = liveAssemblyRefToken;
    tokenRecords[4].relatedToken = liveAssemblySignatureToken;
    tokenRecords[5].token = liveAssemblySignatureToken;
    tokenRecords[5].relatedToken = liveAssemblyRefToken;
    tokenRecords[5].ownerToken = liveAssemblyRefToken;
    tokenRecords[6].token = importedTypeRefToken;
    tokenRecords[6].relatedToken = importedTypeRefSignatureToken;
    tokenRecords[6].ownerToken = liveAssemblyRefToken;
    tokenRecords[6].targetMetadataToken = typeToken;
    tokenRecords[7].token = importedTypeRefSignatureToken;
    tokenRecords[7].relatedToken = importedTypeRefToken;
    tokenRecords[7].ownerToken = importedTypeRefToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = methodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = methodNameOffset;
    methodDefs[0].functionIndex = 1u;

    moduleRefs = (SZrZrpMetadataModuleRefRow *)(void *)(buffer + header.moduleRefs.offset);
    moduleRefs[0].token = orphanAssemblyRefToken;
    moduleRefs[0].nameStringOffset = orphanModuleNameOffset;
    moduleRefs[0].versionStringOffset = orphanModuleVersionOffset;
    moduleRefs[0].flags = 3u;
    moduleRefs[0].moduleSignatureHash = 0x1111222233334444ull;
    moduleRefs[1].token = liveAssemblyRefToken;
    moduleRefs[1].nameStringOffset = liveModuleNameOffset;
    moduleRefs[1].versionStringOffset = liveModuleVersionOffset;
    moduleRefs[1].flags = 7u;
    moduleRefs[1].moduleSignatureHash = 0xAAAABBBBCCCCDDDDull;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, methodNameOffset, "Kept", methodNameBytes);
    copy_literal(stringPool, orphanModuleNameOffset, "OrphanModule", orphanModuleNameBytes);
    copy_literal(stringPool, orphanModuleVersionOffset, "0.0", orphanModuleVersionBytes);
    copy_literal(stringPool, liveModuleNameOffset, "LiveModule", liveModuleNameBytes);
    copy_literal(stringPool, liveModuleVersionOffset, "1.0", liveModuleVersionBytes);

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedLiveModuleNameOffset = expectedLiveModuleNameOffset;
    *outExpectedLiveModuleVersionOffset = expectedLiveModuleVersionOffset;
    *outExpectedPrunedLength =
            (TZrSize)(offset -
                      (tokenRecordBytes * 2u) -
                      moduleRefBytes -
                      orphanModuleNameBytes -
                      orphanModuleVersionBytes);
    return offset;
}

static TZrSize build_module_ref_signature_token_rewrite_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrSize *outExpectedPrunedLength,
        TZrUInt32 *outExpectedStringPoolBytes) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 moduleRefBytes = (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow);
    const TZrUInt32 assemblyRefSignatureBytes = 5u;
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 methodNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 orphanModuleNameBytes = (TZrUInt32)sizeof("OrphanModule");
    const TZrUInt32 orphanModuleVersionBytes = (TZrUInt32)sizeof("0.0");
    const TZrUInt32 liveModuleNameBytes = (TZrUInt32)sizeof("LiveModule");
    const TZrUInt32 liveModuleVersionBytes = (TZrUInt32)sizeof("1.0");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 methodNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 orphanModuleNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 orphanModuleVersionOffset = orphanModuleNameOffset + orphanModuleNameBytes;
    const TZrUInt32 liveModuleNameOffset = orphanModuleVersionOffset + orphanModuleVersionBytes;
    const TZrUInt32 liveModuleVersionOffset = liveModuleNameOffset + liveModuleNameBytes;
    const TZrUInt32 sourceStringPoolBytes = liveModuleVersionOffset + liveModuleVersionBytes;
    const TZrUInt32 retainedStringPoolBytes =
            typeNameBytes + namespaceBytes + methodNameBytes + liveModuleNameBytes + liveModuleVersionBytes;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken orphanAssemblyRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);
    const TZrMetadataToken liveAssemblyRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 2u);
    const TZrMetadataToken importedTypeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataModuleRefRow *moduleRefs;
    TZrByte *stringPool;
    TZrByte *signatureBlobPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                            (tokenRecordBytes * 5u) +
                                            typeDefBytes +
                                            methodDefBytes +
                                            (moduleRefBytes * 2u) +
                                            sourceStringPoolBytes +
                                            assemblyRefSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 5u, 5u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, moduleRefBytes * 2u, 2u, moduleRefBytes);
    set_section(&header.stringPool, &offset, sourceStringPoolBytes, sourceStringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, assemblyRefSignatureBytes, assemblyRefSignatureBytes, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = methodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = methodToken;
    tokenRecords[2].token = orphanAssemblyRefToken;
    tokenRecords[3].token = liveAssemblyRefToken;
    tokenRecords[4].token = importedTypeRefToken;
    tokenRecords[4].ownerToken = liveAssemblyRefToken;
    tokenRecords[4].targetMetadataToken = typeToken;
    tokenRecords[4].signatureBlobOffset = 0u;
    tokenRecords[4].signatureBlobLength = assemblyRefSignatureBytes;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = methodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = methodNameOffset;
    methodDefs[0].functionIndex = 1u;

    moduleRefs = (SZrZrpMetadataModuleRefRow *)(void *)(buffer + header.moduleRefs.offset);
    moduleRefs[0].token = orphanAssemblyRefToken;
    moduleRefs[0].nameStringOffset = orphanModuleNameOffset;
    moduleRefs[0].versionStringOffset = orphanModuleVersionOffset;
    moduleRefs[1].token = liveAssemblyRefToken;
    moduleRefs[1].nameStringOffset = liveModuleNameOffset;
    moduleRefs[1].versionStringOffset = liveModuleVersionOffset;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, methodNameOffset, "Kept", methodNameBytes);
    copy_literal(stringPool, orphanModuleNameOffset, "OrphanModule", orphanModuleNameBytes);
    copy_literal(stringPool, orphanModuleVersionOffset, "0.0", orphanModuleVersionBytes);
    copy_literal(stringPool, liveModuleNameOffset, "LiveModule", liveModuleNameBytes);
    copy_literal(stringPool, liveModuleVersionOffset, "1.0", liveModuleVersionBytes);

    signatureBlobPool = buffer + header.signatureBlobPool.offset;
    signatureBlobPool[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF;
    write_u32_le(signatureBlobPool + 1u, liveAssemblyRefToken);

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedPrunedLength =
            (TZrSize)(offset -
                      tokenRecordBytes -
                      moduleRefBytes -
                      orphanModuleNameBytes -
                      orphanModuleVersionBytes);
    return offset;
}

static TZrSize build_signature_rooted_module_ref_retention_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrSize *outExpectedPrunedLength,
        TZrUInt32 *outExpectedStringPoolBytes,
        TZrUInt32 *outExpectedLiveModuleNameOffset,
        TZrUInt32 *outExpectedLiveModuleVersionOffset) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 moduleRefBytes = (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow);
    const TZrUInt32 fieldSignatureBytes = 7u;
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 methodNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 fieldNameBytes = (TZrUInt32)sizeof("ImportedField");
    const TZrUInt32 orphanModuleNameBytes = (TZrUInt32)sizeof("OrphanModule");
    const TZrUInt32 orphanModuleVersionBytes = (TZrUInt32)sizeof("0.0");
    const TZrUInt32 liveModuleNameBytes = (TZrUInt32)sizeof("LiveModule");
    const TZrUInt32 liveModuleVersionBytes = (TZrUInt32)sizeof("1.0");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 methodNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 fieldNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 orphanModuleNameOffset = fieldNameOffset + fieldNameBytes;
    const TZrUInt32 orphanModuleVersionOffset = orphanModuleNameOffset + orphanModuleNameBytes;
    const TZrUInt32 liveModuleNameOffset = orphanModuleVersionOffset + orphanModuleVersionBytes;
    const TZrUInt32 liveModuleVersionOffset = liveModuleNameOffset + liveModuleNameBytes;
    const TZrUInt32 sourceStringPoolBytes = liveModuleVersionOffset + liveModuleVersionBytes;
    const TZrUInt32 retainedStringPoolBytes =
            typeNameBytes + namespaceBytes + methodNameBytes + fieldNameBytes + liveModuleNameBytes +
            liveModuleVersionBytes;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken orphanAssemblyRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);
    const TZrMetadataToken liveAssemblyRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    SZrZrpMetadataModuleRefRow *moduleRefs;
    TZrByte *stringPool;
    TZrByte *signatureBlobPool;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                            (tokenRecordBytes * 3u) +
                                            typeDefBytes +
                                            methodDefBytes +
                                            fieldDefBytes +
                                            (moduleRefBytes * 2u) +
                                            sourceStringPoolBytes +
                                            fieldSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes, 1u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, moduleRefBytes * 2u, 2u, moduleRefBytes);
    set_section(&header.stringPool, &offset, sourceStringPoolBytes, sourceStringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, fieldSignatureBytes, fieldSignatureBytes, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = methodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = methodToken;
    tokenRecords[2].token = fieldToken;
    tokenRecords[2].ownerToken = typeToken;
    tokenRecords[2].targetMetadataToken = fieldToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = methodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = methodNameOffset;
    methodDefs[0].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = fieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    fieldDefs[0].nameStringOffset = fieldNameOffset;
    fieldDefs[0].signatureBlobOffset = 0u;
    fieldDefs[0].signatureBlobLength = fieldSignatureBytes;

    moduleRefs = (SZrZrpMetadataModuleRefRow *)(void *)(buffer + header.moduleRefs.offset);
    moduleRefs[0].token = orphanAssemblyRefToken;
    moduleRefs[0].nameStringOffset = orphanModuleNameOffset;
    moduleRefs[0].versionStringOffset = orphanModuleVersionOffset;
    moduleRefs[1].token = liveAssemblyRefToken;
    moduleRefs[1].nameStringOffset = liveModuleNameOffset;
    moduleRefs[1].versionStringOffset = liveModuleVersionOffset;
    moduleRefs[1].flags = 9u;
    moduleRefs[1].moduleSignatureHash = 0x1111222233334444ull;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, methodNameOffset, "Kept", methodNameBytes);
    copy_literal(stringPool, fieldNameOffset, "ImportedField", fieldNameBytes);
    copy_literal(stringPool, orphanModuleNameOffset, "OrphanModule", orphanModuleNameBytes);
    copy_literal(stringPool, orphanModuleVersionOffset, "0.0", orphanModuleVersionBytes);
    copy_literal(stringPool, liveModuleNameOffset, "LiveModule", liveModuleNameBytes);
    copy_literal(stringPool, liveModuleVersionOffset, "1.0", liveModuleVersionBytes);

    signatureBlobPool = buffer + header.signatureBlobPool.offset;
    signatureBlobPool[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_FIELD_SIG;
    signatureBlobPool[1] = 0u;
    signatureBlobPool[2] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF;
    write_u32_le(signatureBlobPool + 3u, liveAssemblyRefToken);

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedLiveModuleNameOffset = typeNameBytes + namespaceBytes + methodNameBytes + fieldNameBytes;
    *outExpectedLiveModuleVersionOffset = *outExpectedLiveModuleNameOffset + liveModuleNameBytes;
    *outExpectedPrunedLength =
            (TZrSize)(offset -
                      moduleRefBytes -
                      orphanModuleNameBytes -
                      orphanModuleVersionBytes);
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

static void test_aot_c_zrp_metadata_pool_pruning_remaps_field_default_value_constant_pool_slices(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedConstantPoolBytes;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView fieldView;
    SZrZrpMetadataSectionView constantPoolView;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrByte expectedConstantPool[] = {0xA1u, 0xA2u, 0xA3u, 0xB1u, 0xB2u};

    originalLength = build_field_default_value_constant_pool_pruning_fixture(blob,
                                                                             sizeof(blob),
                                                                             &expectedPrunedLength,
                                                                             &expectedConstantPoolBytes);

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
    TEST_ASSERT_EQUAL_UINT32(expectedConstantPoolBytes, header.constantPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(expectedConstantPoolBytes, header.constantPool.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.constantPool.elementSize);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_CONSTANT_POOL,
                                                       &constantPoolView));
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(0u, fieldDefs[0].defaultValueConstantPoolOffset);
    TEST_ASSERT_EQUAL_UINT32(3u, fieldDefs[0].defaultValueConstantPoolLength);
    TEST_ASSERT_EQUAL_UINT32(3u, fieldDefs[1].defaultValueConstantPoolOffset);
    TEST_ASSERT_EQUAL_UINT32(2u, fieldDefs[1].defaultValueConstantPoolLength);
    TEST_ASSERT_EQUAL_UINT32(expectedConstantPoolBytes, constantPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedConstantPool, constantPoolView.data, expectedConstantPoolBytes));

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

static void test_aot_c_zrp_metadata_pool_pruning_drops_orphan_module_refs(void) {
    TZrByte blob[1536];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedLiveModuleNameOffset;
    TZrUInt32 expectedLiveModuleVersionOffset;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView moduleRefView;
    SZrZrpMetadataSectionView stringPoolView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataModuleRefRow *moduleRefs;
    const TZrByte expectedStringPool[] = "ExampleType\0Example\0Kept\0LiveModule\0"
                                         "1.0\0";
    const TZrMetadataToken compactedAssemblyRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);
    const TZrMetadataToken importedTypeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    const TZrMetadataToken compactedLiveAssemblySignatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrMetadataToken compactedImportedTypeRefSignatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u);

    originalLength = build_orphan_module_ref_pruning_fixture(blob,
                                                             sizeof(blob),
                                                             &expectedPrunedLength,
                                                             &expectedStringPoolBytes,
                                                             &expectedLiveModuleNameOffset,
                                                             &expectedLiveModuleVersionOffset);

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

    TEST_ASSERT_EQUAL_UINT32(6u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.moduleRefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.stringPool.elementSize);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MODULE_REFS,
                                                       &moduleRefView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    moduleRefs = (const SZrZrpMetadataModuleRefRow *)(const void *)moduleRefView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveAssemblySignatureToken, tokenRecords[2].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveAssemblySignatureToken, tokenRecords[3].token);
    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, tokenRecords[3].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, tokenRecords[3].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(importedTypeRefToken, tokenRecords[4].token);
    TEST_ASSERT_EQUAL_UINT32(compactedImportedTypeRefSignatureToken, tokenRecords[4].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, tokenRecords[4].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedImportedTypeRefSignatureToken, tokenRecords[5].token);
    TEST_ASSERT_EQUAL_UINT32(importedTypeRefToken, tokenRecords[5].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(importedTypeRefToken, tokenRecords[5].ownerToken);

    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, moduleRefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(expectedLiveModuleNameOffset, moduleRefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedLiveModuleVersionOffset, moduleRefs[0].versionStringOffset);
    TEST_ASSERT_EQUAL_UINT32(7u, moduleRefs[0].flags);
    TEST_ASSERT_EQUAL_UINT64(0xAAAABBBBCCCCDDDDull, moduleRefs[0].moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringPoolView.data, expectedStringPoolBytes));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pool_pruning_rewrites_signature_assembly_ref_tokens(void) {
    TZrByte blob[1536];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView signatureBlobView;
    const TZrMetadataToken compactedAssemblyRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);

    originalLength = build_module_ref_signature_token_rewrite_fixture(blob,
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

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.moduleRefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(5u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF, signatureBlobView.data[0]);
    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, read_u32_le(signatureBlobView.data + 1u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pool_pruning_retains_module_refs_rooted_only_by_signature_blobs(void) {
    TZrByte blob[1536];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedLiveModuleNameOffset;
    TZrUInt32 expectedLiveModuleVersionOffset;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView moduleRefView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrZrpMetadataModuleRefRow *moduleRefs;
    const TZrMetadataToken compactedAssemblyRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);

    originalLength = build_signature_rooted_module_ref_retention_fixture(blob,
                                                                         sizeof(blob),
                                                                         &expectedPrunedLength,
                                                                         &expectedStringPoolBytes,
                                                                         &expectedLiveModuleNameOffset,
                                                                         &expectedLiveModuleVersionOffset);

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

    TEST_ASSERT_EQUAL_UINT32(3u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.moduleRefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(7u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MODULE_REFS,
                                                       &moduleRefView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    moduleRefs = (const SZrZrpMetadataModuleRefRow *)(const void *)moduleRefView.data;
    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, moduleRefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(expectedLiveModuleNameOffset, moduleRefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedLiveModuleVersionOffset, moduleRefs[0].versionStringOffset);
    TEST_ASSERT_EQUAL_UINT32(9u, moduleRefs[0].flags);
    TEST_ASSERT_EQUAL_UINT64(0x1111222233334444ull, moduleRefs[0].moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_FIELD_SIG, signatureBlobView.data[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF, signatureBlobView.data[2]);
    TEST_ASSERT_EQUAL_UINT32(compactedAssemblyRefToken, read_u32_le(signatureBlobView.data + 3u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_compacts_string_pool_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_drops_orphan_constant_pool_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_remaps_field_default_value_constant_pool_slices);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_deduplicates_retained_string_slices);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_compacts_duplicate_strings_without_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_drops_orphan_module_refs);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_rewrites_signature_assembly_ref_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_pool_pruning_retains_module_refs_rooted_only_by_signature_blobs);
    return UNITY_END();
}
