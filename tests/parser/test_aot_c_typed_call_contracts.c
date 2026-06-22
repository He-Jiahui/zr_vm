#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == NULL) {
        return NULL;
    }

    marker = strstr(sourceFile, "tests/parser/test_aot_c_typed_call_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_typed_call_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}
static void test_aot_c_source_lowers_static_no_arg_i64_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_i64_two_arg_function_call(FILE *file,",
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
            "backend_aot_c_try_get_i64_arg0_unary_return(",
            "backend_aot_c_try_get_i64_arg0_add_constant_return(",
            "backend_aot_c_try_get_i64_arg0_subtract_constant_return(",
            "backend_aot_c_try_get_i64_arg0_multiply_constant_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_add_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_subtract_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_multiply_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(",
            "backend_aot_c_try_get_i64_arg0_arg1_binary_return(",
            "ZR_INSTRUCTION_ENUM(NEG_SIGNED)",
            "ZR_INSTRUCTION_ENUM(BITWISE_NOT)",
            "ZR_INSTRUCTION_ENUM(BITWISE_AND)",
            "ZR_INSTRUCTION_ENUM(BITWISE_OR)",
            "ZR_INSTRUCTION_ENUM(BITWISE_XOR)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST)",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED)",
            "ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)",
            "backend_aot_c_type_ref_is_i64(",
            "!function->hasCallableReturnType",
            "!backend_aot_c_type_ref_is_i64(&function->callableReturnType)",
            "function->parameterMetadataCount < 1u",
            "function->parameterMetadataCount < 2u",
            "!backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type)",
            "!backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type)",
            "backend_aot_write_c_typed_i64_thunk_forward_decls(",
            "backend_aot_write_c_typed_i64_thunks(",
            "backend_aot_c_write_i64_no_arg_thunk_definition(",
            "backend_aot_c_write_i64_one_arg_thunk_definition(",
            "backend_aot_c_write_i64_two_arg_thunk_definition(",
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
            "return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
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
    };
    static const char *const typedDirectCallNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(",
            "backend_aot_can_write_c_static_direct_i64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_i64_two_arg_call(",
            "TZrUInt32 *outArgumentSlot",
            "TZrUInt32 *outFirstArgumentSlot",
            "TZrUInt32 *outSecondArgumentSlot",
            "const TZrUInt32 argumentSlot = functionSlot + 1u;",
            "const TZrUInt32 firstArgumentSlot = functionSlot + 1u;",
            "const TZrUInt32 secondArgumentSlot = functionSlot + 2u;",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_i64_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_i64_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_i64_two_arg_thunk(calleeEntry->function)",
            "TZrBool syncI64StackSlot",
            "TZrUInt32 typedFirstArgumentSlot",
            "TZrUInt32 typedSecondArgumentSlot",
            "backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_i64_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_i64_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_i64_two_arg_function_call(file,",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_bool_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_bool_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_bool_two_arg_function_call(FILE *file,",
    };
    static const char *const emitterSourceNeedles[] = {
            "backend_aot_c_type_ref_is_bool(",
            "backend_aot_c_try_get_bool_constant_return(",
            "backend_aot_c_try_get_bool_identity_return(",
            "backend_aot_c_try_get_bool_arg0_logical_not_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_equal_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(",
            "backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(",
            "ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_AND)",
            "ZR_INSTRUCTION_ENUM(LOGICAL_OR)",
            "ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE)",
            "ZR_INSTRUCTION_ENUM(JUMP)",
            "function->instructionsLength == 6u",
            "function->instructionsLength == 7u",
            "ZR_VALUE_IS_TYPE_BOOL(constantValue->type)",
            "constantValue->value.nativeObject.nativeBool",
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(",
            "backend_aot_write_c_typed_bool_thunk_forward_decls(",
            "backend_aot_write_c_typed_bool_thunks(",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state) {",
            "return %s;",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "return (TZrBool)!zr_aot_arg0;",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);",
            "return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);",
    };
    static const char *const callLoweringNeedles[] = {
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
    };
    static const char *const typedDirectCallNeedles[] = {
            "backend_aot_can_write_c_static_direct_bool_no_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_one_arg_call(",
            "backend_aot_can_write_c_static_direct_bool_two_arg_call(",
            "const TZrUInt32 argumentSlot = functionSlot + 1u;",
            "const TZrUInt32 firstArgumentSlot = functionSlot + 1u;",
            "const TZrUInt32 secondArgumentSlot = functionSlot + 2u;",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(calleeEntry->function)",
            "TZrUInt32 typedFirstArgumentSlot",
            "TZrUInt32 typedSecondArgumentSlot",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_bool_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_bool_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_bool_two_arg_function_call(file,",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_thunks.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));

    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_u64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_u64_two_arg_function_call(FILE *file,",
    };
    static const char *const emitterSourceNeedles[] = {
            "#include \"backend_aot_c_typed_u64_thunk_shapes.h\"",
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
            "backend_aot_write_c_typed_u64_thunk_forward_decls(",
            "backend_aot_write_c_typed_u64_thunks(",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state) {",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
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
            "return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
    };
    static const char *const u64ThunkShapeNeedles[] = {
            "backend_aot_c_type_ref_is_u64(",
            "backend_aot_c_try_get_u64_arg0_arg1_add_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_subtract_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_multiply_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_bitwise_and_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_bitwise_or_return(",
            "backend_aot_c_try_get_u64_arg0_arg1_bitwise_xor_return(",
            "ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)",
            "ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)",
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
            "TZrUInt32 argumentSlot",
            "TZrUInt32 firstArgumentSlot",
            "TZrUInt32 secondArgumentSlot",
            "zr_aot_static_u64_no_arg_direct_call",
            "zr_aot_static_u64_one_arg_direct_call",
            "zr_aot_static_u64_two_arg_direct_call",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(state);",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u);",
            "zr_aot_u%u = zr_aot_typed_u64_fn_%u(state, zr_aot_u%u, zr_aot_u%u);",
            "zr_aot_static_u64_no_arg_direct_call_sync_stack_slot",
            "zr_aot_static_u64_one_arg_direct_call_sync_stack_slot",
            "zr_aot_static_u64_two_arg_direct_call_sync_stack_slot",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "nativeUInt64",
            "ZR_VALUE_TYPE_UINT64",
    };
    static const char *const typedDirectCallNeedles[] = {
            "backend_aot_can_write_c_static_direct_u64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_u64_two_arg_call(",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, argumentSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_u64_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_u64_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_thunk(calleeEntry->function)",
            "TZrBool syncU64StackSlot",
            "TZrUInt32 typedFirstArgumentSlot",
            "TZrUInt32 typedSecondArgumentSlot",
            "backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_u64_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_u64_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_u64_two_arg_function_call(file,",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c");
    char *u64ThunkShapeText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunk_shapes.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));

    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(u64ThunkShapeText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(u64ThunkShapeText, u64ThunkShapeNeedles, ARRAY_COUNT(u64ThunkShapeNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(u64ThunkShapeText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_static_f64_calls_to_typed_thunks(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function)",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function)",
            "backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,",
    };
    static const char *const emitterSourceNeedles[] = {
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
            "ZR_INSTRUCTION_ENUM(ADD_FLOAT)",
            "ZR_INSTRUCTION_ENUM(SUB_FLOAT)",
            "ZR_INSTRUCTION_ENUM(MUL_FLOAT)",
            "ZR_INSTRUCTION_ENUM(DIV_FLOAT)",
            "ZR_INSTRUCTION_ENUM(MOD_FLOAT)",
            "ZR_INSTRUCTION_ENUM(NEG_FLOAT)",
            "ZR_VALUE_TYPE_FLOAT",
            "ZR_VALUE_TYPE_DOUBLE",
            "ZR_STATIC_C_TYPE_F64",
            "ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)",
            "returnValue == 0.0",
            "constantValue->value.nativeObject.nativeDouble",
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(",
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
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,",
            "backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,",
            "TZrUInt32 argumentSlot",
            "TZrUInt32 firstArgumentSlot",
            "TZrUInt32 secondArgumentSlot",
            "zr_aot_static_f64_no_arg_direct_call",
            "zr_aot_static_f64_one_arg_direct_call",
            "zr_aot_static_f64_two_arg_direct_call",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state);",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u);",
            "zr_aot_f%u = zr_aot_typed_f64_fn_%u(state, zr_aot_f%u, zr_aot_f%u);",
            "zr_aot_static_f64_no_arg_direct_call_sync_stack_slot",
            "zr_aot_static_f64_one_arg_direct_call_sync_stack_slot",
            "zr_aot_static_f64_two_arg_direct_call_sync_stack_slot",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "nativeDouble",
            "ZR_VALUE_TYPE_DOUBLE",
    };
    static const char *const typedDirectCallNeedles[] = {
            "backend_aot_can_write_c_static_direct_f64_no_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_one_arg_call(",
            "backend_aot_can_write_c_static_direct_f64_two_arg_call(",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, argumentSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, firstArgumentSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, secondArgumentSlot)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, argumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, firstArgumentSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, secondArgumentSlot, execInstructionIndex)",
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(calleeEntry->function)",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(calleeEntry->function)",
            "backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(",
            "backend_aot_write_c_static_direct_f64_no_arg_function_call(file,",
            "backend_aot_write_c_static_direct_f64_one_arg_function_call(file,",
            "backend_aot_write_c_static_direct_f64_two_arg_function_call(file,",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_try_write_c_static_direct_typed_function_call(file,",
            "backend_aot_try_write_c_static_direct_typed_no_arg_function_call(file,",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.c");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *typedDirectCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));

    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(typedDirectCallText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(typedDirectCallText, typedDirectCallNeedles, ARRAY_COUNT(typedDirectCallNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(emitterSourceText);
    free(callLoweringText);
    free(typedDirectCallText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_i64_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_f64_calls_to_typed_thunks);
    return UNITY_END();
}
