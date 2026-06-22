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

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_logical_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_logical_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1 >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1);
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

static void assert_text_contains_none(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) != NULL) {
            printf("Unexpected source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("found forbidden source contract text");
        }
    }
}

static void test_aot_c_source_lowers_generic_truthiness_to_boundary_helpers(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_not(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "backend_aot_write_c_direct_jump_if(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_c_write_bool_local_sync",
            "backend_aot_write_c_direct_logical_not(",
            "backend_aot_write_c_direct_jump_if(",
            "zr_aot_generic_logical_not",
            "zr_aot_generic_jump_if",
            "TZrBool zr_aot_truthy = ZR_FALSE;",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, %u, %u)",
            "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, %u, &zr_aot_truthy)",
            "backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);",
            "zr_aot_generic_logical_sync_bool_local_boundary",
            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
            "if (!zr_aot_truthy) {",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(struct SZrState *state,",
            "ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "aot_runtime_generic_logical_values(",
            "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(SZrState *state,",
            "unsupported AOT generic primitive truthiness",
            "ZR_VALUE_FAST_SET(destinationValue, nativeBool, !truthy, ZR_VALUE_TYPE_BOOL);",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):",
            "backend_aot_write_c_direct_logical_not(file, functionIr, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF):",
            "backend_aot_write_c_direct_jump_if(file,",
            "entry->flatIndex",
            "destinationSlot",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "backend_aot_c_write_generic_truthiness_unsupported",
            "backend_aot_c_write_primitive_truthiness",
            "ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive truthiness\")",
            "ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_truthy, ZR_VALUE_TYPE_BOOL)",
            "const SZrTypeValue *zr_aot_bool_sync = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_bool_sync->value.nativeObject.nativeBool",
            "ZrLibrary_AotRuntime_LogicalNot",
            "ZrLibrary_AotRuntime_IsTruthy",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalNot(state, &frame",
    };
    static const char *const forbiddenControlNeedles[] = {
            "ZrLibrary_AotRuntime_IsTruthy(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *controlText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(controlText, forbiddenControlNeedles, ARRAY_COUNT(forbiddenControlNeedles));

    free(headerText);
    free(moduleText);
    free(functionBodyText);
    free(controlText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
}

static void test_aot_c_source_lowers_generic_primitive_equality_to_boundary_helpers(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_equal(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "backend_aot_write_c_direct_logical_not_equal(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_write_c_direct_logical_equal(",
            "backend_aot_write_c_direct_logical_not_equal(",
            "zr_aot_generic_logical_equal",
            "zr_aot_generic_logical_not_equal",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(state, &frame, %u, %u, %u)",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(state, &frame, %u, %u, %u)",
            "backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);",
            "zr_aot_generic_logical_sync_bool_local_boundary",
            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(struct SZrState *state,",
            "ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(SZrState *state,",
            "aot_runtime_generic_primitive_equal(",
            "unsupported AOT generic primitive equality",
            "ZR_VALUE_FAST_SET(destinationValue, nativeBool, equal, ZR_VALUE_TYPE_BOOL);",
            "ZR_VALUE_FAST_SET(destinationValue, nativeBool, !equal, ZR_VALUE_TYPE_BOOL);",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):",
            "backend_aot_write_c_direct_logical_equal(file, functionIr, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):",
            "backend_aot_write_c_direct_logical_not_equal(file, functionIr, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "backend_aot_c_write_generic_equality_unsupported",
            "backend_aot_c_write_primitive_equality",
            "ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive equality\")",
            "ZR_VALUE_IS_TYPE_NULL(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)",
            "const SZrTypeValue *zr_aot_bool_sync = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_bool_sync->value.nativeObject.nativeBool",
            "ZrLibrary_AotRuntime_LogicalEqual",
            "ZrCore_Value_Equal",
    };
    static const char *const forbiddenValuesNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqual(state, &frame",
            "backend_aot_write_c_direct_logical_equal(",
            "backend_aot_write_c_direct_logical_not_equal(",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqual(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqual(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *valuesText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(valuesText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(valuesText, forbiddenValuesNeedles, ARRAY_COUNT(forbiddenValuesNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(moduleText);
    free(valuesText);
    free(functionBodyText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
}

static void test_aot_c_source_lowers_bool_logical_and_or_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_and(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "backend_aot_write_c_direct_logical_or(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_c_write_bool_binary_logical",
            "backend_aot_c_write_bool_binary_scalar_local",
            "backend_aot_write_c_direct_logical_and(",
            "backend_aot_write_c_direct_logical_or(",
            "zr_aot_bool_logical_and",
            "zr_aot_bool_logical_or",
            "zr_aot_bool_binary_scalar_local",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, rightSlot, execInstructionIndex)",
            "zr_aot_b%u = (TZrBool)((zr_aot_b%u %s zr_aot_b%u) != 0u);",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)",
            "zr_aot_left_bool && zr_aot_right_bool",
            "zr_aot_left_bool || zr_aot_right_bool",
            "unsupported AOT bool logical binary",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL)",
            "backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, \"zr_aot_result\")",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_AND):",
            "backend_aot_write_c_direct_logical_and(file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_OR):",
            "backend_aot_write_c_direct_logical_or(file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
    };
    static const char *const scalarLocalNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_AND):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_OR):",
            "declaredSlotKinds[destinationSlot] & ZR_AOT_SCALAR_LOCAL_KIND_BOOL",
            "backend_aot_c_scalar_locals_record_slot(slotKinds,",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalAnd",
            "ZrLibrary_AotRuntime_LogicalOr",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalAnd(state, &frame",
            "ZrLibrary_AotRuntime_LogicalOr(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalNeedles, ARRAY_COUNT(scalarLocalNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(moduleText);
    free(functionBodyText);
    free(scalarLocalsText);
}

static void test_aot_c_source_lowers_string_equality_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_equal_string(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "backend_aot_write_c_direct_logical_not_equal_string(FILE *file",
    };
    static const char *const emitterNeedles[] = {
            "#include <string.h>\\n",
            "#include \\\"zr_vm_core/string.h\\\"\\n",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_c_write_string_logical_operand(",
            "ZrCore_Stack_GetValue(frame.slotBase + %u)",
            "backend_aot_c_write_string_equality",
            "backend_aot_c_write_string_bool_scalar_local",
            "backend_aot_write_c_direct_logical_equal_string(",
            "backend_aot_write_c_direct_logical_not_equal_string(",
            "zr_aot_string_logical_equal",
            "zr_aot_string_logical_not_equal",
            "zr_aot_string_logical_bool_scalar_local",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)",
            "zr_aot_b%u = (TZrBool)(%s != 0u);",
            "ZR_VALUE_IS_TYPE_STRING(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_STRING(zr_aot_right->type)",
            "ZR_CAST_STRING(state, zr_aot_left->value.object)",
            "ZrCore_String_GetByteLength(zr_aot_left_string)",
            "ZrCore_String_GetNativeString(zr_aot_left_string)",
            "memcmp(zr_aot_left_bytes, zr_aot_right_bytes, zr_aot_left_length) == 0",
            "unsupported AOT string equality",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_equal, ZR_VALUE_TYPE_BOOL)",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_equal, ZR_VALUE_TYPE_BOOL)",
            "backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, \"zr_aot_equal\")",
            "backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, \"!zr_aot_equal\")",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):",
            "backend_aot_write_c_direct_logical_equal_string(file,",
            "functionIr,",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):",
            "backend_aot_write_c_direct_logical_not_equal_string(file,",
            "instructionIndex);",
    };
    static const char *const scalarLocalNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):",
            "declaredSlotKinds[destinationSlot] & ZR_AOT_SCALAR_LOCAL_KIND_BOOL",
            "backend_aot_c_scalar_locals_record_slot(slotKinds,",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqualString",
            "ZrLibrary_AotRuntime_LogicalNotEqualString",
            "ZrCore_String_Equal",
            "((const TZrByte *)frame.slotBase + %u)",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqualString(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqualString(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalNeedles, ARRAY_COUNT(scalarLocalNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(moduleText);
    free(functionBodyText);
    free(scalarLocalsText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_generic_truthiness_to_boundary_helpers);
    RUN_TEST(test_aot_c_source_lowers_generic_primitive_equality_to_boundary_helpers);
    RUN_TEST(test_aot_c_source_lowers_bool_logical_and_or_to_direct_c);
    RUN_TEST(test_aot_c_source_lowers_string_equality_to_direct_c);
    return UNITY_END();
}
