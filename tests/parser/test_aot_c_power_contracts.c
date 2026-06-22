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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_power_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_power_contracts.c");
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
    memcpy(path + rootLength, relativeLength > 0 ? relativePath : "", relativeLength + 1);
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

static void test_aot_c_source_lowers_typed_power_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_pow_signed(FILE *file",
            "backend_aot_write_c_direct_pow_unsigned(FILE *file",
            "backend_aot_write_c_direct_pow_float(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "TZrUInt32 execInstructionIndex",
    };
    static const char *const emitterNeedles[] = {
            "#include <math.h>\\n",
    };
    static const char *const moduleNeedles[] = {
            "#include \"backend_aot_c_scalar_locals.h\"",
            "backend_aot_write_c_direct_pow_signed(",
            "backend_aot_write_c_direct_pow_unsigned(",
            "backend_aot_write_c_direct_pow_float(",
            "backend_aot_c_write_pow_signed_scalar_local(",
            "backend_aot_c_write_pow_unsigned_scalar_local(",
            "backend_aot_c_write_pow_float_scalar_local(",
            "zr_aot_arith_exec_signed_power",
            "zr_aot_arith_exec_unsigned_power",
            "zr_aot_arith_exec_float_power",
            "zr_aot_arith_exec_signed_power_scalar_local",
            "zr_aot_arith_exec_unsigned_power_scalar_local",
            "zr_aot_arith_exec_float_power_scalar_local",
            "backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(",
            "functionIr, destinationSlot, execInstructionIndex",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex)",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)",
            "power domain error",
            "zr_aot_power_result *= zr_aot_power_base",
            "zr_aot_power_exponent >>= 1",
            "zr_aot_s%u = (TZrInt64)zr_aot_power_result;",
            "zr_aot_u%u = zr_aot_power_result;",
            "zr_aot_f%u = pow(zr_aot_f%u, zr_aot_f%u);",
            "pow(zr_aot_left_scalar, zr_aot_right_scalar)",
            "ZR_VALUE_FAST_SET(zr_aot_destination",
            "ZR_VALUE_TYPE_INT64",
            "ZR_VALUE_TYPE_UINT64",
            "ZR_VALUE_TYPE_DOUBLE",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(POW_SIGNED):",
            "backend_aot_write_c_direct_pow_signed(",
            "case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):",
            "backend_aot_write_c_direct_pow_unsigned(",
            "case ZR_INSTRUCTION_ENUM(POW_FLOAT):",
            "backend_aot_write_c_direct_pow_float(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex",
    };
    static const char *const scalarLocalsNeedles[] = {
            "backend_aot_c_scalar_locals_record_immediate_constant_destinations(",
            "backend_aot_c_scalar_locals_kind_from_power_opcode(",
            "backend_aot_c_scalar_locals_record_power_destinations(",
            "case ZR_INSTRUCTION_OP_POW_SIGNED:",
            "case ZR_INSTRUCTION_OP_POW_UNSIGNED:",
            "case ZR_INSTRUCTION_OP_POW_FLOAT:",
            "return ZR_AOT_SCALAR_LOCAL_KIND_I64;",
            "return ZR_AOT_SCALAR_LOCAL_KIND_U64;",
            "return ZR_AOT_SCALAR_LOCAL_KIND_F64;",
            "kind = backend_aot_c_scalar_locals_kind_from_power_opcode(opcode);",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_PowSigned",
            "ZrLibrary_AotRuntime_PowUnsigned",
            "ZrLibrary_AotRuntime_PowFloat",
            "ZrCore_Math_IntPower",
            "ZrCore_Math_UIntPower",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_PowSigned(state, &frame",
            "ZrLibrary_AotRuntime_PowUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_PowFloat(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_power.c");
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
    assert_text_contains_all(scalarLocalsText, scalarLocalsNeedles, ARRAY_COUNT(scalarLocalsNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(moduleText);
    free(functionBodyText);
    free(scalarLocalsText);
}

static void test_aot_c_source_lowers_generic_power_meta_boundary_to_boundary_helper(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_pow(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
    };
    static const char *const moduleNeedles[] = {
            "#include \"backend_aot_c_scalar_locals.h\"",
            "backend_aot_write_c_direct_pow(",
            "const SZrAotExecIrFunction *functionIr",
            "zr_aot_generic_power_boundary",
            "ZrLibrary_AotRuntime_GenericPower(state, &frame, %u, %u, %u)",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "zr_aot_generic_power_sync_i64_local_boundary",
            "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
            "zr_aot_generic_power_sync_u64_local_boundary",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
            "zr_aot_generic_power_sync_f64_local_boundary",
            "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(POW):",
            "backend_aot_write_c_direct_pow(file, functionIr, destinationSlot, operandA1, operandB1);",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPower(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPower(SZrState *state,",
            "ZrCore_Value_GetMeta(state, leftValue, ZR_META_POW)",
            "ZrCore_Value_ResetAsNull(destinationValue)",
            "unsupported AOT generic power meta dispatch",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_Pow",
            "SZrMeta *zr_aot_meta",
            "ZrCore_Value_GetMeta(state, zr_aot_left, ZR_META_POW)",
            "ZrCore_Value_ResetAsNull(zr_aot_destination)",
            "ZrCore_Debug_RunError(state, \"unsupported AOT generic power meta dispatch\")",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "ZrCore_Math_IntPower",
            "ZrCore_Math_UIntPower",
            "pow(zr_aot",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_Pow(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_power.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(moduleText);
    free(functionBodyText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_typed_power_to_direct_c);
    RUN_TEST(test_aot_c_source_lowers_generic_power_meta_boundary_to_boundary_helper);
    return UNITY_END();
}
