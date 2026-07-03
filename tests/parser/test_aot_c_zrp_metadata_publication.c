#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}

void tearDown(void) {}

static void set_section(SZrZrpMetadataSection *section,
                        TZrUInt32 *offset,
                        TZrUInt32 byteLength,
                        TZrUInt32 count,
                        TZrUInt32 elementSize) {
    TEST_ASSERT_NOT_NULL(section);
    TEST_ASSERT_NOT_NULL(offset);

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

static TZrInstruction make_instruction_2(EZrInstructionCode opcode,
                                         TZrUInt16 operandExtra,
                                         TZrUInt16 operandA,
                                         TZrUInt16 operandB) {
    TZrInstruction instruction;

    instruction.value = 0u;
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static SZrFunction *create_two_child_trim_fixture(SZrState *state) {
    SZrFunction *root;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = make_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
    root->instructionsLength = 1u;
    root->stackSize = 1u;
    root->parameterCount = 0u;
    root->lineInSourceStart = 1u;
    root->lineInSourceEnd = 1u;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction) * 2u);
    root->childFunctionLength = 2u;

    root->childFunctionList[0].parameterCount = 0u;
    root->childFunctionList[0].stackSize = 1u;
    root->childFunctionList[0].ownerFunction = root;
    root->childFunctionList[0].lineInSourceStart = 10u;
    root->childFunctionList[0].lineInSourceEnd = 10u;

    root->childFunctionList[1].parameterCount = 0u;
    root->childFunctionList[1].stackSize = 1u;
    root->childFunctionList[1].ownerFunction = root;
    root->childFunctionList[1].lineInSourceStart = 20u;
    root->childFunctionList[1].lineInSourceEnd = 20u;
    return root;
}

static TZrSize build_zrp_metadata_method_def_trim_fixture(TZrByte *buffer,
                                                          TZrSize bufferLength,
                                                          TZrSize *outExpectedPublishedLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefRowBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodDefBytesBeforeTrim = methodDefRowBytes * 2u;
    const TZrUInt32 stringPoolBytesBeforeTrim = 6u;
    const TZrUInt32 stringPoolBytesAfterTrim = 1u;
    const TZrUInt32 signatureBlobPoolBytesBeforeTrim = 7u;
    const TZrUInt32 constantPoolBytesBeforeTrim = 5u;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(outExpectedPublishedLength);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           tokenRecordBytes +
                                           typeDefBytes +
                                           methodDefBytesBeforeTrim +
                                           stringPoolBytesBeforeTrim +
                                           signatureBlobPoolBytesBeforeTrim +
                                           constantPoolBytesBeforeTrim);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes, 1u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytesBeforeTrim, 2u, methodDefRowBytes);
    set_section(&header.fieldDefs, &offset, 0u, 0u, 0u);
    set_section(&header.genericParams, &offset, 0u, 0u, 0u);
    set_section(&header.genericParamConstraints, &offset, 0u, 0u, 0u);
    set_section(&header.typeSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.methodSpecs, &offset, 0u, 0u, 0u);
    set_section(&header.moduleRefs, &offset, 0u, 0u, 0u);
    set_section(&header.stringPool, &offset, stringPoolBytesBeforeTrim, stringPoolBytesBeforeTrim, 1u);
    set_section(&header.signatureBlobPool,
                &offset,
                signatureBlobPoolBytesBeforeTrim,
                signatureBlobPoolBytesBeforeTrim,
                1u);
    set_section(&header.constantPool, &offset, constantPoolBytesBeforeTrim, constantPoolBytesBeforeTrim, 1u);

    memset(buffer, 0, bufferLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(buffer, offset, &header));

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    methodDefs[0].ownerTypeToken = typeDefs[0].token;
    methodDefs[0].functionIndex = 2u;
    methodDefs[1].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    methodDefs[1].ownerTypeToken = typeDefs[0].token;
    methodDefs[1].functionIndex = 1u;

    *outExpectedPublishedLength =
            (TZrSize)(offset -
                      methodDefRowBytes -
                      (stringPoolBytesBeforeTrim - stringPoolBytesAfterTrim) -
                      signatureBlobPoolBytesBeforeTrim -
                      constantPoolBytesBeforeTrim);
    return offset;
}

static TZrSize build_zrp_metadata_invalid_definition_tables_fixture(TZrByte *buffer, TZrSize bufferLength) {
    const TZrUInt32 methodDefRowBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    SZrZrpMetadataHeader header;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE + methodDefRowBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, 0u, 0u, 0u);
    set_section(&header.typeDefs, &offset, 0u, 0u, 0u);
    set_section(&header.methodDefs, &offset, methodDefRowBytes, 1u, methodDefRowBytes);
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
    return offset;
}

