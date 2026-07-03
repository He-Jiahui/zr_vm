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

static void copy_literal(TZrByte *target,
                         TZrUInt32 offset,
                         const char *literal,
                         TZrUInt32 byteLength) {
    memcpy(target + offset, literal, byteLength);
}

static TZrSize build_imported_member_ref_method_spec_fixture(TZrByte *buffer,
                                                             TZrSize bufferLength,
                                                             TZrBool includeImportedMemberRefRecord,
                                                             TZrBool importedMemberRefTargetsRemovedMethod,
                                                             TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 tokenRecordCount = includeImportedMemberRefRecord ? 5u : 4u;
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 methodSpecSignatureBytes = 15u;
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * tokenRecordCount) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           methodSpecBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * tokenRecordCount, tokenRecordCount, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool,
                &offset,
                methodSpecSignatureBytes,
                methodSpecSignatureBytes,
                1u);
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
    if (includeImportedMemberRefRecord) {
        tokenRecords[3].token = importedMethodRefToken;
        tokenRecords[3].targetMetadataToken =
                importedMemberRefTargetsRemovedMethod ? removedMethodToken : importedMethodRefToken;
        tokenRecords[4].token = methodSpecToken;
        tokenRecords[4].relatedToken = importedMethodRefToken;
        tokenRecords[4].ownerToken = importedMethodRefToken;
        tokenRecords[4].targetMetadataToken = importedMethodRefToken;
        tokenRecords[4].signatureBlobOffset = 0u;
        tokenRecords[4].signatureBlobLength = methodSpecSignatureBytes;
        tokenRecords[4].signatureHash = 0x1111222233334444ull;
    } else {
        tokenRecords[3].token = methodSpecToken;
        tokenRecords[3].relatedToken = importedMethodRefToken;
        tokenRecords[3].ownerToken = importedMethodRefToken;
        tokenRecords[3].targetMetadataToken = importedMethodRefToken;
        tokenRecords[3].signatureBlobOffset = 0u;
        tokenRecords[3].signatureBlobLength = methodSpecSignatureBytes;
        tokenRecords[3].signatureHash = 0x1111222233334444ull;
    }

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

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = methodSpecToken;
    methodSpecs[0].methodToken = importedMethodRefToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x1111222233334444ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, importedMethodRefToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 11u, 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlobTarget,
                                                              methodSpecSignatureBytes));

    if (includeImportedMemberRefRecord && !importedMemberRefTargetsRemovedMethod) {
        *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes);
    } else if (includeImportedMemberRefRecord) {
        *outExpectedPrunedLength = (TZrSize)(offset -
                                             (tokenRecordBytes * 3u) -
                                             methodDefBytes -
                                             methodSpecBytes -
                                             methodSpecSignatureBytes);
    } else {
        *outExpectedPrunedLength = (TZrSize)(offset -
                                             (tokenRecordBytes * 2u) -
                                             methodDefBytes -
                                             methodSpecBytes -
                                             methodSpecSignatureBytes);
    }
    return offset;
}

static TZrSize build_nested_imported_member_ref_method_spec_fixture(TZrByte *buffer,
                                                                    TZrSize bufferLength,
                                                                    TZrBool nestedMemberRefTargetsRemovedMethod,
                                                                    TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 tokenRecordCount = 6u;
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 methodSpecSignatureBytes = 15u;
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken removedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken nestedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 2u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * tokenRecordCount) +
                                           typeDefBytes +
                                           (methodDefBytes * 2u) +
                                           methodSpecBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * tokenRecordCount, tokenRecordCount, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes * 2u, 2u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, 0u, 0u, 0u);
    set_section(&header.signatureBlobPool,
                &offset,
                methodSpecSignatureBytes,
                methodSpecSignatureBytes,
                1u);
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
    tokenRecords[3].token = importedMethodRefToken;
    tokenRecords[3].targetMetadataToken = nestedMethodRefToken;
    tokenRecords[4].token = nestedMethodRefToken;
    tokenRecords[4].targetMetadataToken =
            nestedMemberRefTargetsRemovedMethod ? removedMethodToken : nestedMethodRefToken;
    tokenRecords[5].token = methodSpecToken;
    tokenRecords[5].relatedToken = importedMethodRefToken;
    tokenRecords[5].ownerToken = importedMethodRefToken;
    tokenRecords[5].targetMetadataToken = importedMethodRefToken;
    tokenRecords[5].signatureBlobOffset = 0u;
    tokenRecords[5].signatureBlobLength = methodSpecSignatureBytes;
    tokenRecords[5].signatureHash = 0x2222333344445555ull;

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

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = methodSpecToken;
    methodSpecs[0].methodToken = importedMethodRefToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x2222333344445555ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, importedMethodRefToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 11u, 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlobTarget,
                                                              methodSpecSignatureBytes));

    if (nestedMemberRefTargetsRemovedMethod) {
        *outExpectedPrunedLength = (TZrSize)(offset -
                                             (tokenRecordBytes * 4u) -
                                             methodDefBytes -
                                             methodSpecBytes -
                                             methodSpecSignatureBytes);
    } else {
        *outExpectedPrunedLength = (TZrSize)(offset - tokenRecordBytes - methodDefBytes);
    }
    return offset;
}

