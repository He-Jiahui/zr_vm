#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include <dlfcn.h>
#endif

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
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

#if defined(ZR_PLATFORM_UNIX)
static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}

static void *load_symbol(void *library, const char *symbolName) {
    void *symbol;

    dlerror();
    symbol = dlsym(library, symbolName);
    if (symbol == NULL) {
        const char *error = dlerror();
        printf("dlsym(%s) failed: %s\n", symbolName, error != NULL ? error : "<unknown>");
    }
    return symbol;
}
#endif

void setUp(void) {}

void tearDown(void) {}

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static TZrInstruction create_jump_instruction(TZrInt32 offset) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(JUMP);
    instruction.instruction.operand.operand2[0] = offset;
    return instruction;
}

static SZrFunction *create_invalid_jump_boundary_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_jump_instruction(100);
    function->instructionsLength = 1u;

    function->stackSize = 1u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static TZrInstruction create_pending_control_instruction(EZrInstructionCode opcode,
                                                         TZrUInt16 sourceSlot,
                                                         TZrInt32 targetInstructionIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = sourceSlot;
    instruction.instruction.operand.operand2[0] = targetInstructionIndex;
    return instruction;
}

static TZrInstruction create_return_instruction(TZrUInt16 returnCount, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static SZrFunction *create_pending_control_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] =
            create_pending_control_instruction(ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN), 0u, 3);
    function->instructionsList[1] =
            create_pending_control_instruction(ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK), 0u, 3);
    function->instructionsList[2] =
            create_pending_control_instruction(ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE), 0u, 3);
    function->instructionsList[3] = create_return_instruction(1u, 0u);
    function->instructionsLength = 4u;
    function->stackSize = 1u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static void write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));

    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    TEST_ASSERT_NOT_NULL(path);
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    fileSize = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, fileSize);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    buffer = (char *)malloc((size_t)fileSize + 1u);
    TEST_ASSERT_NOT_NULL(buffer);
    if (fileSize > 0) {
        TEST_ASSERT_EQUAL_size_t((size_t)fileSize, fread(buffer, 1, (size_t)fileSize, file));
    }
    buffer[fileSize] = '\0';
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return buffer;
}

