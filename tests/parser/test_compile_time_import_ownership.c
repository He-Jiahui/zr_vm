#include "unity.h"
#include "runtime_support.h"
#include "path_support.h"
#include "module_fixture_support.h"
#include "compiler/compiler_internal.h"
#include "zr_vm_core/memory.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static SZrAstNode *g_ast;
static SZrFunction *g_provider;
static FZrAllocator g_allocator;
static TZrSize g_ioAllocated;
static TZrSize g_ioFreed;
static TZrSize g_ioAllocatedBytes;
static TZrSize g_ioFreedBytes;
static TZrSize g_opened;
static TZrSize g_closed;
static TZrByte *g_bytes;
static TZrSize g_byteCount;
static char g_binaryPath[ZR_TESTS_PATH_MAX];

static TZrPtr counting_allocator(TZrPtr userData, TZrPtr pointer, TZrSize oldSize,
                                TZrSize newSize, TZrInt64 type) {
    TZrBool hadAllocation = pointer != ZR_NULL;
    TZrPtr result = g_allocator(userData, pointer, oldSize, newSize, type);
    if (type == ZR_MEMORY_NATIVE_TYPE_IO) {
        if (hadAllocation && (newSize == 0 || result != ZR_NULL)) {
            g_ioFreed++;
            g_ioFreedBytes += oldSize;
        }
        if (result != ZR_NULL && newSize != 0) {
            g_ioAllocated++;
            g_ioAllocatedBytes += newSize;
        }
    }
    return result;
}

static void close_reader(SZrState *state, TZrPtr reader) {
    g_closed++;
    ZrTests_Fixture_ReaderClose(state, reader);
}

static TZrBool load_provider(SZrState *state, TZrNativeString name, TZrNativeString hash, SZrIo *io) {
    ZrTestsFixtureReader *reader;
    ZR_UNUSED_PARAMETER(hash);
    if (name == ZR_NULL || strcmp(name, "provider") != 0) {
        return ZR_FALSE;
    }
    reader = malloc(sizeof(*reader));
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }
    reader->bytes = g_bytes;
    reader->length = g_byteCount;
    reader->consumed = ZR_FALSE;
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, close_reader, reader);
    io->isBinary = ZR_TRUE;
    g_opened++;
    return ZR_TRUE;
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_ToGlobalState_Register(g_state);
    g_ast = ZR_NULL;
    g_provider = ZR_NULL;
    g_bytes = ZR_NULL;
    g_byteCount = 0;
    g_opened = g_closed = 0;
    g_ioAllocated = g_ioFreed = 0;
    g_ioAllocatedBytes = g_ioFreedBytes = 0;
    g_allocator = g_state->global->allocator;
    g_state->global->allocator = counting_allocator;
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("compile_time_import_ownership", "", "provider", ".zro",
            g_binaryPath, sizeof(g_binaryPath)));
}

void tearDown(void) {
    if (g_compiler.topLevelFunction != ZR_NULL &&
        g_compiler.topLevelFunction != g_compiler.currentFunction) {
        ZrCore_Function_Free(g_state, g_compiler.topLevelFunction);
    }
    if (g_compiler.currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_compiler.currentFunction);
    }
    g_compiler.topLevelFunction = g_compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&g_compiler);
    ZrParser_Ast_Free(g_state, g_ast);
    if (g_provider != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_provider);
    }
    ZrTests_Runtime_State_Destroy(g_state);
    free(g_bytes);
    remove(g_binaryPath);
}

