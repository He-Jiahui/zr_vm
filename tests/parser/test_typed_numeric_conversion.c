#include <string.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"

static TZrUInt32 count_opcode_recursive(const SZrFunction *function, EZrInstructionCode opcode, TZrUInt32 depth) {
    TZrUInt32 count = 0;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(depth < 64);

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            count += count_opcode_recursive(&function->childFunctionList[index], opcode, depth + 1);
        }
    }

    return count;
}

static SZrFunction *compile_source(SZrState *state, const char *source) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_numeric_conversion_test.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_typed_signed_to_float_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var i: int = 7;\n"
            "return <float> i;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 7.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_unsigned_to_float_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var u: uint = <uint>9;\n"
            "return <float> u;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 9.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_float_to_signed_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var f: float = 2.75;\n"
            "return <int> f;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_FLOAT), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(2, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_unsigned_to_signed_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var u: uint = <uint>17;\n"
            "return <int> u;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(17, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_unsigned_to_signed_cast_wraps_high_bit_like_unchecked_csharp(void) {
    const char *source =
            "var u: uint = <uint>-1;\n"
            "return <int> u;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED), 0));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(-1, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_float_to_unsigned_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var f: float = 12.75;\n"
            "return <uint> f;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_UINT64(12, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_signed_to_unsigned_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var i: int = -3;\n"
            "return <uint> i;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)-3, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_signed_to_float_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_to_float_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_float_to_signed_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_to_signed_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_to_signed_cast_wraps_high_bit_like_unchecked_csharp);
    RUN_TEST(test_typed_float_to_unsigned_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_signed_to_unsigned_cast_emits_direct_opcode_and_executes);
    return UNITY_END();
}
