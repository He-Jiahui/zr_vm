#include <stdint.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

/* BUG: 三个用例在局部创建 VM，断言提前退出会跳过末尾的 Destroy；
 * 空 tearDown 无法回收其 global、字符串或模块分配。 */
void tearDown(void) {}

/* 选择超过短串阈值的输入，验证暴露给 native 调用者的长串指针 ABI 对齐。 */
static void test_long_string_storage_is_native_pointer_aligned(void) {
    static TZrChar longText[] =
            "this string is deliberately longer than the complete short-string inline payload "
            "so the runtime must store its native buffer through the long-string pointer slot";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *string;
    uintptr_t storageAddress;

    TEST_ASSERT_NOT_NULL(state);

    string = ZrCore_String_Create(state, longText, sizeof(longText) - 1u);
    TEST_ASSERT_NOT_NULL(string);
    TEST_ASSERT_FALSE(ZrCore_String_IsShort(string));

    storageAddress = (uintptr_t)ZrCore_String_GetNativeStringLong(string);
    TEST_ASSERT_EQUAL_UINT64(0u, storageAddress % _Alignof(TZrNativeString));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_global_shutdown_releases_unfreed_function_buffers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->typedExportedSymbols =
            (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrFunctionTypedExportSymbol),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->typedExportedSymbols);
    ZrCore_Memory_RawSet(function->typedExportedSymbols,
                         0,
                         sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1u;

    /* Global teardown owns raw function objects that callers did not tombstone. */
    ZrTests_Runtime_State_Destroy(state);
}

static void test_module_object_deconstructs_private_exports_and_descriptors(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObjectModule *module;
    SZrString *name;
    SZrTypeValue value;
    SZrModuleExportDescriptor descriptor;

    TEST_ASSERT_NOT_NULL(state);
    module = ZrCore_Module_Create(state);
    name = ZrCore_String_CreateFromNative(state, "private_export");
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_NOT_NULL(name);

    ZrCore_Value_ResetAsNull(&value);
    ZrCore_Module_AddProExport(state, module, name, &value);
    descriptor.name = name;
    descriptor.accessModifier = 0u;
    descriptor.exportKind = 0u;
    descriptor.readiness = 0u;
    descriptor.isReady = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCore_Module_RegisterExportDescriptor(state, module, &descriptor));

    /* 模块显式析构要释放私有导出映射和描述符；随后 global shutdown
     * 仍负责其余 VM 对象，不能重复持有这些 native 缓冲区。 */
    ZrCore_Object_Deconstruct(state, &module->super);
    TEST_ASSERT_FALSE(module->proNodeMap.isValid);
    TEST_ASSERT_NULL(module->exportDescriptors);
    TEST_ASSERT_EQUAL_UINT32(0u, module->exportDescriptorLength);

    ZrTests_Runtime_State_Destroy(state);
}

/* TODO: CMake 当前只构建此目标，仓库内未见 CTest/suite 引用；
 * 核查 CI 是否单独执行，若无则接入常规测试入口。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_long_string_storage_is_native_pointer_aligned);
    RUN_TEST(test_global_shutdown_releases_unfreed_function_buffers);
    RUN_TEST(test_module_object_deconstructs_private_exports_and_descriptors);
    return UNITY_END();
}