static void hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, bufferSize);

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        for (TZrSize index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static void test_aot_c_generated_source_compiles_and_exports_module_descriptor(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    static const TZrByte embeddedBlob[] = {0x7a, 0x72, 0x6f};
    const char *source =
            "pub fn answer(): int { return 42; }\n"
            "pub fn unsigned_answer(): uint { return 13; }\n"
            "pub fn truth(): bool { return true; }\n"
            "pub fn ratio(): float { return 2.5; }\n"
            "pub fn echo(value: int): int { return value; }\n"
            "pub fn echo_unsigned(value: uint): uint { return value; }\n"
            "pub fn echo_truth(value: bool): bool { return value; }\n"
            "pub fn echo_ratio(value: float): float { return value; }\n"
            "pub fn sum_values(left: int, right: int): int { return left + right; }\n"
            "pub fn sum_unsigned(left: uint, right: uint): uint { return left + right; }\n"
            "pub fn same_truth(left: bool, right: bool): bool { return left == right; }\n"
            "pub fn sum_ratio(left: float, right: float): float { return left + right; }\n"
            "pub fn less_values(left: int, right: int): bool { return left < right; }\n"
            "pub fn unsigned_after(left: uint, right: uint): bool { return left > right; }\n"
            "pub fn ratio_equal(left: float, right: float): bool { return left == right; }\n"
            "pub fn sum_three(left: int, middle: int, right: int): int { return left + middle + right; }\n"
            "pub fn sum_three_unsigned(left: uint, middle: uint, right: uint): uint { return left + middle + right; }\n"
            "pub fn sum_three_ratio(left: float, middle: float, right: float): float { return left + middle + right; }\n"
            "pub fn all_truth(left: bool, middle: bool, right: bool): bool { return left && middle && right; }\n"
            "pub var left: int = 40;\n"
            "pub var right: int = 2;\n"
            "return left + right;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    void *library;
    void *symbol;
    FZrVmGetAotCompiledModule getModule;
    const ZrAotCompiledModule *module;
    SZrTypeValue reflectionReturn;
    SZrTypeValue reflectionUnsignedReturn;
    SZrTypeValue reflectionBoolReturn;
    SZrTypeValue reflectionFloatReturn;
    SZrTypeValue reflectionI64OneArg;
    SZrTypeValue reflectionI64OneArgReturn;
    SZrTypeValue reflectionU64OneArg;
    SZrTypeValue reflectionU64OneArgReturn;
    SZrTypeValue reflectionBoolOneArg;
    SZrTypeValue reflectionBoolOneArgReturn;
    SZrTypeValue reflectionF64OneArg;
    SZrTypeValue reflectionF64OneArgReturn;
    SZrTypeValue reflectionI64TwoArgs[2];
    SZrTypeValue reflectionI64TwoArgReturn;
    SZrTypeValue reflectionU64TwoArgs[2];
    SZrTypeValue reflectionU64TwoArgReturn;
    SZrTypeValue reflectionBoolTwoArgs[2];
    SZrTypeValue reflectionBoolTwoArgReturn;
    SZrTypeValue reflectionF64TwoArgs[2];
    SZrTypeValue reflectionF64TwoArgReturn;
    SZrTypeValue reflectionBoolI64TwoArgs[2];
    SZrTypeValue reflectionBoolI64TwoArgReturn;
    SZrTypeValue reflectionBoolU64TwoArgs[2];
    SZrTypeValue reflectionBoolU64TwoArgReturn;
    SZrTypeValue reflectionBoolF64TwoArgs[2];
    SZrTypeValue reflectionBoolF64TwoArgReturn;
    SZrTypeValue reflectionI64ThreeArgs[3];
    SZrTypeValue reflectionI64ThreeArgReturn;
    SZrTypeValue reflectionU64ThreeArgs[3];
    SZrTypeValue reflectionU64ThreeArgReturn;
    SZrTypeValue reflectionF64ThreeArgs[3];
    SZrTypeValue reflectionF64ThreeArgReturn;
    SZrTypeValue reflectionBoolThreeArgs[3];
    SZrTypeValue reflectionBoolThreeArgReturn;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_c_shared_library_smoke.zr");
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_shared_library_smoke";
    options.sourceHash = "source-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "source-smoke";
    options.embeddedModuleBlob = embeddedBlob;
    options.embeddedModuleBlobLength = sizeof(embeddedBlob);
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "src",
                                                       "aot_c_shared_library_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "lib",
                                                       "libaot_c_shared_library_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_publish_exports_boundary */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_core/value.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const TZrUInt32 zr_aot_method_tokens[] = {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "0x03000001u,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".methodTokens = zr_aot_method_tokens,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".methodTokenCount = "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_i64_no_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->parameterCount != 0u"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "switch (method->functionIndex)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 1u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_i64_fn_1();"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_InitAsInt(state, outReturn, zr_aot_return_value);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_u64_no_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 2u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_u64_fn_2();"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_InitAsUInt(state, outReturn, zr_aot_return_value);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_no_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 3u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_bool_fn_3();"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_InitAsBool(state, outReturn, zr_aot_return_value);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_no_arg(state, method, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_f64_no_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 4u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_f64_fn_4();"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_InitAsFloat(state, outReturn, zr_aot_return_value);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_f64_no_arg(state, method, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_i64_one_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->parameterCount != 1u"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->parameterTypes == ZR_NULL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args == ZR_NULL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_INT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 5u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_i64_fn_5(zr_aot_arg0);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_i64_one_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_u64_one_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 6u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrUInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeUInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_u64_fn_6(zr_aot_arg0);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_u64_one_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_one_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 7u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrBool zr_aot_arg0 = args[0].value.nativeObject.nativeBool;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_bool_fn_7(zr_aot_arg0);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_one_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_f64_one_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 8u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrFloat64 zr_aot_arg0 = args[0].value.nativeObject.nativeDouble;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_f64_fn_8(zr_aot_arg0);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_f64_one_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_i64_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->parameterCount != 2u"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_INT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 9u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_return_value = zr_aot_typed_i64_fn_9(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_i64_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_u64_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 10u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrUInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeUInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_u64_fn_10(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_u64_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 11u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrBool zr_aot_arg1 = args[1].value.nativeObject.nativeBool;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_bool_fn_11(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_f64_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 12u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrFloat64 zr_aot_arg1 = args[1].value.nativeObject.nativeDouble;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_f64_fn_12(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_f64_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_i64_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 13u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_bool_fn_13(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_i64_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_u64_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 14u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrUInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeUInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_bool_fn_14(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_u64_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_f64_two_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 15u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrFloat64 zr_aot_arg1 = args[1].value.nativeObject.nativeDouble;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_bool_fn_15(zr_aot_arg0, zr_aot_arg1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_f64_two_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_i64_three_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->parameterCount != 3u"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_INT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 16u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_i64_fn_16(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_i64_three_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_u64_three_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 17u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrUInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeUInt64;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_u64_fn_17(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_u64_three_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_f64_three_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 18u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrFloat64 zr_aot_arg2 = args[2].value.nativeObject.nativeDouble;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_f64_fn_18(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_f64_three_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrBool zr_aot_try_invoke_bool_three_arg("));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_BOOL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case 19u:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrBool zr_aot_arg2 = args[2].value.nativeObject.nativeBool;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_return_value = zr_aot_typed_bool_fn_19(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_try_invoke_bool_three_arg(state, method, args, outReturn))"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_publish_exports_direct */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Module_AddPubExport(state, frame.module"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_Copy(state, &zr_aot_published_value, zr_aot_export_value);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrClosureNative *zr_aot_export_closure = ZrCore_ClosureNative_New(state, 0);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Closure_FindOrCreateValue(state, frame.slotBase + zr_aot_closure_variable->index);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT module export closure capture materialization"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MaterializeModuleExportValue"));
    TEST_ASSERT_NULL(strstr(generatedCText, "TZrStackValuePointer zr_aot_result_slot;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_result_value;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_caller_result_value;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "execution_discard_exception_handlers_for_callinfo(state, zr_aot_call_info);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, zr_aot_call_info);"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    library = dlopen(sharedLibraryPath, RTLD_NOW | RTLD_LOCAL);
    if (library == NULL) {
        printf("dlopen(%s) failed: %s\n", sharedLibraryPath, dlerror());
    }
    TEST_ASSERT_NOT_NULL(library);

    symbol = load_symbol(library, "ZrVm_GetAotCompiledModule");
    TEST_ASSERT_NOT_NULL(symbol);
    memcpy(&getModule, &symbol, sizeof(getModule));
    module = getModule();
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_UINT32(ZR_VM_AOT_ABI_VERSION, module->abiVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_BACKEND_KIND_C, module->backendKind);
    TEST_ASSERT_EQUAL_STRING("aot_c_shared_library_smoke", module->moduleName);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_INPUT_KIND_SOURCE, module->inputKind);
    TEST_ASSERT_EQUAL_STRING("source-smoke", module->inputHash);
    TEST_ASSERT_NOT_NULL(module->embeddedModuleBlob);
    TEST_ASSERT_EQUAL_UINT64(sizeof(embeddedBlob), module->embeddedModuleBlobLength);
    TEST_ASSERT_NOT_NULL(module->functionThunks);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, module->functionThunkCount);
    TEST_ASSERT_NOT_NULL(module->entryThunk);
    TEST_ASSERT_NOT_NULL(module->codeRegistration);
    TEST_ASSERT_EQUAL_UINT32(module->functionThunkCount, module->codeRegistration->functionCount);
    TEST_ASSERT_EQUAL_PTR(module->functionThunks, module->codeRegistration->functionPointers);
    TEST_ASSERT_NOT_NULL(module->methodInfos);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(20u, module->methodInfoCount);
    TEST_ASSERT_EQUAL_UINT32(module->methodInfoCount, module->codeRegistration->methodInfoCount);
    TEST_ASSERT_EQUAL_PTR(module->methodInfos, module->codeRegistration->methodInfos);
    TEST_ASSERT_NOT_NULL(module->methodTokens);
    TEST_ASSERT_EQUAL_UINT32(module->methodInfoCount, module->methodTokenCount);
    TEST_ASSERT_EQUAL_PTR(module->methodTokens, module->codeRegistration->methodTokens);
    TEST_ASSERT_EQUAL_UINT32(module->methodTokenCount, module->codeRegistration->methodTokenCount);
    TEST_ASSERT_EQUAL_UINT32(0x03000001u, module->methodTokens[1]);
    TEST_ASSERT_EQUAL_UINT32(0x03000002u, module->methodTokens[2]);
    TEST_ASSERT_EQUAL_UINT32(0x03000003u, module->methodTokens[3]);
    TEST_ASSERT_EQUAL_UINT32(0x03000004u, module->methodTokens[4]);
    TEST_ASSERT_EQUAL_UINT32(0x03000005u, module->methodTokens[5]);
    TEST_ASSERT_EQUAL_UINT32(0x03000006u, module->methodTokens[6]);
    TEST_ASSERT_EQUAL_UINT32(0x03000007u, module->methodTokens[7]);
    TEST_ASSERT_EQUAL_UINT32(0x03000008u, module->methodTokens[8]);
    TEST_ASSERT_EQUAL_UINT32(0x03000009u, module->methodTokens[9]);
    TEST_ASSERT_EQUAL_UINT32(0x0300000Au, module->methodTokens[10]);
    TEST_ASSERT_EQUAL_UINT32(0x0300000Bu, module->methodTokens[11]);
    TEST_ASSERT_EQUAL_UINT32(0x0300000Cu, module->methodTokens[12]);
    TEST_ASSERT_EQUAL_UINT32(0x0300000Du, module->methodTokens[13]);
    TEST_ASSERT_EQUAL_UINT32(0x0300000Eu, module->methodTokens[14]);
    TEST_ASSERT_EQUAL_UINT32(0x0300000Fu, module->methodTokens[15]);
    TEST_ASSERT_EQUAL_UINT32(0x03000010u, module->methodTokens[16]);
    TEST_ASSERT_EQUAL_UINT32(0x03000011u, module->methodTokens[17]);
    TEST_ASSERT_EQUAL_UINT32(0x03000012u, module->methodTokens[18]);
    TEST_ASSERT_EQUAL_UINT32(0x03000013u, module->methodTokens[19]);
    TEST_ASSERT_NOT_NULL(module->codeRegistration->invokers);
    TEST_ASSERT_EQUAL_UINT32(1u, module->codeRegistration->invokerCount);
    TEST_ASSERT_NOT_NULL(module->methodInfos[0]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[0]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[0]->invoker);
    TEST_ASSERT_EQUAL_PTR(module->codeRegistration->invokers[0], module->methodInfos[0]->invoker);
    TEST_ASSERT_EQUAL_UINT8(ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING,
                            module->methodInfos[0]->reflectionMetadataLevel);
    TEST_ASSERT_NOT_NULL(module->methodInfos[1]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[1]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[1]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[1]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[1]);
    ZrCore_Value_ResetAsNull(&reflectionReturn);
    module->methodInfos[1]->invoker(state,
                                    module->functionThunks[1],
                                    module->methodInfos[1],
                                    ZR_NULL,
                                    ZR_NULL,
                                    &reflectionReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_INT64, (TZrUInt16)reflectionReturn.type);
    TEST_ASSERT_EQUAL_INT64(42, reflectionReturn.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[2]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[2]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[2]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[2]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[2]);
    ZrCore_Value_ResetAsNull(&reflectionUnsignedReturn);
    module->methodInfos[2]->invoker(state,
                                    module->functionThunks[2],
                                    module->methodInfos[2],
                                    ZR_NULL,
                                    ZR_NULL,
                                    &reflectionUnsignedReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_UINT64, (TZrUInt16)reflectionUnsignedReturn.type);
    TEST_ASSERT_EQUAL_UINT64(13u, reflectionUnsignedReturn.value.nativeObject.nativeUInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[3]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[3]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[3]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[3]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[3]);
    ZrCore_Value_ResetAsNull(&reflectionBoolReturn);
    module->methodInfos[3]->invoker(state,
                                    module->functionThunks[3],
                                    module->methodInfos[3],
                                    ZR_NULL,
                                    ZR_NULL,
                                    &reflectionBoolReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolReturn.type);
    TEST_ASSERT_TRUE(reflectionBoolReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NOT_NULL(module->methodInfos[4]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[4]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[4]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[4]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[4]);
    ZrCore_Value_ResetAsNull(&reflectionFloatReturn);
    module->methodInfos[4]->invoker(state,
                                    module->functionThunks[4],
                                    module->methodInfos[4],
                                    ZR_NULL,
                                    ZR_NULL,
                                    &reflectionFloatReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_DOUBLE, (TZrUInt16)reflectionFloatReturn.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 2.5, reflectionFloatReturn.value.nativeObject.nativeDouble);
    TEST_ASSERT_NOT_NULL(module->methodInfos[5]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[5]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[5]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[5]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[5]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[5]);
    ZrCore_Value_InitAsInt(state, &reflectionI64OneArg, 99);
    ZrCore_Value_ResetAsNull(&reflectionI64OneArgReturn);
    module->methodInfos[5]->invoker(state,
                                    module->functionThunks[5],
                                    module->methodInfos[5],
                                    ZR_NULL,
                                    &reflectionI64OneArg,
                                    &reflectionI64OneArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_INT64, (TZrUInt16)reflectionI64OneArgReturn.type);
    TEST_ASSERT_EQUAL_INT64(99, reflectionI64OneArgReturn.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[6]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[6]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[6]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[6]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[6]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[6]);
    ZrCore_Value_InitAsUInt(state, &reflectionU64OneArg, 101u);
    ZrCore_Value_ResetAsNull(&reflectionU64OneArgReturn);
    module->methodInfos[6]->invoker(state,
                                    module->functionThunks[6],
                                    module->methodInfos[6],
                                    ZR_NULL,
                                    &reflectionU64OneArg,
                                    &reflectionU64OneArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_UINT64, (TZrUInt16)reflectionU64OneArgReturn.type);
    TEST_ASSERT_EQUAL_UINT64(101u, reflectionU64OneArgReturn.value.nativeObject.nativeUInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[7]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[7]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[7]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[7]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[7]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[7]);
    ZrCore_Value_InitAsBool(state, &reflectionBoolOneArg, ZR_FALSE);
    ZrCore_Value_ResetAsNull(&reflectionBoolOneArgReturn);
    module->methodInfos[7]->invoker(state,
                                    module->functionThunks[7],
                                    module->methodInfos[7],
                                    ZR_NULL,
                                    &reflectionBoolOneArg,
                                    &reflectionBoolOneArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolOneArgReturn.type);
    TEST_ASSERT_FALSE(reflectionBoolOneArgReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NOT_NULL(module->methodInfos[8]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[8]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[8]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[8]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[8]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[8]);
    ZrCore_Value_InitAsFloat(state, &reflectionF64OneArg, 1.75);
    ZrCore_Value_ResetAsNull(&reflectionF64OneArgReturn);
    module->methodInfos[8]->invoker(state,
                                    module->functionThunks[8],
                                    module->methodInfos[8],
                                    ZR_NULL,
                                    &reflectionF64OneArg,
                                    &reflectionF64OneArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_DOUBLE, (TZrUInt16)reflectionF64OneArgReturn.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.75, reflectionF64OneArgReturn.value.nativeObject.nativeDouble);
    TEST_ASSERT_NOT_NULL(module->methodInfos[9]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[9]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[9]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[9]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[9]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[9]);
    ZrCore_Value_InitAsInt(state, &reflectionI64TwoArgs[0], 20);
    ZrCore_Value_InitAsInt(state, &reflectionI64TwoArgs[1], 22);
    ZrCore_Value_ResetAsNull(&reflectionI64TwoArgReturn);
    module->methodInfos[9]->invoker(state,
                                    module->functionThunks[9],
                                    module->methodInfos[9],
                                    ZR_NULL,
                                    reflectionI64TwoArgs,
                                    &reflectionI64TwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_INT64, (TZrUInt16)reflectionI64TwoArgReturn.type);
    TEST_ASSERT_EQUAL_INT64(42, reflectionI64TwoArgReturn.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[10]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[10]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[10]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[10]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[10]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[10]);
    ZrCore_Value_InitAsUInt(state, &reflectionU64TwoArgs[0], 100u);
    ZrCore_Value_InitAsUInt(state, &reflectionU64TwoArgs[1], 23u);
    ZrCore_Value_ResetAsNull(&reflectionU64TwoArgReturn);
    module->methodInfos[10]->invoker(state,
                                     module->functionThunks[10],
                                     module->methodInfos[10],
                                     ZR_NULL,
                                     reflectionU64TwoArgs,
                                     &reflectionU64TwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_UINT64, (TZrUInt16)reflectionU64TwoArgReturn.type);
    TEST_ASSERT_EQUAL_UINT64(123u, reflectionU64TwoArgReturn.value.nativeObject.nativeUInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[11]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[11]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[11]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[11]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[11]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[11]);
    ZrCore_Value_InitAsBool(state, &reflectionBoolTwoArgs[0], ZR_TRUE);
    ZrCore_Value_InitAsBool(state, &reflectionBoolTwoArgs[1], ZR_TRUE);
    ZrCore_Value_ResetAsNull(&reflectionBoolTwoArgReturn);
    module->methodInfos[11]->invoker(state,
                                     module->functionThunks[11],
                                     module->methodInfos[11],
                                     ZR_NULL,
                                     reflectionBoolTwoArgs,
                                     &reflectionBoolTwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolTwoArgReturn.type);
    TEST_ASSERT_TRUE(reflectionBoolTwoArgReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NOT_NULL(module->methodInfos[12]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[12]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[12]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[12]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[12]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[12]);
    ZrCore_Value_InitAsFloat(state, &reflectionF64TwoArgs[0], 1.25);
    ZrCore_Value_InitAsFloat(state, &reflectionF64TwoArgs[1], 2.5);
    ZrCore_Value_ResetAsNull(&reflectionF64TwoArgReturn);
    module->methodInfos[12]->invoker(state,
                                     module->functionThunks[12],
                                     module->methodInfos[12],
                                     ZR_NULL,
                                     reflectionF64TwoArgs,
                                     &reflectionF64TwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_DOUBLE, (TZrUInt16)reflectionF64TwoArgReturn.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.75, reflectionF64TwoArgReturn.value.nativeObject.nativeDouble);
    TEST_ASSERT_NOT_NULL(module->methodInfos[13]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[13]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[13]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[13]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[13]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[13]);
    ZrCore_Value_InitAsInt(state, &reflectionBoolI64TwoArgs[0], 3);
    ZrCore_Value_InitAsInt(state, &reflectionBoolI64TwoArgs[1], 7);
    ZrCore_Value_ResetAsNull(&reflectionBoolI64TwoArgReturn);
    module->methodInfos[13]->invoker(state,
                                     module->functionThunks[13],
                                     module->methodInfos[13],
                                     ZR_NULL,
                                     reflectionBoolI64TwoArgs,
                                     &reflectionBoolI64TwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolI64TwoArgReturn.type);
    TEST_ASSERT_TRUE(reflectionBoolI64TwoArgReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NOT_NULL(module->methodInfos[14]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[14]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[14]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[14]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[14]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[14]);
    ZrCore_Value_InitAsUInt(state, &reflectionBoolU64TwoArgs[0], 9u);
    ZrCore_Value_InitAsUInt(state, &reflectionBoolU64TwoArgs[1], 4u);
    ZrCore_Value_ResetAsNull(&reflectionBoolU64TwoArgReturn);
    module->methodInfos[14]->invoker(state,
                                     module->functionThunks[14],
                                     module->methodInfos[14],
                                     ZR_NULL,
                                     reflectionBoolU64TwoArgs,
                                     &reflectionBoolU64TwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolU64TwoArgReturn.type);
    TEST_ASSERT_TRUE(reflectionBoolU64TwoArgReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NOT_NULL(module->methodInfos[15]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[15]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[15]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[15]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[15]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[15]);
    ZrCore_Value_InitAsFloat(state, &reflectionBoolF64TwoArgs[0], 2.5);
    ZrCore_Value_InitAsFloat(state, &reflectionBoolF64TwoArgs[1], 2.5);
    ZrCore_Value_ResetAsNull(&reflectionBoolF64TwoArgReturn);
    module->methodInfos[15]->invoker(state,
                                     module->functionThunks[15],
                                     module->methodInfos[15],
                                     ZR_NULL,
                                     reflectionBoolF64TwoArgs,
                                     &reflectionBoolF64TwoArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolF64TwoArgReturn.type);
    TEST_ASSERT_TRUE(reflectionBoolF64TwoArgReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NOT_NULL(module->methodInfos[16]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[16]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[16]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[16]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[16]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[16]);
    ZrCore_Value_InitAsInt(state, &reflectionI64ThreeArgs[0], 10);
    ZrCore_Value_InitAsInt(state, &reflectionI64ThreeArgs[1], 20);
    ZrCore_Value_InitAsInt(state, &reflectionI64ThreeArgs[2], 12);
    ZrCore_Value_ResetAsNull(&reflectionI64ThreeArgReturn);
    module->methodInfos[16]->invoker(state,
                                     module->functionThunks[16],
                                     module->methodInfos[16],
                                     ZR_NULL,
                                     reflectionI64ThreeArgs,
                                     &reflectionI64ThreeArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_INT64, (TZrUInt16)reflectionI64ThreeArgReturn.type);
    TEST_ASSERT_EQUAL_INT64(42, reflectionI64ThreeArgReturn.value.nativeObject.nativeInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[17]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[17]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[17]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[17]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[17]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[17]);
    ZrCore_Value_InitAsUInt(state, &reflectionU64ThreeArgs[0], 50u);
    ZrCore_Value_InitAsUInt(state, &reflectionU64ThreeArgs[1], 20u);
    ZrCore_Value_InitAsUInt(state, &reflectionU64ThreeArgs[2], 5u);
    ZrCore_Value_ResetAsNull(&reflectionU64ThreeArgReturn);
    module->methodInfos[17]->invoker(state,
                                     module->functionThunks[17],
                                     module->methodInfos[17],
                                     ZR_NULL,
                                     reflectionU64ThreeArgs,
                                     &reflectionU64ThreeArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_UINT64, (TZrUInt16)reflectionU64ThreeArgReturn.type);
    TEST_ASSERT_EQUAL_UINT64(75u, reflectionU64ThreeArgReturn.value.nativeObject.nativeUInt64);
    TEST_ASSERT_NOT_NULL(module->methodInfos[18]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[18]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[18]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[18]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[18]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[18]);
    ZrCore_Value_InitAsFloat(state, &reflectionF64ThreeArgs[0], 1.5);
    ZrCore_Value_InitAsFloat(state, &reflectionF64ThreeArgs[1], 2.25);
    ZrCore_Value_InitAsFloat(state, &reflectionF64ThreeArgs[2], 3.25);
    ZrCore_Value_ResetAsNull(&reflectionF64ThreeArgReturn);
    module->methodInfos[18]->invoker(state,
                                     module->functionThunks[18],
                                     module->methodInfos[18],
                                     ZR_NULL,
                                     reflectionF64ThreeArgs,
                                     &reflectionF64ThreeArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_DOUBLE, (TZrUInt16)reflectionF64ThreeArgReturn.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 7.0, reflectionF64ThreeArgReturn.value.nativeObject.nativeDouble);
    TEST_ASSERT_NOT_NULL(module->methodInfos[19]);
    TEST_ASSERT_NOT_NULL(module->methodInfos[19]->signature);
    TEST_ASSERT_NOT_NULL(module->methodInfos[19]->signature->returnType);
    TEST_ASSERT_NOT_NULL(module->methodInfos[19]->signature->parameterTypes);
    TEST_ASSERT_NOT_NULL(module->methodInfos[19]->invoker);
    TEST_ASSERT_NOT_NULL(module->functionThunks[19]);
    ZrCore_Value_InitAsBool(state, &reflectionBoolThreeArgs[0], ZR_TRUE);
    ZrCore_Value_InitAsBool(state, &reflectionBoolThreeArgs[1], ZR_TRUE);
    ZrCore_Value_InitAsBool(state, &reflectionBoolThreeArgs[2], ZR_TRUE);
    ZrCore_Value_ResetAsNull(&reflectionBoolThreeArgReturn);
    module->methodInfos[19]->invoker(state,
                                     module->functionThunks[19],
                                     module->methodInfos[19],
                                     ZR_NULL,
                                     reflectionBoolThreeArgs,
                                     &reflectionBoolThreeArgReturn);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_BOOL, (TZrUInt16)reflectionBoolThreeArgReturn.type);
    TEST_ASSERT_TRUE(reflectionBoolThreeArgReturn.value.nativeObject.nativeBool);
    TEST_ASSERT_NULL(module->typeLayouts);
    TEST_ASSERT_EQUAL_UINT32(0u, module->typeLayoutCount);
    TEST_ASSERT_NULL(module->codeRegistration->typeLayouts);
    TEST_ASSERT_EQUAL_UINT32(0u, module->codeRegistration->typeLayoutCount);
    TEST_ASSERT_NULL(module->typeLayoutTokens);
    TEST_ASSERT_EQUAL_UINT32(0u, module->typeLayoutTokenCount);
    TEST_ASSERT_NULL(module->codeRegistration->typeLayoutTokens);
    TEST_ASSERT_EQUAL_UINT32(0u, module->codeRegistration->typeLayoutTokenCount);
    TEST_ASSERT_NULL(module->gcDescriptors);
    TEST_ASSERT_EQUAL_UINT32(0u, module->gcDescriptorCount);
    TEST_ASSERT_NULL(module->codeRegistration->gcDescriptors);
    TEST_ASSERT_EQUAL_UINT32(0u, module->codeRegistration->gcDescriptorCount);

    dlclose(library);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_entry_through_runtime_loader(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C runtime shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "var left: int = 40;\n"
            "var right: int = 2;\n"
            "return left + right;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "runtime_project",
                                                       "runtime_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "runtime_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "runtime_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "runtime_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "runtime_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_call_spread(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C call-spread smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "var values = [10, 20, 12];\n"
            "return sum(...values);\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-call-spread-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionStackAnchor functionRootAnchor;
    TZrStackValuePointer functionRoot;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBool executeOk;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);
    functionRoot = state->stackTop.valuePointer;
    functionRoot = ZrCore_Function_CheckStackAndAnchor(
            state, 1u, functionRoot, functionRoot, &functionRootAnchor);
    TEST_ASSERT_NOT_NULL(functionRoot);
    ZrCore_Stack_SetRawObjectValue(
            state, functionRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    state->stackTop.valuePointer = functionRoot + 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "spread_project",
                                                       "spread_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "spread_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "spread_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "spread_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "spread_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(
            state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(
            zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &aotOptions));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText,
            "ZrLibrary_AotRuntime_CallSpread(state, &frame,"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(
            state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(
            state->global,
            ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
            ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    executeOk = ZrLibrary_AotRuntime_ExecuteEntry(
            state, ZR_AOT_BACKEND_KIND_C, &result);
    if (!executeOk) {
        const TZrChar *lastError =
                ZrLibrary_AotRuntime_GetLastError(state->global);
        printf("AOT C call-spread execution failed: %s\n",
               lastError != ZR_NULL ? lastError : "<no error>");
    }
    TEST_ASSERT_TRUE_MESSAGE(
            executeOk, ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIBRARY_EXECUTED_VIA_AOT_C,
            ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    functionRoot = ZrCore_Function_StackAnchorRestore(state, &functionRootAnchor);
    TEST_ASSERT_NOT_NULL(functionRoot);
    state->stackTop.valuePointer = functionRoot;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionRoot));
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_primitive_constant_writes(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C primitive-constant shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "var left: int = 7;\n"
            "var right: int = 5;\n"
            "return left + right;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-constant-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "constant_project",
                                                       "runtime_constant_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "constant_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "constant_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "constant_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "constant_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s0 = (TZrInt64)7;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_i64_binary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " + (TZrInt64)5;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return_i64_local"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Stack_GetValue("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET("));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_value_exec_primitive_constant"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_destination->value.nativeObject.nativeInt64"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SetConstant"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(12, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_elides_frame_for_bool_constant_return(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C bool constant frame-elision smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "var flag: bool = true;\n"
            "return flag;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-bool-constant-return-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "bool_constant_return_project",
                                                       "runtime_bool_constant_return_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "bool_constant_return_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "bool_constant_return_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "bool_constant_return_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "bool_constant_return_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_bool_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return_bool_local"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Stack_GetValue("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET("));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_value_exec_primitive_constant"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_destination->value.nativeObject.nativeBool"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result.type));
    TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_signed_branch_comparisons(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C signed-branch shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "var sum: int = 0;\n"
            "var cursor: int = 4;\n"
            "while (cursor > 0) {\n"
            "    sum = sum + cursor;\n"
            "    cursor = cursor - 1;\n"
            "}\n"
            "return sum;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-branch-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "branch_project",
                                                       "runtime_branch_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "branch_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "branch_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "branch_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "branch_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_signed_compare"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s3 = (TZrInt64)0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_s1 <= zr_aot_s3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_value_exec_primitive_constant"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[3].value"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(10, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_numeric_arithmetic_direct_expressions(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C numeric-arithmetic shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "pub var left = fn() => {\n"
            "    return 20;\n"
            "};\n"
            "pub var right = fn() => {\n"
            "    return 6;\n"
            "};\n"
            "var add = left() + right();\n"
            "var sub = left() - right();\n"
            "var mul = left() * right();\n"
            "var div = left() / right();\n"
            "var mod = left() % right();\n"
            "var neg = -right();\n"
            "return add + sub + mul + div + mod + neg;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-numeric-arithmetic-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "numeric_arithmetic_project",
                                                       "runtime_numeric_arithmetic_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "numeric_arithmetic_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "numeric_arithmetic_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "numeric_arithmetic_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "numeric_arithmetic_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_i64_binary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " + "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " - "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " * "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " / "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " % "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_static_i64_no_arg_direct_call"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=7 srcSlot=8"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s7 = zr_aot_s8;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_direct_stack_copy_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Add(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Sub(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Mul(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Div(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Mod(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Neg(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=1 srcSlot=2"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "const SZrTypeValue *zr_aot_direct_call_result = ZrCore_Stack_GetValue(frame.slotBase +"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_direct_stack_copy_sync_destination"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(159, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_elides_frame_for_signed_const_scalar_pipeline(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C signed const scalar frame-elision smoke currently validates the Unix toolchain path");
#else
    const char *source =
            "var seed: int = 10;\n"
            "var plus: int = seed + 5;\n"
            "var minus: int = plus - 3;\n"
            "var scaled: int = minus * 2;\n"
            "var ratio: int = scaled / 4;\n"
            "var remainder: int = ratio % 5;\n"
            "return remainder + 41;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-signed-const-scalar-frame-elision-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "signed_const_scalar_frame_elision_project",
                                                       "runtime_signed_const_scalar_frame_elision_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "signed_const_scalar_frame_elision_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "signed_const_scalar_frame_elision_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "signed_const_scalar_frame_elision_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "signed_const_scalar_frame_elision_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_i64_binary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return_i64_local"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Stack_GetValue("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET("));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_elides_frame_for_unsigned_const_scalar_pipeline(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C unsigned const scalar frame-elision smoke currently validates the Unix toolchain path");
#else
    const char *source =
            "var seed: uint = 10;\n"
            "var plus: uint = seed + <uint>5;\n"
            "var minus: uint = plus - <uint>3;\n"
            "var scaled: uint = minus * <uint>2;\n"
            "var ratio: uint = scaled / <uint>4;\n"
            "var remainder: uint = ratio % <uint>5;\n"
            "return remainder;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-unsigned-const-scalar-frame-elision-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_const_scalar_frame_elision_project",
                                                       "runtime_unsigned_const_scalar_frame_elision_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_const_scalar_frame_elision_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_const_scalar_frame_elision_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_const_scalar_frame_elision_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_const_scalar_frame_elision_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_u64_binary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return_u64_local"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Stack_GetValue("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET("));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_UINT64(1u, result.value.nativeObject.nativeUInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_elides_frame_for_unsigned_to_signed_conversion_pipeline(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C unsigned-to-signed conversion frame-elision smoke currently validates the Unix toolchain path");
#else
    const char *source =
            "var seed: uint = 10;\n"
            "var signedSeed: int = <int> seed;\n"
            "var plus: int = signedSeed + 5;\n"
            "return plus;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-unsigned-to-signed-conversion-frame-elision-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_to_signed_conversion_frame_elision_project",
                                                       "runtime_unsigned_to_signed_conversion_frame_elision_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_to_signed_conversion_frame_elision_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_to_signed_conversion_frame_elision_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_to_signed_conversion_frame_elision_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_to_signed_conversion_frame_elision_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_i64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_i64_binary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return_i64_local"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Stack_GetValue("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET("));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(15, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_elides_frame_for_unsigned_mixed_literal_pipeline(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C unsigned mixed-literal frame-elision smoke currently validates the Unix toolchain path");
#else
    const char *source =
            "var seed: uint = 10;\n"
            "var plus: int = seed + 5;\n"
            "var minus: int = plus - 3;\n"
            "var scaled: int = minus * 2;\n"
            "var ratio: int = scaled / 4;\n"
            "var remainder: int = ratio % 5;\n"
            "return remainder + 41;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-unsigned-mixed-literal-frame-elision-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_mixed_literal_frame_elision_project",
                                                       "runtime_unsigned_mixed_literal_frame_elision_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_mixed_literal_frame_elision_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_mixed_literal_frame_elision_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_mixed_literal_frame_elision_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "unsigned_mixed_literal_frame_elision_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_i64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_i64_binary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_direct_return_i64_local"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Stack_GetValue("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET("));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_primitive_conversions(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic primitive conversion shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    const char *source =
            "var flag: bool = true;\n"
            "var truthSource: int = 7;\n"
            "var truthy: bool = <bool> truthSource;\n"
            "var signed: int = <int> flag;\n"
            "var unsigned: uint = <uint> flag;\n"
            "var floating: float = <float> flag;\n"
            "if (!truthy) {\n"
            "    return 0;\n"
            "}\n"
            "return signed + <int> unsigned + <int> floating;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-conversion-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_conversion_project",
                                                       "runtime_generic_conversion_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_conversion_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_conversion_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_conversion_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_conversion_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_bool"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_bool_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_i64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_u64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_f64"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b0 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b0 ? (TZrUInt64)1u : (TZrUInt64)0u;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b0 ? (TZrFloat64)1.0 : (TZrFloat64)0.0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_exec_to_bool dstSlot=4 srcSlot=1"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b4 = (TZrBool)(zr_aot_s1 != (TZrInt64)0);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ConvertGenericToBool(state, &frame, 4, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToBool(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToInt(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToUInt(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToFloat(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[0].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[1].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[6].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[7].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_convert_generic_sync_bool_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_convert_generic_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT generic primitive conversion"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "*zr_aot_destination = *zr_aot_source;"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(3, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_unsupported_instruction_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_invalid_jump_boundary_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "src",
                                                       "aot_c_unsupported_instruction_boundary",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "lib",
                                                       "libaot_c_unsupported_instruction_boundary",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_unsupported_instruction_boundary";
    options.sourceHash = "unsupported-instruction-boundary";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "unsupported-instruction-boundary";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_unsupported_instruction"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const TZrUInt32 zr_aot_instruction_index"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const TZrUInt32 zr_aot_opcode"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"unsupported AOT instruction\");"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_pending_control_helper_blocks(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C pending-control shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_pending_control_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "src",
                                                       "aot_c_pending_control_direct",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "lib",
                                                       "libaot_c_pending_control_direct",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_pending_control_direct";
    options.sourceHash = "pending-control-direct";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "pending-control-direct";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_pending_return"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_pending_break"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_pending_continue"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrLibrary_AotRuntime_SetPendingReturn(state, &frame, 0, 3, &zr_aot_next_instruction)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrLibrary_AotRuntime_SetPendingBreak(state, &frame, 3, &zr_aot_next_instruction)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrLibrary_AotRuntime_SetPendingContinue(state, &frame, 3, &zr_aot_next_instruction)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_direct_return */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_Return(state, &frame, 0, ZR_FALSE));"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "goto zr_aot_fn_0_dispatch;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "TZrStackValuePointer zr_aot_result_slot;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_result_value;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_caller_result_value;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "execution_discard_exception_handlers_for_callinfo(state, zr_aot_call_info);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, zr_aot_call_info);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_pending_value = ZR_NULL;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "execution_set_pending_control(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "execution_jump_to_instruction_offset(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "state->pendingControl.targetInstructionOffset"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_source_compiles_and_exports_module_descriptor);
    RUN_TEST(test_aot_c_generated_shared_library_executes_entry_through_runtime_loader);
    RUN_TEST(test_aot_c_generated_shared_library_executes_call_spread);
    RUN_TEST(test_aot_c_generated_shared_library_executes_primitive_constant_writes);
    RUN_TEST(test_aot_c_generated_shared_library_elides_frame_for_bool_constant_return);
    RUN_TEST(test_aot_c_generated_shared_library_executes_signed_branch_comparisons);
    RUN_TEST(test_aot_c_generated_shared_library_executes_numeric_arithmetic_direct_expressions);
    RUN_TEST(test_aot_c_generated_shared_library_elides_frame_for_signed_const_scalar_pipeline);
    RUN_TEST(test_aot_c_generated_shared_library_elides_frame_for_unsigned_const_scalar_pipeline);
    RUN_TEST(test_aot_c_generated_shared_library_elides_frame_for_unsigned_to_signed_conversion_pipeline);
    RUN_TEST(test_aot_c_generated_shared_library_elides_frame_for_unsigned_mixed_literal_pipeline);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_primitive_conversions);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_unsupported_instruction_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_pending_control_helper_blocks);
    return UNITY_END();
}