static TZrSize build_imported_member_ref_method_spec_typeref_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrUInt32 *outExpectedStringPoolBytes,
        TZrUInt32 *outExpectedArgTypeNameOffset,
        TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 tokenRecordCount = 4u;
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 methodNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 argTypeNameBytes = (TZrUInt32)sizeof("ExternalArg");
    const TZrUInt32 unusedNameBytes = (TZrUInt32)sizeof("Unused");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 methodNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 argTypeNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 unusedNameOffset = argTypeNameOffset + argTypeNameBytes;
    const TZrUInt32 stringPoolBytes = unusedNameOffset + unusedNameBytes;
    const TZrUInt32 retainedStringPoolBytes = argTypeNameOffset + argTypeNameBytes;
    const TZrUInt32 methodSpecSignatureBytes = 19u;
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *stringPool;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(outExpectedStringPoolBytes);
    TEST_ASSERT_NOT_NULL(outExpectedArgTypeNameOffset);
    TEST_ASSERT_NOT_NULL(outExpectedPrunedLength);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * tokenRecordCount) +
                                           typeDefBytes +
                                           methodDefBytes +
                                           methodSpecBytes +
                                           stringPoolBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * tokenRecordCount, tokenRecordCount, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_section(&header.signatureBlobPool,
                &offset,
                methodSpecSignatureBytes,
                methodSpecSignatureBytes,
                1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = importedMethodRefToken;
    tokenRecords[2].targetMetadataToken = importedMethodRefToken;
    tokenRecords[3].token = methodSpecToken;
    tokenRecords[3].relatedToken = importedMethodRefToken;
    tokenRecords[3].ownerToken = importedMethodRefToken;
    tokenRecords[3].targetMetadataToken = importedMethodRefToken;
    tokenRecords[3].signatureBlobOffset = 0u;
    tokenRecords[3].signatureBlobLength = methodSpecSignatureBytes;
    tokenRecords[3].signatureHash = 0x3333444455556666ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = methodNameOffset;
    methodDefs[0].functionIndex = 1u;

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = methodSpecToken;
    methodSpecs[0].methodToken = importedMethodRefToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x3333444455556666ull;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, methodNameOffset, "Kept", methodNameBytes);
    copy_literal(stringPool, argTypeNameOffset, "ExternalArg", argTypeNameBytes);
    copy_literal(stringPool, unusedNameOffset, "Unused", unusedNameBytes);

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, importedMethodRefToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(signatureBlobTarget + 11u, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    write_u32_le(signatureBlobTarget + 15u, argTypeNameOffset);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlobTarget,
                                                              methodSpecSignatureBytes));

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedArgTypeNameOffset = argTypeNameOffset;
    *outExpectedPrunedLength = (TZrSize)(offset - (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_imported_member_ref_method_spec_module_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrUInt32 *outExpectedStringPoolBytes,
        TZrUInt32 *outExpectedModuleNameOffset,
        TZrUInt32 *outExpectedModuleVersionOffset,
        TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 tokenRecordCount = 4u;
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 methodNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 unusedNameBytes = (TZrUInt32)sizeof("Unused");
    const TZrUInt32 moduleNameBytes = (TZrUInt32)sizeof("__entry");
    const TZrUInt32 moduleVersionBytes = (TZrUInt32)sizeof("1.0.0");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 methodNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 unusedNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 moduleNameOffset = unusedNameOffset + unusedNameBytes;
    const TZrUInt32 moduleVersionOffset = moduleNameOffset + moduleNameBytes;
    const TZrUInt32 stringPoolBytes = moduleVersionOffset + moduleVersionBytes;
    const TZrUInt32 expectedModuleNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 expectedModuleVersionOffset = expectedModuleNameOffset + moduleNameBytes;
    const TZrUInt32 retainedStringPoolBytes = expectedModuleVersionOffset + moduleVersionBytes;
    const TZrUInt32 methodSpecSignatureBytes = 19u;
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *stringPool;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(outExpectedStringPoolBytes);
    TEST_ASSERT_NOT_NULL(outExpectedModuleNameOffset);
    TEST_ASSERT_NOT_NULL(outExpectedModuleVersionOffset);
    TEST_ASSERT_NOT_NULL(outExpectedPrunedLength);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * tokenRecordCount) +
                                           typeDefBytes +
                                           methodDefBytes +
                                           methodSpecBytes +
                                           stringPoolBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * tokenRecordCount, tokenRecordCount, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_section(&header.signatureBlobPool,
                &offset,
                methodSpecSignatureBytes,
                methodSpecSignatureBytes,
                1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = importedMethodRefToken;
    tokenRecords[2].targetMetadataToken = importedMethodRefToken;
    tokenRecords[3].token = methodSpecToken;
    tokenRecords[3].relatedToken = importedMethodRefToken;
    tokenRecords[3].ownerToken = importedMethodRefToken;
    tokenRecords[3].targetMetadataToken = importedMethodRefToken;
    tokenRecords[3].signatureBlobOffset = 0u;
    tokenRecords[3].signatureBlobLength = methodSpecSignatureBytes;
    tokenRecords[3].signatureHash = 0x4444555566667777ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = methodNameOffset;
    methodDefs[0].functionIndex = 1u;

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = methodSpecToken;
    methodSpecs[0].methodToken = importedMethodRefToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x4444555566667777ull;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, methodNameOffset, "Kept", methodNameBytes);
    copy_literal(stringPool, unusedNameOffset, "Unused", unusedNameBytes);
    copy_literal(stringPool, moduleNameOffset, "__entry", moduleNameBytes);
    copy_literal(stringPool, moduleVersionOffset, "1.0.0", moduleVersionBytes);

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, importedMethodRefToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MODULE;
    write_u32_le(signatureBlobTarget + 11u, moduleNameOffset);
    write_u32_le(signatureBlobTarget + 15u, moduleVersionOffset);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlobTarget,
                                                              methodSpecSignatureBytes));

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedModuleNameOffset = expectedModuleNameOffset;
    *outExpectedModuleVersionOffset = expectedModuleVersionOffset;
    *outExpectedPrunedLength = (TZrSize)(offset - (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static TZrSize build_imported_member_ref_method_spec_union_fixture(
        TZrByte *buffer,
        TZrSize bufferLength,
        TZrUInt32 *outExpectedStringPoolBytes,
        TZrUInt32 *outExpectedUnionBaseNameOffset,
        TZrSize *outExpectedPrunedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 tokenRecordCount = 4u;
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodSpecRow);
    const TZrUInt32 typeNameBytes = (TZrUInt32)sizeof("ExampleType");
    const TZrUInt32 namespaceBytes = (TZrUInt32)sizeof("Example");
    const TZrUInt32 methodNameBytes = (TZrUInt32)sizeof("Kept");
    const TZrUInt32 unusedNameBytes = (TZrUInt32)sizeof("Unused");
    const TZrUInt32 unionBaseNameBytes = (TZrUInt32)sizeof("Option");
    const TZrUInt32 typeNameOffset = 0u;
    const TZrUInt32 namespaceOffset = typeNameOffset + typeNameBytes;
    const TZrUInt32 methodNameOffset = namespaceOffset + namespaceBytes;
    const TZrUInt32 unusedNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 unionBaseNameOffset = unusedNameOffset + unusedNameBytes;
    const TZrUInt32 stringPoolBytes = unionBaseNameOffset + unionBaseNameBytes;
    const TZrUInt32 expectedUnionBaseNameOffset = methodNameOffset + methodNameBytes;
    const TZrUInt32 retainedStringPoolBytes = expectedUnionBaseNameOffset + unionBaseNameBytes;
    const TZrUInt32 methodSpecSignatureBytes = 28u;
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken keptMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataMethodSpecRow *methodSpecs;
    TZrByte *stringPool;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(outExpectedStringPoolBytes);
    TEST_ASSERT_NOT_NULL(outExpectedUnionBaseNameOffset);
    TEST_ASSERT_NOT_NULL(outExpectedPrunedLength);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * tokenRecordCount) +
                                           typeDefBytes +
                                           methodDefBytes +
                                           methodSpecBytes +
                                           stringPoolBytes +
                                           methodSpecSignatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * tokenRecordCount, tokenRecordCount, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, methodSpecBytes, 1u, methodSpecBytes);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_section(&header.signatureBlobPool,
                &offset,
                methodSpecSignatureBytes,
                methodSpecSignatureBytes,
                1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = keptMethodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = keptMethodToken;
    tokenRecords[2].token = importedMethodRefToken;
    tokenRecords[2].targetMetadataToken = importedMethodRefToken;
    tokenRecords[3].token = methodSpecToken;
    tokenRecords[3].relatedToken = importedMethodRefToken;
    tokenRecords[3].ownerToken = importedMethodRefToken;
    tokenRecords[3].targetMetadataToken = importedMethodRefToken;
    tokenRecords[3].signatureBlobOffset = 0u;
    tokenRecords[3].signatureBlobLength = methodSpecSignatureBytes;
    tokenRecords[3].signatureHash = 0x5555666677778888ull;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].nameStringOffset = typeNameOffset;
    typeDefs[0].namespaceStringOffset = namespaceOffset;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = keptMethodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].nameStringOffset = methodNameOffset;
    methodDefs[0].functionIndex = 1u;

    methodSpecs = (SZrZrpMetadataMethodSpecRow *)(void *)(buffer + header.methodSpecs.offset);
    methodSpecs[0].token = methodSpecToken;
    methodSpecs[0].methodToken = importedMethodRefToken;
    methodSpecs[0].instantiationBlobOffset = 0u;
    methodSpecs[0].instantiationBlobLength = methodSpecSignatureBytes;
    methodSpecs[0].instantiationHash = 0x5555666677778888ull;

    stringPool = buffer + header.stringPool.offset;
    copy_literal(stringPool, typeNameOffset, "ExampleType", typeNameBytes);
    copy_literal(stringPool, namespaceOffset, "Example", namespaceBytes);
    copy_literal(stringPool, methodNameOffset, "Kept", methodNameBytes);
    copy_literal(stringPool, unusedNameOffset, "Unused", unusedNameBytes);
    copy_literal(stringPool, unionBaseNameOffset, "Option", unionBaseNameBytes);

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST;
    signatureBlobTarget[1] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF;
    write_u32_le(signatureBlobTarget + 2u, importedMethodRefToken);
    write_u32_le(signatureBlobTarget + 6u, 1u);
    signatureBlobTarget[10] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_UNION;
    write_u32_le(signatureBlobTarget + 11u, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    write_u32_le(signatureBlobTarget + 15u, unionBaseNameOffset);
    write_u32_le(signatureBlobTarget + 19u, 1u);
    signatureBlobTarget[23] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 24u, 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ValidateSignatureBlob(signatureBlobTarget,
                                                              methodSpecSignatureBytes));

    *outExpectedStringPoolBytes = retainedStringPoolBytes;
    *outExpectedUnionBaseNameOffset = expectedUnionBaseNameOffset;
    *outExpectedPrunedLength = (TZrSize)(offset - (stringPoolBytes - retainedStringPoolBytes));
    return offset;
}

