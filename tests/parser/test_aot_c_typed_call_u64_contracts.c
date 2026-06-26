#include "aot_c_typed_call_contract_support.h"

#include "aot_c_typed_call_contract_cases.h"

void test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_u64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_three_arg_function_call(FILE *file,",
    };
    static const char *const emitterSourceNeedles[] = {
            "#include \"backend_aot_c_typed_u64_thunk_shapes.h\"",
            "#include \"backend_aot_c_typed_u64_three_arg_thunks.h\"",
            "backend_aot_c_type_ref_is_u64(",
            "backend_aot_c_try_get_u64_constant_return(",
            "backend_aot_c_try_get_u64_identity_return(",
            "backend_aot_c_try_get_u64_arg0_add_constant_return(",
            "backend_aot_c_try_get_u64_arg0_subtract_constant_return(",
            "backend_aot_c_try_get_u64_arg0_multiply_constant_return(",
            "backend_aot_c_try_get_u64_arg0_bitwise_and_constant_return(",
            "backend_aot_c_try_get_u64_arg0_bitwise_or_constant_return(",
            "backend_aot_c_try_get_u64_arg0_bitwise_xor_constant_return(",
            "ZR_VALUE_TYPE_UINT32",
            "ZR_STATIC_C_TYPE_U32",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)",
            "constantValue->value.nativeObject.nativeUInt64",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)",
            "constantValue->value.nativeObject.nativeInt64 < 0",
            "ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(TO_INT)",
            "ZR_INSTRUCTION_ENUM(TO_UINT)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(BITWISE_AND)",
            "ZR_INSTRUCTION_ENUM(BITWISE_OR)",
            "ZR_INSTRUCTION_ENUM(BITWISE_XOR)",
            "backend_aot_c_can_emit_typed_u64_no_arg_thunk(",
            "backend_aot_c_can_emit_typed_u64_one_arg_thunk(",
            "backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(",
            "backend_aot_write_c_typed_u64_thunk_forward_decls(",
            "backend_aot_write_c_typed_u64_thunks(",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(void);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(void) {",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0) {",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "backend_aot_c_write_u64_two_arg_divide_thunk_definition(",
            "backend_aot_c_write_u64_two_arg_modulo_thunk_definition(",
            "backend_aot_c_write_u64_three_arg_thunk_forward_decl(",
            "backend_aot_c_write_u64_three_arg_state_free_thunk_forward_decl(",
            "backend_aot_c_try_write_u64_three_arg_thunk_definition(",
            "return (TZrUInt64)%llu;",
            "return zr_aot_arg0;",
            "return (TZrUInt64)(zr_aot_arg0 + (TZrUInt64)%llu);",
            "return (TZrUInt64)(zr_aot_arg0 - (TZrUInt64)%llu);",
            "return (TZrUInt64)(zr_aot_arg0 * (TZrUInt64)%llu);",
            "return (TZrUInt64)(zr_aot_arg0 & (TZrUInt64)%llu);",
            "return (TZrUInt64)(zr_aot_arg0 | (TZrUInt64)%llu);",
            "return (TZrUInt64)(zr_aot_arg0 ^ (TZrUInt64)%llu);",
            "return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1);",
            "generated AOT unsigned divide by zero",
            "generated AOT unsigned modulo by zero",
            "return (TZrUInt64)0;",
            "return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 %% zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
    };
    static const char *const u64ThreeArgThunkHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(const SZrFunction *function)",
            "backend_aot_c_write_u64_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex)",
            "backend_aot_c_write_u64_three_arg_state_free_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex)",
            "backend_aot_c_try_write_u64_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry)",
    };
    static const char *const u64ThreeArgThunkNeedles[] = {
            "#include \"backend_aot_c_typed_u64_three_arg_thunks.h\"",
            "#include \"backend_aot_c_typed_u64_thunk_shapes.h\"",
            "backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(const SZrFunction *function)",
            "backend_aot_c_write_u64_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex)",
            "backend_aot_c_write_u64_three_arg_state_free_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex)",
            "backend_aot_c_try_write_u64_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_divide_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return(function)",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return(function)",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {",
            "return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);",
            "return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);",
            "return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);",
            "backend_aot_c_write_u64_three_arg_divide_thunk_definition(",
            "backend_aot_c_write_u64_three_arg_modulo_thunk_definition(",
            "generated AOT unsigned three-arg divide by zero",
            "generated AOT unsigned three-arg modulo by zero",
            "zr_aot_arg1 == 0u || zr_aot_arg2 == 0u",
            "return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);",
            "return (TZrUInt64)(zr_aot_arg0 %% zr_aot_arg1 %% zr_aot_arg2);",
            "return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);",
            "return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);",
            "return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);",
    };
    static const char *const u64ThunkShapeNeedles[] = {
            "backend_aot_c_type_ref_is_u64(",
            "backend_aot_c_try_get_u64_arg0_arg1_add_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_subtract_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_multiply_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_divide_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_modulo_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_bitwise_and_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_bitwise_or_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_bitwise_xor_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_divide_return(",
            "backend_aot_c_try_read_u64_divide_operands(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return(",
            "backend_aot_c_try_read_u64_modulo_operands(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return(",
            "backend_aot_c_try_read_u64_bitwise_or_operands(",
            "backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return(",
            "backend_aot_c_try_read_u64_bitwise_xor_operands(",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)",
            "ZR_INSTRUCTION_ENUM(DIV_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(MOD_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(BITWISE_AND)",
            "ZR_INSTRUCTION_ENUM(BITWISE_OR)",
            "ZR_INSTRUCTION_ENUM(BITWISE_XOR)",
            "ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)",
            "leftSlot = 0u;",
            "rightSlot = 1u;",
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_static_direct_u64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_three_arg_function_call(FILE *file,",
            "TZrUInt32 argumentSlot",
            "TZrUInt32 firstArgumentSlot",
            "TZrUInt32 secondArgumentSlot",
            "TZrUInt32 thirdArgumentSlot",
            "TZrBool passStateToThunk",
            "zr_aot_static_u64_no_arg_direct_call",
            "zr_aot_static_u64_one_arg_direct_call",
            "zr_aot_static_u64_two_arg_direct_call",
            "zr_aot_static_u64_three_arg_direct_call",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u();",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u);",
            "if (passStateToThunk) {",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u);",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u, zr_aot_u%u);",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u, zr_aot_u%u);",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(zr_aot_u%u, zr_aot_u%u, zr_aot_u%u);",
            "zr_aot_static_u64_no_arg_direct_call_sync_stack_slot",
            "zr_aot_static_u64_one_arg_direct_call_sync_stack_slot",
            "zr_aot_static_u64_two_arg_direct_call_sync_stack_slot",
            "zr_aot_static_u64_three_arg_direct_call_sync_stack_slot",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "nativeUInt64",
            "ZR_VALUE_TYPE_UINT64",
    };
    static const char *const typedDirectCallNeedles[] = {
            "backend_aot_can_write_c_static_direct_u64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_three_arg_call(",
            "TZrBool typedU64TwoArgCallPassState",
            "TZrBool typedU64ThreeArgCallPassState",
            "TZrBool syncU64StackSlot",
            "backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_u64_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_u64_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_u64_two_arg_function_call(file,",
            "&typedU64TwoArgCallPassState",
            "backend_aot_write_c_static_direct_u64_three_arg_function_call(file,",
            "&typedU64ThreeArgCallPassState",
    };
    static const char *const typedDirectU64CallNeedles[] = {
            "backend_aot_can_write_c_static_direct_u64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_two_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_three_arg_call(",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, argumentSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, thirdArgumentSlot)",
            "TZrBool *outPassStateToThunk",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, thirdArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_u64_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_u64_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(calleeEntry->function)",
            "*outPassStateToThunk = (TZrBool)!backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(",
            "backend_aot_c_can_emit_typed_u64_three_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(calleeEntry->function)",
            "*outPassStateToThunk = (TZrBool)!backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c");
    char *u64ThreeArgThunkHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.h");
    char *u64ThreeArgThunkText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.c");
    char *u64ThunkShapeText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *typedDirectU64CallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_u64_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));

    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(u64ThreeArgThunkHeaderText);
    TEST_ASSERT_NOT_NULL(u64ThreeArgThunkText);
    TEST_ASSERT_NOT_NULL(u64ThunkShapeText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(typedDirectU64CallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(u64ThreeArgThunkHeaderText,
                             u64ThreeArgThunkHeaderNeedles,
                             ARRAY_COUNT(u64ThreeArgThunkHeaderNeedles));
    assert_text_contains_all(u64ThreeArgThunkText, u64ThreeArgThunkNeedles, ARRAY_COUNT(u64ThreeArgThunkNeedles));
    assert_text_contains_all(u64ThunkShapeText, u64ThunkShapeNeedles, ARRAY_COUNT(u64ThunkShapeNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(typedDirectU64CallText,
                             typedDirectU64CallNeedles,
                             ARRAY_COUNT(typedDirectU64CallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(u64ThreeArgThunkHeaderText);
    free(u64ThreeArgThunkText);
    free(u64ThunkShapeText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(typedDirectU64CallText);
    free(functionBodyText);
}
