#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

typedef struct SZrRegressionTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrRegressionTestTimer;

void setUp(void) {}

void tearDown(void) {}

static char *read_text_file_owned(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static const TZrChar *function_name_or_anonymous(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "<anonymous>";
    }

    return ZrCore_String_GetNativeString(function->functionName);
}

static TZrBool function_tree_contains_exact_child_pointer(const SZrFunction *function, const SZrFunction *target) {
    if (function == ZR_NULL || target == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        const SZrFunction *child = &function->childFunctionList[index];
        if (child == target || function_tree_contains_exact_child_pointer(child, target)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void assert_function_constant_operands_in_range_recursive(SZrState *state,
                                                                 const SZrFunction *function,
                                                                 TZrUInt32 depth) {
    char message[256];

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Function constant recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) || opcode == ZR_INSTRUCTION_ENUM(SET_CONSTANT)) {
            TZrInt32 constantIndex = (TZrInt32)instruction->instruction.operand.operand2[0];

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted negative constant index %d at instruction %u",
                     function_name_or_anonymous(function),
                     (int)constantIndex,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE(constantIndex >= 0, message);

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted constant index %u but pool length is %u at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)constantIndex,
                     (unsigned int)function->constantValueLength,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE((TZrUInt32)constantIndex < function->constantValueLength, message);
        }
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];

        if (constant->type == ZR_VALUE_TYPE_FUNCTION && constant->value.object != ZR_NULL) {
            const SZrFunction *child = ZR_CAST_FUNCTION(state, constant->value.object);
            assert_function_constant_operands_in_range_recursive(state, child, depth + 1);
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            const SZrFunction *child = &function->childFunctionList[index];
            assert_function_constant_operands_in_range_recursive(state, child, depth + 1);
        }
    }
}

static void assert_create_closure_targets_are_reachable_children_recursive(SZrState *state,
                                                                           const SZrFunction *function,
                                                                           TZrUInt32 depth) {
    char message[256];

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "CREATE_CLOSURE child reachability recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(CREATE_CLOSURE)) {
            TZrUInt32 constantIndex = instruction->instruction.operand.operand1[0];
            const SZrTypeValue *constant;
            const SZrFunction *targetFunction = ZR_NULL;

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted CREATE_CLOSURE with constant index %u but pool length is %u at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)constantIndex,
                     (unsigned int)function->constantValueLength,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE(constantIndex < function->constantValueLength, message);

            constant = &function->constantValueList[constantIndex];
            if ((constant->type == ZR_VALUE_TYPE_FUNCTION || constant->type == ZR_VALUE_TYPE_CLOSURE) &&
                constant->value.object != ZR_NULL &&
                constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                targetFunction = ZR_CAST_FUNCTION(state, constant->value.object);
            }

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted CREATE_CLOSURE with non-function constant at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)index);
            TEST_ASSERT_NOT_NULL_MESSAGE(targetFunction, message);

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted CREATE_CLOSURE for function constant %u that is not reachable from childFunctions at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)constantIndex,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE(function_tree_contains_exact_child_pointer(function, targetFunction), message);
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            assert_create_closure_targets_are_reachable_children_recursive(state,
                                                                           &function->childFunctionList[index],
                                                                           depth + 1);
        }
    }
}

static void test_class_member_nested_functions_keep_constant_indices_in_range(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Class Member Nested Functions Keep Constant Indices In Range";
    SZrState *state;
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Nested function constant cache reset",
                 "Testing that class member methods compile with local constant indices after nested function state resets");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/fixtures/projects/classes/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);
    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "projects_classes_main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    assert_function_constant_operands_in_range_recursive(state, function, 0);

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

static void test_lambda_create_closure_targets_are_reachable_from_child_function_graph(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Lambda Create Closure Targets Are Reachable From Child Function Graph";
    const char *source =
            "var build = () => {\n"
            "    var emit = () => { return 1; };\n"
            "    return emit();\n"
            "};\n"
            "return build();";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Lambda child function graph completeness",
                 "Testing that CREATE_CLOSURE function constants are reachable from childFunctions instead of relying on constant-only recovery");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "lambda_child_function_graph_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    assert_create_closure_targets_are_reachable_children_recursive(state, function, 0);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

static void test_classes_full_module_compiles_without_static_and_receiver_signature_regressions(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Classes Full Module Compiles Without Static And Receiver Signature Regressions";
    SZrState *state;
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Static class access and instance parameter metadata",
                 "Testing that classes_full hero module compiles with static member access and instance method signatures");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/fixtures/projects/classes_full/src/hero.zr",
             ZR_VM_TESTS_SOURCE_DIR);
    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "projects_classes_full_hero.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("ZR-VM Compiler Regression Unit Tests\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();
    RUN_TEST(test_class_member_nested_functions_keep_constant_indices_in_range);
    RUN_TEST(test_lambda_create_closure_targets_are_reachable_from_child_function_graph);
    RUN_TEST(test_classes_full_module_compiles_without_static_and_receiver_signature_regressions);
    return UNITY_END();
}
