#include "unity.h"

#include "backend_aot_c_zrp_metadata_prune.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"
#include "zr_vm_common/zr_aot_abi.h"

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

static TZrSize build_method_def_token_pruning_fixture(TZrByte *buffer,
                                                      TZrSize bufferLength,
                                                      TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u));

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
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 2u;

    *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes);
    return offset;
}

static TZrSize build_trailing_orphan_type_def_pruning_fixture(TZrByte *buffer,
                                                              TZrSize bufferLength,
                                                              TZrSize *outExpectedPrunedLength,
                                                              TZrUInt32 *outExpectedStringPoolBytes) {
    static const TZrByte stringPool[] = "LiveType\0Example\0Kept\0OrphanType\0Unused";
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 stringPoolBytes = (TZrUInt32)sizeof(stringPool);
    const TZrUInt32 retainedStringPoolBytes = (TZrUInt32)sizeof("LiveType\0Example\0Kept");
    const TZrUInt32 liveNameOffset = 0u;
    const TZrUInt32 liveNamespaceOffset = (TZrUInt32)sizeof("LiveType");
    const TZrUInt32 keptMethodNameOffset = (TZrUInt32)sizeof("LiveType\0Example");
    const TZrUInt32 orphanNameOffset = (TZrUInt32)sizeof("LiveType\0Example\0Kept");
    const TZrUInt32 orphanNamespaceOffset = orphanNameOffset + (TZrUInt32)sizeof("OrphanType");
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken orphanTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
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
    typeDefs[0].token = liveTypeToken;
    typeDefs[0].nameStringOffset = liveNameOffset;
    typeDefs[0].namespaceStringOffset = liveNamespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;
    typeDefs[1].token = orphanTypeToken;
    typeDefs[1].nameStringOffset = orphanNameOffset;
    typeDefs[1].namespaceStringOffset = orphanNamespaceOffset;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].nameStringOffset = keptMethodNameOffset;
    methodDefs[0].functionIndex = 1u;

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedPrunedLength = (TZrSize)(offset - typeDefBytes - (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_pruned_type_def_owned_generic_param_fixture(TZrByte *buffer,
                                                                 TZrSize bufferLength,
                                                                 TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrMetadataToken deadTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataGenericParamRow *genericParams;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 2u) +
                                           (typeDefBytes * 2u) +
                                           methodDefBytes +
                                           genericParamBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 2u, 2u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes * 2u, 2u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, genericParamBytes, 1u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = liveTypeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = liveTypeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = deadTypeToken;
    typeDefs[0].firstGenericParamIndex = 0u;
    typeDefs[0].genericParamCount = 1u;
    typeDefs[1].token = liveTypeToken;
    typeDefs[1].firstMethodDefIndex = 0u;
    typeDefs[1].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].functionIndex = 1u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = deadTypeToken;
    genericParams[0].parameterIndex = 0u;

    *outExpectedPrunedLength = (TZrSize)(offset - typeDefBytes - genericParamBytes);
    return offset;
}

