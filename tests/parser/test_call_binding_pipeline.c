#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_parser/compiler.h"

static SZrState *state;

void setUp(void) {
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(state);
    state = ZR_NULL;
}

static SZrFunction *compile_source(const char *source) {
    return ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "call_binding_pipeline.zr"));
}

static const SZrFunctionCallSiteCacheEntry *find_binding(const SZrFunction *function) {
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->binding.contract.bindingKind != 0u) {
            return entry;
        }
    }
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = find_binding(&function->childFunctionList[index]);
        if (entry != ZR_NULL) return entry;
    }
    return ZR_NULL;
}

static void count_property_bindings(const SZrFunction *function,
                                    TZrUInt32 *getters,
                                    TZrUInt32 *setters) {
    if (function == ZR_NULL) return;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (entry->binding.contract.operation == ZR_CALL_BINDING_OPERATION_GET) ++*getters;
        if (entry->binding.contract.operation == ZR_CALL_BINDING_OPERATION_SET) ++*setters;
    }
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        count_property_bindings(&function->childFunctionList[index], getters, setters);
    }
}

static void assert_bound_result(const char *source, TZrInt64 expected) {
    SZrFunction *function = compile_source(source);
    const SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    entry = find_binding(function);
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "a statically selected call needs a binding contract");
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.targetMetadataToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.signatureToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, entry->binding.contract.signatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, entry->binding.contract.moduleSignatureHash);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
}

static void test_static_method_has_token_binding(void) {
    assert_bound_result(
            "class Math { pub static fn answer(): int { return 42; } }\n"
            "return Math.answer();\n", 42);
}

static void test_object_chain_preserves_field_reads_and_binds_final_call(void) {
    assert_bound_result(
            "class Leaf { pub fn read(): int { return 17; } }\n"
            "class Middle { pub var leaf: Leaf; pub @constructor() { this.leaf = new Leaf(); } }\n"
            "class Root { pub var middle: Middle; pub @constructor() { this.middle = new Middle(); } }\n"
            "var root = new Root(); return root.middle.leaf.read();\n", 17);
}

static void test_unknown_static_member_is_a_compile_error(void) {
    TEST_ASSERT_NULL(compile_source(
            "class Box { pub fn read(): int { return 1; } }\n"
            "var box = new Box(); return box.missing();\n"));
}

static void test_invalidated_generation_rejects_static_call(void) {
    SZrFunction *function = compile_source(
            "class Math { pub static fn answer(): int { return 42; } } return Math.answer();");
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(find_binding(function));
    ++function->callBindingGeneration;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, state->lastCallBindingError.targetMetadataToken);
}

static void test_property_getter_and_setter_have_binding_contracts(void) {
    SZrFunction *function = compile_source(
            "class Box { pri var stored: int = 1; "
            "pub property value: int { get { return this.stored; } set { this.stored = value; } } } "
            "var box = new Box(); box.value = 31; return box.value;");
    TZrUInt32 getters = 0u, setters = 0u;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    count_property_bindings(function, &getters, &setters);
    TEST_ASSERT_EQUAL_UINT32(1u, getters);
    TEST_ASSERT_EQUAL_UINT32(1u, setters);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(31, result);
}

static void test_virtual_binding_uses_receiver_override(void) {
    assert_bound_result(
            "class Base { pub virtual fn read(): int { return 1; } } "
            "class Derived : Base { pub override fn read(): int { return 23; } } "
            "var box: Base = new Derived(); return box.read();", 23);
}

static void test_interface_binding_uses_contract_slot(void) {
    SZrFunction *function = compile_source(
            "interface Readable { fn read(): int; } "
            "class Box : Readable { pub fn read(): int { return 37; } } "
            "var box: Readable = new Box(); return box.read();");
    TEST_ASSERT_NOT_NULL(function);
    assert_bound_result(
            "interface Readable { fn read(): int; } "
            "class Box : Readable { pub fn read(): int { return 37; } } "
            "var box: Readable = new Box(); return box.read();", 37);
}

static void test_virtual_parameter_dispatches_multiple_receiver_types(void) {
    assert_bound_result(
            "class Base { pub virtual fn read(): int { return 1; } } "
            "class First : Base { pub override fn read(): int { return 17; } } "
            "class Second : Base { pub override fn read(): int { return 25; } } "
            "fn readValue(value: Base): int { return value.read(); } "
            "return readValue(new First()) + readValue(new Second());", 42);
}

