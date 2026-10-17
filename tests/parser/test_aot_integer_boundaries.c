#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "harness/aot_c_link_support.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}
void tearDown(void) {}

static void write_text(const char *path, const char *text) {
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

/* Some frontend parameter rows carry declaration IDs without executable places.
 * Omit optional identities in this fixture so that the unrelated AOT metadata
 * validator does not prevent exercising the actual arithmetic and call thunks. */
static void omit_optional_identities(SZrFunction *function) {
    for (TZrUInt32 i = 0; i < function->typedLocalBindingLength; ++i) {
        function->typedLocalBindings[i].symbolId = 0;
        function->typedLocalBindings[i].typeId = 0;
        function->typedLocalBindings[i].placeId = 0;
    }
    for (TZrUInt32 i = 0; i < function->childFunctionLength; ++i) {
        omit_optional_identities(&function->childFunctionList[i]);
    }
}

/* Compile the actual source into a generated shared library, then execute its
 * entry through the AOT loader. UBSan instruments the generated arithmetic too. */
static void check_source(unsigned caseIndex, const char *source, TZrBool expectedSuccess, TZrInt64 expected) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("Generated library execution currently requires the Unix test linker");
#else
    static const char projectJson[] = "{\"name\":\"integer_bounds\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *name;
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrTypeValue result;
    SZrBinaryWriterOptions binaryOptions = {0};
    SZrAotWriterOptions options = {0};
    TZrBytePtr blob = ZR_NULL;
    TZrSize blobLength = 0;
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    char group[96], projectPath[ZR_TESTS_PATH_MAX], sourcePath[ZR_TESTS_PATH_MAX];
    char binaryPath[ZR_TESTS_PATH_MAX], cPath[ZR_TESTS_PATH_MAX], libraryPath[ZR_TESTS_PATH_MAX];
    char hashText[32], command[8192];
    TZrBool success;
    TEST_ASSERT_NOT_NULL(state);
    snprintf(group, sizeof(group), "aot_integer_boundaries/case_%u", caseIndex);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(group, "project", "project", ".zrp", projectPath, sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(group, "project/src", "main", ".zr", sourcePath, sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(group, "project/bin", "main", ".zro", binaryPath, sizeof(binaryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(group, "project/bin/aot_c/src", "main", ".c", cPath, sizeof(cPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(group, "project/bin/aot_c/lib", "zrvm_aot_main", ".so", libraryPath, sizeof(libraryPath)));
    write_text(projectPath, projectJson);
    write_text(sourcePath, source);
    name = ZrCore_String_Create(state, "main.zr", 7);
    function = ZrParser_Source_Compile(state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(function);
    omit_optional_identities(function);
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &binaryOptions));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(binaryPath, &blob, &blobLength));
    for (TZrSize i = 0; i < blobLength; ++i) {
        hash ^= blob[i];
        hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
    }
    snprintf(hashText, sizeof(hashText), ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
    options.moduleName = "main";
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = hashText;
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = blobLength;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(cPath));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(libraryPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, cPath, &options));
    snprintf(command, sizeof(command),
             "\"%s\" %s -O2 -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-fsanitize=undefined -fno-sanitize-recover=undefined "
             "-I\"%s/zr_vm_common/include\" -I\"%s/zr_vm_core/include\" -I\"%s/zr_vm_library/include\" "
             "\"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER, ZR_VM_TESTS_C_FLAGS,
             ZR_VM_TESTS_REPO_ROOT, ZR_VM_TESTS_REPO_ROOT, ZR_VM_TESTS_REPO_ROOT,
             cPath, ZR_VM_TESTS_BUILD_LIB_DIR, ZR_VM_TESTS_BUILD_LIB_DIR, libraryPath);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, system(command), command);
    /* Generated code loads its own function tree from the embedded binary. */
    ZrCore_Function_Free(state, function);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global, ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C, ZR_TRUE));
    success = ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result);
    if (success != expectedSuccess) {
        printf("Case %u: %s\n%s\n", caseIndex, source, ZrLibrary_AotRuntime_GetLastError(state->global));
    }
    if (!success) ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(blob);
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_EQUAL_INT(expectedSuccess, success);
    if (success) TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
#endif
}

static void test_generated_integer_boundaries(void) {
    unsigned n = 0;
    check_source(n++, "return 9223372036854775807 + 1;", ZR_FALSE, 0);
    check_source(n++, "return (-9223372036854775807 - 1) - 1;", ZR_FALSE, 0);
    check_source(n++, "var x: int = (-9223372036854775807 - 1); return -x;", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int): int { return x + 1; } return f(9223372036854775807);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int): int { return x - 1; } return f(-9223372036854775807 - 1);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int): int { return -x; } return f(-9223372036854775807 - 1);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int, y: int): int { return x + y; } return f(9223372036854775807, 1);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int, y: int): int { return x - y; } return f(-9223372036854775807 - 1, 1);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int, y: int, z: int): int { return x + y + z; } return f(9223372036854775807, 1, -1);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int, y: int): int { return (x + y) % 7; } return f(9223372036854775807, 1);", ZR_FALSE, 0);
    check_source(n++, "fn f(x: int): int { return x + 1; } return f(41);", ZR_TRUE, 42);
    check_source(n++, "var x: int = -2; var k: int = 65; var r: int = x >> k; return r + k;", ZR_TRUE, 64);
    check_source(n++, "var x: int = 1; var k: int = -1; return x << k;", ZR_TRUE, INT64_MIN);
    check_source(n++, "var x: int = -1; var k: int = 64; return x << k;", ZR_TRUE, -1);
    check_source(n++, "var x: uint = <uint>1; var k: int = 64; return <int>(x << k);", ZR_TRUE, 1);
    check_source(n++, "var a: int[] = [1, 2]; a[0] = 3; return a[0] + a[1];", ZR_TRUE, 5);
}

static void test_compile_time_integer_boundaries(void) {
    const char *sources[] = {
        "comptime { var x: int = 9223372036854775807; var y: int = x + 1; } return 0;",
        "comptime { var x: int = (-9223372036854775807 - 1); var y: int = x - 1; } return 0;",
        "comptime { var x: int = (-9223372036854775807 - 1); var y: int = -x; } return 0;"
    };
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(sources); ++i) {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrString *name = ZrCore_String_Create(state, "overflow.zr", 12);
        SZrFunction *function = ZrParser_Source_Compile(state, sources[i], strlen(sources[i]), name);
        TZrBool rejected = function == ZR_NULL;
        if (function != ZR_NULL) ZrCore_Function_Free(state, function);
        ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        ZrTests_Runtime_State_Destroy(state);
        TEST_ASSERT_TRUE_MESSAGE(rejected, sources[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generated_integer_boundaries);
    RUN_TEST(test_compile_time_integer_boundaries);
    return UNITY_END();
}
