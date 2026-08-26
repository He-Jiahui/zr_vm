#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrFunction *compile_source(const TZrChar *source, const TZrChar *name) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    ZrParser_Ast_Free(g_state, script);
    return function;
}

static TZrBool function_tree_contains_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode,
        TZrUInt32 depth) {
    if (function == ZR_NULL || depth > 32U) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0U; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index]
                    .instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }
    for (TZrUInt32 index = 0U; index < function->childFunctionLength; index++) {
        if (function_tree_contains_opcode(
                    &function->childFunctionList[index], opcode, depth + 1U)) {
            return ZR_TRUE;
        }
    }
    for (TZrUInt32 index = 0U; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        const SZrFunction *child;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        child = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (child != function && function_tree_contains_opcode(child, opcode, depth + 1U)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const TZrChar *counter_resource_declaration(void) {
    return
            "resource class Counter {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub const fn read(): int { return this.value; }\n"
            "  pub fn write(next: int): int { this.value = next; return this.value; }\n"
            "  pub const fn borrowValue(): ref readonly int { return this.value; }\n"
            "}\n";
}

static SZrFunction *compile_counter_program(
        const TZrChar *body,
        const TZrChar *name) {
    TZrChar source[4096];
    int written = snprintf(
            source,
            sizeof(source),
            "%s%s",
            counter_resource_declaration(),
            body);

    TEST_ASSERT_GREATER_THAN_INT(0, written);
    TEST_ASSERT_LESS_THAN_INT((int)sizeof(source), written);
    return compile_source(source, name);
}

static void assert_no_runtime_loan_enforcement(const SZrFunction *function) {
    TEST_ASSERT_FALSE(function_tree_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(OWN_LOAN), 0U));
    TEST_ASSERT_FALSE(function_tree_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN), 0U));
}

static void test_unique_shared_and_in_receivers_use_compile_time_loans(void) {
    SZrFunction *function = compile_counter_program(
            "fn inspect(value: in Counter): int { return value.read(); }\n"
            "fn run(): int {\n"
            "  var unique: Unique<Counter> = own Counter(1);\n"
            "  unique.write(unique.read() + 1);\n"
            "  var observed = inspect(unique);\n"
            "  var shared: Shared<Counter> = share(unique);\n"
            "  var sharedValue = shared.read();\n"
            "  drop(shared);\n"
            "  return observed + sharedValue;\n"
            "}\n"
            "return run();\n",
            "resource_owner_in_receiver.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_tree_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED), 0U));
    TEST_ASSERT_FALSE(function_tree_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(OWN_BORROW), 0U));
    TEST_ASSERT_FALSE(function_tree_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(OWN_DETACH), 0U));
    assert_no_runtime_loan_enforcement(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(4, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_shared_write_is_rejected_and_weak_direct_read_is_guarded(void) {
    SZrFunction *sharedWrite = compile_counter_program(
            "var unique: Unique<Counter> = own Counter(1);\n"
            "var shared: Shared<Counter> = share(unique);\n"
            "shared.write(2);\n",
            "resource_shared_write_rejected.zr");
    SZrFunction *weakRead = compile_counter_program(
            "var unique: Unique<Counter> = own Counter(1);\n"
            "var shared: Shared<Counter> = share(unique);\n"
            "var weak: Weak<Counter> = degrade(shared);\n"
            "weak.read();\n",
            "resource_weak_direct_read_rejected.zr");

    TEST_ASSERT_NULL(sharedWrite);
    TEST_ASSERT_NOT_NULL(weakRead);
    TEST_ASSERT_TRUE(function_tree_contains_opcode(
            weakRead, ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL), 0U));
    ZrCore_Function_Free(g_state, weakRead);
}

static void test_unique_receiver_two_phase_allows_read_and_rejects_write_argument(void) {
    SZrFunction *valid = compile_counter_program(
            "var owner: Unique<Counter> = own Counter(1);\n"
            "owner.write(owner.read() + 1);\n"
            "return owner.read();\n",
            "resource_unique_two_phase_read.zr");
    SZrFunction *invalid = compile_counter_program(
            "var owner: Unique<Counter> = own Counter(1);\n"
            "owner.write(owner.write(2));\n",
            "resource_unique_two_phase_write_rejected.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(valid);
    assert_no_runtime_loan_enforcement(valid);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, valid, &result));
    TEST_ASSERT_EQUAL_INT64(2, result);
    TEST_ASSERT_NULL(invalid);
    ZrCore_Function_Free(g_state, valid);
}

static void test_live_owner_ref_blocks_drop_share_and_move(void) {
    SZrFunction *dropConflict = compile_counter_program(
            "fn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  var alias: ref int = owner.borrowValue();\n"
            "  drop(owner);\n"
            "  return alias;\n"
            "}\n"
            "return run();\n",
            "resource_owner_ref_drop_conflict.zr");
    SZrFunction *shareConflict = compile_counter_program(
            "fn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  var alias: ref int = owner.borrowValue();\n"
            "  var shared: Shared<Counter> = share(owner);\n"
            "  return alias;\n"
            "}\n"
            "return run();\n",
            "resource_owner_ref_share_conflict.zr");
    SZrFunction *moveConflict = compile_counter_program(
            "fn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  var alias: ref int = owner.borrowValue();\n"
            "  var moved: Unique<Counter> = owner;\n"
            "  return alias;\n"
            "}\n"
            "return run();\n",
            "resource_owner_ref_move_conflict.zr");

    TEST_ASSERT_NULL(dropConflict);
    TEST_ASSERT_NULL(shareConflict);
    TEST_ASSERT_NULL(moveConflict);
}

