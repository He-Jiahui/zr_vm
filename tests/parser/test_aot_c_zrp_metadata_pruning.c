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
    const TZrMetadataToken fieldSignatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u);

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
    TEST_ASSERT_EQUAL_UINT32(fieldSignatureToken, tokenRecords[3].token);
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
    const TZrMetadataToken keptMethodSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 11u);
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
    TEST_ASSERT_EQUAL_UINT32(keptMethodSpecToken, tokenRecords[2].token);
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
    TEST_ASSERT_EQUAL_UINT32(keptMethodSpecToken, methodSpecs[0].token);
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
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_field_def_member_tokens_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_generic_param_owner_tokens_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_generic_param_constraints_after_method_pruning);
    RUN_TEST(test_aot_c_zrp_metadata_pruning_remaps_method_specs_after_method_pruning);
    return UNITY_END();
}
