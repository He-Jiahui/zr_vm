#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTailCallPipelineTimer;

static TZrUInt32 count_call_info_chain_nodes(const SZrState *state) {
    TZrUInt32 count = 0;
    const SZrCallInfo *callInfo;

    if (state == ZR_NULL) {
        return 0;
    }

    for (callInfo = &state->baseCallInfo; callInfo != ZR_NULL; callInfo = callInfo->next) {
        count++;
    }

    return count;
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_tree_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 childIndex;

    if (function_contains_opcode(function, opcode)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (function_tree_contains_opcode(&function->childFunctionList[childIndex], opcode)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool semir_contains_opcode_with_deopt(const SZrFunction *function,
                                                EZrSemIrOpcode opcode,
                                                TZrBool requireDeopt) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if ((EZrSemIrOpcode)instruction->opcode != opcode) {
            continue;
        }
        if (!requireDeopt || instruction->deoptId != 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

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

static SZrFunction *compile_dynamic_tail_call_fixture(SZrState *state) {
    const char *source =
            "func makeAdder(base: int) {\n"
            "    func inner(value: int): int {\n"
            "        return base + value;\n"
            "    }\n"
            "    return inner;\n"
            "}\n"
            "var callable = makeAdder(7);\n"
            "return callable(5);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "dynamic_tail_call_pipeline_test.zr", 32);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_direct_tail_reuse_fixture(SZrState *state) {
    const char *source =
            "func loop(n: int, acc: int): int {\n"
            "    if (n == 0) {\n"
            "        return acc;\n"
            "    }\n"
            "    return loop(n - 1, acc + 1);\n"
            "}\n"
            "return loop(8, 0);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "direct_tail_frame_reuse_test.zr", 31);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_meta_tail_call_fixture(SZrState *state) {
    const char *source =
            "class Adder {\n"
            "    pub var base: int;\n"
            "    pub @constructor(base: int) {\n"
            "        this.base = base;\n"
            "    }\n"
            "    pub @call(value: int): int {\n"
            "        return this.base + value;\n"
            "    }\n"
            "}\n"
            "var adder = new Adder(7);\n"
            "return adder(5);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "meta_tail_call_pipeline_test.zr", 29);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_meta_tail_reuse_fixture(SZrState *state) {
    const char *source =
            "class Loop {\n"
            "    pub @call(n: int, acc: int): int {\n"
            "        if (n == 0) {\n"
            "            return acc;\n"
            "        }\n"
            "        return this(n - 1, acc + 1);\n"
            "    }\n"
            "}\n"
            "var loop = new Loop();\n"
            "return loop(8, 0);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "meta_tail_frame_reuse_test.zr", 29);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_dynamic_tail_call_emits_semir_runtime_contracts(void) {
    SZrTailCallPipelineTimer timer = {0};
    const char *testSummary = "Dynamic Tail Call Emits SemIR Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic tail call semir pipeline",
                 "Testing that unresolved callable values in tail position quicken to cached DYN tail-call ExecBC while SemIR keeps the DYN_TAIL_CALL runtime contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *intermediatePath = "dynamic_tail_call_pipeline_test.zri";
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_dynamic_tail_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)));
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCacheLength);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL, function->callSiteCaches[0].kind);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].argumentCount);
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_TAIL_CALL, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYN_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_DYN_TAIL_CALL_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYNAMIC_RUNTIME"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(12, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_meta_tail_call_emits_semir_runtime_contracts(void) {
    SZrTailCallPipelineTimer timer = {0};
    const char *testSummary = "Meta Tail Call Emits SemIR Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("meta tail call semir pipeline",
                 "Testing that @call receivers in tail position quicken to cached META tail-call ExecBC while SemIR keeps the META_TAIL_CALL runtime contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *intermediatePath = "meta_tail_call_pipeline_test.zri";
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_meta_tail_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_TAIL_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)));
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCacheLength);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL, function->callSiteCaches[0].kind);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].argumentCount);
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_TAIL_CALL, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_TAIL_CALL_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYNAMIC_RUNTIME"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(12, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_tail_call_aot_backends_emit_runtime_contracts(void) {
    SZrTailCallPipelineTimer timer = {0};
    const char *testSummary = "Tail Call AOT Backends Emit Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic/meta tail call aot pipeline",
                 "Testing that SemIR-driven AOT artifacts retain DYN_TAIL_CALL and META_TAIL_CALL together with the shared function-precall runtime contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *dynamicFunction;
        SZrFunction *metaFunction;
        const char *dynamicCPath = "dynamic_tail_call_pipeline_test.c";
        const char *dynamicLlvmPath = "dynamic_tail_call_pipeline_test.ll";
        const char *metaCPath = "meta_tail_call_pipeline_test.c";
        const char *metaLlvmPath = "meta_tail_call_pipeline_test.ll";
        char *dynamicCText;
        char *dynamicLlvmText;
        char *metaCText;
        char *metaLlvmText;

        TEST_ASSERT_NOT_NULL(state);

        dynamicFunction = compile_dynamic_tail_call_fixture(state);
        metaFunction = compile_meta_tail_call_fixture(state);
        TEST_ASSERT_NOT_NULL(dynamicFunction);
        TEST_ASSERT_NOT_NULL(metaFunction);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, dynamicFunction, dynamicCPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, dynamicFunction, dynamicLlvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, metaFunction, metaCPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, metaFunction, metaLlvmPath));

        dynamicCText = read_text_file_owned(dynamicCPath);
        dynamicLlvmText = read_text_file_owned(dynamicLlvmPath);
        metaCText = read_text_file_owned(metaCPath);
        metaLlvmText = read_text_file_owned(metaLlvmPath);
        TEST_ASSERT_NOT_NULL(dynamicCText);
        TEST_ASSERT_NOT_NULL(dynamicLlvmText);
        TEST_ASSERT_NOT_NULL(metaCText);
        TEST_ASSERT_NOT_NULL(metaLlvmText);

        TEST_ASSERT_NOT_NULL(strstr(dynamicCText, "DYN_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(dynamicCText, "ZrCore_Function_PreCall"));
        TEST_ASSERT_NOT_NULL(strstr(dynamicLlvmText, "DYN_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(dynamicLlvmText, "declare ptr @ZrCore_Function_PreCall"));

        TEST_ASSERT_NOT_NULL(strstr(metaCText, "META_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(metaCText, "ZrCore_Function_PreCall"));
        TEST_ASSERT_NOT_NULL(strstr(metaLlvmText, "META_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(metaLlvmText, "declare ptr @ZrCore_Function_PreCall"));

        free(dynamicCText);
        free(dynamicLlvmText);
        free(metaCText);
        free(metaLlvmText);
        remove(dynamicCPath);
        remove(dynamicLlvmPath);
        remove(metaCPath);
        remove(metaLlvmPath);
        ZrCore_Function_Free(state, dynamicFunction);
        ZrCore_Function_Free(state, metaFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_direct_tail_call_reuses_call_info_frame(void) {
    SZrTailCallPipelineTimer timer = {0};
    const char *testSummary = "Direct Tail Call Reuses Call Info Frame";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("direct tail call runtime reuse",
                 "Testing that deep FUNCTION_TAIL_CALL recursion does not keep extending the VM call-info chain");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        TZrInt64 result = 0;
        TZrUInt32 callInfoNodes;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_direct_tail_reuse_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(8, result);

        callInfoNodes = count_call_info_chain_nodes(state);
        TEST_ASSERT_EQUAL_UINT32(2, callInfoNodes);
        TEST_ASSERT_EQUAL_UINT32(1, state->callInfoListLength);

        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_meta_tail_call_reuses_call_info_frame(void) {
    SZrTailCallPipelineTimer timer = {0};
    const char *testSummary = "Meta Tail Call Reuses Call Info Frame";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("meta tail call runtime reuse",
                 "Testing that deep META_TAIL_CALL recursion keeps the VM call-info chain bounded after constructor setup");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        TZrInt64 result = 0;
        TZrUInt32 callInfoNodes;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_meta_tail_reuse_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(8, result);

        callInfoNodes = count_call_info_chain_nodes(state);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(3, callInfoNodes);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(2, state->callInfoListLength);

        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dynamic_tail_call_emits_semir_runtime_contracts);
    RUN_TEST(test_meta_tail_call_emits_semir_runtime_contracts);
    RUN_TEST(test_tail_call_aot_backends_emit_runtime_contracts);
    RUN_TEST(test_direct_tail_call_reuses_call_info_frame);
    RUN_TEST(test_meta_tail_call_reuses_call_info_frame);
    return UNITY_END();
}
