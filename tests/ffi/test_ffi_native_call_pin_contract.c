#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "harness/path_support.h"

void setUp(void) {
}

void tearDown(void) {
}

static char *read_repo_text_file_or_fail(const char *relativePath) {
    char path[ZR_TESTS_PATH_MAX];
    TZrSize length = 0u;
    int written;
    char *text;

    TEST_ASSERT_NOT_NULL(relativePath);
    written = snprintf(path, sizeof(path), "%s/../%s", ZR_VM_TESTS_SOURCE_DIR, relativePath);
    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_TRUE((size_t)written < sizeof(path));

    text = ZrTests_ReadTextFile(path, &length);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, length);
    return text;
}

static const char *assert_text_contains(const char *text, const char *needle) {
    const char *match;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    match = strstr(text, needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(match, needle);
    return match;
}

static void assert_text_order(const char *before, const char *after, const char *message) {
    TEST_ASSERT_NOT_NULL(before);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_TRUE_MESSAGE(before < after, message);
}

static void test_zr_ffi_symbol_call_pins_gc_values_across_native_call_boundary(void) {
    char *runtimeSource;
    const char *selfPin;
    const char *ownerPin;
    const char *argumentPin;
    const char *nativeInvoke;
    const char *argumentUnpin;
    const char *ownerUnpin;
    const char *selfUnpin;

    runtimeSource = read_repo_text_file_or_fail("zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c");
    assert_text_contains(runtimeSource, "SZrGcNativeCallPin selfPin");
    assert_text_contains(runtimeSource, "SZrGcNativeCallPin ownerPin");
    assert_text_contains(runtimeSource, "SZrGcNativeCallPin *argumentPins");

    selfPin = assert_text_contains(runtimeSource,
                                   "ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(selfObject), &selfPin)");
    ownerPin = assert_text_contains(runtimeSource, "ZrCore_Gc_NativeCallPinValue(state, ownerValue, &ownerPin)");
    argumentPin = assert_text_contains(runtimeSource,
                                       "ZrCore_Gc_NativeCallPinValue(state, argumentValue, &argumentPins[index])");
    nativeInvoke = assert_text_contains(runtimeSource,
                                        "zr_ffi_invoke_native_symbol(symbolData, returnStorage, ffiArguments, errorBuffer, sizeof(errorBuffer))");
    argumentUnpin = assert_text_contains(runtimeSource, "ZrCore_Gc_NativeCallUnpin(state->global, &argumentPins[");
    ownerUnpin = assert_text_contains(runtimeSource, "ZrCore_Gc_NativeCallUnpin(state->global, &ownerPin)");
    selfUnpin = assert_text_contains(runtimeSource, "ZrCore_Gc_NativeCallUnpin(state->global, &selfPin)");

    assert_text_order(selfPin, nativeInvoke, "self handle must be pinned before ffi_call");
    assert_text_order(ownerPin, nativeInvoke, "library owner handle must be pinned before ffi_call");
    assert_text_order(argumentPin, nativeInvoke, "GC argument values must be pinned before ffi_call");
    assert_text_order(nativeInvoke, argumentUnpin, "argument pins must be released after ffi_call");
    assert_text_order(nativeInvoke, ownerUnpin, "owner pin must be released after ffi_call");
    assert_text_order(nativeInvoke, selfUnpin, "self pin must be released after ffi_call");

    free(runtimeSource);
}

static void test_zr_ffi_callback_trampoline_reanchors_saved_stack_after_native_callback(void) {
    char *internalHeader;
    char *callbackSource;

    internalHeader = read_repo_text_file_or_fail("zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h");
    callbackSource = read_repo_text_file_or_fail("zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c");

    assert_text_contains(internalHeader, "#include \"zr_vm_core/function.h\"");
    assert_text_contains(callbackSource, "SZrFunctionStackAnchor savedStackTopAnchor;");
    assert_text_contains(callbackSource,
                         "ZrCore_Function_StackAnchorInit(callbackData->state, savedStackTop, &savedStackTopAnchor)");
    assert_text_contains(callbackSource,
                         "savedStackTop = ZrCore_Function_StackAnchorRestore(callbackData->state, &savedStackTopAnchor)");
    assert_text_contains(callbackSource,
                         "callbackData->state->stackTop.valuePointer != savedStackTop");

    free(internalHeader);
    free(callbackSource);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zr_ffi_symbol_call_pins_gc_values_across_native_call_boundary);
    RUN_TEST(test_zr_ffi_callback_trampoline_reanchors_saved_stack_after_native_callback);
    return UNITY_END();
}