static TZrSize build_orphan_type_def_with_field_pruning_fixture(TZrByte *buffer,
                                                                TZrSize bufferLength,
                                                                TZrSize *outExpectedPrunedLength,
                                                                TZrUInt32 *outExpectedStringPoolBytes) {
    static const TZrByte stringPool[] = "LiveType\0Example\0Kept\0liveField\0DeadType\0Dead\0deadField";
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 stringPoolBytes = (TZrUInt32)sizeof(stringPool);
    const TZrUInt32 retainedStringPoolBytes = (TZrUInt32)sizeof("LiveType\0Example\0Kept\0liveField");
    const TZrUInt32 liveNameOffset = 0u;
    const TZrUInt32 liveNamespaceOffset = (TZrUInt32)sizeof("LiveType");
    const TZrUInt32 keptMethodNameOffset = (TZrUInt32)sizeof("LiveType\0Example");
    const TZrUInt32 liveFieldNameOffset = (TZrUInt32)sizeof("LiveType\0Example\0Kept");
    const TZrUInt32 deadNameOffset = (TZrUInt32)sizeof("LiveType\0Example\0Kept\0liveField");
    const TZrUInt32 deadNamespaceOffset = deadNameOffset + (TZrUInt32)sizeof("DeadType");
    const TZrUInt32 deadFieldNameOffset = deadNamespaceOffset + (TZrUInt32)sizeof("Dead");
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken deadTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken liveFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken deadFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 2u) +
                                           (typeDefBytes * 2u) +
                                           methodDefBytes +
                                           (fieldDefBytes * 2u) +
                                           stringPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 2u, 2u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes * 2u, 2u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes * 2u, 2u, fieldDefBytes);
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
    typeDefs[0].token = liveTypeToken;
    typeDefs[0].nameStringOffset = liveNameOffset;
    typeDefs[0].namespaceStringOffset = liveNamespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;
    typeDefs[1].token = deadTypeToken;
    typeDefs[1].nameStringOffset = deadNameOffset;
    typeDefs[1].namespaceStringOffset = deadNamespaceOffset;
    typeDefs[1].firstFieldDefIndex = 1u;
    typeDefs[1].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].nameStringOffset = keptMethodNameOffset;
    methodDefs[0].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = liveFieldToken;
    fieldDefs[0].ownerTypeToken = liveTypeToken;
    fieldDefs[0].nameStringOffset = liveFieldNameOffset;
    fieldDefs[0].byteOffset = 12u;
    fieldDefs[0].typeLayoutId = 7u;
    fieldDefs[1].token = deadFieldToken;
    fieldDefs[1].ownerTypeToken = deadTypeToken;
    fieldDefs[1].nameStringOffset = deadFieldNameOffset;
    fieldDefs[1].byteOffset = 20u;
    fieldDefs[1].typeLayoutId = 9u;

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedPrunedLength = (TZrSize)(offset -
                                         typeDefBytes -
                                         fieldDefBytes -
                                         (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_dead_field_before_live_field_token_pruning_fixture(TZrByte *buffer,
                                                                        TZrSize bufferLength,
                                                                        TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrMetadataToken deadTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken deadFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken liveFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 4u) +
                                           (typeDefBytes * 2u) +
                                           methodDefBytes +
                                           (fieldDefBytes * 2u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 4u, 4u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes * 2u, 2u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes * 2u, 2u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = liveTypeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = liveTypeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = deadFieldToken;
    tokenRecords[2].targetMetadataToken = deadFieldToken;
    tokenRecords[3].token = liveFieldToken;
    tokenRecords[3].ownerToken = liveTypeToken;
    tokenRecords[3].targetMetadataToken = liveFieldToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = deadTypeToken;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;
    typeDefs[1].token = liveTypeToken;
    typeDefs[1].firstMethodDefIndex = 0u;
    typeDefs[1].methodDefCount = 1u;
    typeDefs[1].firstFieldDefIndex = 1u;
    typeDefs[1].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = deadFieldToken;
    fieldDefs[0].ownerTypeToken = deadTypeToken;
    fieldDefs[0].byteOffset = 4u;
    fieldDefs[0].typeLayoutId = 13u;
    fieldDefs[1].token = liveFieldToken;
    fieldDefs[1].ownerTypeToken = liveTypeToken;
    fieldDefs[1].byteOffset = 12u;
    fieldDefs[1].typeLayoutId = 7u;

    *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - typeDefBytes - fieldDefBytes);
    return offset;
}

static TZrSize build_dead_field_owner_type_token_record_pruning_fixture(TZrByte *buffer,
                                                                        TZrSize bufferLength,
                                                                        TZrSize *outExpectedPrunedLength) {
    const TZrMetadataToken deadTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    TZrSize length;

    length = build_dead_field_before_live_field_token_pruning_fixture(buffer,
                                                                      bufferLength,
                                                                      outExpectedPrunedLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(buffer, length, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[2].ownerToken = deadTypeToken;
    return length;
}

static TZrSize build_typespec_dead_field_token_record_root_fixture(TZrByte *buffer,
                                                                   TZrSize bufferLength,
                                                                   TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 typeSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
    const TZrUInt32 typeSpecSignatureBytes = 5u;
    const TZrMetadataToken deadTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken deadFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken liveFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken deadTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    const TZrMetadataToken liveTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 6u) +
                                           (typeDefBytes * 2u) +
                                           methodDefBytes +
                                           (fieldDefBytes * 2u) +
                                           (typeSpecBytes * 2u) +
                                           (typeSpecSignatureBytes * 2u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 6u, 6u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes * 2u, 2u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes * 2u, 2u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, typeSpecBytes * 2u, 2u, typeSpecBytes);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool,
                &offset,
                typeSpecSignatureBytes * 2u,
                typeSpecSignatureBytes * 2u,
                1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = liveTypeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = liveTypeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = deadFieldToken;
    tokenRecords[2].targetMetadataToken = deadFieldToken;
    tokenRecords[3].token = liveFieldToken;
    tokenRecords[3].ownerToken = liveTypeToken;
    tokenRecords[3].targetMetadataToken = liveFieldToken;
    tokenRecords[4].token = deadTypeSpecToken;
    tokenRecords[4].ownerToken = deadFieldToken;
    tokenRecords[4].targetMetadataToken = deadFieldToken;
    tokenRecords[4].signatureBlobOffset = 0u;
    tokenRecords[4].signatureBlobLength = typeSpecSignatureBytes;
    tokenRecords[5].token = liveTypeSpecToken;
    tokenRecords[5].ownerToken = liveFieldToken;
    tokenRecords[5].targetMetadataToken = liveFieldToken;
    tokenRecords[5].signatureBlobOffset = typeSpecSignatureBytes;
    tokenRecords[5].signatureBlobLength = typeSpecSignatureBytes;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = deadTypeToken;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;
    typeDefs[1].token = liveTypeToken;
    typeDefs[1].firstMethodDefIndex = 0u;
    typeDefs[1].methodDefCount = 1u;
    typeDefs[1].firstFieldDefIndex = 1u;
    typeDefs[1].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = liveTypeToken;
    methodDefs[0].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = deadFieldToken;
    fieldDefs[0].ownerTypeToken = deadTypeToken;
    fieldDefs[0].byteOffset = 4u;
    fieldDefs[0].typeLayoutId = 13u;
    fieldDefs[1].token = liveFieldToken;
    fieldDefs[1].ownerTypeToken = liveTypeToken;
    fieldDefs[1].byteOffset = 12u;
    fieldDefs[1].typeLayoutId = 7u;

    typeSpecs = (SZrZrpMetadataTypeSpecRow *)(void *)(buffer + header.typeSpecs.offset);
    typeSpecs[0].token = deadTypeSpecToken;
    typeSpecs[0].signatureBlobOffset = 0u;
    typeSpecs[0].signatureBlobLength = typeSpecSignatureBytes;
    typeSpecs[0].typeLayoutId = 21u;
    typeSpecs[1].token = liveTypeSpecToken;
    typeSpecs[1].signatureBlobOffset = typeSpecSignatureBytes;
    typeSpecs[1].signatureBlobLength = typeSpecSignatureBytes;
    typeSpecs[1].typeLayoutId = 34u;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 1u, 7u);
    signatureBlobTarget[typeSpecSignatureBytes] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + typeSpecSignatureBytes + 1u, 9u);

    *outExpectedPrunedLength = (TZrSize)(offset -
                                         (tokenRecordBytes * 2u) -
                                         typeDefBytes -
                                         fieldDefBytes -
                                         typeSpecBytes -
                                         typeSpecSignatureBytes);
    return offset;
}

static TZrSize build_method_def_with_field_fixture(TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken fieldSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 5u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           fieldDefBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 5u, 5u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes, 1u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
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
    tokenRecords[3].token = fieldToken;
    tokenRecords[3].ownerToken = typeToken;
    tokenRecords[3].targetMetadataToken = fieldToken;
    tokenRecords[4].token = fieldSignatureToken;
    tokenRecords[4].relatedToken = fieldToken;
    tokenRecords[4].ownerToken = fieldToken;
    tokenRecords[4].targetMetadataToken = fieldToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 2u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = fieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    fieldDefs[0].byteOffset = 12u;
    fieldDefs[0].typeLayoutId = 7u;

    *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes);
    return offset;
}