static void test_two_interfaces_with_equal_slots_remain_distinct(void) {
    assert_bound_result(
            "interface Left { fn left(): int; } interface Right { fn right(): int; } "
            "class Both : Left, Right { pub fn left(): int { return 13; } "
            "pub fn right(): int { return 29; } } "
            "fn fromLeft(value: Left): int { return value.left(); } "
            "fn fromRight(value: Right): int { return value.right(); } "
            "var value = new Both(); return fromLeft(value) + fromRight(value);", 42);
}

static void test_meta_call_consumes_bound_target_with_and_without_arguments(void) {
    assert_bound_result(
            "class Callable { pub @call(value: int): int { return value + 1; } } "
            "var value = new Callable(); return value(41);", 42);
    assert_bound_result(
            "class Callable { pub @call(): int { return 42; } } "
            "var value = new Callable(); return value();", 42);
}

static void test_meta_call_rejects_stale_binding_generation(void) {
    SZrFunction *function = compile_source(
            "class Callable { pub @call(): int { return 42; } } "
            "var value = new Callable(); return value();");
    const SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    entry = find_binding(function);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_OPERATION_META, entry->binding.contract.operation);
    ++function->callBindingGeneration;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
}

static void test_nested_method_body_is_linked_and_invalidated(void) {
    SZrFunction *function = compile_source(
            "class Box { pub fn read(): int { return 42; } } "
            "class Holder { pub var box: Box; pub @constructor() { this.box = new Box(); } "
            "pub fn read(): int { return this.box.read(); } } "
            "var holder = new Holder(); return holder.read();");
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
    TEST_ASSERT_TRUE(ZrCore_CallBinding_AdvanceGeneration(function));
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
}

static void test_interface_implementation_layout_change_is_rejected(void) {
    SZrFunction *function = compile_source(
            "interface Readable { fn read(): int; } "
            "class Box : Readable { pub fn read(): int { return 42; } } "
            "var value: Readable = new Box(); return value.read();");
    TZrInt64 result = 0;
    TZrUInt32 changed = 0u;
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0u; index < function->prototypeInstancesLength; ++index) {
        SZrObjectPrototype *prototype = function->prototypeInstances[index];
        if (prototype != ZR_NULL && prototype->interfaceDispatchCount != 0u) {
            ZrCore_ObjectPrototype_MarkMutation(prototype);
            ++changed;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, changed);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_LAYOUT_MISMATCH, state->lastCallBindingError.status);
}

static void test_bound_graph_survives_full_collection(void) {
    SZrFunction *function = compile_source(
            "class Box { pub fn read(): int { return 42; } } "
            "class Holder { pub var box: Box; pub @constructor() { this.box = new Box(); } "
            "pub fn read(): int { return this.box.read(); } } "
            "var holder = new Holder(); return holder.read();");
    TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Stack_SetRawObjectValue(state, rootSlot, (SZrRawObject *)function);
    state->stackTop.valuePointer = rootSlot + 1;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    function = (SZrFunction *)ZrCore_Stack_GetValue(rootSlot)->value.object;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_static_method_has_token_binding);
    RUN_TEST(test_object_chain_preserves_field_reads_and_binds_final_call);
    RUN_TEST(test_unknown_static_member_is_a_compile_error);
    RUN_TEST(test_invalidated_generation_rejects_static_call);
    RUN_TEST(test_property_getter_and_setter_have_binding_contracts);
    RUN_TEST(test_virtual_binding_uses_receiver_override);
    RUN_TEST(test_interface_binding_uses_contract_slot);
    RUN_TEST(test_virtual_parameter_dispatches_multiple_receiver_types);
    RUN_TEST(test_two_interfaces_with_equal_slots_remain_distinct);
    RUN_TEST(test_meta_call_consumes_bound_target_with_and_without_arguments);
    RUN_TEST(test_meta_call_rejects_stale_binding_generation);
    RUN_TEST(test_nested_method_body_is_linked_and_invalidated);
    RUN_TEST(test_interface_implementation_layout_change_is_rejected);
    RUN_TEST(test_bound_graph_survives_full_collection);
    return UNITY_END();
}
