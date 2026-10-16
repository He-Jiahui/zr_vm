#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "exec_ir/exec_ir_mutation_internal.h"

typedef struct SModuleConstantFixture {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
} SModuleConstantFixture;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void build_constant_module(SModuleConstantFixture *fixture,
                                  TZrBool hasModulePool) {
    const SZrExecIrConstant constant = {
        (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64, 0u, UINT64_C(42)};
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId constantValue;
    SZrExecIrRange resultRange = {0};
    SZrExecIrRange returnOperand = {0};
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId instructionId;

    memset(fixture, 0, sizeof(*fixture));
    ZrCore_ExecIr_ModuleInit(&fixture->module);
    fixture->module.id = 1u;
    fixture->module.moduleHash = UINT64_C(0x4d4f4455);

    if (hasModulePool) {
        SZrExecIrRange constantRange = {0};
        expect_true(ZrCore_ExecIr_ModuleAppendConstant(
                            &fixture->module, &constant, 1u, &constantRange) &&
                        constantRange.start == 0u && constantRange.count == 1u,
                    "module constant fixture pool setup failed");
    }

    expect_true(ZrCore_ExecIr_ModuleAddFunction(
                        &fixture->module,
                        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 91u),
                        UINT64_C(0x535341), &functionId),
                "module constant fixture function setup failed");
    fixture->function = ZrCore_ExecIr_ModuleFunctionAt(&fixture->module,
                                                       functionId);
    expect_true(fixture->function != ZR_NULL,
                "module constant fixture function lookup failed");
    expect_true(ZrCore_ExecIr_FunctionAddBlock(
                        fixture->function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
                        ZR_EXEC_IR_BLOCK_ID_ENTRY,
                "module constant fixture entry setup failed");

    constantValue = ZrCore_ExecIr_FunctionAddValue(
            fixture->function, (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    expect_true(constantValue != ZR_EXEC_IR_VALUE_ID_INVALID &&
                    ZrCore_ExecIr_FunctionAppendResults(
                            fixture->function, &constantValue, 1u, &resultRange),
                "module constant fixture result setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = resultRange;
    instruction.layoutId = 0u;
    instruction.sourceId = 701u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        fixture->function, &instruction, &instructionId) &&
                    instructionId == 1u,
                "module constant fixture CONSTANT append failed");

    expect_true(ZrCore_ExecIr_FunctionAppendOperands(
                        fixture->function, &constantValue, 1u, &returnOperand),
                "module constant fixture return operand setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperand;
    instruction.sourceId = 702u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        fixture->function, &instruction, &instructionId) &&
                    instructionId == 2u,
                "module constant fixture RETURN append failed");
    fixture->function->blocks[0].instructions.start = 0u;
    fixture->function->blocks[0].instructions.count = 2u;
    fixture->function->blocks[0].terminatorInstructionId = 2u;
}

static void free_constant_module(SModuleConstantFixture *fixture) {
    ZrCore_ExecIr_FreeModule(&fixture->module);
    memset(fixture, 0, sizeof(*fixture));
}

#include "test_ssa_module_constant_pool_verifier_cases.inc"

int main(void) {
    test_module_constant_pool_accepts_valid_reference();
    test_module_constant_pool_rejects_count_boundary_index();
    test_module_constant_pool_rejects_uint32_max_index();
    test_module_constant_pool_rejects_constant_type_mismatch();
    test_module_constant_pool_rejects_instruction_type_mismatch();
    test_function_only_verifier_does_not_require_an_owned_pool();
    test_module_without_owned_pool_accepts_external_oracle_pool();
    puts("ssa module constant pool verifier PASS");
    return EXIT_SUCCESS;
}

/**
 * @struct SModuleConstantFixture
 *
 * ## Summary
 * 保存专用模块常量校验测试的唯一模块所有者，以及指向该模块单个函数记录的借用指针。它供本翻译单元和文本包含的场景使用，不是生产 ABI 或通用夹具。
 *
 * ## Fields
 * - module：拥有由 Core 构造接口分配的函数数组、常量池及函数内部池；由 free_constant_module 交给 FreeModule 统一释放。
 * - function：从 ModuleFunctionAt 获得、指向 module.functions 元素的非拥有借用；不得单独 free。
 *
 * ## Constraints and lifecycle
 * 调用 build_constant_module 前，记录必须尚未持有需释放的池，或已先完成释放；该构造助手会直接清零整个记录。正常构造产生单个函数和 entry block，包含 source701 的 CONSTANT 与 source702 的 RETURN；hasModulePool 决定是否另有 INT64/bits42 的模块池项。结构本身不验证这些状态。
 * function 只在所属函数数组未被迁移且模块仍存活时有效；追加模块函数可能重分配该数组，FreeModule 则使其借用失效。不得按位复制后让两份记录各自释放同一组池。free_constant_module 完成后记录本身仍存在且清零，旧函数及池借用不再有效。
 * 所有当前用例是串行局部所有者；没有共享状态、自动析构或失败断言清理钩子。意外 expect_true 失败会 exit，未执行的局部 Free 不会自动补做。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:9：类型定义；tests/parser/test_ssa_module_constant_pool_verifier.c:21、tests/parser/test_ssa_module_constant_pool_verifier.c:92：构造与释放接口的类型使用。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:2、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:15、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:45、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:68、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:98、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:114：全部局部夹具声明。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:50、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:561：函数借用来源；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:504：模块函数数组可增长。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:93、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237：模块及其内部资源释放路径。
 */

/**
 * @fn void expect_true(int condition, const char *message)
 *
 * ## Summary
 * 为本专用可执行程序提供失败即退出的检查入口。调用前已按普通 C 规则求值 condition，包括其中的构造、验证或 Oracle 调用；此处不是可被 NDEBUG 删除的 assert 宏。
 *
 * ## Parameters
 * @param[in] condition 已求值的检查结果；非零表示该检查成立，0 表示失败。
 * @param[in] message 失败时交给 fprintf 的非空、可读且以 NUL 结尾的消息字符串；当前调用均使用静态字符串，或由索引助手转交此类字符串。
 *
 * ## Returns
 * condition 非零时正常返回，无 C 返回值；为 0 时向 stderr 写失败消息后调用 exit(EXIT_FAILURE)，不返回调用方，也没有可供恢复的错误码。
 *
 * ## Constraints and lifecycle
 * 不取得 message 的所有权、不保存其地址，也不执行模块或 Oracle 资源清理。若调用点已持有局部资源，失败会跳过其尾部 Free；进程结束不能写作该助手完成了逐资源回滚。
 * 本体不保证 stderr 写入成功，也不验证 message 指针。main 直接串行运行场景，没有 Unity case 隔离或失败后继续运行的机制。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:16、tests/parser/test_ssa_module_constant_pool_verifier.c:17：实现、消息输出与进程退出。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:39、tests/parser/test_ssa_module_constant_pool_verifier.c:45、tests/parser/test_ssa_module_constant_pool_verifier.c:52、tests/parser/test_ssa_module_constant_pool_verifier.c:54、tests/parser/test_ssa_module_constant_pool_verifier.c:62、tests/parser/test_ssa_module_constant_pool_verifier.c:71、tests/parser/test_ssa_module_constant_pool_verifier.c:76、tests/parser/test_ssa_module_constant_pool_verifier.c:83：构造助手全部检查调用。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:6、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:20、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:22、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:51、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:53、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:84、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:87、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:102、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:118：场景与索引助手全部检查调用。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:99：直接程序入口，未接入 Unity runner。
 */

/**
 * @fn void build_constant_module(SModuleConstantFixture *fixture, TZrBool hasModulePool)
 *
 * ## Summary
 * 构造供七个场景复用的固定模块：单个函数、一个 entry block、一个 INT64 值，以及产生该值的 CONSTANT 和消费它的 RETURN。可选择是否放入一项 INT64/bits42 的模块常量池。
 *
 * ## Parameters
 * @param[out] fixture 非空、可写且尚无待释放所有资源的夹具记录；可为未初始化局部记录，已有活跃夹具须先释放。
 * @param[in] hasModulePool 非零时追加一项模块常量并检查其范围为 start0/count1；为 0 时保留空模块池。
 *
 * ## Returns
 * 各构造检查成立时正常返回，无 C 返回值；分配、追加或固定身份检查失败时经 expect_true 调用 exit(EXIT_FAILURE)，不返回可恢复的失败状态。
 *
 * ## Constraints and lifecycle
 * 入口直接清零 fixture 并调用 ModuleInit，不会先释放旧内容。后续构造可能逐步分配常量、函数、block、value、result、operand 与 instruction 存储；失败退出没有本地回滚事务。
 * function 借用由 ModuleFunctionAt 获取；助手之后只扩展函数内部池，不再追加模块函数。调用者在模块存活且其函数数组未重分配期间使用此借用，正常路径最终用 free_constant_module 释放唯一模块所有者。
 * CONSTANT 的 layoutId 固定为 0、sourceId 为 701；RETURN 的 sourceId 为 702，块指令区间为 start0/count2、终结指令 ID2。指令先清零，因此显式 typeToken 保持 0。本助手不调用 VerifyModule，也不替调用者验证后来施加的字段突变；固定夹具成功不是 OOM 事务或通用 IR 构造覆盖。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:32、tests/parser/test_ssa_module_constant_pool_verifier.c:33、tests/parser/test_ssa_module_constant_pool_verifier.c:37、tests/parser/test_ssa_module_constant_pool_verifier.c:39：清零、初始化与可选池。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:45、tests/parser/test_ssa_module_constant_pool_verifier.c:50、tests/parser/test_ssa_module_constant_pool_verifier.c:54、tests/parser/test_ssa_module_constant_pool_verifier.c:59、tests/parser/test_ssa_module_constant_pool_verifier.c:63：函数及结果存储构造。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:67、tests/parser/test_ssa_module_constant_pool_verifier.c:69、tests/parser/test_ssa_module_constant_pool_verifier.c:70、tests/parser/test_ssa_module_constant_pool_verifier.c:80、tests/parser/test_ssa_module_constant_pool_verifier.c:82、tests/parser/test_ssa_module_constant_pool_verifier.c:87：固定指令和块形状。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:5、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:18、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:48、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:74、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:100、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:116：全部六处物理调用；两种索引场景共用同一索引助手。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:496、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:529、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:561：Core 依赖接口，仅作资源和借用证据。
 */

/**
 * @fn void free_constant_module(SModuleConstantFixture *fixture)
 *
 * ## Summary
 * 在场景的正常检查路径结束后，释放夹具拥有的整个 Core 模块，并清零夹具记录及其缓存的函数借用。
 *
 * ## Parameters
 * @param[in,out] fixture 非空且可写的夹具；其 module 必须为空或持有可由 Core FreeModule 合法释放的唯一所有资源。
 *
 * ## Returns
 * 无 C 返回值；依照输入所有权前提调用 FreeModule 后清零 fixture，不返回诊断或失败码。
 *
 * ## Constraints and lifecycle
 * 不验证任意损坏指针、重复所有权或池分配来源。对语义字段的测试突变不改变池所有者；当前调用均来自相应构造后的局部夹具。
 * 释放使原 module.functions 元素、函数内部池及模块池的借用失效；fixture 记录自身地址仍有效，清零后 function 为 NULL。此助手不是自动析构钩子，意外 expect_true 失败会先 exit，尚未执行的调用会被跳过。专用场景没有独立断言重复释放、释放后字段或资源泄漏计数。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:92、tests/parser/test_ssa_module_constant_pool_verifier.c:93、tests/parser/test_ssa_module_constant_pool_verifier.c:94：函数、模块释放与夹具清零。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:244、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:246、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:247、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:250：下层逐函数与模块池释放。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:10、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:31、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:64、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:94、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:110、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:122：全部六处物理调用。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:17：失败即退出使本地尾部释放不具必达性。
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 按固定顺序直接运行七个模块常量校验场景，全部返回后输出专用 PASS 标记。两种边界索引各有独立场景调用，虽然共享一个内部索引断言助手。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 七个场景均正常返回后返回 EXIT_SUCCESS；任一内部 expect_true 失败会先以 exit(EXIT_FAILURE) 结束程序，main 不到达此返回语句。
 *
 * ## Constraints and lifecycle
 * 这是普通可执行入口，使用 main 到场景的直接调用；没有 RUN_TEST、UnityDefaultTestRun、setUp/tearDown 或 case 失败后的继续派发。全程序失败状态由 expect_true 的 exit 传达，不是 void case 的返回码。
 * 每个场景负责其局部夹具的正常释放，main 不集中回收它们；意外失败会跳过当前场景剩余释放与后续场景。该入口的 PASS 字符串是所有七次调用返回后的输出，不提供逐场景计时、分支命中、内存或全平台验证证明。本提案没有执行此程序。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:97：七场景及索引助手真正定义所在 .inc 的文本包含入口。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:100、tests/parser/test_ssa_module_constant_pool_verifier.c:101、tests/parser/test_ssa_module_constant_pool_verifier.c:102、tests/parser/test_ssa_module_constant_pool_verifier.c:103、tests/parser/test_ssa_module_constant_pool_verifier.c:104、tests/parser/test_ssa_module_constant_pool_verifier.c:105、tests/parser/test_ssa_module_constant_pool_verifier.c:106：全部七次直接调用及顺序。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:107、tests/parser/test_ssa_module_constant_pool_verifier.c:108：PASS 输出与成功返回。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:17：失败进程退出。
 * - tests/cmake/ssa-module-constant-pool-verifier.cmake:2、tests/cmake/ssa-module-constant-pool-verifier.cmake:3、tests/cmake/ssa-module-constant-pool-verifier.cmake:10、tests/cmake/ssa-module-constant-pool-verifier.cmake:11：真实可执行目标、源文件与 CTest 启动注册。
 */
