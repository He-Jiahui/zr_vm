#include <string.h>
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

static SZrState *state;

void setUp(void) {
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}

void tearDown(void) { ZrTests_Runtime_State_Destroy(state); }

static SZrFunction *compile_answer(void) {
    const char *source = "fn answer(): int { return 42; } return answer();";
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "function_lifetime.zr"));
    TEST_ASSERT_NOT_NULL(function);
    return function;
}

static void expect_answer(SZrFunction *function) {
    TZrInt64 answer = 0;
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &answer));
    TEST_ASSERT_EQUAL_INT64(42, answer);
}

static void test_null_function_super_conversion_is_safe(void) {
    SZrFunction *volatile missing = ZR_NULL;
    TEST_ASSERT_NULL(ZR_CAST_RAW_OBJECT_AS_SUPER(missing));
}

static void test_compiled_functions_survive_gc_before_and_between_executions(void) {
    SZrFunction *first = compile_answer();
    SZrFunction *second = compile_answer();
    for (unsigned i = 0; i < 3; ++i) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        expect_answer(second);
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        expect_answer(first);
    }
    ZrCore_Function_Free(state, second);
    ZrCore_Function_Free(state, first);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
}

static void test_host_ownership_and_explicit_roots_are_independent(void) {
    SZrFunction *function = compile_answer();
    SZrGcRootLease independent = {ZR_NULL};
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &function->super, &independent));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, &function->super));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(state->global, &function->super));
    ZrCore_Function_ReleaseHostRoot(state, function);
    ZrCore_Function_ReleaseHostRoot(state, function);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    expect_answer(function);
    ZrCore_Function_Free(state, function);
    ZrCore_Gc_RootLeaseRelease(state->global, &independent);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
}

static void test_host_root_can_transfer_to_a_vm_closure(void) {
    SZrFunction *function = compile_answer();
    SZrClosure *closure = ZrCore_Closure_New(state, 0);
    SZrGcRootLease closureRoot = {ZR_NULL};
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = function;
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &closure->super, &closureRoot));
    ZrCore_Function_ReleaseHostRoot(state, function);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    expect_answer(function);
    ZrCore_Gc_RootLeaseRelease(state->global, &closureRoot);
    /* Let GC destroy the function after all VM references disappear. */
}

static void test_nested_closure_retains_embedded_function_storage(void) {
    const char *source = "fn factory() { fn answer(): int { return 42; } return answer; } return factory();";
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "nested_function_lifetime.zr"));
    SZrTypeValue result;
    SZrGcRootLease closureRoot = {ZR_NULL};
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_CLOSURE, result.value.object->type);
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, result.value.object);
    TEST_ASSERT_NOT_NULL(closure->function->ownerFunction);
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &closure->super, &closureRoot));
    ZrCore_Function_ReleaseHostRoot(state, function);
    for (unsigned i = 0; i < 3; ++i) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        expect_answer(closure->function);
    }
    ZrCore_Gc_RootLeaseRelease(state->global, &closureRoot);
}

static void test_free_releases_the_compilation_root(void) {
    for (unsigned i = 0; i < 10; ++i) {
        SZrFunction *function = compile_answer();
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
        ZrCore_Function_Free(state, function);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    }
}

static void test_loaded_function_has_the_same_host_lifetime(void) {
    SZrIoSource source = {0};
    SZrIoModule module = {0};
    SZrIoFunction ioFunction = {0};
    SZrFunction *function;
    module.entryFunction = &ioFunction;
    source.modulesLength = 1;
    source.modules = &module;
    function = ZrCore_Io_LoadEntryFunctionToRuntime(state, &source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_NULL(function->instructionsList);
    ZrCore_Function_Free(state, function);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, &function->super));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
}

static void test_inline_children_do_not_copy_compiler_root_ownership(void) {
    SZrFunction *function = compile_answer();
    TEST_ASSERT_GREATER_THAN_UINT32(0, function->childFunctionLength);
    for (TZrUInt32 i = 0; i < function->childFunctionLength; ++i) {
        SZrFunction *child = &function->childFunctionList[i];
        TEST_ASSERT_NULL(child->hostRootOwner);
        TEST_ASSERT_EQUAL_UINT64(0, child->super.garbageCollectMark.temporaryRootRefCount);
        TEST_ASSERT_EQUAL_UINT64(ZR_MAX_SIZE, child->super.garbageCollectMark.ignoredRegistryIndex);
    }
    ZrCore_Function_Free(state, function);
}

static void test_compilation_failure_releases_temporary_function_roots(void) {
    const char *source = "fn answer(): int { return missing; } return answer();";
    TZrSize rootsBefore = state->global->garbageCollector->ignoredObjectCount;
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "failed_function.zr"));
    TEST_ASSERT_NULL(function);
    TEST_ASSERT_EQUAL_UINT64(rootsBefore, state->global->garbageCollector->ignoredObjectCount);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_null_function_super_conversion_is_safe);
    RUN_TEST(test_compiled_functions_survive_gc_before_and_between_executions);
    RUN_TEST(test_host_ownership_and_explicit_roots_are_independent);
    RUN_TEST(test_host_root_can_transfer_to_a_vm_closure);
    RUN_TEST(test_nested_closure_retains_embedded_function_storage);
    RUN_TEST(test_free_releases_the_compilation_root);
    RUN_TEST(test_loaded_function_has_the_same_host_lifetime);
    RUN_TEST(test_inline_children_do_not_copy_compiler_root_ownership);
    RUN_TEST(test_compilation_failure_releases_temporary_function_roots);
    return UNITY_END();
}
