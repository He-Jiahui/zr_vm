#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "aot_source_contract_match.h"

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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_ownership_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_ownership_contracts.c");
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
        if (!zr_test_aot_source_contract_contains(text, needles[index])) {
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

static void assert_text_occurs_before(const char *text, const char *first, const char *second) {
    const char *firstPosition = strstr(text, first);
    const char *secondPosition = strstr(text, second);

    TEST_ASSERT_NOT_NULL(firstPosition);
    TEST_ASSERT_NOT_NULL(secondPosition);
    TEST_ASSERT_TRUE(firstPosition < secondPosition);
}

static void test_aot_c_source_exposes_canonical_and_legacy_artifact_boundary_helpers(void) {
    static const char *const headerNeedles[] = {
            "void backend_aot_write_c_direct_own_unique(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_borrow(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_return_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_share(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_degrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_into_gc_box(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_wake(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
            "void backend_aot_write_c_direct_own_drop(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/ownership.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_OwnUnique(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnBorrow(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnLoan(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnReturnLoan(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnShare(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnDegrade(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnDetach(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnIntoGcBox(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnReturnToGc(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnWake(struct SZrState *state,",
            "ZrLibrary_AotRuntime_OwnDrop(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_OwnUnique(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_UniqueValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnBorrow(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_BorrowValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnLoan(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_LoanValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnReturnLoan(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_ReturnLoanValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnShare(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_ShareValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnDegrade(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_DegradeValue);",
            "static TZrBool aot_runtime_into_gc_box_or_detach(",
            "return ZrCore_Ownership_IntoGcBoxValue(state, destination, source) ||",
            "ZrCore_Ownership_DetachValue(state, destination, source);",
            "TZrBool ZrLibrary_AotRuntime_OwnDetach(SZrState *state,",
            "aot_runtime_into_gc_box_or_detach);",
            "TZrBool ZrLibrary_AotRuntime_OwnIntoGcBox(SZrState *state,",
            "ZrCore_Ownership_IntoGcBoxValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnReturnToGc(SZrState *state,",
            "ZrCore_Ownership_DetachValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnWake(SZrState *state,",
            "return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_WakeValue);",
            "TZrBool ZrLibrary_AotRuntime_OwnDrop(SZrState *state,",
            "ZrCore_Ownership_ReleaseValue(state, sourceValue);",
            "ZrCore_Value_ResetAsNull(destinationValue);",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_ownership_helper_call(",
            "zr_aot_value_exec_ownership_helper",
            "ZR_AOT_C_GUARD(%s(state, &frame, %u, %u));",
            "\"ZrLibrary_AotRuntime_OwnUnique\"",
            "\"ZrLibrary_AotRuntime_OwnBorrow\"",
            "\"ZrLibrary_AotRuntime_OwnLoan\"",
            "\"ZrLibrary_AotRuntime_OwnReturnLoan\"",
            "\"ZrLibrary_AotRuntime_OwnShare\"",
            "\"ZrLibrary_AotRuntime_OwnDegrade\"",
            "\"ZrLibrary_AotRuntime_OwnDetach\"",
            "\"ZrLibrary_AotRuntime_OwnIntoGcBox\"",
            "\"ZrLibrary_AotRuntime_OwnReturnToGc\"",
            "\"ZrLibrary_AotRuntime_OwnWake\"",
            "\"ZrLibrary_AotRuntime_OwnDrop\"",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):",
            "backend_aot_write_c_direct_own_unique(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_BORROW):",
            "backend_aot_write_c_direct_own_borrow(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_LOAN):",
            "backend_aot_write_c_direct_own_loan(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN):",
            "backend_aot_write_c_direct_own_return_loan(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_SHARE):",
            "backend_aot_write_c_direct_own_share(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_DEGRADE):",
            "backend_aot_write_c_direct_own_degrade(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_DETACH):",
            "backend_aot_write_c_direct_own_detach(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED):",
            "case ZR_INSTRUCTION_ENUM(OWN_VIEW_MUT):",
            "case ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX):",
            "backend_aot_write_c_direct_own_into_gc_box(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_RETURN_TO_GC):",
            "backend_aot_write_c_direct_own_return_to_gc(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_WAKE):",
            "backend_aot_write_c_direct_own_wake(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(OWN_DROP):",
            "backend_aot_write_c_direct_own_drop(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "backend_aot_write_c_direct_ownership_core_call(",
            "zr_aot_value_exec_ownership_core",
            "ZrCore_Ownership_UniqueValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_BorrowValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_LoanValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_ReturnLoanValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_ShareValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_DegradeValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_DetachValue(state, zr_aot_destination, zr_aot_source)",
            "ZrCore_Ownership_WakeValue(state, zr_aot_destination, zr_aot_source)",
            "zr_aot_value_exec_ownership_release",
            "ZrCore_Ownership_ReleaseValue(state, zr_aot_source);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_Own",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_occurs_before(runtimeSourceText,
                              "ZrCore_Ownership_IntoGcBoxValue(state, destination, source)",
                              "ZrCore_Ownership_DetachValue(state, destination, source)");
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_exposes_canonical_and_legacy_artifact_boundary_helpers);
    return UNITY_END();
}
