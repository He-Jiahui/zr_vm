#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "compiler/compiler_aot.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "project/project.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"
#include "zr_vm_library/project.h"

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

static SZrFunction *create_two_child_fixture(SZrState *state) {
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

static TZrSize build_zrp_metadata_method_def_fixture(TZrByte *buffer, TZrSize bufferLength) {
    const TZrUInt32 tokenRecordBytes = (TZrUInt32)sizeof(SZrMetadataTokenRecord);
    const TZrUInt32 typeDefBytes = (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow);
    const TZrUInt32 methodDefRowBytes = (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow);
    const TZrUInt32 methodDefBytes = methodDefRowBytes * 2u;
    const TZrUInt32 stringPoolBytes = 6u;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefs;
    SZrZrpMetadataMethodDefRow *methodDefs;
    TZrUInt32 offset = ZR_ZRP_METADATA_HEADER_SIZE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength >= ZR_ZRP_METADATA_HEADER_SIZE +
                                           tokenRecordBytes +
                                           typeDefBytes +
                                           methodDefBytes +
                                           stringPoolBytes);

    ZrCore_ZrpMetadata_InitHeader(&header);
    set_section(&header.tokenRecords, &offset, tokenRecordBytes, 1u, tokenRecordBytes);
    set_section(&header.typeDefs, &offset, typeDefBytes, 1u, typeDefBytes);
    set_section(&header.methodDefs, &offset, methodDefBytes, 2u, methodDefRowBytes);
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

    typeDefs = (SZrZrpMetadataTypeDefRow *)(void *)(buffer + header.typeDefs.offset);
    typeDefs[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    typeDefs[0].firstMethodDefIndex = 0u;
    typeDefs[0].methodDefCount = 2u;

    methodDefs = (SZrZrpMetadataMethodDefRow *)(void *)(buffer + header.methodDefs.offset);
    methodDefs[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    methodDefs[0].ownerTypeToken = typeDefs[0].token;
    methodDefs[0].functionIndex = 1u;
    methodDefs[1].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    methodDefs[1].ownerTypeToken = typeDefs[0].token;
    methodDefs[1].functionIndex = 2u;

    memcpy(buffer + header.stringPool.offset, "\0main", stringPoolBytes);
    return offset;
}

static TZrSize build_zrp_metadata_invalid_method_def_fixture(TZrByte *buffer, TZrSize bufferLength) {
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

    if (path == ZR_NULL || bytes == ZR_NULL || length == 0u ||
        !ZrCli_Project_EnsureParentDirectory(path)) {
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

static int make_directory(const TZrChar *path) {
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

static int remove_directory(const TZrChar *path) {
#if defined(_WIN32)
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

static TZrBool append_path_segment(const TZrChar *basePath,
                                   const TZrChar *segment,
                                   TZrChar *output,
                                   TZrSize outputLength) {
    int written;

    if (basePath == ZR_NULL || segment == ZR_NULL || output == ZR_NULL || outputLength == 0u) {
        return ZR_FALSE;
    }

    written = snprintf(output, (size_t)outputLength, "%s/%s", basePath, segment);
    return (TZrBool)(written > 0 && (TZrSize)written < outputLength);
}

static void test_cli_project_derives_compacted_metadata_sidecar_path_from_aot_c_path(void) {
    TZrChar metadataPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_TRUE(ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath(
            "build/bin/aot_c/src/tools/seed.c",
            metadataPath,
            sizeof(metadataPath)));
    TEST_ASSERT_EQUAL_STRING("build/bin/aot_c/src/tools/seed.zrp", metadataPath);
}

static void test_cli_aot_writer_publishes_metadata_sidecar_for_zrp_input_blob(void) {
    TZrByte metadataBlob[768];
    TZrSize metadataBytes;
    TZrBytePtr publishedBytes = ZR_NULL;
    TZrSize publishedLength = 0u;
    SZrZrpMetadataHeader publishedHeader;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrCliProjectContext projectContext;
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar compactedMetadataPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_two_child_fixture(state);
    metadataBytes = build_zrp_metadata_method_def_fixture(metadataBlob, sizeof(metadataBlob));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "input",
                                                       "module",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "generated",
                                                       "module",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath(generatedCPath,
                                                                               compactedMetadataPath,
                                                                               sizeof(compactedMetadataPath)));

    remove(zroPath);
    remove(generatedCPath);
    remove(compactedMetadataPath);
    TEST_ASSERT_TRUE(write_binary_file(zroPath, metadataBlob, metadataBytes));

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&projectContext, 0, sizeof(projectContext));
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    projectContext.libraryProject = &libraryProject;

    TEST_ASSERT_TRUE(ZrCli_Compiler_WriteAotCFileForModule(&projectContext,
                                                           state,
                                                           function,
                                                           "main",
                                                           "source-hash",
                                                           "zro-hash",
                                                           zroPath,
                                                           generatedCPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(generatedCPath));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(compactedMetadataPath, &publishedBytes, &publishedLength));
    TEST_ASSERT_EQUAL_UINT64(metadataBytes, publishedLength);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_ReadHeader(publishedBytes, publishedLength, &publishedHeader));
    TEST_ASSERT_EQUAL_UINT32(2u, publishedHeader.methodDefs.count);

    free(publishedBytes);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_does_not_publish_sidecar_for_invalid_zrp_definition_tables(void) {
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrCliProjectContext projectContext;
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar compactedMetadataPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_two_child_fixture(state);
    metadataBytes = build_zrp_metadata_invalid_method_def_fixture(metadataBlob, sizeof(metadataBlob));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "input",
                                                       "invalid_definition_tables",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "generated",
                                                       "invalid_definition_tables",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath(generatedCPath,
                                                                               compactedMetadataPath,
                                                                               sizeof(compactedMetadataPath)));

    remove(zroPath);
    remove(generatedCPath);
    remove(compactedMetadataPath);
    TEST_ASSERT_TRUE(write_binary_file(zroPath, metadataBlob, metadataBytes));

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&projectContext, 0, sizeof(projectContext));
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    projectContext.libraryProject = &libraryProject;

    TEST_ASSERT_TRUE(ZrCli_Compiler_WriteAotCFileForModule(&projectContext,
                                                           state,
                                                           function,
                                                           "main",
                                                           "source-hash",
                                                           "zro-hash",
                                                           zroPath,
                                                           generatedCPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(generatedCPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(compactedMetadataPath));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_removes_stale_sidecar_when_metadata_is_not_publishable(void) {
    static const TZrByte staleSidecarBytes[] = {'s', 't', 'a', 'l', 'e'};
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrCliProjectContext projectContext;
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar compactedMetadataPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_two_child_fixture(state);
    metadataBytes = build_zrp_metadata_invalid_method_def_fixture(metadataBlob, sizeof(metadataBlob));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "input",
                                                       "stale_invalid_metadata",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "generated",
                                                       "stale_invalid_metadata",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath(generatedCPath,
                                                                               compactedMetadataPath,
                                                                               sizeof(compactedMetadataPath)));

    remove(zroPath);
    remove(generatedCPath);
    remove(compactedMetadataPath);
    TEST_ASSERT_TRUE(write_binary_file(zroPath, metadataBlob, metadataBytes));
    TEST_ASSERT_TRUE(write_binary_file(compactedMetadataPath,
                                       staleSidecarBytes,
                                       sizeof(staleSidecarBytes)));

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&projectContext, 0, sizeof(projectContext));
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    projectContext.libraryProject = &libraryProject;

    TEST_ASSERT_TRUE(ZrCli_Compiler_WriteAotCFileForModule(&projectContext,
                                                           state,
                                                           function,
                                                           "main",
                                                           "source-hash",
                                                           "zro-hash",
                                                           zroPath,
                                                           generatedCPath));
    TEST_ASSERT_TRUE(ZrTests_File_Exists(generatedCPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(compactedMetadataPath));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_cli_aot_writer_fails_when_stale_sidecar_cannot_be_removed(void) {
    static const TZrByte staleSidecarBytes[] = {'s', 't', 'a', 'l', 'e'};
    TZrByte metadataBlob[512];
    TZrSize metadataBytes;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project libraryProject;
    SZrCliProjectContext projectContext;
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar compactedMetadataPath[ZR_TESTS_PATH_MAX];
    TZrChar blockedChildPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = create_two_child_fixture(state);
    metadataBytes = build_zrp_metadata_invalid_method_def_fixture(metadataBlob, sizeof(metadataBlob));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "input",
                                                       "blocked_stale_invalid_metadata",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli_aot_compacted_metadata_sidecar",
                                                       "generated",
                                                       "blocked_stale_invalid_metadata",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath(generatedCPath,
                                                                               compactedMetadataPath,
                                                                               sizeof(compactedMetadataPath)));
    TEST_ASSERT_TRUE(append_path_segment(compactedMetadataPath,
                                         "locked.bin",
                                         blockedChildPath,
                                         sizeof(blockedChildPath)));

    remove(blockedChildPath);
    remove_directory(compactedMetadataPath);
    remove(zroPath);
    remove(generatedCPath);
    remove(compactedMetadataPath);
    TEST_ASSERT_TRUE(write_binary_file(zroPath, metadataBlob, metadataBytes));
    TEST_ASSERT_TRUE(ZrCli_Project_EnsureParentDirectory(compactedMetadataPath));
    TEST_ASSERT_EQUAL_INT(0, make_directory(compactedMetadataPath));
    TEST_ASSERT_TRUE(write_binary_file(blockedChildPath, staleSidecarBytes, sizeof(staleSidecarBytes)));

    memset(&libraryProject, 0, sizeof(libraryProject));
    memset(&projectContext, 0, sizeof(projectContext));
    libraryProject.aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    projectContext.libraryProject = &libraryProject;

    TEST_ASSERT_FALSE(ZrCli_Compiler_WriteAotCFileForModule(&projectContext,
                                                            state,
                                                            function,
                                                            "main",
                                                            "source-hash",
                                                            "zro-hash",
                                                            zroPath,
                                                            generatedCPath));
    TEST_ASSERT_FALSE(ZrTests_File_Exists(generatedCPath));

    remove(blockedChildPath);
    remove_directory(compactedMetadataPath);
    remove(generatedCPath);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cli_project_derives_compacted_metadata_sidecar_path_from_aot_c_path);
    RUN_TEST(test_cli_aot_writer_publishes_metadata_sidecar_for_zrp_input_blob);
    RUN_TEST(test_cli_aot_writer_does_not_publish_sidecar_for_invalid_zrp_definition_tables);
    RUN_TEST(test_cli_aot_writer_removes_stale_sidecar_when_metadata_is_not_publishable);
    RUN_TEST(test_cli_aot_writer_fails_when_stale_sidecar_cannot_be_removed);
    return UNITY_END();
}
