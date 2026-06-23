#include "aot_c_typed_call_contract_support.h"

#include "aot_c_typed_call_contract_cases.h"

void test_aot_c_source_lowers_static_f64_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_f64_three_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_three_arg_function_call(FILE *file,",
    };
    static const char *const emitterSourceNeedles[] = {
            "backend_aot_c_typed_f64_thunk_shapes.h",
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(",
            "backend_aot_c_can_emit_typed_f64_three_arg_thunk(",
            "backend_aot_c_write_f64_three_arg_divide_thunk_definition(",
            "backend_aot_c_write_f64_three_arg_modulo_thunk_definition(",
            "backend_aot_write_c_typed_f64_thunk_forward_decls(",
            "backend_aot_write_c_typed_f64_thunks(",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state);",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state) {",
            "return (TZrFloat64)%.17g;",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "return (TZrFloat64)(-zr_aot_arg0);",
            "return (TZrFloat64)(zr_aot_arg0 + (TZrFloat64)%.17g);",
            "return (TZrFloat64)(zr_aot_arg0 - (TZrFloat64)%.17g);",
            "return (TZrFloat64)(zr_aot_arg0 * (TZrFloat64)%.17g);",
            "return (TZrFloat64)(zr_aot_arg0 / (TZrFloat64)%.17g);",
            "return (TZrFloat64)fmod(zr_aot_arg0, (TZrFloat64)%.17g);",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1);",
            "return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1);",
            "return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1);",
            "generated AOT float divide by zero",
            "generated AOT float modulo by zero",
            "return (TZrFloat64)0.0;",
            "return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1);",
            "return (TZrFloat64)fmod(zr_aot_arg0, zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);",
            "static TZrFloat64 zr_aot_typed_f64_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {",
            "return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);",
            "return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);",
            "return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);",
            "generated AOT float three-arg divide by zero",
            "zr_aot_arg1 == 0.0 || zr_aot_arg2 == 0.0",
            "return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);",
            "generated AOT float three-arg modulo by zero",
            "return (TZrFloat64)fmod(fmod(zr_aot_arg0, zr_aot_arg1), zr_aot_arg2);",
    };
    static const char *const f64ThunkShapeNeedles[] = {
            "backend_aot_c_type_ref_is_f64(",
            "backend_aot_c_try_get_f64_constant_return(",
            "backend_aot_c_try_get_f64_identity_return(",
            "backend_aot_c_try_get_f64_arg0_negate_return(",
            "backend_aot_c_try_get_f64_arg0_add_constant_return(",
            "backend_aot_c_try_get_f64_arg0_subtract_constant_return(",
            "backend_aot_c_try_get_f64_arg0_multiply_constant_return(",
            "backend_aot_c_try_get_f64_arg0_divide_constant_return(",
            "backend_aot_c_try_get_f64_arg0_modulo_constant_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_add_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_subtract_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_multiply_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_divide_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_modulo_return(",
            "ZR_INSTRUCTION_ENUM(DIV_FLOAT)",
            "ZR_INSTRUCTION_ENUM(MOD_FLOAT)",
            "ZR_INSTRUCTION_ENUM(NEG_FLOAT)",
            "ZR_VALUE_TYPE_FLOAT",
            "ZR_VALUE_TYPE_DOUBLE",
            "ZR_STATIC_C_TYPE_F64",
            "ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)",
            "returnValue == 0.0",
            "constantValue->value.nativeObject.nativeDouble",
    };
    static const char *const f64ThreeArgShapeNeedles[] = {
            "backend_aot_c_type_ref_is_f64(",
            "backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_arg2_subtract_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_arg2_multiply_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return(",
            "backend_aot_c_try_get_f64_arg0_arg1_arg2_modulo_return(",
            "ZR_INSTRUCTION_ENUM(ADD_FLOAT)",
            "ZR_INSTRUCTION_ENUM(SUB_FLOAT)",
            "ZR_INSTRUCTION_ENUM(MUL_FLOAT)",
            "ZR_INSTRUCTION_ENUM(DIV_FLOAT)",
            "ZR_INSTRUCTION_ENUM(MOD_FLOAT)",
            "ZR_VALUE_TYPE_DOUBLE",
            "ZR_STATIC_C_TYPE_F64",
            "function->parameterMetadataCount < 3u",
            "!backend_aot_c_type_ref_is_f64(&function->parameterMetadata[2].type)",
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_three_arg_function_call(FILE *file,",
            "TZrUInt32 argumentSlot",
            "TZrUInt32 firstArgumentSlot",
            "TZrUInt32 secondArgumentSlot",
            "TZrUInt32 thirdArgumentSlot",
            "zr_aot_static_f64_no_arg_direct_call",
            "zr_aot_static_f64_one_arg_direct_call",
            "zr_aot_static_f64_two_arg_direct_call",
            "zr_aot_static_f64_three_arg_direct_call",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state);",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u);",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u);",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u, zr_aot_f%u);",
            "zr_aot_static_f64_no_arg_direct_call_sync_stack_slot",
            "zr_aot_static_f64_one_arg_direct_call_sync_stack_slot",
            "zr_aot_static_f64_two_arg_direct_call_sync_stack_slot",
            "zr_aot_static_f64_three_arg_direct_call_sync_stack_slot",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "nativeDouble",
            "ZR_VALUE_TYPE_DOUBLE",
    };
    static const char *const typedDirectCallNeedles[] = {
            "backend_aot_can_write_c_static_direct_f64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_three_arg_call(",
            "backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(",
            "TZrUInt32 typedThirdArgumentSlot",
            "backend_aot_write_c_static_direct_f64_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_f64_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_f64_two_arg_function_call(file,",
            "backend_aot_write_c_static_direct_f64_three_arg_function_call(file,",
    };
    static const char *const typedDirectF64CallNeedles[] = {
            "backend_aot_can_write_c_static_direct_f64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_three_arg_call(",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, argumentSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, thirdArgumentSlot)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, thirdArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_f64_three_arg_thunk(calleeEntry->function)",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.c");
    char *f64ThunkShapeText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunk_shapes.c");
    char *f64ThreeArgShapeText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_three_arg_shapes.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *typedDirectF64CallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_f64_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));

    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(f64ThunkShapeText);
    TEST_ASSERT_NOT_NULL(f64ThreeArgShapeText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(typedDirectF64CallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(f64ThunkShapeText, f64ThunkShapeNeedles, ARRAY_COUNT(f64ThunkShapeNeedles));
    assert_text_contains_all(f64ThreeArgShapeText, f64ThreeArgShapeNeedles, ARRAY_COUNT(f64ThreeArgShapeNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(typedDirectF64CallText,
                             typedDirectF64CallNeedles,
                             ARRAY_COUNT(typedDirectF64CallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(f64ThunkShapeText);
    free(f64ThreeArgShapeText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(typedDirectF64CallText);
    free(functionBodyText);
}