static void test_owner_ref_last_use_allows_later_move(void) {
    SZrFunction *function = compile_counter_program(
            "fn run(): int {\n"
            "  var owner: Unique<Counter> = own Counter(7);\n"
            "  var alias: ref int = owner.borrowValue();\n"
            "  var observed = alias + 0;\n"
            "  var moved: Unique<Counter> = owner;\n"
            "  drop(moved);\n"
            "  return observed;\n"
            "}\n"
            "return run();\n",
            "resource_owner_ref_last_use_move.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    assert_no_runtime_loan_enforcement(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_owner_ref_cannot_escape_its_owner(void) {
    SZrFunction *function = compile_counter_program(
            "fn leak(): ref readonly int {\n"
            "  var owner: Unique<Counter> = own Counter(1);\n"
            "  return owner.borrowValue();\n"
            "}\n",
            "resource_owner_ref_escape_rejected.zr");
    SZrFunction *overloaded = compile_source(
            "resource class Box {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub const fn borrowValue(marker: int): int { return marker; }\n"
            "  pub fn borrowValue(): ref int { return this.value; }\n"
            "}\n"
            "fn leak(): ref int {\n"
            "  var owner: Unique<Box> = own Box(1);\n"
            "  return owner.borrowValue();\n"
            "}\n",
            "resource_owner_overloaded_ref_escape_rejected.zr");

    TEST_ASSERT_NULL(function);
    TEST_ASSERT_NULL(overloaded);
}

static void test_weak_receiver_ref_results_cannot_escape_temporary_wake(void) {
    SZrFunction *methodResult = compile_source(
            "resource class ReadonlyCounter {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub const fn borrowValue(): ref readonly int {\n"
            "    return ref this.value;\n"
            "  }\n"
            "}\n"
            "fn leak(): ref readonly int {\n"
            "  var owner: Unique<ReadonlyCounter> = own ReadonlyCounter(1);\n"
            "  var shared: Shared<ReadonlyCounter> = share(owner);\n"
            "  var weak: Weak<ReadonlyCounter> = degrade(shared);\n"
            "  return weak.borrowValue();\n"
            "}\n",
            "resource_weak_ref_method_escape_rejected.zr");
    SZrFunction *propertyResult = compile_source(
            "resource class Box {\n"
            "  pub var stored: int;\n"
            "  pub @constructor(value: int) { this.stored = value; }\n"
            "  pub property value: ref int {\n"
            "    get { return ref this.stored; }\n"
            "  }\n"
            "}\n"
            "fn leak(): ref int {\n"
            "  var owner: Unique<Box> = own Box(1);\n"
            "  var shared: Shared<Box> = share(owner);\n"
            "  var weak: Weak<Box> = degrade(shared);\n"
            "  return ref weak.value;\n"
            "}\n",
            "resource_weak_ref_property_escape_rejected.zr");
    SZrFunction *deepMemberResult = compile_source(
            "resource class Leaf {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "  pub fn borrowValue(): ref int { return this.value; }\n"
            "}\n"
            "resource class Root {\n"
            "  pub var child: Unique<Leaf>;\n"
            "  pub @constructor() { this.child = own Leaf(1); }\n"
            "}\n"
            "fn leak(): ref int {\n"
            "  var owner: Unique<Root> = own Root();\n"
            "  var shared: Shared<Root> = share(owner);\n"
            "  var weak: Weak<Root> = degrade(shared);\n"
            "  return weak.child.borrowValue();\n"
            "}\n",
            "resource_weak_deep_ref_method_escape_rejected.zr");

    TEST_ASSERT_NULL(methodResult);
    TEST_ASSERT_NULL(propertyResult);
    TEST_ASSERT_NULL(deepMemberResult);
}

static void test_weak_receiver_value_results_can_leave_temporary_wake(void) {
    SZrFunction *function = compile_source(
            "resource class Leaf {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Root {\n"
            "  pub var child: Unique<Leaf>;\n"
            "  pub @constructor() { this.child = own Leaf(7); }\n"
            "}\n"
            "fn read(): int {\n"
            "  var owner: Unique<Root> = own Root();\n"
            "  var shared: Shared<Root> = share(owner);\n"
            "  var weak: Weak<Root> = degrade(shared);\n"
            "  return weak.child.value;\n"
            "}\n"
            "return read();\n",
            "resource_weak_deep_value_return_allowed.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unique_shared_and_in_receivers_use_compile_time_loans);
    RUN_TEST(test_shared_write_is_rejected_and_weak_direct_read_is_guarded);
    RUN_TEST(test_unique_receiver_two_phase_allows_read_and_rejects_write_argument);
    RUN_TEST(test_live_owner_ref_blocks_drop_share_and_move);
    RUN_TEST(test_owner_ref_last_use_allows_later_move);
    RUN_TEST(test_owner_ref_cannot_escape_its_owner);
    RUN_TEST(test_weak_receiver_ref_results_cannot_escape_temporary_wake);
    RUN_TEST(test_weak_receiver_value_results_can_leave_temporary_wake);
    return UNITY_END();
}
