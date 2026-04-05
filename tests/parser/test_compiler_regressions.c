#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_network/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser.h"

typedef struct SZrRegressionTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrRegressionTestTimer;

typedef struct ZrProjectRunRequest {
    SZrTypeValue *result;
    EZrThreadStatus status;
} ZrProjectRunRequest;

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

static void run_project_body(SZrState *state, TZrPtr arguments) {
    ZrProjectRunRequest *request = (ZrProjectRunRequest *)arguments;

    if (state == ZR_NULL || request == ZR_NULL || request->result == ZR_NULL) {
        return;
    }

    request->status = ZrLibrary_Project_Run(state, request->result);
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

static void test_native_network_optional_argument_import_compiles_without_unknown_parameter_blowup(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Native Network Optional Argument Import Compiles Without Unknown Parameter Blowup";
    const char *source =
            "var network = %import(\"zr.network\");\n"
            "network.tcp.connect(\"127.0.0.1\", 1);\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Imported native optional parameters",
                 "Testing that optional-argument native members like zr.network.tcp.connect do not treat UNKNOWN parameter counts as allocation sizes during compile");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(state->global));

    sourceName = ZrCore_String_CreateFromNative(state, "native_network_optional_parameter_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

static void test_native_network_loopback_runtime_returns_expected_payload(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Native Network Loopback Runtime Returns Expected Payload";
    const char *source =
            "var network = %import(\"zr.network\");\n"
            "var tcp = network.tcp;\n"
            "var udp = network.udp;\n"
            "var listener = tcp.listen(\"127.0.0.1\", 0);\n"
            "var client = tcp.connect(\"127.0.0.1\", listener.port());\n"
            "var server = listener.accept(3000);\n"
            "var ping = \"ping\";\n"
            "var pong = \"pong\";\n"
            "var echo = \"echo\";\n"
            "var wrotePing = client.write(ping);\n"
            "var readPing = server.read(16, 3000);\n"
            "var wrotePong = server.write(pong);\n"
            "var readPong = client.read(16, 3000);\n"
            "var socket = udp.bind(\"127.0.0.1\", 0);\n"
            "var sentEcho = socket.send(\"127.0.0.1\", socket.port(), echo);\n"
            "var packet = socket.receive(16, 3000);\n"
            "server.close();\n"
            "client.close();\n"
            "listener.close();\n"
            "socket.close();\n"
            "if (wrotePing != 4 || wrotePong != 4 || sentEcho != 4) {\n"
            "    return \"NETWORK_LOOPBACK_FAIL write\";\n"
            "}\n"
            "if (readPing != ping || readPong != pong) {\n"
            "    return \"NETWORK_LOOPBACK_FAIL tcp\";\n"
            "}\n"
            "if (packet == null || packet.payload != echo || packet.length != 4) {\n"
            "    return \"NETWORK_LOOPBACK_FAIL udp\";\n"
            "}\n"
            "return \"NETWORK_LOOPBACK_PASS \" + readPing + \" \" + readPong + \" \" + packet.payload;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;
    SZrString *resultString;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Loopback TCP/UDP runtime",
                 "Testing that zr.network TCP and UDP loopback client/server flows return the expected payload on the current platform");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(state->global));

    sourceName = ZrCore_String_CreateFromNative(state, "native_network_loopback_runtime_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    resultString = ZR_CAST_STRING(state, result.value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("NETWORK_LOOPBACK_PASS ping pong echo", ZrCore_String_GetNativeString(resultString));

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

static void test_native_network_loopback_project_run_returns_expected_payload(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Native Network Loopback Project Run Returns Expected Payload";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;
    SZrString *resultString;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Project runtime loopback",
                 "Testing that ZrLibrary_Project_Run executes the network_loopback project without escaping through an unhandled runtime exception");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/network_loopback/network_loopback.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, request.result->type);

    resultString = ZR_CAST_STRING(state, request.result->value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("NETWORK_LOOPBACK_PASS ping pong echo", ZrCore_String_GetNativeString(resultString));

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
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
    RUN_TEST(test_native_network_optional_argument_import_compiles_without_unknown_parameter_blowup);
    RUN_TEST(test_native_network_loopback_runtime_returns_expected_payload);
    RUN_TEST(test_native_network_loopback_project_run_returns_expected_payload);
    return UNITY_END();
}
