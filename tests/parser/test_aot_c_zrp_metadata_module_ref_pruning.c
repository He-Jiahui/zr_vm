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

static TZrSize build_constraint_rooted_module_ref_fixture(TZrByte *buffer, TZrSize bufferLength) {
    static const TZrByte stringPool[] = "Provider\0" "1.2.3";
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 genericParamBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow);
    const TZrUInt32 constraintBytes = (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow);
    const TZrUInt32 typeSpecBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow);
    const TZrUInt32 moduleRefBytes = (TZrUInt32)sizeof(SZrZrpMetadataModuleRefRow);
    const TZrUInt32 signatureBytes = 5u;
    const TZrUInt32 stringPoolBytes = (TZrUInt32)sizeof(stringPool);
    const TZrUInt32 providerNameOffset = 0u;
    const TZrUInt32 providerVersionOffset = (TZrUInt32)sizeof("Provider");
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken moduleRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);
    const TZrMetadataToken typeRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u);
    const TZrMetadataToken typeSpecToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    SZrZrpMetadataHeader header;
    SZrMetadataTokenRecord *tokenRecords;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    SZrZrpMetadataGenericParamRow *genericParams;
    SZrZrpMetadataGenericParamConstraintRow *constraints;
    SZrZrpMetadataTypeSpecRow *typeSpecs;
    SZrZrpMetadataModuleRefRow *moduleRefs;
    TZrByte *signatureBlobTarget;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           (tokenRecordBytes * 3u) +
                                           typeDefBytes +
                                           methodDefBytes +
                                           genericParamBytes +
                                           constraintBytes +
                                           typeSpecBytes +
                                           moduleRefBytes +
                                           stringPoolBytes +
                                           signatureBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes * 3u, 3u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 1u, methodDefBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, genericParamBytes, 1u, genericParamBytes);
    set_section(&header.genericParamConstraints, &offset, constraintBytes, 1u, constraintBytes);
    set_section(&header.typeSpecs, &offset, typeSpecBytes, 1u, typeSpecBytes);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, moduleRefBytes, 1u, moduleRefBytes);
    set_section(&header.stringPool, &offset, stringPoolBytes, stringPoolBytes, 1u);
    set_section(&header.signatureBlobPool, &offset, signatureBytes, signatureBytes, 1u);
    set_section(&header.constantPool, &offset, 0u, 0u, 0u);
    set_section(&header.manifestExports, &offset, 0u, 0u, 0u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));
    memcpy(buffer + header.stringPool.offset, stringPool, stringPoolBytes);

    tokenRecords = (SZrMetadataTokenRecord *)(void *)(buffer + header.tokenRecords.offset);
    tokenRecords[0].token = typeToken;
    tokenRecords[1].token = methodToken;
    tokenRecords[1].ownerToken = typeToken;
    tokenRecords[1].targetMetadataToken = methodToken;
    tokenRecords[2].token = typeRefToken;
    tokenRecords[2].relatedToken = moduleRefToken;
    tokenRecords[2].targetMetadataToken = typeSpecToken;

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = typeToken;
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 1u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = methodToken;
    methodDefs[0].ownerTypeToken = typeToken;
    methodDefs[0].signatureBlobOffset = 0u;
    methodDefs[0].signatureBlobLength = signatureBytes;
    methodDefs[0].functionIndex = 1u;
    methodDefs[0].firstGenericParamIndex = 0u;
    methodDefs[0].genericParamCount = 1u;

    genericParams = (SZrZrpMetadataGenericParamRow *)(void *)(buffer + header.genericParams.offset);
    genericParams[0].ownerToken = methodToken;
    genericParams[0].firstConstraintIndex = 0u;
    genericParams[0].constraintCount = 1u;

    constraints = (SZrZrpMetadataGenericParamConstraintRow *)(void *)(buffer + header.genericParamConstraints.offset);
    constraints[0].genericParamIndex = 0u;
    constraints[0].constraintTypeToken = typeSpecToken;
    constraints[0].signatureBlobOffset = 0u;
    constraints[0].signatureBlobLength = signatureBytes;

    typeSpecs = (SZrZrpMetadataTypeSpecRow *)(void *)(buffer + header.typeSpecs.offset);
    typeSpecs[0].token = typeSpecToken;
    typeSpecs[0].signatureBlobOffset = 0u;
    typeSpecs[0].signatureBlobLength = signatureBytes;
    typeSpecs[0].typeLayoutId = 77u;
    typeSpecs[0].signatureHash = 0x1111222233334444ull;

    moduleRefs = (SZrZrpMetadataModuleRefRow *)(void *)(buffer + header.moduleRefs.offset);
    moduleRefs[0].token = moduleRefToken;
    moduleRefs[0].nameStringOffset = providerNameOffset;
    moduleRefs[0].versionStringOffset = providerVersionOffset;
    moduleRefs[0].moduleSignatureHash = 0x5555666677778888ull;

    signatureBlobTarget = buffer + header.signatureBlobPool.offset;
    signatureBlobTarget[0] = (TZrByte)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE;
    write_u32_le(signatureBlobTarget + 1u, 7u);

    return offset;
}

static void test_aot_c_zrp_module_ref_pruning_keeps_generic_constraint_rooted_import_ref(void) {
    TZrByte blob[1280];
    TZrSize originalLength;
    SZrAotWriterOptions options;
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata prunedMetadata;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView tokenView;
    SZrZrpMetadataSectionView moduleRefView;
    const SZrMetadataTokenRecord *tokenRecords;
    const SZrZrpMetadataModuleRefRow *moduleRefs;
    const TZrMetadataToken compactedModuleRefToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u);

    originalLength = build_constraint_rooted_module_ref_fixture(blob, sizeof(blob));

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
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(prunedMetadata.blob, prunedMetadata.length, &header));
    TEST_ASSERT_EQUAL_UINT32(1u, header.moduleRefs.count);

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

    tokenRecords = (const SZrMetadataTokenRecord *)(const void *)tokenView.data;
    moduleRefs = (const SZrZrpMetadataModuleRefRow *)(const void *)moduleRefView.data;

    TEST_ASSERT_EQUAL_UINT32(compactedModuleRefToken, moduleRefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(compactedModuleRefToken, tokenRecords[2].relatedToken);

    backend_aot_c_release_embedded_zrp_metadata(&prunedMetadata);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_module_ref_pruning_keeps_generic_constraint_rooted_import_ref);
    return UNITY_END();
}