static TZrSize build_method_def_with_manifest_exports_fixture(TZrByte *buffer,
                                                              TZrSize bufferLength,
                                                              TZrSize *outExpectedPrunedLength,
                                                              TZrUInt32 *outExpectedStringPoolBytes,
                                                              TZrUInt32 *outExpectedKeptNameOffset,
                                                              TZrUInt32 *outExpectedFieldNameOffset) {
    static const TZrByte stringPool[] = "LiveType\0Example\0removed\0kept\0field\0unused";
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 manifestExportBytes = (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow);
    const TZrUInt32 stringPoolBytes = (TZrUInt32)sizeof(stringPool);
    const TZrUInt32 retainedStringPoolBytes = (TZrUInt32)sizeof("LiveType\0Example\0kept\0field");
    const TZrUInt32 liveNameOffset = 0u;
    const TZrUInt32 liveNamespaceOffset = (TZrUInt32)sizeof("LiveType");
    const TZrUInt32 removedMethodNameOffset = (TZrUInt32)sizeof("LiveType\0Example");
    const TZrUInt32 keptMethodNameOffset = (TZrUInt32)sizeof("LiveType\0Example\0removed");
    const TZrUInt32 fieldNameOffset = (TZrUInt32)sizeof("LiveType\0Example\0removed\0kept");
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    SZrZrpMetadataManifestExportRow *manifestExports;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 4u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           fieldDefBytes +
                                           stringPoolBytes +
                                           (manifestExportBytes * 2u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 4u, 4u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes, 1u, fieldDefBytes);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, 0u, 0u, 0u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);
    set_section(&header.manifestExports, &offset, manifestExportBytes * 2u, 2u, manifestExportBytes);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));
    memcpy(buffer + header.stringPool.offset, stringPool, stringPoolBytes);

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = removedMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = removedMethodToken;
    tokenRecords[2].token = keptMethodToken;
    tokenRecords[2].ownerToken = typeToken;
    tokenRecords[2].targetMetadataToken = keptMethodToken;
    tokenRecords[3].token = fieldToken;
    tokenRecords[3].ownerToken = typeToken;
    tokenRecords[3].targetMetadataToken = fieldToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = liveNameOffset;
    typeDefs[0].namespaceStringOffset = liveNamespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = removedMethodNameOffset;
    methodDefs[0].functionIndex = 2u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].nameStringOffset = keptMethodNameOffset;
    methodDefs[1].functionIndex = 1u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = fieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    fieldDefs[0].nameStringOffset = fieldNameOffset;
    fieldDefs[0].byteOffset = 12u;
    fieldDefs[0].typeLayoutId = 7u;

    manifestExports = (SZrZrpMetadataManifestExportRow *)(void *)(buffer + header.manifestExports.offset);
    manifestExports[0].kind = ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD;
    manifestExports[0].flags = ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN;
    manifestExports[0].targetStringOffset = keptMethodNameOffset;
    manifestExports[0].memberToken = keptMethodToken;
    manifestExports[1].kind = ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD;
    manifestExports[1].flags = ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN;
    manifestExports[1].targetStringOffset = fieldNameOffset;
    manifestExports[1].memberToken = fieldToken;

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedKeptNameOffset = (TZrUInt32)sizeof("LiveType\0Example");
    *outExpectedFieldNameOffset = (TZrUInt32)sizeof("LiveType\0Example\0kept");
    *outExpectedPrunedLength = (TZrSize)(offset -
                                         tokenRecordBytes -
                                         methodDefBytes -
                                         (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_type_def_manifest_export_declaration_fixture(TZrByte *buffer,
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

static TZrSize build_method_def_with_generic_param_fixture(TZrByte *buffer,
                                                           TZrSize bufferLength,
                                                           TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataGenericParamRow *genericParams;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           (genericParamBytes * 3u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, genericParamBytes * 3u, 3u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
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
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;
    typeDefs[0].firstGenericParamIndex = 2u;
    typeDefs[0].genericParamCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;
    methodDefs[0].firstGenericParamIndex = 0u;
    methodDefs[0].genericParamCount = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 2u;
    methodDefs[1].firstGenericParamIndex = 1u;
    methodDefs[1].genericParamCount = 1u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = keptMethodToken;
    genericParams[0].parameterIndex = 0u;
    genericParams[1].ownerToken = removedMethodToken;
    genericParams[1].parameterIndex = 0u;
    genericParams[2].ownerToken = typeToken;
    genericParams[2].parameterIndex = 0u;

    *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes - genericParamBytes);
    return offset;
}

static TZrSize build_field_def_as_method_owner_fixture(TZrByte *buffer,
                                                       TZrSize bufferLength,
                                                       TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 fieldDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 methodSpecSignatureBytes = 15u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken missingMethodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 11u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataFieldDefRow *fieldDefs;
    SZrZrpMetadataGenericParamRow *genericParams;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 4u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           fieldDefBytes +
                                           genericParamBytes +
                                           methodSpecBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 4u, 4u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, fieldDefBytes, 1u, fieldDefBytes);
    set_section(&header.genericParams, &offset, genericParamBytes, 1u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, methodSpecSignatureBytes, methodSpecSignatureBytes, 1u);
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
    tokenRecords[3].token = fieldToken;
    tokenRecords[3].ownerToken = typeToken;
    tokenRecords[3].targetMetadataToken = fieldToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;
    typeDefs[0].firstFieldDefIndex = 0u;
    typeDefs[0].fieldDefCount = 1u;
    typeDefs[0].firstGenericParamIndex = 0u;
    typeDefs[0].genericParamCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 2u;

    fieldDefs = (SZrZrpMetadataFieldDefRow *)(void *)(buffer + header.fieldDefs.offset);
    fieldDefs[0].token = fieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    fieldDefs[0].byteOffset = 4u;
    fieldDefs[0].typeLayoutId = 3u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = fieldToken;
    genericParams[0].parameterIndex = 0u;

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = missingMethodSpecToken;
    methodSpecs[0].methodToken = fieldToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x1111222233334444ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, fieldToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 11u, 1u);

    *outExpectedPrunedLength =
            (TZrSize)(offset -
                      tokenRecordBytes -
                      methodDefBytes -
                      genericParamBytes -
                      methodSpecBytes -
                      methodSpecSignatureBytes);
    return offset;
}

static TZrSize build_method_def_with_generic_param_constraint_fixture(TZrByte *buffer,
                                                                      TZrSize bufferLength,
                                                                      TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrUInt32 constraintBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataGenericParamRow *genericParams;
    SZrZrpMetadataGenericParamConstraintRow *constraints;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           (genericParamBytes * 3u) +
                                           (constraintBytes * 4u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, genericParamBytes * 3u, 3u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, constraintBytes * 4u, 4u, constraintBytes);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
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
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;
    typeDefs[0].firstGenericParamIndex = 2u;
    typeDefs[0].genericParamCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 1u;
    methodDefs[0].firstGenericParamIndex = 0u;
    methodDefs[0].genericParamCount = 1u;
    methodDefs[1].token = removedMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 2u;
    methodDefs[1].firstGenericParamIndex = 1u;
    methodDefs[1].genericParamCount = 1u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = keptMethodToken;
    genericParams[0].parameterIndex = 0u;
    genericParams[0].firstConstraintIndex = 0u;
    genericParams[0].constraintCount = 2u;
    genericParams[1].ownerToken = removedMethodToken;
    genericParams[1].parameterIndex = 0u;
    genericParams[1].firstConstraintIndex = 2u;
    genericParams[1].constraintCount = 1u;
    genericParams[2].ownerToken = typeToken;
    genericParams[2].parameterIndex = 0u;
    genericParams[2].firstConstraintIndex = 3u;
    genericParams[2].constraintCount = 1u;

    constraints = (SZrZrpMetadataGenericParamConstraintRow *)(void *)(buffer +
                                                                      header.genericParamConstraints.offset);
    constraints[0].genericParamIndex = 0u;
    constraints[0].constraintTypeToken = typeToken;
    constraints[1].genericParamIndex = 0u;
    constraints[1].constraintTypeToken = typeToken;
    constraints[2].genericParamIndex = 1u;
    constraints[2].constraintTypeToken = typeToken;
    constraints[3].genericParamIndex = 2u;
    constraints[3].constraintTypeToken = typeToken;

    *outExpectedPrunedLength =
            (TZrSize)(offset - tokenRecordBytes - methodDefBytes - genericParamBytes - constraintBytes);
    return offset;
}

static TZrSize build_generic_param_constraint_with_typespec_fixture(TZrByte *buffer,
                                                                    TZrSize bufferLength,
                                                                    TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrUInt32 constraintBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow);
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
    SZrZrpMetadataGenericParamRow *genericParams;
    SZrZrpMetadataGenericParamConstraintRow *constraints;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 5u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           genericParamBytes +
                                           constraintBytes +
                                           (typeSpecBytes * 2u) +
                                           (typeSpecSignatureBytes * 2u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 5u, 5u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, genericParamBytes, 1u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, constraintBytes, 1u, constraintBytes);
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
    methodDefs[1].firstGenericParamIndex = 0u;
    methodDefs[1].genericParamCount = 1u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = keptMethodToken;
    genericParams[0].parameterIndex = 0u;
    genericParams[0].firstConstraintIndex = 0u;
    genericParams[0].constraintCount = 1u;

    constraints = (SZrZrpMetadataGenericParamConstraintRow *)(void *)(buffer +
                                                                      header.genericParamConstraints.offset);
    constraints[0].genericParamIndex = 0u;
    constraints[0].constraintTypeToken = keptTypeSpecToken;

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

static TZrSize build_generic_param_constraint_constraint_rooted_typespec_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrUInt32 constraintBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow);
    const TZrUInt32 typeSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
    const TZrUInt32 typeSpecSignatureBytes = 5u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken orphanTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    const TZrMetadataToken constraintTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataGenericParamRow *genericParams;
    SZrZrpMetadataGenericParamConstraintRow *constraints;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           genericParamBytes +
                                           constraintBytes +
                                           (typeSpecBytes * 2u) +
                                           (typeSpecSignatureBytes * 2u));

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, genericParamBytes, 1u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, constraintBytes, 1u, constraintBytes);
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
    methodDefs[1].firstGenericParamIndex = 0u;
    methodDefs[1].genericParamCount = 1u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = keptMethodToken;
    genericParams[0].parameterIndex = 0u;
    genericParams[0].firstConstraintIndex = 0u;
    genericParams[0].constraintCount = 1u;

    constraints = (SZrZrpMetadataGenericParamConstraintRow *)(void *)(buffer +
                                                                      header.genericParamConstraints.offset);
    constraints[0].genericParamIndex = 0u;
    constraints[0].constraintTypeToken = constraintTypeSpecToken;

    typeSpecs = (SZrZrpMetadataTypeSpecRow *)(void *)(buffer + header.typeSpecs.offset);
    typeSpecs[0].token = orphanTypeSpecToken;
    typeSpecs[0].signatureBlobOffset = 0u;
    typeSpecs[0].signatureBlobLength = typeSpecSignatureBytes;
    typeSpecs[0].typeLayoutId = 77u;
    typeSpecs[0].signatureHash = 0x1111222233334444ull;
    typeSpecs[1].token = constraintTypeSpecToken;
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
            (TZrSize)(offset - tokenRecordBytes - methodDefBytes - typeSpecBytes - typeSpecSignatureBytes);
    return offset;
}

