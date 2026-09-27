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

// 用例局部计时状态；仅服务本测试的性能诊断，不进入编译器或运行时。
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrDynamicIterationTimer;

// 仅检查单个 ExecBC 函数的 opcode；动态迭代用例用它确认 quickening 未吞掉关键指令。
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

// 把普通 SemIR 与 deopt 备份路径一并纳入搜索，确保动态迭代契约在回退后仍可观察。
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

// 读取生成的 AOT 文本供契约断言使用；返回堆缓冲由测试释放，失败返回 NULL。
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

// 从动态容器 foreach 源码建立可供 SemIR、ExecBC 和后端共享的编译输入。
static SZrFunction *compile_dynamic_foreach_fixture(SZrState *state) {
    const char *source =
            "fn makeValues() {\n"
            "    return [1, 2, 3];\n"
            "}\n"
            "var values = makeValues();\n"
            "var sum = 0;\n"
            "for (var item in values) {\n"
            "    sum = sum + <int> item;\n"
            "}\n"
            "return sum;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "dynamic_foreach_pipeline_test.zr", 32);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

// 检验动态 foreach 在 SemIR 和 ExecBC 中保留运行时迭代协议，避免优化误选静态容器路径。
static void test_dynamic_foreach_emits_semir_dynamic_iterator_contracts(void) {
    SZrDynamicIterationTimer timer = {0};
    const char *testSummary = "Dynamic Foreach Emits SemIR Dynamic Iterator Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic foreach semir pipeline",
                 "Testing that foreach over a runtime-erased iterable keeps raw SemIR contracts but quickens ExecBC loop guards into a superinstruction");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *intermediatePath = "dynamic_foreach_pipeline_test.zri";
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_dynamic_foreach_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_ITER_INIT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function,
                                                 ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_INIT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_ITER_INIT, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYN_ITER_INIT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYN_ITER_MOVE_NEXT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYNAMIC_RUNTIME"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(6, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 用同一动态 foreach 输入核对 C/LLVM 工件仍调用迭代协议。TODO: 当前按生成文本匹配，需用可执行工件补证实际控制流。
static void test_dynamic_foreach_aot_backends_emit_iterator_runtime_contracts(void) {
    SZrDynamicIterationTimer timer = {0};
    const char *testSummary = "Dynamic Foreach AOT Backends Emit Iterator Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic foreach aot pipeline",
                 "Testing that SemIR-driven AOT artifacts advertise runtime iterator contracts for runtime-erased foreach lowering");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *cPath = "dynamic_foreach_pipeline_test.c";
        const char *llvmPath = "dynamic_foreach_pipeline_test.ll";
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_dynamic_foreach_fixture(state);
        TEST_ASSERT_NOT_NULL(function);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR AOT C Backend"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "DYN_ITER_INIT"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "DYN_ITER_MOVE_NEXT"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_IterInit"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_IterMoveNext"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZR AOT LLVM Backend"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "DYN_ITER_INIT"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "DYN_ITER_MOVE_NEXT"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare i1 @ZrLibrary_AotRuntime_IterInit"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare i1 @ZrLibrary_AotRuntime_IterMoveNext"));

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

// Unity 入口依次运行动态迭代的中间码与双后端契约用例。
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dynamic_foreach_emits_semir_dynamic_iterator_contracts);
    RUN_TEST(test_dynamic_foreach_aot_backends_emit_iterator_runtime_contracts);
    return UNITY_END();
}
