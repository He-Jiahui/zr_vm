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

// 仅扫描当前函数的 SemIR 表，并按需要求 deoptId 非零；不搜索子函数或独立 deopt 备份。
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
            "fn makeValues(): object {\n"
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

/**
 * @unit AOTPIPE-001
 * @typedef SZrDynamicIterationTimer
 *
 * ## Summary
 * 保存动态迭代用例的两次 clock() 读数，用于打印性能诊断。
 *
 * ## Fields
 * - `clock_t startTime;`
 * - `clock_t endTime;`
 *
 * ## Value model
 * startTime/endTime 是用例局部 clock_t；不传入编译器或 VM。
 *
 * ## Invariants
 * 只在同一用例普通返回路径中计算时间差；时间值不参与正确性断言。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:15
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:131
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:185
 */

/**
 * @unit AOTPIPE-002
 * @fn TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode)
 *
 * ## Summary
 * 检查当前函数的 ExecBC 表是否含指定 opcode，供本文件断言使用。
 *
 * ## Parameters
 * @param[in] function 借用待检查的函数及其数组；为空时按本函数的返回规则处理。
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 *
 * ## Returns
 * @return 找到返回 ZR_TRUE；函数或指令表为空、或未找到时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 只读当前 instructionList，不递归子函数，也不验证操作数或执行行为。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:21
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:150
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:151
 */

/**
 * @unit AOTPIPE-003
 * @fn TZrBool semir_contains_opcode_with_deopt(const SZrFunction *function, EZrSemIrOpcode opcode, TZrBool requireDeopt)
 *
 * ## Summary
 * 在当前函数的 SemIR 表中查找 opcode，并可附加 deoptId 非零条件。
 *
 * ## Parameters
 * @param[in] function 借用待检查的函数及其数组；为空时按本函数的返回规则处理。
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 * @param[in] requireDeopt 为真时还要求匹配 SemIR 指令的 deoptId 非零。
 *
 * ## Returns
 * @return 匹配 opcode 且满足 requireDeopt 条件返回 ZR_TRUE；空表或无匹配返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 不读取独立 deopt 备份，也不递归子函数；requireDeopt 为真只检查同一条 SemIR 指令的 deoptId。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:38
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:156
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:157
 */

/**
 * @unit AOTPIPE-004
 * @fn char *read_text_file_owned(const TZrChar *path)
 *
 * ## Summary
 * 把生成工件按字节读入独立的 NUL 结尾缓冲，供文本断言使用。
 *
 * ## Parameters
 * @param[in] path 待只读打开的 NUL 结尾路径；为空返回 NULL。
 *
 * ## Returns
 * @return 成功返回 malloc 缓冲，调用方负责 free；打开、定位、分配或短读失败返回 NULL。
 *
 * ## Constraints and lifecycle
 * 失败分支关闭已打开的 FILE，并释放本函数已分配的缓冲；不删除源文件，不把工件文本当作可执行验证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:61
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:160
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:209
 */

/**
 * @unit AOTPIPE-005
 * @fn SZrFunction *compile_dynamic_foreach_fixture(SZrState *state)
 *
 * ## Summary
 * 编译 object 返回值参与 foreach 的固定源码，供 ExecBC、SemIR 和文本后端测试共享。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 *
 * ## Returns
 * @return state/sourceName 无效或编译普通失败返回 NULL；成功返回该 VM 中的编译函数。
 *
 * ## Constraints and lifecycle
 * makeValues 返回数组但签名为 object；测试用显式 int 转换求和。调用方负责函数及 state 收尾，不改变源码字符串。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:104
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:148
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:203
 */

/**
 * @unit AOTPIPE-006
 * @fn void test_dynamic_foreach_emits_semir_dynamic_iterator_contracts(void)
 *
 * ## Summary
 * 核对动态迭代的 ExecBC 融合、SemIR deopt 标记与 .zri 文本，再解释执行求和得到 6。
 *
 * ## Constraints and lifecycle
 * 唯一 .zri 相对 cwd；正常路径 free 文本、remove 工件、释放函数并销毁 VM。clock 输出仅诊断。BUG: AOTPIPE-CLEANUP-01：工件 fopen 失败后 Unity longjmp 绕过局部 VM 收尾，空 tearDown 不接管。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:130
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:246
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c:1391
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 * - tests/harness/runtime_support.c:403
 */

/**
 * @unit AOTPIPE-007
 * @fn void test_dynamic_foreach_aot_backends_emit_iterator_runtime_contracts(void)
 *
 * ## Summary
 * 对同一动态 foreach 编译输入生成默认 C/LLVM 文本，核对迭代运行时符号。
 *
 * ## Constraints and lifecycle
 * 使用默认 writer，不启用本文件以外的严格 lowering 选项；检查为 C 调用片段与 LLVM 声明文本，不编译或运行生成物。正常路径释放两缓冲并删除两个相对工件。TODO: AOTPIPE-AOT-EXEC-01：生成物的本机可执行语义待相关现有 smoke 入口补证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:184
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:247
 */

/**
 * @unit AOTPIPE-010
 * @fn int main(void)
 *
 * ## Summary
 * 通过 Unity 按固定顺序运行两个动态迭代测试。
 *
 * ## Returns
 * @return 返回 UNITY_END() 的测试失败计数；不能将进程启动或未到达的断言视为通过。
 *
 * ## Constraints and lifecycle
 * 无 argv selector；RUN_TEST 绑定本翻译单元静态函数，setUp/tearDown 为空。两个用例共享 cwd 工件名，需串行独占工作目录。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:244
 */

/**
 * @unit AOTPIPE-L01
 * @brief 内嵌 Zr fixture::makeValues；定义位于本文件字符串，未另建 C 实体。
 *
 * ## Summary
 * 返回固定数组但对调用方声明 object，迫使 foreach 走动态迭代合同。
 *
 * ## Returns
 * @return 返回包含 1、2、3 的数组对象，静态返回注记为 object。
 *
 * ## Constraints and lifecycle
 * 源码字符串由 compile_dynamic_foreach_fixture 编译；首个 Unity 用例在 ExecuteExpectInt64 路径实际调用，双后端用例仅生成文本。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:105
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:106
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:169
 * - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c:170
 */