static TZrSize build_method_def_with_method_spec_fixture(TZrByte *buffer,
                                                         TZrSize bufferLength,
                                                         TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 keptMethodSpecSignatureBytes = 15u;
    const TZrUInt32 removedMethodSpecSignatureBytes = 15u;
    const TZrUInt32 signatureBlobBytes = keptMethodSpecSignatureBytes + removedMethodSpecSignatureBytes;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken removedBeforeMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken removedAfterMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken keptMethodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 11u);
    const TZrMetadataToken removedMethodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 12u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 6u) +
                                           typeDefBytes +
                                           (methodDefBytes * 3u) +
                                           (methodSpecBytes * 2u) +
                                           signatureBlobBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 6u, 6u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 3u, 3u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes * 2u, 2u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, signatureBlobBytes, signatureBlobBytes, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = removedBeforeMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = removedBeforeMethodToken;
    tokenRecords[2].token = keptMethodToken;
    tokenRecords[2].ownerToken = typeToken;
    tokenRecords[2].targetMetadataToken = keptMethodToken;
    tokenRecords[3].token = removedAfterMethodToken;
    tokenRecords[3].ownerToken = typeToken;
    tokenRecords[3].targetMetadataToken = removedAfterMethodToken;
    tokenRecords[4].token = keptMethodSpecToken;
    tokenRecords[4].relatedToken = keptMethodToken;
    tokenRecords[4].ownerToken = keptMethodToken;
    tokenRecords[4].signatureBlobOffset = 0u;
    tokenRecords[4].signatureBlobLength = keptMethodSpecSignatureBytes;
    tokenRecords[4].signatureHash = 0x1111222233334444ull;
    tokenRecords[4].targetMetadataToken = keptMethodToken;
    tokenRecords[5].token = removedMethodSpecToken;
    tokenRecords[5].relatedToken = removedAfterMethodToken;
    tokenRecords[5].ownerToken = removedAfterMethodToken;
    tokenRecords[5].signatureBlobOffset = keptMethodSpecSignatureBytes;
    tokenRecords[5].signatureBlobLength = removedMethodSpecSignatureBytes;
    tokenRecords[5].signatureHash = 0x5555666677778888ull;
    tokenRecords[5].targetMetadataToken = removedAfterMethodToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 3u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = removedBeforeMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 1u;
    methodDefs[2].token = removedAfterMethodToken;
    methodDefs[2].ownerTypeToken = typeToken;
    methodDefs[2].functionIndex = 2u;

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = keptMethodSpecToken;
    methodSpecs[0].methodToken = keptMethodToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = keptMethodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x1111222233334444ull;
    methodSpecs[1].token = removedMethodSpecToken;
    methodSpecs[1].methodToken = removedAfterMethodToken;
    methodSpecs[1].instantiationBlobOffset = keptMethodSpecSignatureBytes;
    methodSpecs[1].instantiationBlobLength = removedMethodSpecSignatureBytes;
    methodSpecs[1].instantiationHash = 0x5555666677778888ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, keptMethodToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 11u, 1u);
    signatureBlobTarget[15] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[16] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 17u, removedAfterMethodToken);
    write_u32_le(signatureBlobTarget + 21u, 1u);
    signatureBlobTarget[25] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 26u, 2u);

    *outExpectedPrunedLength =
            (TZrSize)(offset -
                      (tokenRecordBytes * 3u) -
                      (methodDefBytes * 2u) -
                      methodSpecBytes -
                      removedMethodSpecSignatureBytes);
    return offset;
}

static TZrSize build_method_spec_missing_signature_record_fixture(TZrByte *buffer, TZrSize bufferLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 methodSpecSignatureBytes = 15u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken missingMethodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 11u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           methodSpecBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool, &offset, methodSpecSignatureBytes, methodSpecSignatureBytes, 1u);
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

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 1u;

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = missingMethodSpecToken;
    methodSpecs[0].methodToken = keptMethodToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x1111222233334444ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, keptMethodToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 11u, 1u);

    return offset;
}

static TZrSize build_signature_member_ref_token_rewrite_fixture(TZrByte *buffer,
                                                                TZrSize bufferLength,
                                                                TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 signatureBytes = 5u;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                            (tokenRecordBytes * 3u) +
                                            typeDefBytes +
                                            (methodDefBytes * 2u) +
                                            signatureBytes);

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
    set_section(&header.signatureBlobPool, &offset, signatureBytes, signatureBytes, 1u);
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
    tokenRecords[2].signatureBlobOffset = 0u;
    tokenRecords[2].signatureBlobLength = signatureBytes;
    tokenRecords[2].signatureHash = 0x1111222233334444ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].ownerTypeToken = typeToken;
    methodDefs[1].functionIndex = 1u;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 1u, keptMethodToken);

    *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes);
    return offset;
}