static void test_aot_c_zrp_metadata_methodspec_pruning_keeps_imported_member_ref_method_token(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView methodSpecView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataMethodSpecRow *methodSpecs;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);

    originalLength = build_imported_member_ref_method_spec_fixture(blob,
                                                                   sizeof(blob),
                                                                   ZR_TRUE,
                                                                   ZR_FALSE,
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

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(15u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
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
    methodSpecs = (const SZrZrpMetadataMethodSpecRow *)(const void *)methodSpecView.data;
    signatureBlobPool = signatureBlobView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(methodSpecToken, tokenRecords[3].token);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[3].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[3].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[3].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[3].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(15u, tokenRecords[3].signatureBlobLength);
    TEST_ASSERT_TRUE(tokenRecords[3].signatureHash != 0u);
    TEST_ASSERT_TRUE(tokenRecords[3].signatureHash != 0x1111222233334444ull);
    TEST_ASSERT_EQUAL_UINT32(methodSpecToken, methodSpecs[0].token);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, methodSpecs[0].methodToken);
    TEST_ASSERT_EQUAL_UINT32(0u, methodSpecs[0].instantiationBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(15u, methodSpecs[0].instantiationBlobLength);
    TEST_ASSERT_EQUAL_UINT64(tokenRecords[3].signatureHash, methodSpecs[0].instantiationHash);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, read_u32_le(signatureBlobPool + 2u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 6u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, signatureBlobPool[10]);
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 11u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_keeps_nested_imported_member_ref_method_token(void) {
    TZrByte blob[1024];
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView methodSpecView;
    SZrZrpMetadataSectionView signatureBlobView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataMethodSpecRow *methodSpecs;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);
    const TZrMetadataToken nestedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 2u);
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken methodSpecToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);

    originalLength = build_nested_imported_member_ref_method_spec_fixture(blob,
                                                                          sizeof(blob),
                                                                          ZR_FALSE,
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

    TEST_ASSERT_EQUAL_UINT32(5u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(15u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));
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
    methodSpecs = (const SZrZrpMetadataMethodSpecRow *)(const void *)methodSpecView.data;
    signatureBlobPool = signatureBlobView.data;

    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[2].token);
    TEST_ASSERT_EQUAL_UINT32(nestedMethodRefToken, tokenRecords[2].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(nestedMethodRefToken, tokenRecords[3].token);
    TEST_ASSERT_EQUAL_UINT32(nestedMethodRefToken, tokenRecords[3].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(methodSpecToken, tokenRecords[4].token);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[4].relatedToken);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[4].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, tokenRecords[4].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(0u, tokenRecords[4].signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(15u, tokenRecords[4].signatureBlobLength);
    TEST_ASSERT_TRUE(tokenRecords[4].signatureHash != 0u);
    TEST_ASSERT_TRUE(tokenRecords[4].signatureHash != 0x2222333344445555ull);
    TEST_ASSERT_EQUAL_UINT32(methodSpecToken, methodSpecs[0].token);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, methodSpecs[0].methodToken);
    TEST_ASSERT_EQUAL_UINT32(0u, methodSpecs[0].instantiationBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(15u, methodSpecs[0].instantiationBlobLength);
    TEST_ASSERT_EQUAL_UINT64(tokenRecords[4].signatureHash, methodSpecs[0].instantiationHash);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, read_u32_le(signatureBlobPool + 2u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 6u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, signatureBlobPool[10]);
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 11u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_drops_orphan_imported_member_ref_method_token(void) {
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
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_imported_member_ref_method_spec_fixture(blob,
                                                                   sizeof(blob),
                                                                   ZR_FALSE,
                                                                   ZR_FALSE,
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
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_drops_imported_member_ref_with_pruned_target(void) {
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
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_imported_member_ref_method_spec_fixture(blob,
                                                                   sizeof(blob),
                                                                   ZR_TRUE,
                                                                   ZR_TRUE,
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
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_remaps_typeref_string_offset(void) {
    TZrByte blob[1024];
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedArgTypeNameOffset;
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView signatureBlobView;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);

    originalLength = build_imported_member_ref_method_spec_typeref_fixture(blob,
                                                                           sizeof(blob),
                                                                           &expectedStringPoolBytes,
                                                                           &expectedArgTypeNameOffset,
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

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(19u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    signatureBlobPool = signatureBlobView.data;
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0,
                          memcmp("ExternalArg",
                                 stringPoolView.data + expectedArgTypeNameOffset,
                                 sizeof("ExternalArg")));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, read_u32_le(signatureBlobPool + 2u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 6u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_TYPE_REF, signatureBlobPool[10]);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, read_u32_le(signatureBlobPool + 11u));
    TEST_ASSERT_EQUAL_UINT32(expectedArgTypeNameOffset, read_u32_le(signatureBlobPool + 15u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_remaps_module_string_offsets(void) {
    TZrByte blob[1024];
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedModuleNameOffset;
    TZrUInt32 expectedModuleVersionOffset;
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView signatureBlobView;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);

    originalLength = build_imported_member_ref_method_spec_module_fixture(blob,
                                                                          sizeof(blob),
                                                                          &expectedStringPoolBytes,
                                                                          &expectedModuleNameOffset,
                                                                          &expectedModuleVersionOffset,
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

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(19u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    signatureBlobPool = signatureBlobView.data;
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0,
                          memcmp("__entry",
                                 stringPoolView.data + expectedModuleNameOffset,
                                 sizeof("__entry")));
    TEST_ASSERT_EQUAL_INT(0,
                          memcmp("1.0.0",
                                 stringPoolView.data + expectedModuleVersionOffset,
                                 sizeof("1.0.0")));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, read_u32_le(signatureBlobPool + 2u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 6u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MODULE, signatureBlobPool[10]);
    TEST_ASSERT_EQUAL_UINT32(expectedModuleNameOffset, read_u32_le(signatureBlobPool + 11u));
    TEST_ASSERT_EQUAL_UINT32(expectedModuleVersionOffset, read_u32_le(signatureBlobPool + 15u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_remaps_union_base_name_string_offset(void) {
    TZrByte blob[1024];
    TZrUInt32 expectedStringPoolBytes;
    TZrUInt32 expectedUnionBaseNameOffset;
    TZrSize expectedPrunedLength;
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView stringPoolView;
    SZrZrpMetadataSectionView signatureBlobView;
    const TZrByte *signatureBlobPool;
    const TZrMetadataToken importedMethodRefToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u);

    originalLength = build_imported_member_ref_method_spec_union_fixture(blob,
                                                                         sizeof(blob),
                                                                         &expectedStringPoolBytes,
                                                                         &expectedUnionBaseNameOffset,
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

    TEST_ASSERT_EQUAL_UINT32(4u, header.tokenRecords.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, header.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(28u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                       &stringPoolView));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                       &signatureBlobView));

    signatureBlobPool = signatureBlobView.data;
    TEST_ASSERT_EQUAL_UINT32(expectedStringPoolBytes, stringPoolView.byteLength);
    TEST_ASSERT_EQUAL_INT(0,
                          memcmp("Option",
                                 stringPoolView.data + expectedUnionBaseNameOffset,
                                 sizeof("Option")));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, signatureBlobPool[0]);
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, signatureBlobPool[1]);
    TEST_ASSERT_EQUAL_UINT32(importedMethodRefToken, read_u32_le(signatureBlobPool + 2u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 6u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_UNION, signatureBlobPool[10]);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, read_u32_le(signatureBlobPool + 11u));
    TEST_ASSERT_EQUAL_UINT32(expectedUnionBaseNameOffset, read_u32_le(signatureBlobPool + 15u));
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 19u));
    TEST_ASSERT_EQUAL_UINT8((TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, signatureBlobPool[23]);
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(signatureBlobPool + 24u));

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

static void test_aot_c_zrp_metadata_methodspec_pruning_drops_nested_imported_member_ref_with_pruned_target(void) {
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
    const TZrMetadataToken typeToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken compactedMethodToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    originalLength = build_nested_imported_member_ref_method_spec_fixture(blob,
                                                                          sizeof(blob),
                                                                          ZR_TRUE,
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
    TEST_ASSERT_EQUAL_UINT32(1u, header.typeDefs.count);
    TEST_ASSERT_EQUAL_UINT32(1u, header.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.methodSpecs.count);
    TEST_ASSERT_EQUAL_UINT32(0u, header.signatureBlobPool.byteLength);

    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_GetSectionView(prunedMetadata.blob,
                                                       prunedMetadata.length,
                                                       &header,
                                                       ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS,
                                                       &tokenView));

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].token);
    TEST_ASSERT_EQUAL_UINT32(typeToken, tokenRecords[1].ownerToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, tokenRecords[1].targetMetadataToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_keeps_imported_member_ref_method_token);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_keeps_nested_imported_member_ref_method_token);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_drops_orphan_imported_member_ref_method_token);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_drops_imported_member_ref_with_pruned_target);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_drops_nested_imported_member_ref_with_pruned_target);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_remaps_typeref_string_offset);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_remaps_module_string_offsets);
    RUN_TEST(test_aot_c_zrp_metadata_methodspec_pruning_remaps_union_base_name_string_offset);
    return UNITY_END();
}
