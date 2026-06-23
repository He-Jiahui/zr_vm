#include "aot_c_typed_call_contract_support.h"

#include "aot_c_typed_call_contract_cases.h"

void test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_three_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_bool_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_bool_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_bool_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_bool_three_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(FILE *file,",
    };
    static const char *const emitterSourceNeedles[] = {
            "#include \"backend_aot_c_typed_bool_two_arg_thunks.h\"",
            "#include \"backend_aot_c_typed_bool_three_arg_thunks.h\"",
            "backend_aot_c_type_ref_is_bool(",
            "backend_aot_c_type_ref_is_i64(",
            "backend_aot_c_type_ref_is_u64(",
            "backend_aot_c_type_ref_is_f64(",
            "backend_aot_c_try_get_bool_constant_return(",
            "backend_aot_c_try_get_bool_identity_return(",
            "backend_aot_c_try_get_bool_arg0_logical_not_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_less_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_equal_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_not_equal_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_greater_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(",
            "backend_aot_c_try_get_bool_i64_arg0_arg1_greater_equal_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_less_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_equal_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_not_equal_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_less_equal_return(",
            "backend_aot_c_try_get_bool_u64_arg0_arg1_greater_equal_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_less_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_equal_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_not_equal_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_greater_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_less_equal_return(",
            "backend_aot_c_try_get_bool_f64_arg0_arg1_greater_equal_return(",
            "backend_aot_c_can_emit_typed_bool_three_arg_thunk(",
            "ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT)",
            "ZR_VALUE_IS_TYPE_BOOL(constantValue->type)",
            "constantValue->value.nativeObject.nativeBool",
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_three_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(",
            "backend_aot_write_c_typed_bool_thunk_forward_decls(",
            "backend_aot_write_c_typed_bool_thunks(",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state) {",
            "return %s;",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "return (TZrBool)!zr_aot_arg0;",
            "backend_aot_c_write_bool_two_arg_thunk_forward_decl(",
            "backend_aot_c_try_write_bool_two_arg_thunk_definition(",
            "backend_aot_c_write_bool_three_arg_thunk_forward_decl(",
            "backend_aot_c_try_write_bool_three_arg_thunk_definition(",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "backend_aot_c_write_bool_f64_two_arg_less_thunk_definition(",
            "backend_aot_c_write_bool_f64_two_arg_equal_thunk_definition(",
            "backend_aot_c_write_bool_f64_two_arg_not_equal_thunk_definition(",
            "backend_aot_c_write_bool_f64_two_arg_greater_thunk_definition(",
            "backend_aot_c_write_bool_f64_two_arg_less_equal_thunk_definition(",
            "backend_aot_c_write_bool_f64_two_arg_greater_equal_thunk_definition(",
            "return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);",
    };
    static const char *const boolTwoArgThunkHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_write_bool_two_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex)",
            "backend_aot_c_try_write_bool_two_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry)",
    };
    static const char *const boolTwoArgThunkNeedles[] = {
            "#include \"backend_aot_c_typed_bool_two_arg_thunks.h\"",
            "backend_aot_c_try_get_bool_arg0_arg1_equal_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_not_equal_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_write_bool_two_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex)",
            "backend_aot_c_try_write_bool_two_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_AND)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_OR)",
            "ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE)",
            "ZR_INSTRUCTION_ENUM(JUMP)",
            "function->instructionsLength == 6u",
            "function->instructionsLength == 7u",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);",
    };
    static const char *const boolThreeArgThunkNeedles[] = {
            "backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_and_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_or_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_return(",
            "backend_aot_c_write_bool_three_arg_logical_or_thunk_definition(",
            "function->parameterCount != 3",
            "function->parameterMetadataCount < 3u",
            "!backend_aot_c_type_ref_is_bool(&function->parameterMetadata[2].type)",
            "function->instructionsLength != 10u",
            "function->instructionsLength != 12u",
            "ZR_INSTRUCTION_ENUM(LOGICAL_AND)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_OR)",
            "ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE)",
            "ZR_INSTRUCTION_ENUM(JUMP)",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {",
            "return (TZrBool)(zr_aot_arg0 && zr_aot_arg1 && zr_aot_arg2);",
            "return (TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2);",
    };
    static const char *const boolCallLoweringNeedles[] = {
            "#include \"backend_aot_c_emitter.h\"",
            "backend_aot_write_c_static_direct_bool_no_arg_function_call(FILE *file,",
            "zr_aot_static_bool_no_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state);",
            "zr_aot_static_bool_no_arg_direct_call_sync_stack_slot",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "nativeBool",
            "ZR_VALUE_TYPE_BOOL",
            "backend_aot_write_c_static_direct_bool_one_arg_function_call(FILE *file,",
            "TZrUInt32 argumentSlot",
            "zr_aot_static_bool_one_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_b%u);",
            "zr_aot_static_bool_one_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_bool_two_arg_function_call(FILE *file,",
            "TZrUInt32 firstArgumentSlot",
            "TZrUInt32 secondArgumentSlot",
            "zr_aot_static_bool_two_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_b%u, zr_aot_b%u);",
            "zr_aot_static_bool_two_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_bool_three_arg_function_call(FILE *file,",
            "TZrUInt32 thirdArgumentSlot",
            "zr_aot_static_bool_three_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_b%u, zr_aot_b%u, zr_aot_b%u);",
            "zr_aot_static_bool_three_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(FILE *file,",
            "zr_aot_static_i64_bool_two_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_s%u, zr_aot_s%u);",
            "zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(FILE *file,",
            "zr_aot_static_u64_bool_two_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_u%u, zr_aot_u%u);",
            "zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot",
            "backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(FILE *file,",
            "zr_aot_static_f64_bool_two_arg_direct_call",
            "zr_aot_b%u = zr_aot_typed_bool_fn_%u(state, zr_aot_f%u, zr_aot_f%u);",
            "zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot",
    };
    static const char *const typedDirectCallNeedles[] = {
            "#include \"backend_aot_c_typed_direct_bool_calls.h\"",
            "backend_aot_can_write_c_static_direct_bool_no_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_one_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_two_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_three_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_bool_two_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_bool_two_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_bool_two_arg_call(",
            "TZrUInt32 typedFirstArgumentSlot",
            "TZrUInt32 typedSecondArgumentSlot",
            "TZrUInt32 typedThirdArgumentSlot",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_bool_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_bool_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_bool_two_arg_function_call(file,",
            "backend_aot_write_c_static_direct_bool_three_arg_function_call(file,",
            "backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(file,",
            "backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(file,",
            "backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(file,",
    };
    static const char *const typedDirectBoolCallHeaderNeedles[] = {
            "#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_BOOL_CALLS_H",
            "backend_aot_can_write_c_static_direct_bool_no_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_one_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_two_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_three_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_bool_two_arg_call(",
    };
    static const char *const typedDirectBoolCallNeedles[] = {
            "#include \"backend_aot_c_typed_direct_bool_calls.h\"",
            "#include \"backend_aot_c_emitter.h\"",
            "#include \"backend_aot_c_scalar_locals.h\"",
            "backend_aot_typed_direct_bool_call_find_function_entry_by_flat_index(",
            "backend_aot_can_write_c_static_direct_bool_no_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_one_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_two_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_three_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_bool_two_arg_call(",
            "const TZrUInt32 argumentSlot = functionSlot + 1u;",
            "const TZrUInt32 firstArgumentSlot = functionSlot + 1u;",
            "const TZrUInt32 secondArgumentSlot = functionSlot + 2u;",
            "const TZrUInt32 thirdArgumentSlot = functionSlot + 3u;",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, thirdArgumentSlot)",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, thirdArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_bool_three_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(calleeEntry->function)",
    };
    static const char *const typedDirectU64CallNeedles[] = {
            "backend_aot_can_write_c_static_direct_u64_bool_two_arg_call(",
            "const TZrUInt32 firstArgumentSlot = functionSlot + 1u;",
            "const TZrUInt32 secondArgumentSlot = functionSlot + 2u;",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(calleeEntry->function)",
    };
    static const char *const typedDirectF64CallNeedles[] = {
            "backend_aot_can_write_c_static_direct_f64_bool_two_arg_call(",
            "const TZrUInt32 firstArgumentSlot = functionSlot + 1u;",
            "const TZrUInt32 secondArgumentSlot = functionSlot + 2u;",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(calleeEntry->function)",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    static const char *const callableProvenanceNeedles[] = {
            "backend_aot_resolve_callable_closure_function_index(",
            "backend_aot_find_owner_child_function_by_name(",
            "backend_aot_find_owner_child_function_by_stack_slot(",
            "function->closureValueList == ZR_NULL",
            "closureIndex >= function->closureValueLength",
            "function->ownerFunction",
            "closure->inStack",
            "ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE",
            "backend_aot_find_function_table_index(table, childFunction)",
            "backend_aot_resolve_callable_slot_function_index_before_instruction(table,",
            "ZR_INSTRUCTION_ENUM(GET_CLOSURE)",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_thunks.c");
    char *boolTwoArgThunkHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_two_arg_thunks.h");
    char *boolTwoArgThunkText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_two_arg_thunks.c");
    char *boolThreeArgThunkText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_three_arg_thunks.c");
    char *boolCallLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bool_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *typedDirectBoolCallHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_bool_calls.h");
    char *typedDirectBoolCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_bool_calls.c");
    char *typedDirectU64CallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_u64_calls.c");
    char *typedDirectF64CallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_f64_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *callableProvenanceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));

    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(boolTwoArgThunkHeaderText);
    TEST_ASSERT_NOT_NULL(boolTwoArgThunkText);
    TEST_ASSERT_NOT_NULL(boolThreeArgThunkText);
    TEST_ASSERT_NOT_NULL(boolCallLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(typedDirectBoolCallHeaderText);
    TEST_ASSERT_NOT_NULL(typedDirectBoolCallText);
    TEST_ASSERT_NOT_NULL(typedDirectU64CallText);
    TEST_ASSERT_NOT_NULL(typedDirectF64CallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(callableProvenanceText);

    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(boolTwoArgThunkHeaderText,
                             boolTwoArgThunkHeaderNeedles,
                             ARRAY_COUNT(boolTwoArgThunkHeaderNeedles));
    assert_text_contains_all(boolTwoArgThunkText, boolTwoArgThunkNeedles, ARRAY_COUNT(boolTwoArgThunkNeedles));
    assert_text_contains_all(boolThreeArgThunkText,
                             boolThreeArgThunkNeedles,
                             ARRAY_COUNT(boolThreeArgThunkNeedles));
    assert_text_contains_all(boolCallLoweringText,
                             boolCallLoweringNeedles,
                             ARRAY_COUNT(boolCallLoweringNeedles));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_bool_no_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_bool_one_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_bool_two_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_bool_three_arg_call("));
    TEST_ASSERT_NULL(strstr(typedDirectCallText,
                            "static TZrBool backend_aot_can_write_c_static_direct_i64_bool_two_arg_call("));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(typedDirectBoolCallHeaderText,
                             typedDirectBoolCallHeaderNeedles,
                             ARRAY_COUNT(typedDirectBoolCallHeaderNeedles));
    assert_text_contains_all(typedDirectBoolCallText,
                             typedDirectBoolCallNeedles,
                             ARRAY_COUNT(typedDirectBoolCallNeedles));
    assert_text_contains_all(typedDirectU64CallText,
                             typedDirectU64CallNeedles,
                             ARRAY_COUNT(typedDirectU64CallNeedles));
    assert_text_contains_all(typedDirectF64CallText,
                             typedDirectF64CallNeedles,
                             ARRAY_COUNT(typedDirectF64CallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(callableProvenanceText,
                             callableProvenanceNeedles,
                             ARRAY_COUNT(callableProvenanceNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(boolTwoArgThunkHeaderText);
    free(boolTwoArgThunkText);
    free(boolThreeArgThunkText);
    free(boolCallLoweringText);
    free(typedDirectCallText);
    free(typedDirectBoolCallHeaderText);
    free(typedDirectBoolCallText);
    free(typedDirectU64CallText);
    free(typedDirectF64CallText);
    free(functionBodyText);
    free(callableProvenanceText);
}
