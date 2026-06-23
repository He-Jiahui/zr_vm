#include "aot_c_typed_call_contract_support.h"

#include "aot_c_typed_call_contract_cases.h"

void test_aot_c_source_lowers_static_no_arg_i64_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_i64_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_i64_three_arg_function_call(FILE *file,",
            "TZrBool syncStackSlot",
    };
    static const char *const emitterSourceNeedles[] = {
            "backend_aot_c_try_get_i64_constant_return(",
            "backend_aot_c_try_get_i64_identity_return(",
            "backend_aot_c_try_get_i64_arg0_negate_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_not_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(",
            "backend_aot_c_try_get_i64_arg0_add_constant_return(",
            "backend_aot_c_try_get_i64_arg0_subtract_constant_return(",
            "backend_aot_c_try_get_i64_arg0_multiply_constant_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_add_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_subtract_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_multiply_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_divide_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_modulo_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(",
            "backend_aot_write_c_typed_i64_thunk_forward_decls(",
            "backend_aot_write_c_typed_i64_thunks(",
            "backend_aot_c_write_i64_no_arg_thunk_definition(",
            "backend_aot_c_write_i64_one_arg_thunk_definition(",
            "backend_aot_c_write_i64_two_arg_thunk_definition(",
            "backend_aot_c_write_i64_three_arg_thunk_definition(",
            "backend_aot_c_write_i64_three_arg_divide_thunk_definition(",
            "backend_aot_c_write_i64_three_arg_modulo_thunk_definition(",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state);",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state) {",
            "return (TZrInt64)%lld;",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0);",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "return (TZrInt64)(-zr_aot_arg0);",
            "return (TZrInt64)(~zr_aot_arg0);",
            "return (TZrInt64)(zr_aot_arg0 & (TZrInt64)%lld);",
            "return (TZrInt64)(zr_aot_arg0 | (TZrInt64)%lld);",
            "return (TZrInt64)(zr_aot_arg0 ^ (TZrInt64)%lld);",
            "return (TZrInt64)(zr_aot_arg0 + (TZrInt64)%lld);",
            "return (TZrInt64)(zr_aot_arg0 - (TZrInt64)%lld);",
            "return (TZrInt64)(zr_aot_arg0 * (TZrInt64)%lld);",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1);",
            "generated AOT signed divide by zero",
            "return (TZrInt64)0;",
            "return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1);",
            "generated AOT signed modulo by zero",
            "return (TZrInt64)(zr_aot_arg0 %% zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);",
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {",
            "return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);",
            "return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);",
            "return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);",
            "generated AOT signed three-arg divide by zero",
            "zr_aot_arg1 == 0 || zr_aot_arg2 == 0",
            "return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);",
            "generated AOT signed three-arg modulo by zero",
            "return (TZrInt64)(zr_aot_arg0 %% zr_aot_arg1 %% zr_aot_arg2);",
            "return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);",
            "return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);",
            "return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);",
    };
    static const char *const i64ThunkShapeNeedles[] = {
            "backend_aot_c_try_get_i64_constant_return(",
            "backend_aot_c_try_get_i64_identity_return(",
            "backend_aot_c_try_get_i64_arg0_negate_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_not_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(",
            "backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(",
            "backend_aot_c_try_get_i64_arg0_unary_return(",
            "backend_aot_c_try_get_i64_arg0_add_constant_return(",
            "backend_aot_c_try_get_i64_arg0_subtract_constant_return(",
            "backend_aot_c_try_get_i64_arg0_multiply_constant_return(",
            "backend_aot_c_type_ref_is_i64(",
            "!function->hasCallableReturnType",
            "!backend_aot_c_type_ref_is_i64(&function->callableReturnType)",
            "function->parameterMetadataCount < 1u",
            "!backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type)",
            "ZR_INSTRUCTION_ENUM(NEG_SIGNED)",
            "ZR_INSTRUCTION_ENUM(BITWISE_NOT)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST)",
            "backend_aot_c_try_get_i64_arg0_arg1_add_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_subtract_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_multiply_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_divide_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_modulo_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_binary_return(",
            "function->parameterMetadataCount < 2u",
            "!backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type)",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(",
            "backend_aot_c_try_read_i64_add_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(",
            "backend_aot_c_try_read_i64_subtract_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(",
            "backend_aot_c_try_read_i64_multiply_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(",
            "backend_aot_c_try_read_i64_divide_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(",
            "backend_aot_c_try_read_i64_modulo_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(",
            "backend_aot_c_try_read_i64_bitwise_and_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(",
            "backend_aot_c_try_read_i64_bitwise_or_operands(",
            "backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(",
            "backend_aot_c_try_read_i64_bitwise_xor_operands(",
            "function->parameterMetadataCount < 3u",
            "!backend_aot_c_type_ref_is_i64(&function->parameterMetadata[2].type)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)",
            "ZR_INSTRUCTION_ENUM(DIV_SIGNED)",
            "ZR_INSTRUCTION_ENUM(MOD_SIGNED)",
            "ZR_INSTRUCTION_ENUM(BITWISE_AND)",
            "ZR_INSTRUCTION_ENUM(BITWISE_OR)",
            "ZR_INSTRUCTION_ENUM(BITWISE_XOR)",
            "returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)",
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file,",
            "TZrBool syncStackSlot",
            "if (syncStackSlot) {",
            "zr_aot_static_i64_no_arg_direct_call",
            "zr_aot_s%u = zr_aot_typed_i64_fn_%u(state);",
            "zr_aot_static_i64_no_arg_direct_call_sync_stack_slot",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file,",
            "TZrUInt32 argumentSlot",
            "zr_aot_static_i64_one_arg_direct_call",
            "zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u);",
            "zr_aot_static_i64_one_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_i64_two_arg_function_call(FILE *file,",
            "TZrUInt32 firstArgumentSlot",
            "TZrUInt32 secondArgumentSlot",
            "zr_aot_static_i64_two_arg_direct_call",
            "zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u);",
            "zr_aot_static_i64_two_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_i64_three_arg_function_call(FILE *file,",
            "TZrUInt32 thirdArgumentSlot",
            "zr_aot_static_i64_three_arg_direct_call",
            "zr_aot_s%u = zr_aot_typed_i64_fn_%u(state, zr_aot_s%u, zr_aot_s%u, zr_aot_s%u);",
            "zr_aot_static_i64_three_arg_direct_call_sync_stack_slot",
    };
    static const char *const typedDirectCallNeedles[] = {
            "#include \"backend_aot_c_typed_direct_i64_calls.h\"",
            "backend_aot_try_write_c_static_direct_typed_function_call(",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(",
            "backend_aot_can_write_c_static_direct_i64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_three_arg_call(",
            "TZrBool syncI64StackSlot",
            "TZrUInt32 typedFirstArgumentSlot",
            "TZrUInt32 typedSecondArgumentSlot",
            "TZrUInt32 typedThirdArgumentSlot",
            "backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_i64_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_i64_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_i64_two_arg_function_call(file,",
            "backend_aot_write_c_static_direct_i64_three_arg_function_call(file,",
    };
    static const char *const typedDirectI64CallHeaderNeedles[] = {
            "#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_I64_CALLS_H",
            "backend_aot_can_write_c_static_direct_i64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_three_arg_call(",
    };
    static const char *const typedDirectI64CallNeedles[] = {
            "#include \"backend_aot_c_typed_direct_i64_calls.h\"",
            "#include \"backend_aot_c_emitter.h\"",
            "#include \"backend_aot_c_scalar_locals.h\"",
            "backend_aot_typed_direct_i64_call_find_function_entry_by_flat_index(",
            "backend_aot_can_write_c_static_direct_i64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_three_arg_call(",
            "TZrUInt32 *outArgumentSlot",
            "TZrUInt32 *outFirstArgumentSlot",
            "TZrUInt32 *outSecondArgumentSlot",
            "TZrUInt32 *outThirdArgumentSlot",
            "const TZrUInt32 argumentSlot = functionSlot + 1u;",
            "const TZrUInt32 firstArgumentSlot = functionSlot + 1u;",
            "const TZrUInt32 secondArgumentSlot = functionSlot + 2u;",
            "const TZrUInt32 thirdArgumentSlot = functionSlot + 3u;",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, thirdArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_i64_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_i64_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_i64_two_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_i64_three_arg_thunk(calleeEntry->function)",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c");
    char *i64ThunkShapesText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunk_shapes.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *typedDirectI64CallHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_i64_calls.h");
    char *typedDirectI64CallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_i64_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(i64ThunkShapesText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(typedDirectI64CallHeaderText);
    TEST_ASSERT_NOT_NULL(typedDirectI64CallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(i64ThunkShapesText, i64ThunkShapeNeedles, ARRAY_COUNT(i64ThunkShapeNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_i64_no_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_i64_one_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_i64_two_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_i64_three_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "backend_aot_typed_direct_call_find_function_entry_by_flat_index("));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(typedDirectI64CallHeaderText,
                             typedDirectI64CallHeaderNeedles,
                             ARRAY_COUNT(typedDirectI64CallHeaderNeedles));
    assert_text_contains_all(typedDirectI64CallText,
                             typedDirectI64CallNeedles,
                             ARRAY_COUNT(typedDirectI64CallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(i64ThunkShapesText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(typedDirectI64CallHeaderText);
    free(typedDirectI64CallText);
    free(functionBodyText);
}