static TZrBool write_binary_file(const TZrChar *path, const TZrByte *bytes, TZrSize length) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || bytes == ZR_NULL || length == 0u) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(bytes, 1u, (size_t)length, file);
    fclose(file);
    return (TZrBool)(written == (size_t)length);
}

static void test_aot_c_writer_publishes_compacted_zrp_metadata_file(void) {
    TZrByte metadataBlob[768];
    TZrSize metadataBytesBeforeTrim;
    TZrSize expectedPublishedLength;
    TZrBytePtr publishedBytes = ZR_NULL;
    TZrSize publishedLength = 0u;
    SZrZrpMetadataHeader publishedHeader;
    const SZrZrpMetadataMethodDefRow *publishedMethodDefs;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar compactedMetadataPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_two_child_trim_fixture(state);
    metadataBytesBeforeTrim = build_zrp_metadata_method_def_trim_fixture(metadataBlob,
                                                                         sizeof(metadataBlob),
                                                                         &expectedPublishedLength);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_zrp_metadata_publication",
                                                       "generated",
                                                       "metadata_publication",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_zrp_metadata_publication",
                                                       "generated",
                                                       "metadata_publication",
                                                       ".zrp",
                                                       compactedMetadataPath,
                                                       sizeof(compactedMetadataPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_zrp_metadata_publication";
    options.sourceHash = "aot-c-zrp-metadata-publication";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-zrp-metadata-publication";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytesBeforeTrim;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.compactedZrpMetadataOutputPath = compactedMetadataPath;

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(compactedMetadataPath, &publishedBytes, &publishedLength));
    TEST_ASSERT_EQUAL_UINT64(expectedPublishedLength, publishedLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(publishedBytes, publishedLength, &publishedHeader));
    TEST_ASSERT_EQUAL_UINT32(1u, publishedHeader.methodDefs.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow),
                             publishedHeader.methodDefs.elementSize);

    publishedMethodDefs = (const SZrZrpMetadataMethodDefRow *)(const void *)(publishedBytes +
                                                                             publishedHeader.methodDefs.offset);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u),
                             publishedMethodDefs[0].token);
    TEST_ASSERT_EQUAL_UINT32(1u, publishedMethodDefs[0].functionIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, publishedHeader.stringPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(0u, publishedHeader.signatureBlobPool.byteLength);
    TEST_ASSERT_EQUAL_UINT32(0u, publishedHeader.constantPool.byteLength);

    free(publishedBytes);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_writer_rejects_invalid_compacted_zrp_metadata_sidecar(void) {
    static const TZrByte staleSidecarBytes[] = {'s', 't', 'a', 'l', 'e'};
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    TZrBytePtr publishedBytes = ZR_NULL;
    TZrSize publishedLength = 0u;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar compactedMetadataPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_two_child_trim_fixture(state);
    metadataBytes = build_zrp_metadata_invalid_definition_tables_fixture(metadataBlob, sizeof(metadataBlob));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_zrp_metadata_publication",
                                                       "generated",
                                                       "invalid_metadata_publication",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_zrp_metadata_publication",
                                                       "generated",
                                                       "invalid_metadata_publication",
                                                       ".zrp",
                                                       compactedMetadataPath,
                                                       sizeof(compactedMetadataPath)));
    remove(generatedCPath);
    remove(compactedMetadataPath);
    TEST_ASSERT_TRUE(write_binary_file(compactedMetadataPath,
                                       staleSidecarBytes,
                                       sizeof(staleSidecarBytes)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_zrp_metadata_publication";
    options.sourceHash = "aot-c-zrp-metadata-publication-invalid";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = "aot-c-zrp-metadata-publication-invalid";
    options.embeddedModuleBlob = metadataBlob;
    options.embeddedModuleBlobLength = metadataBytes;
    options.requireExecutableLowering = ZR_TRUE;
    options.compactedZrpMetadataOutputPath = compactedMetadataPath;

    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    TEST_ASSERT_FALSE(ZrTests_ReadFileBytes(compactedMetadataPath, &publishedBytes, &publishedLength));

    free(publishedBytes);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_writer_publishes_compacted_zrp_metadata_file);
    RUN_TEST(test_aot_c_writer_rejects_invalid_compacted_zrp_metadata_sidecar);
    return UNITY_END();
}
