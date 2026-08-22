#include <stdint.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

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

    ZrCore_Object_Deconstruct(state, &module->super);
    TEST_ASSERT_FALSE(module->proNodeMap.isValid);
    TEST_ASSERT_NULL(module->exportDescriptors);
    TEST_ASSERT_EQUAL_UINT32(0u, module->exportDescriptorLength);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_long_string_storage_is_native_pointer_aligned);
    RUN_TEST(test_global_shutdown_releases_unfreed_function_buffers);
    RUN_TEST(test_module_object_deconstructs_private_exports_and_descriptors);
    return UNITY_END();
}
