#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

#ifndef ZR_VM_TESTS_C_COMPILER
#define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

void setUp(void) {}

void tearDown(void) {}

static SZrFunction *compile_source(SZrState *state, const TZrChar *source, const TZrChar *sourceNameText) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString) sourceNameText);

    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode == (TZrUInt16) opcode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void assert_reflection_spread_call_ast(SZrState *state, const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "reflection_construction_ast.zr");
    SZrAstNode *script = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrAstNode *declaration;
    SZrAstNode *expression;
    SZrAstNode *call;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(7u, script->data.script.statements->count);
    declaration = script->data.script.statements->nodes[5];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    expression = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT32(2u, expression->data.primaryExpression.members->count);
    call = expression->data.primaryExpression.members->nodes[1];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT32(1u, call->data.functionCall.args->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SPREAD_ARGUMENT, call->data.functionCall.args->nodes[0]->type);
    ZrParser_Ast_Free(state, script);
}

static void write_text_file_or_fail(const TZrChar *path, const TZrChar *text) {
    FILE *file;

    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file = fopen(path, "rb");
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1u, sizeof(chunk), file)) > 0u) {
        for (TZrSize index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long) hash);
}

static void test_reflection_construction_executes_equivalently_in_vm_and_aot_c(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C reflection construction execution currently validates the Unix shared-library path");
#else
    static const TZrChar *source = "pub class Box {\n"
                                   "  pub var value: int;\n"
                                   "  pub @constructor(value: int) { this.value = value; }\n"
                                   "}\n"
                                   "let seed = new Box(0);\n"
                                   "let descriptor = typeof(seed);\n"
                                   "let direct = descriptor.createInstance(40);\n"
                                   "let constructionArgs = [2];\n"
                                   "let spread = descriptor.createInstance(...constructionArgs);\n"
                                   "return direct.value + spread.value;\n";
    static const TZrChar *projectJson = "{"
                                        "\"name\":\"aot-reflection-construction\","
                                        "\"source\":\"src\","
                                        "\"binary\":\"bin\","
                                        "\"entry\":\"main\""
                                        "}";
    SZrState *vmState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrState *aotState;
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue aotResult;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrInt64 vmResult = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar command[4096];

    TEST_ASSERT_NOT_NULL(vmState);
    assert_reflection_spread_call_ast(vmState, source);
    function = compile_source(vmState, source, "reflection_construction_vm.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD)));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(vmState, function, &vmResult));
    TEST_ASSERT_EQUAL_INT64(42, vmResult);
    ZrCore_Function_Free(vmState, function);
    ZrTests_Runtime_State_Destroy(vmState);

    aotState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(aotState);
    function = compile_source(aotState, source, "reflection_construction_aot.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_construction", "project",
                                                       "reflection_construction", ".zrp", projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_construction", "project/src", "main", ".zr",
                                                       sourcePath, sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_construction", "project/bin", "main", ".zro",
                                                       zroPath, sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_construction", "project/bin/aot_c/src", "main",
                                                       ".c", generatedCPath, sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_construction", "project/bin/aot_c/lib",
                                                       "zrvm_aot_main", ".so", sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));
    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(aotState, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(aotState, function, generatedCPath, &aotOptions));

    snprintf(command, sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -o \"%s\"",
             ZR_VM_TESTS_C_COMPILER, ZR_VM_TESTS_REPO_ROOT, ZR_VM_TESTS_REPO_ROOT, ZR_VM_TESTS_REPO_ROOT,
             generatedCPath, ZR_VM_TESTS_BUILD_LIB_DIR, ZR_VM_TESTS_BUILD_LIB_DIR, sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, system(command));

    ZrCore_Function_Free(aotState, function);
    project = ZrLibrary_Project_New(aotState, (TZrNativeString) projectJson, (TZrNativeString) projectPath);
    TEST_ASSERT_NOT_NULL(project);
    aotState->global->userData = project;
    TEST_ASSERT_TRUE(
            ZrLibrary_AotRuntime_ConfigureGlobal(aotState->global, ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C, ZR_TRUE));
    ZrCore_Value_ResetAsNull(&aotResult);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(aotState, ZR_AOT_BACKEND_KIND_C, &aotResult),
                             ZrLibrary_AotRuntime_GetLastError(aotState->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(aotResult.type));
    TEST_ASSERT_EQUAL_INT64(vmResult, aotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(aotState->global));

    aotState->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(aotState, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(aotState);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reflection_construction_executes_equivalently_in_vm_and_aot_c);
    return UNITY_END();
}
