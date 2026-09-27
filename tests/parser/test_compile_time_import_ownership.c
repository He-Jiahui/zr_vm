#include "unity.h"
#include "runtime_support.h"
#include "path_support.h"
#include "module_fixture_support.h"
#include "compiler/compiler_internal.h"
#include "zr_vm_core/memory.h"

/* Unity 每例独立持有运行时、编译状态、解析树及编译出的 provider 函数。 */
static SZrState *g_state;
static SZrCompilerState g_compiler;
static SZrAstNode *g_ast;
static SZrFunction *g_provider;
/* 包装当前运行时分配器，只计量二进制解码产生的 IO 原生内存。 */
static FZrAllocator g_allocator;
static TZrSize g_ioAllocated;
static TZrSize g_ioFreed;
static TZrSize g_ioAllocatedBytes;
static TZrSize g_ioFreedBytes;
/* loader 成功打开的 reader 必须由 IO 层 close 回调逐一配对。 */
static TZrSize g_opened;
static TZrSize g_closed;
/* fixture 文件读入的字节缓冲区归测试持有，reader 只借用。 */
static TZrByte *g_bytes;
static TZrSize g_byteCount;
/* 每例的二进制产物路径由 path helper 构造，tearDown 删除产物。 */
static char g_binaryPath[ZR_TESTS_PATH_MAX];

/* 对 IO realloc 同时记录旧块释放和新块分配，比较总次数与字节数。 */
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

/* IO 关闭回调释放 reader 外壳；缓冲区仍归测试的 g_bytes。 */
static void close_reader(SZrState *state, TZrPtr reader) {
    g_closed++;
    ZrTests_Fixture_ReaderClose(state, reader);
}

/* 只响应 provider 名称，为内存中的二进制 fixture 创建借用字节的 reader。 */
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

/* 注册解析器与带计数的分配器后初始化编译状态，准备独立产物路径。 */
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

/* 正常及断言退出后先释放编译器/AST/函数，再销毁运行时、缓冲区和产物。 */
/* BUG: 上一例 tearDown 未清空 g_compiler/g_ast/g_provider；若下一例创建运行时
 * 失败，setUp 会在重置前跳出，本回调将用旧 state 清理悬空资源。 */
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

/* 编译 provider、写入二进制并经 loader 导入；返回值借用 g_compiler 的模块表。 */
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
    /* 源树由导入过程消费并释放；两组计数排除遗留 reader 和 IO 原生块。 */
    TEST_ASSERT_GREATER_THAN_UINT64(0, g_opened);
    TEST_ASSERT_EQUAL_UINT64(g_opened, g_closed);
    TEST_ASSERT_GREATER_THAN_UINT64(0, g_ioAllocated);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(g_ioAllocated, g_ioFreed,
            "compile-time import must release decoded IO storage before returning");
    TEST_ASSERT_EQUAL_UINT64(g_ioAllocatedBytes, g_ioFreedBytes);
    if (invalidateProjection) {
        /* 故意破坏导出的投影名后应拒绝收集模块，但仍完成源树清理。 */
        TEST_ASSERT_EQUAL_UINT(0, g_compiler.importedCompileTimeModules.length);
        return ZR_NULL;
    }
    TEST_ASSERT_EQUAL_UINT(1, g_compiler.importedCompileTimeModules.length);
    return *(SZrImportedCompileTimeModule **)ZrCore_Array_Get(&g_compiler.importedCompileTimeModules, 0);
}

/* 无 comptime 声明也必须在导入完成后释放源对象。 */
static void test_binary_import_without_compile_time_declarations_releases_source(void) {
    SZrImportedCompileTimeModule *module = compile_import(
            "pub fn identity<T>(value: T): T { return value; }\n", ZR_FALSE);
    TEST_ASSERT_EQUAL_UINT(0, module->compileTimeFunctions.length);
}

/* 投影函数的名称、参数默认值与返回类型必须脱离已释放的二进制源树存活。 */
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

/* 损坏单个投影名后拒绝模块，同时验证 reader 与 IO 分配已回收。 */
static void test_rejected_binary_compile_time_projection_releases_source(void) {
    compile_import("pub comptime fn measure(value: int = 7): int { return value; }\n", ZR_TRUE);
}

/* 前一投影可被部分收集时，末尾无效投影仍应触发整体拒绝并释放源树。 */
static void test_partially_collected_binary_compile_time_projection_releases_source(void) {
    compile_import(
            "pub comptime fn first(value: int = 3): int { return value; }\n"
            "pub comptime fn second(value: int = 7): int { return value; }\n", ZR_TRUE);
}

/* CTest compile_time_import_ownership 经此入口执行四条所有权回归。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_import_without_compile_time_declarations_releases_source);
    RUN_TEST(test_binary_compile_time_projection_survives_source_release);
    RUN_TEST(test_rejected_binary_compile_time_projection_releases_source);
    RUN_TEST(test_partially_collected_binary_compile_time_projection_releases_source);
    return UNITY_END();
}