static SZrImportedCompileTimeModule *compile_import(const char *providerSource, TZrBool invalidateProjection) {
    const char *source = "let provider = import(\"provider\");\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "comptime_import_owner.zr");
    SZrBinaryWriterOptions options = {0};

    g_state->global->emitCompileTimeRuntimeSupport = ZR_TRUE;
    g_provider = ZrParser_Source_Compile(g_state, providerSource, strlen(providerSource),
            ZrCore_String_CreateFromNative(g_state, "provider.zr"));
    g_state->global->emitCompileTimeRuntimeSupport = ZR_FALSE;
    TEST_ASSERT_NOT_NULL(g_provider);
    if (invalidateProjection) {
        TEST_ASSERT_GREATER_THAN_UINT(0, g_provider->compileTimeFunctionInfoLength);
        g_provider->compileTimeFunctionInfos[g_provider->compileTimeFunctionInfoLength - 1].name = ZR_NULL;
    }
    options.moduleName = "provider";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(g_state, g_provider, g_binaryPath, &options));
    g_bytes = ZrTests_Fixture_ReadFileBytes(g_binaryPath, &g_byteCount);
    TEST_ASSERT_NOT_NULL(g_bytes);
    g_state->global->sourceLoader = load_provider;
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
    compile_script(&g_compiler, g_ast);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    TEST_ASSERT_GREATER_THAN_UINT64(0, g_opened);
    TEST_ASSERT_EQUAL_UINT64(g_opened, g_closed);
    TEST_ASSERT_GREATER_THAN_UINT64(0, g_ioAllocated);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(g_ioAllocated, g_ioFreed,
            "compile-time import must release decoded IO storage before returning");
    TEST_ASSERT_EQUAL_UINT64(g_ioAllocatedBytes, g_ioFreedBytes);
    if (invalidateProjection) {
        TEST_ASSERT_EQUAL_UINT(0, g_compiler.importedCompileTimeModules.length);
        return ZR_NULL;
    }
    TEST_ASSERT_EQUAL_UINT(1, g_compiler.importedCompileTimeModules.length);
    return *(SZrImportedCompileTimeModule **)ZrCore_Array_Get(&g_compiler.importedCompileTimeModules, 0);
}

static void test_binary_import_without_compile_time_declarations_releases_source(void) {
    SZrImportedCompileTimeModule *module = compile_import(
            "pub fn identity<T>(value: T): T { return value; }\n", ZR_FALSE);
    TEST_ASSERT_EQUAL_UINT(0, module->compileTimeFunctions.length);
}

static void test_binary_compile_time_projection_survives_source_release(void) {
    SZrImportedCompileTimeModule *module = compile_import(
            "pub comptime fn measure(value: int = 7): int { return value; }\n", ZR_FALSE);
    SZrCompileTimeFunction *function;
    SZrString *parameterName;
    SZrInferredType *parameterType;
    SZrTypeValue *defaultValue;

    TEST_ASSERT_EQUAL_UINT(1, module->compileTimeFunctions.length);
    function = *(SZrCompileTimeFunction **)ZrCore_Array_Get(&module->compileTimeFunctions, 0);
    TEST_ASSERT_EQUAL_STRING("measure", ZrCore_String_GetNativeString(function->name));
    TEST_ASSERT_TRUE(function->isRuntimeProjection);
    TEST_ASSERT_EQUAL_PTR(module, function->ownerModule);
    TEST_ASSERT_EQUAL_UINT(1, function->paramTypes.length);
    parameterName = *(SZrString **)ZrCore_Array_Get(&function->paramNames, 0);
    parameterType = ZrCore_Array_Get(&function->paramTypes, 0);
    defaultValue = ZrCore_Array_Get(&function->paramDefaultValues, 0);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(parameterName));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, parameterType->baseType);
    TEST_ASSERT_TRUE(*(TZrBool *)ZrCore_Array_Get(&function->paramHasDefaultValues, 0));
    TEST_ASSERT_EQUAL_INT64(7, defaultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, function->returnType.baseType);
}

static void test_rejected_binary_compile_time_projection_releases_source(void) {
    compile_import("pub comptime fn measure(value: int = 7): int { return value; }\n", ZR_TRUE);
}

static void test_partially_collected_binary_compile_time_projection_releases_source(void) {
    compile_import(
            "pub comptime fn first(value: int = 3): int { return value; }\n"
            "pub comptime fn second(value: int = 7): int { return value; }\n", ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_import_without_compile_time_declarations_releases_source);
    RUN_TEST(test_binary_compile_time_projection_survives_source_release);
    RUN_TEST(test_rejected_binary_compile_time_projection_releases_source);
    RUN_TEST(test_partially_collected_binary_compile_time_projection_releases_source);
    return UNITY_END();
}