static void test_aot_c_zrp_metadata_pruning_prunes_token_records_for_removed_method_defs(void) {
    TZrByte blob[768];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView methodView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataMethodDefRow *methodDefs;

    originalLength = build_method_def_token_pruning_fixture(blob, sizeof(blob), &expectedPrunedLength);

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
    TEST_ASSERT_EQUAL_UINT32(2u * (TZrUInt32)sizeof(SZrMetadataTokenRecord), header.tokenRecords.byteLength);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow), header.methodDefs.byteLength);

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

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;

    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u), tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u), tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u),
                             tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u), methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].functionIndex);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_trailing_orphan_type_defs(void) {
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
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView stringPoolView;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_trailing_orphan_type_def_pruning_fixture(blob,
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
    functionTable.indexSpace = 1u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow), header.typeDefs.byteLength);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);

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

    TEST_ASSERT_EQUAL_UINT32(liveTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("LiveType"), typeDefs[0].namespaceStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(liveTypeToken, methodDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("LiveType\0Example"), methodDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringPoolView.data, expectedStringPoolBytes));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_orphan_type_defs_with_fields(void) {
    static const TZrByte expectedStringPool[] = "LiveType\0Example\0Kept\0liveField";
    TZrByte blob[1280];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
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
    const TZrMetadataToken liveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    originalLength = build_orphan_type_def_with_field_pruning_fixture(blob,
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
    memset(&prunedMetadata, 0, sizeof(prunedMetadata));

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow), header.fieldDefs.byteLength);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);

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

    TEST_ASSERT_EQUAL_UINT32(liveTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstFieldDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].fieldDefCount);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(liveTypeToken, methodDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, fieldDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(liveTypeToken, fieldDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof("LiveType\0Example\0Kept"), fieldDefs[0].nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expectedStringPool, stringPoolView.data, expectedStringPoolBytes));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_remaps_field_def_member_tokens_after_method_pruning(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView fieldView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken compactedFieldSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);

    originalLength = build_method_def_with_field_fixture(blob, sizeof(blob), &expectedPrunedLength);

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

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(4u * (TZrUInt32)sizeof(SZrMetadataTokenRecord), header.tokenRecords.byteLength);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);

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
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldSignatureToken, tokenRecords[3].token);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[3].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[3].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[3].targetMetadataToken);

    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstFieldDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].fieldDefCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].functionIndex);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, fieldDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, fieldDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32(12u, fieldDefs[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(7u, fieldDefs[0].typeLayoutId);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_pruned_field_def_member_tokens_before_live_fields(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView fieldView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrMetadataToken compactedLiveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    originalLength = build_dead_field_before_live_field_token_pruning_fixture(blob,
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
    functionTable.indexSpace = 2u;
    memset(&prunedMetadata, 0, sizeof(prunedMetadata));

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(3u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);

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
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[2].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].targetMetadataToken);

    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstFieldDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].fieldDefCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, methodDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, fieldDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, fieldDefs[0].ownerTypeToken);
    TEST_ASSERT_EQUAL_UINT32(12u, fieldDefs[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(7u, fieldDefs[0].typeLayoutId);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_typedef_rooted_only_by_pruned_field_owner_token(void) {
    TZrByte blob[1024];
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
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrMetadataToken compactedLiveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    originalLength = build_dead_field_owner_type_token_record_pruning_fixture(blob,
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
    functionTable.indexSpace = 2u;
    memset(&prunedMetadata, 0, sizeof(prunedMetadata));

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(3u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);

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

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[2].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, fieldDefs[0].ownerTypeToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_typespec_rooted_only_by_pruned_field_token_record(void) {
    TZrByte blob[1536];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeSpecView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeSpecRow *typeSpecs;
    const TZrMetadataToken compactedLiveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken compactedTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);

    originalLength =
            build_typespec_dead_field_token_record_root_fixture(blob, sizeof(blob), &expectedPrunedLength);

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;
    memset(&prunedMetadata, 0, sizeof(prunedMetadata));

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);
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
                                                       ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                                       &typeSpecView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeSpecs = (const SZrZrpMetadataTypeSpecRow *)(const void *)typeSpecView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, tokenRecords[3].token);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[3].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[3].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[3].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(5u, tokenRecords[3].signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, typeSpecs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeSpecs[0].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(5u, typeSpecs[0].signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT32(34u, typeSpecs[0].typeLayoutId);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, signatureBlobView.data[0]);
    TEST_ASSERT_EQUAL_UINT32(9u, read_u32_le(signatureBlobView.data + 1u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_remaps_manifest_export_rows_after_method_pruning(void) {
    TZrByte blob[2048];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedKeptNameOffset;
    TZrUInt32 expectedFieldNameOffset;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrZrpMetadataManifestExportRow *manifestExports;
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    originalLength = build_method_def_with_manifest_exports_fixture(blob,
                                                                    sizeof(blob),
                                                                    &expectedPrunedLength,
                                                                    &expectedStringPoolBytes,
                                                                    &expectedKeptNameOffset,
                                                                    &expectedFieldNameOffset);

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
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(2u, header.manifestExports.count);
    TEST_ASSERT_EQUAL_UINT32(2u * (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow),
                             header.manifestExports.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                       &manifestExportView));

    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD, manifestExports[0].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN, manifestExports[0].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedKeptNameOffset, manifestExports[0].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, manifestExports[0].memberToken);
    TEST_ASSERT_EQUAL_STRING("kept", (const char *)stringPoolView.data + manifestExports[0].targetStringOffset);

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD, manifestExports[1].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN, manifestExports[1].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedFieldNameOffset, manifestExports[1].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[1].typeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, manifestExports[1].memberToken);
    TEST_ASSERT_EQUAL_STRING("field", (const char *)stringPoolView.data + manifestExports[1].targetStringOffset);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_keeps_manifest_export_target_only_strings(void) {
    TZrByte blob[2048];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedKeptNameOffset;
    TZrUInt32 expectedFieldNameOffset;
    TZrUInt32 expectedManifestTargetOffset;
    TZrSize originalLength;
    SZrZrpMetadataHeader sourceHeader;
    SZrZrpMetadataManifestExportRow *sourceManifestExports;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrZrpMetadataManifestExportRow *manifestExports;
    const TZrUInt32 sourceManifestTargetOffset = (TZrUInt32)sizeof("LiveType\0Example\0removed\0kept\0field");
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_method_def_with_manifest_exports_fixture(blob,
                                                                    sizeof(blob),
                                                                    &expectedPrunedLength,
                                                                    &expectedStringPoolBytes,
                                                                    &expectedKeptNameOffset,
                                                                    &expectedFieldNameOffset);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(blob, originalLength, &sourceHeader));
    sourceManifestExports =
            (SZrZrpMetadataManifestExportRow *)(void *)(blob + sourceHeader.manifestExports.offset);
    sourceManifestExports[0].targetStringOffset = sourceManifestTargetOffset;

    expectedManifestTargetOffset = expectedStringPoolBytes;
    expectedStringPoolBytes += (TZrUInt32)sizeof("unused");
    expectedPrunedLength += (TZrSize)sizeof("unused");

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
    TEST_ASSERT_EQUAL_UINT32(2u, header.manifestExports.count);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                       &manifestExportView));

    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD, manifestExports[0].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN, manifestExports[0].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedManifestTargetOffset, manifestExports[0].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, manifestExports[0].memberToken);
    TEST_ASSERT_EQUAL_STRING("unused", (const char *)stringPoolView.data + manifestExports[0].targetStringOffset);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_preserves_unbound_manifest_export_rows_after_method_pruning(void) {
    TZrByte blob[2048];
    TZrSize expectedPrunedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedKeptNameOffset;
    TZrUInt32 expectedFieldNameOffset;
    TZrSize originalLength;
    SZrZrpMetadataHeader sourceHeader;
    SZrZrpMetadataManifestExportRow *sourceManifestExports;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrZrpMetadataManifestExportRow *manifestExports;
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_method_def_with_manifest_exports_fixture(blob,
                                                                    sizeof(blob),
                                                                    &expectedPrunedLength,
                                                                    &expectedStringPoolBytes,
                                                                    &expectedKeptNameOffset,
                                                                    &expectedFieldNameOffset);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(blob, originalLength, &sourceHeader));
    sourceManifestExports =
            (SZrZrpMetadataManifestExportRow *)(void *)(blob + sourceHeader.manifestExports.offset);
    sourceManifestExports[1].flags = 0u;
    sourceManifestExports[1].memberToken = 0u;

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
    TEST_ASSERT_EQUAL_UINT32(2u, header.manifestExports.count);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                       &manifestExportView));

    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD, manifestExports[0].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN, manifestExports[0].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedKeptNameOffset, manifestExports[0].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, manifestExports[0].memberToken);

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD, manifestExports[1].kind);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[1].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedFieldNameOffset, manifestExports[1].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[1].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[1].memberToken);
    TEST_ASSERT_EQUAL_STRING("field", (const char *)stringPoolView.data + manifestExports[1].targetStringOffset);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_publishes_manifest_export_declarations_as_rows(void) {
    TZrByte blob[2048];
    TZrSize expectedPrunedLength;
    TZrSize expectedPublishedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedPublishedStringPoolBytes;
    TZrUInt32 expectedKeptNameOffset;
    TZrUInt32 expectedFieldNameOffset;
    TZrUInt32 expectedApiKeptOffset;
    TZrUInt32 expectedApiFieldOffset;
    TZrSize originalLength;
    SZrAotManifestExportDeclaration declarations[2];
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrZrpMetadataManifestExportRow *manifestExports;
    const TZrMetadataToken sourceKeptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken sourceFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    originalLength = build_method_def_with_manifest_exports_fixture(blob,
                                                                    sizeof(blob),
                                                                    &expectedPrunedLength,
                                                                    &expectedStringPoolBytes,
                                                                    &expectedKeptNameOffset,
                                                                    &expectedFieldNameOffset);
    expectedApiKeptOffset = expectedStringPoolBytes;
    expectedApiFieldOffset = expectedApiKeptOffset + (TZrUInt32)sizeof("api.kept");
    expectedPublishedStringPoolBytes = expectedStringPoolBytes +
                                       (TZrUInt32)sizeof("api.kept") +
                                       (TZrUInt32)sizeof("api.field");
    expectedPublishedLength = expectedPrunedLength +
                              (TZrSize)sizeof("api.kept") +
                              (TZrSize)sizeof("api.field") +
                              ((TZrSize)2u * (TZrSize)sizeof(SZrZrpMetadataManifestExportRow));

    memset(declarations, 0, sizeof(declarations));
    declarations[0].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declarations[0].target = "api.kept";
    declarations[0].hasMemberTokenBinding = ZR_TRUE;
    declarations[0].memberToken = sourceKeptMethodToken;
    declarations[1].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD;
    declarations[1].target = "api.field";
    declarations[1].hasMemberTokenBinding = ZR_TRUE;
    declarations[1].memberToken = sourceFieldToken;

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;
    options.manifestExportDeclarations = declarations;
    options.manifestExportDeclarationCount = 2u;

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
    TEST_ASSERT_EQUAL_UINT64(expectedPublishedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(3u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedPublishedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(4u, header.manifestExports.count);
    TEST_ASSERT_EQUAL_UINT32(4u * (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow),
                             header.manifestExports.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                       &manifestExportView));

    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD, manifestExports[2].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN, manifestExports[2].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedApiKeptOffset, manifestExports[2].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[2].typeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, manifestExports[2].memberToken);
    TEST_ASSERT_EQUAL_STRING("api.kept", (const char *)stringPoolView.data + manifestExports[2].targetStringOffset);

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD, manifestExports[3].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN, manifestExports[3].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedApiFieldOffset, manifestExports[3].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[3].typeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, manifestExports[3].memberToken);
    TEST_ASSERT_EQUAL_STRING("api.field", (const char *)stringPoolView.data + manifestExports[3].targetStringOffset);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_publishes_unbound_manifest_export_declarations_as_rows(void) {
    TZrByte blob[2048];
    TZrSize expectedPrunedLength;
    TZrSize expectedPublishedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedPublishedStringPoolBytes;
    TZrUInt32 expectedKeptNameOffset;
    TZrUInt32 expectedFieldNameOffset;
    TZrUInt32 expectedApiMethodOffset;
    TZrUInt32 expectedApiTypeOffset;
    TZrUInt32 expectedApiFieldOffset;
    TZrSize originalLength;
    SZrAotManifestExportDeclaration declarations[3];
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrZrpMetadataManifestExportRow *manifestExports;

    originalLength = build_method_def_with_manifest_exports_fixture(blob,
                                                                    sizeof(blob),
                                                                    &expectedPrunedLength,
                                                                    &expectedStringPoolBytes,
                                                                    &expectedKeptNameOffset,
                                                                    &expectedFieldNameOffset);
    (void)expectedKeptNameOffset;
    (void)expectedFieldNameOffset;
    expectedApiMethodOffset = expectedStringPoolBytes;
    expectedApiTypeOffset = expectedApiMethodOffset + (TZrUInt32)sizeof("api.dynamic");
    expectedApiFieldOffset = expectedApiTypeOffset + (TZrUInt32)sizeof("api.DynamicType");
    expectedPublishedStringPoolBytes = expectedStringPoolBytes +
                                       (TZrUInt32)sizeof("api.dynamic") +
                                       (TZrUInt32)sizeof("api.DynamicType") +
                                       (TZrUInt32)sizeof("api.value");
    expectedPublishedLength = expectedPrunedLength +
                              (TZrSize)sizeof("api.dynamic") +
                              (TZrSize)sizeof("api.DynamicType") +
                              (TZrSize)sizeof("api.value") +
                              ((TZrSize)3u * (TZrSize)sizeof(SZrZrpMetadataManifestExportRow));

    memset(declarations, 0, sizeof(declarations));
    declarations[0].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declarations[0].target = "api.dynamic";
    declarations[1].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE;
    declarations[1].target = "api.DynamicType";
    declarations[2].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD;
    declarations[2].target = "api.value";

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;
    options.manifestExportDeclarations = declarations;
    options.manifestExportDeclarationCount = 3u;

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
    TEST_ASSERT_EQUAL_UINT64(expectedPublishedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(expectedPublishedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(5u, header.manifestExports.count);
    TEST_ASSERT_EQUAL_UINT32(5u * (TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow),
                             header.manifestExports.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                       &manifestExportView));

    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD, manifestExports[2].kind);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[2].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedApiMethodOffset, manifestExports[2].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[2].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[2].memberToken);
    TEST_ASSERT_EQUAL_STRING("api.dynamic", (const char *)stringPoolView.data + manifestExports[2].targetStringOffset);

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE, manifestExports[3].kind);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[3].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedApiTypeOffset, manifestExports[3].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[3].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[3].memberToken);
    TEST_ASSERT_EQUAL_STRING("api.DynamicType",
                             (const char *)stringPoolView.data + manifestExports[3].targetStringOffset);

    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD, manifestExports[4].kind);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[4].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedApiFieldOffset, manifestExports[4].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[4].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[4].memberToken);
    TEST_ASSERT_EQUAL_STRING("api.value", (const char *)stringPoolView.data + manifestExports[4].targetStringOffset);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_publishes_type_manifest_export_declarations_as_rows(void) {
    TZrByte blob[2048];
    TZrSize expectedPrunedLength;
    TZrSize expectedPublishedLength;
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedPublishedStringPoolBytes;
    TZrUInt32 expectedApiTypeOffset;
    TZrSize originalLength;
    SZrAotManifestExportDeclaration declaration;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView manifestExportView;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataManifestExportRow *manifestExports;
    const TZrMetadataToken sourceLiveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    const TZrMetadataToken compactedLiveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    originalLength = build_type_def_manifest_export_declaration_fixture(blob,
                                                                        sizeof(blob),
                                                                        &expectedPrunedLength,
                                                                        &expectedStringPoolBytes);
    expectedApiTypeOffset = expectedStringPoolBytes;
    expectedPublishedStringPoolBytes = expectedStringPoolBytes + (TZrUInt32)sizeof("api.LiveType");
    expectedPublishedLength = expectedPrunedLength +
                              (TZrSize)sizeof("api.LiveType") +
                              (TZrSize)sizeof(SZrZrpMetadataManifestExportRow);

    memset(&declaration, 0, sizeof(declaration));
    declaration.kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE;
    declaration.target = "api.LiveType";
    declaration.hasTypeTokenBinding = ZR_TRUE;
    declaration.typeToken = sourceLiveTypeToken;

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;
    options.manifestExportDeclarations = &declaration;
    options.manifestExportDeclarationCount = 1u;

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
    TEST_ASSERT_EQUAL_UINT64(expectedPublishedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(2u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedPublishedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(1u, header.manifestExports.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataManifestExportRow),
                             header.manifestExports.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                       &typeView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS,
                                                       &manifestExportView));

    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    manifestExports = (const SZrZrpMetadataManifestExportRow *)(const void *)manifestExportView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE, manifestExports[0].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN, manifestExports[0].flags);
    TEST_ASSERT_EQUAL_UINT32(expectedApiTypeOffset, manifestExports[0].targetStringOffset);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, manifestExports[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, manifestExports[0].memberToken);
    TEST_ASSERT_EQUAL_STRING("api.LiveType", (const char *)stringPoolView.data + manifestExports[0].targetStringOffset);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_remaps_generic_param_owner_tokens_after_method_pruning(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView genericParamView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataGenericParamRow *genericParams;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_method_def_with_generic_param_fixture(blob, sizeof(blob), &expectedPrunedLength);

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
    TEST_ASSERT_EQUAL_UINT32(2u, header.genericParams.count);
    TEST_ASSERT_EQUAL_UINT32(2u * (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow),
                             header.genericParams.byteLength);

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
                                                       ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS,
                                                       &genericParamView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    genericParams = (const SZrZrpMetadataGenericParamRow *)(const void *)genericParamView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);

    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].firstGenericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].genericParamCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, methodDefs[0].firstGenericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].genericParamCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, genericParams[0].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(0u, genericParams[0].parameterIndex);
    TEST_ASSERT_EQUAL_UINT32(typeToken, genericParams[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(0u, genericParams[1].parameterIndex);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_generic_params_owned_by_pruned_type_defs(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const TZrMetadataToken compactedLiveTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_pruned_type_def_owned_generic_param_fixture(blob,
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
    TEST_ASSERT_EQUAL_UINT32(0u, header.genericParams.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.genericParams.byteLength);

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

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, typeDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstGenericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].genericParamCount);
    TEST_ASSERT_EQUAL_UINT32(keptMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedLiveTypeToken, methodDefs[0].ownerTypeToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_drops_field_def_method_only_member_tokens(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView typeView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView fieldView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataFieldDefRow *fieldDefs;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    originalLength = build_field_def_as_method_owner_fixture(blob, sizeof(blob), &expectedPrunedLength);

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
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.fieldDefs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.genericParams.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.signatureBlobPool.byteLength);

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
                                                       ZR_ZRP_METADATA_SECTION_FIELD_DEFS,
                                                       &fieldView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    fieldDefs = (const SZrZrpMetadataFieldDefRow *)(const void *)fieldView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[2].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstGenericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].genericParamCount);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstFieldDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].fieldDefCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedFieldToken, fieldDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, fieldDefs[0].ownerTypeToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_remaps_generic_param_constraints_after_method_pruning(void) {
    TZrByte blob[1280];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView genericParamView;
    SZrZrpMetadataSectionView constraintView;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataGenericParamRow *genericParams;
    const SZrZrpMetadataGenericParamConstraintRow *constraints;
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    originalLength =
            build_method_def_with_generic_param_constraint_fixture(blob, sizeof(blob), &expectedPrunedLength);

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

    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(2u, header.genericParams.count);
    TEST_ASSERT_EQUAL_UINT32(3u, header.genericParamConstraints.count);
    TEST_ASSERT_EQUAL_UINT32(3u * (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow),
                             header.genericParamConstraints.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_METHOD_DEFS,
                                                       &methodView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS,
                                                       &genericParamView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                                       &constraintView));

    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    genericParams = (const SZrZrpMetadataGenericParamRow *)(const void *)genericParamView.data;
    constraints = (const SZrZrpMetadataGenericParamConstraintRow *)(const void *)constraintView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, methodDefs[0].firstGenericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].genericParamCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, genericParams[0].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(0u, genericParams[0].firstConstraintIndex);
    TEST_ASSERT_EQUAL_UINT32(2u, genericParams[0].constraintCount);
    TEST_ASSERT_EQUAL_UINT32(typeToken, genericParams[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(2u, genericParams[1].firstConstraintIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, genericParams[1].constraintCount);
    TEST_ASSERT_EQUAL_UINT32(0u, constraints[0].genericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, constraints[1].genericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, constraints[2].genericParamIndex);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void
test_aot_c_zrp_metadata_pruning_remaps_generic_param_constraint_typespec_tokens_after_typespec_pruning(void) {
    TZrByte blob[1536];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView genericParamView;
    SZrZrpMetadataSectionView constraintView;
    SZrZrpMetadataSectionView typeSpecView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataGenericParamRow *genericParams;
    const SZrZrpMetadataGenericParamConstraintRow *constraints;
    const SZrZrpMetadataTypeSpecRow *typeSpecs;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);

    originalLength =
            build_generic_param_constraint_with_typespec_fixture(blob, sizeof(blob), &expectedPrunedLength);

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
    TEST_ASSERT_EQUAL_UINT32(1u, header.genericParams.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.genericParamConstraints.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeSpecs.count);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS,
                                                       &genericParamView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                                       &constraintView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                                       &typeSpecView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    genericParams = (const SZrZrpMetadataGenericParamRow *)(const void *)genericParamView.data;
    constraints = (const SZrZrpMetadataGenericParamConstraintRow *)(const void *)constraintView.data;
    typeSpecs = (const SZrZrpMetadataTypeSpecRow *)(const void *)typeSpecView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, genericParams[0].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(0u, genericParams[0].firstConstraintIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, genericParams[0].constraintCount);
    TEST_ASSERT_EQUAL_UINT32(0u, constraints[0].genericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, constraints[0].constraintTypeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, typeSpecs[0].token);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_keeps_typespec_referenced_only_by_generic_param_constraint(void) {
    TZrByte blob[1536];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView constraintView;
    SZrZrpMetadataSectionView typeSpecView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataGenericParamConstraintRow *constraints;
    const SZrZrpMetadataTypeSpecRow *typeSpecs;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedTypeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);

    originalLength = build_generic_param_constraint_constraint_rooted_typespec_fixture(blob,
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

    TEST_ASSERT_EQUAL_UINT32(2u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.genericParams.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.genericParamConstraints.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeSpecs.count);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                                       &constraintView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TYPE_SPECS,
                                                       &typeSpecView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    constraints = (const SZrZrpMetadataGenericParamConstraintRow *)(const void *)constraintView.data;
    typeSpecs = (const SZrZrpMetadataTypeSpecRow *)(const void *)typeSpecView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, constraints[0].constraintTypeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedTypeSpecToken, typeSpecs[0].token);
    TEST_ASSERT_EQUAL_UINT32(0u, typeSpecs[0].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(5u, typeSpecs[0].signatureBlobLength);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_rejects_method_spec_without_signature_record(void) {
    TZrByte blob[1024];
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;

    originalLength = build_method_spec_missing_signature_record_fixture(blob, sizeof(blob));

    memset(&options, 0, sizeof(options));
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = originalLength;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;
    memset(&prunedMetadata, 0, sizeof(prunedMetadata));

    TEST_ASSERT_FALSE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                  ZR_TRUE,
                                                                  &functionTable,
                                                                  &prunedMetadata));
    TEST_ASSERT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(0u, prunedMetadata.length);
}

static void test_aot_c_zrp_metadata_pruning_rewrites_signature_member_ref_tokens(void) {
    TZrByte blob[768];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_signature_member_ref_token_rewrite_fixture(blob,
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
    functionTable.indexSpace = 2u;

    TEST_ASSERT_TRUE(backend_aot_c_prepare_embedded_zrp_metadata(&options,
                                                                 ZR_TRUE,
                                                                 &functionTable,
                                                                 &prunedMetadata));
    TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob);
    TEST_ASSERT_EQUAL_UINT64(expectedPrunedLength, prunedMetadata.length);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));

    TEST_ASSERT_EQUAL_UINT32(2u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
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
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    signatureBlobPool = signatureBlobView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[1].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(5u, tokenRecords[1].signatureBlobLength);
    TEST_ASSERT_TRUE(tokenRecords[1].signatureHash != 0u);
    TEST_ASSERT_TRUE(tokenRecords[1].signatureHash != 0x1111222233334444ull);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, read_u32_le(signatureBlobPool + 1u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_pruning_remaps_method_specs_after_method_pruning(void) {
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
    SZrZrpMetadataSectionView methodView;
    SZrZrpMetadataSectionView methodSpecView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataTypeDefRow *typeDefs;
    const SZrZrpMetadataMethodDefRow *methodDefs;
    const SZrZrpMetadataMethodSpecRow *methodSpecs;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken compactedMethodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    const TZrUInt32 keptMethodSpecSignatureBytes = 15u;

    originalLength = build_method_def_with_method_spec_fixture(blob, sizeof(blob), &expectedPrunedLength);

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
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow), header.methodSpecs.byteLength);
    TEST_ASSERT_EQUAL_UINT32(keptMethodSpecSignatureBytes, header.signatureBlobPool.byteLength);

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
                                                       ZR_ZRP_METADATA_SECTION_METHOD_SPECS,
                                                       &methodSpecView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    typeDefs = (const SZrZrpMetadataTypeDefRow *)(const void *)typeView.data;
    methodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)methodView.data;
    methodSpecs = (const SZrZrpMetadataMethodSpecRow *)(const void *)methodSpecView.data;
    signatureBlobPool = signatureBlobView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodSpecToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[2].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[2].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[2].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(keptMethodSpecSignatureBytes, tokenRecords[2].signatureBlobLength);
    TEST_ASSERT_TRUE(tokenRecords[2].signatureHash != 0u);
    TEST_ASSERT_TRUE(tokenRecords[2].signatureHash != 0x1111222233334444ull);
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefs[0].firstMethodDefIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefs[0].methodDefCount);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefs[0].functionIndex);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodSpecToken, methodSpecs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, methodSpecs[0].methodToken);
    TEST_ASSERT_EQUAL_UINT32(0u, methodSpecs[0].instantiationBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(keptMethodSpecSignatureBytes, methodSpecs[0].instantiationBlobLength);
    TEST_ASSERT_EQUAL_UINT64(tokenRecords[2].signatureHash, methodSpecs[0].instantiationHash);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, read_u32_le(signatureBlobPool + 2u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 6u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, signatureBlobPool[10]);
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 11u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_pruning_prunes_token_records_for_removed_method_defs);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_trailing_orphan_type_defs);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_orphan_type_defs_with_fields);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_field_def_member_tokens_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_pruned_field_def_member_tokens_before_live_fields);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_typedef_rooted_only_by_pruned_field_owner_token);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_typespec_rooted_only_by_pruned_field_token_record);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_manifest_export_rows_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_keeps_manifest_export_target_only_strings);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_preserves_unbound_manifest_export_rows_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_publishes_manifest_export_declarations_as_rows);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_publishes_unbound_manifest_export_declarations_as_rows);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_publishes_type_manifest_export_declarations_as_rows);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_generic_param_owner_tokens_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_generic_params_owned_by_pruned_type_defs);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_drops_field_def_method_only_member_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_generic_param_constraints_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_generic_param_constraint_typespec_tokens_after_typespec_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_keeps_typespec_referenced_only_by_generic_param_constraint);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_rejects_method_spec_without_signature_record);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_rewrites_signature_member_ref_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_method_specs_after_method_pruning);
    return UNITY_END();
}
