#ifndef ZR_TEST_SSA_STATE_MAP_FIXTURE_H
#define ZR_TEST_SSA_STATE_MAP_FIXTURE_H

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

static SZrExecIrFunction function;
static SZrExecIrDiagnostic diagnostic;

void setUp(void) {
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
}

void tearDown(void) {
    ZrCore_ExecIr_FreeFunction(&function);
}

static TZrExecIrValueId value(EZrExecIrOwnership ownership, TZrBool external) {
    TZrExecIrValueId id = external
            ? ZrCore_ExecIr_FunctionAddExternalValue(
                    &function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE)
            : ZrCore_ExecIr_FunctionAddValue(
                    &function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_NOT_EQUAL(0u, id);
    return id;
}

static void blocks(TZrUInt32 count) {
    TZrUInt32 index;
    for (index = 0u; index < count; ++index) {
        TEST_ASSERT_EQUAL_UINT32(index + 1u, ZrCore_ExecIr_FunctionAddBlock(
                &function, index == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u));
    }
}

static void edges(TZrExecIrBlockId block, TZrExecIrBlockId first,
                  TZrExecIrBlockId second) {
    TZrExecIrBlockId targets[2] = {first, second};
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, targets, second == 0u ? 1u : 2u,
            &function.blocks[block - 1u].successorRange));
}

static void predecessors(void) {
    TZrUInt32 target, source, edge;
    for (target = 0u; target < function.blockCount; ++target) {
        SZrExecIrRange range = {0};
        range.start = function.predecessorCount;
        for (source = 0u; source < function.blockCount; ++source) {
            const SZrExecIrBlock *block = &function.blocks[source];
            for (edge = block->successorRange.start;
                 edge < block->successorRange.start + block->successorRange.count;
                 ++edge) {
                if (function.successors[edge] == target + 1u) {
                    TZrExecIrBlockId id = source + 1u;
                    SZrExecIrRange appended;
                    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
                            &function, &id, 1u, &appended));
                    ++range.count;
                }
            }
        }
        function.blocks[target].predecessorRange = range;
    }
}

static TZrExecIrInstructionId emit(TZrExecIrBlockId blockId, EZrExecIrOpcode opcode,
                                   TZrUInt16 flags, TZrExecIrValueId operand,
                                   TZrExecIrValueId result) {
    SZrExecIrInstruction instruction = {0};
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(opcode);
    TZrExecIrInstructionId id;
    instruction.opcode = opcode;
    instruction.flags = flags;
    instruction.sourceId = 100u + function.instructionCount + 1u;
    if (blockId != 0u && info != ZR_NULL &&
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u) {
        instruction.successorRange = function.blocks[blockId - 1u].successorRange;
    }
    if (operand != 0u) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
                &function, &operand, 1u, &instruction.operandRange));
    }
    if (result != 0u) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
                &function, &result, 1u, &instruction.resultRange));
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    if (blockId != 0u) {
        SZrExecIrBlock *block = &function.blocks[blockId - 1u];
        if (block->instructionRange.count == 0u) {
            block->instructionRange.start = id - 1u;
        }
        ++block->instructionRange.count;
        if (info != ZR_NULL &&
            (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u) {
            block->terminatorInstructionId = id;
        }
    }
    return id;
}

static const SZrExecIrStateMapEntry *checkpoint(TZrExecIrInstructionId instruction,
                                                EZrExecIrStateMapPhase phase) {
    TZrUInt32 index;
    const SZrExecIrStateMapEntry *found = NULL;
    TEST_ASSERT_NOT_NULL(function.stateMap);
    for (index = 0u; index < function.stateMap->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &function.stateMap->entries[index];
        if (entry->instructionId == instruction && entry->phase == phase) {
            found = entry;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(found, "missing checkpoint");
    return found;
}

static void assert_only_root(TZrExecIrInstructionId instruction,
                             EZrExecIrStateMapPhase phase, TZrExecIrValueId root) {
    const SZrExecIrStateMapEntry *entry = checkpoint(instruction, phase);
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request = {0};
    TEST_ASSERT_EQUAL_UINT32(root == 0u ? 0u : 1u, entry->rootValues.count);
    if (root != 0u) {
        TEST_ASSERT_EQUAL_UINT32(root,
                function.stateMap->rootPool[entry->rootValues.start]);
    }
    ZrCore_ExecIr_MaterializedStateInit(&target);
    request.function = &function;
    request.map = function.stateMap;
    request.functionToken = function.functionToken;
    request.generation = function.contract.generation;
    request.signatureHash = function.signatureHash;
    request.sourceId = entry->sourceId;
    request.resumeId = entry->resumeId;
    request.phase = phase;
    request.target = &target;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(entry->rootValues.count, target.rootCount);
    if (root != 0u) {
        TEST_ASSERT_EQUAL_UINT32(root, target.roots[0]);
    }
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void build(void) {
    TZrBool verified;
    char failure[192];
    predecessors();
    verified = ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic);
    (void)snprintf(failure, sizeof(failure),
            "fixture verify code=%u block=%u instruction=%u expected=%u actual=%u",
            (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
            (unsigned)diagnostic.instructionId,
            (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
    TEST_ASSERT_TRUE_MESSAGE(verified, failure);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
}

#endif

/**
 * @file tests/parser/ssa_state_map_fixture.h
 * ## Summary
 * 为 state-map、owner-state、resume 与 conditional-cleanup Unity 测试提供
 * 每个包含它的测试翻译单元各自拥有的函数、诊断和构造/检查辅助函数。
 * setUp 初始化空 ExecIR 函数并写入稳定身份字段；各测试按场景使用部分 helper
 * 构造/校验 CFG 或 state map，build 提供“补前驱、验证、构造 state map”的组合路径。
 * tearDown 释放该翻译单元所拥有的函数及其图、state map 等内部存储。Unity 在每个
 * RUN_TEST 周围调用生命周期钩子。
 *
 * ## Usage and constraints
 * 本头当前分别由 test_ssa_conditional_cleanup.c、test_ssa_oracle_resume.c、
 * test_ssa_state_map_ownership.c 和 test_ssa_state_map_liveness.c 包含；这些是
 * 独立测试目标中的不同翻译单元，static 状态不跨翻译单元共享。构造辅助函数
 * 修改当前函数，只适用于 setUp 建立的空函数及当前测试阶段；predecessors
 * 不清空已有前驱，重复执行会再次按同一 successor 图追加源块 ID 序列，并把各块
 * range 改指向本次新追加片段；前次片段仍留在数组前缀中，但不再由块 range 引用。
 * build 也不替调用方重置函数。测试断言失败时当前 Unity 配置
 * 通过 longjmp 返回 Unity runner；局部栈对象不会自动析构，之后仍调用 tearDown，
 * 但 tearDown 只释放全局 function，不知道测试辅助函数中的其他局部对象。
 *
 * ## Ownership and lifecycle
 * function 拥有其 ExecIR 数组与 state map；diagnostic 是供当前测试及构造辅助函数
 * 复用的静态诊断值。setUp 不显式清零 diagnostic，因此测试只能在产生诊断的 API
 * 调用后读取它，不能把新一例开始当作 diagnostic 已重置。checkpoint 返回的 entry
 * 是 function.stateMap 内部的借用指针；state map 被重建/替换或 function 被释放后
 * 不得继续使用。材料化测试目标由 assert_only_root 在栈上拥有并在正常路径释放；
 * 若材料化成功后的 root 断言失败，Unity longjmp 会跳过该局部释放语句。当前已审
 * 用例没有触发这条失败分支，不据此登记第二个缺陷。
 *
 * ## References
 * 四个包含点为 tests/parser/test_ssa_conditional_cleanup.c:1、
 * tests/parser/test_ssa_oracle_resume.c:1、
 * tests/parser/test_ssa_state_map_ownership.c:1、
 * tests/parser/test_ssa_state_map_liveness.c:1。对应目标在
 * tests/cmake/ssa-cleanup-tests.cmake:8 创建 cleanup target，:17 配置 UNITY_INCLUDE_CONFIG_H；
 * 另外三个 target 在 tests/cmake/ssa-tests.cmake:267、:297、:465 配置该宏。
 * Unity 实际失败控制流见 tests/third_party/zr_unity/Unity/src/unity_internals.h:881、:891
 * 与 tests/third_party/zr_unity/Unity/src/unity.c:2296、:2301、:2303；项目配置入口
 * tests/unity_config.h:4 当前为空。
 *
 * @par BUG: includes::unity_before_ucrt
 * 当前源码先在第 4 行包含 Unity，后在第 7 行包含 stdlib。使用
 * Windows SDK 10.0.26100.0、Clang 19.1.5 和 C11 的修改前 R1 native 构建中，
 * Unity 的 unity_internals.h:82 自动包含 stdnoreturn.h；其 noreturn 宏随后污染
 * UCRT 对 __declspec(noreturn) 的声明，首个失败翻译单元为
 * tests/parser/test_ssa_state_map_ownership.c，记录 5 条编译错误，构建在测试启动
 * 前失败。具体复现记录为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/
 * parser-conditional-cleanup-preedit-native-r1-build.stdout.log，触发诊断为
 * E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/
 * state-map-fixture-unity-noreturn-baseline-diagnosis-r1.json。R2 在同一配置的
 * CMAKE_C_FLAGS 原值后追加 Unity 的 UNITY_NORETURN=_Noreturn 覆盖后 native
 * build 退出码为 0；这只验证该构建配置的规避方式，不修复默认源码顺序，也不关闭
 * Unity 默认 setjmp/longjmp。R2 cleanup 目标仍有一个既有 OOM 测试失败，不能据
 * R2 构建成功宣称 cleanup suite 全通过。源码顺序及行为保持不变。
 */

/**
 * @var function
 * ## Summary
 * 当前测试翻译单元独占的 ExecIR 函数夹具；测试 builder 在其中追加值、块、边、
 * 指令并由 BuildStateMaps 关联 state map。
 *
 * ## Invariants and lifecycle
 * 由 setUp 调用 FunctionInit 开始一个空生命周期，测试期间仅由当前翻译单元的
 * helper 和用例修改，tearDown 调用 FreeFunction 后其拥有的内部缓冲区不再有效。
 * checkpoint 返回的 entry 指针借用此对象的 stateMap，不能跨 stateMap 重建、替换
 * 或 FreeFunction 保存使用。static 保证不同测试翻译单元各有独立实例。
 *
 * ## References
 * 初始化见本文件 setUp；构造见 blocks、edges、emit、predecessors、build；释放见
 * 本文件 tearDown；state map 由 ZrParser_ExecIr_BuildStateMaps 构建。
 */

/**
 * @var diagnostic
 * ## Summary
 * 供当前测试翻译单元的 VerifyFunction、BuildStateMaps 和其他被测 API 写入的诊断
 * 输出，fixture 的 build 使用它生成可读的验证失败信息。
 *
 * ## Invariants and lifecycle
 * 本变量只在当前翻译单元内共享；setUp 不显式清零它。每次读取前应先执行会写入
 * 诊断的 API，并按该 API 的成功/失败契约解释内容；不能假设测试用例边界清空旧值。
 * 它不拥有函数或 state map 存储。
 *
 * ## References
 * 本文件 build 将其传给 ZrCore_ExecIr_VerifyFunction 与
 * ZrParser_ExecIr_BuildStateMaps；其他测试可将它传给所测试的诊断 API。
 */

/**
 * @fn void setUp(void)
 * ## Summary
 * Unity 在每个测试用例前初始化当前翻译单元的空 ExecIR 夹具，并赋予稳定的函数
 * 身份、签名及 generation 元数据，使各测试从一致的函数状态开始。
 *
 * ## Returns
 * 初始化结果写入静态 function。函数初始化不建立 CFG，也不构建 state
 * map；由当前用例追加测试图。它不显式重置 diagnostic。
 *
 * ## Constraints and lifecycle
 * 由 Unity runner 在受保护的 setUp 阶段调用；它要求上一用例已经由 tearDown 释放
 * function。各包含该头的测试翻译单元有独立静态实例。
 *
 * ## References
 * 对应释放钩子为本文件 tearDown；执行顺序由 UnityDefaultTestRun 在
 * tests/third_party/zr_unity/Unity/src/unity.c:2298、:2303 管理。
 */

/**
 * @fn void tearDown(void)
 * ## Summary
 * Unity 在每个用例正常完成或断言失败后释放当前测试函数及其所拥有的 ExecIR
 * 动态存储，避免夹具的函数数组和 state map 泄漏到下一用例。
 *
 * ## Returns
 * 无返回值；调用 ZrCore_ExecIr_FreeFunction 释放 function 管理的资源。
 *
 * ## Constraints and lifecycle
 * Unity 默认断言失败经 longjmp 离开测试函数后仍进入 runner 的 tearDown 阶段；
 * 这不是 C 栈展开，未挂在 function 下的测试局部资源不会由本钩子释放。释放后
 * function 的内部借用指针（包括 checkpoint 返回值）均失效。
 *
 * ## References
 * 对应初始化钩子为本文件 setUp；Unity 的失败及 tearDown 顺序见
 * tests/third_party/zr_unity/Unity/src/unity.c:2301、:2303、:2306。
 */

/**
 * @fn TZrExecIrValueId value(EZrExecIrOwnership ownership, TZrBool external)
 * ## Summary
 * 在当前 function 中追加一个 nullable ExecIR 值，按 external 选择普通值或外部
 * 值构造入口，供测试声明 CFG 输入和中间结果。
 *
 * ## Parameters
 * @param[in] ownership 为新值指定 ExecIR ownership 分类，必须是被测构造 API 接受的值。
 * @param[in] external 非零时创建外部值，零时创建函数内部值。
 *
 * ## Returns
 * @return 新值 ID；ID 为 0 时 Unity 断言失败并中止当前用例。值存储属于 function，
 * ID 仅在该 function 生命周期内可用。
 *
 * ## Constraints and lifecycle
 * 调用前 function 必须已由 setUp 初始化；失败断言可能在值追加 API 已改变夹具后
 * longjmp，随后 tearDown 释放 function。ownership 与 external 语义由 ExecIR API 定义。
 *
 * ## References
 * 本文件 setUp 建立目标对象；调用方见
 * tests/parser/test_ssa_conditional_cleanup.c:117、:118、:119、
 * tests/parser/test_ssa_state_map_liveness.c:4、:5、:6、
 * tests/parser/test_ssa_state_map_ownership.c:47、:48、:49、
 * tests/parser/test_ssa_oracle_resume.c:154、:155。
 */

/**
 * @fn void blocks(TZrUInt32 count)
 * ## Summary
 * 向当前 function 追加 count 个基本块，并把本次追加的第一个块标记为入口块，供
 * 测试构建带 CFG 的 ExecIR 函数。
 *
 * ## Parameters
 * @param[in] count 本次要追加的块数；0 时循环不执行、不标记入口且不改变 function，
 * 若要构成带入口的 CFG 则须大于 0。每次 ZrCore_ExecIr_FunctionAddBlock 返回的 ID 必须与预期连续 ID 相符。
 *
 * ## Returns
 * 无返回值；块写入 function。ID 不符时 Unity 断言中止当前用例。
 *
 * ## Constraints and lifecycle
 * 该 helper 不清除已有块；入口标志依据本次循环 index==0 传给 AddBlock。返回 ID
 * 按 index+1 检查，因此测试应在 setUp 的空 function 上调用，并在写入边或指令前
 * 完成所需块的追加。
 *
 * ## References
 * 调用示例见 tests/parser/test_ssa_conditional_cleanup.c:120、
 * tests/parser/test_ssa_state_map_liveness.c:7、
 * tests/parser/test_ssa_state_map_ownership.c:50、
 * tests/parser/test_ssa_oracle_resume.c:156。
 */

/**
 * @fn void edges(TZrExecIrBlockId block, TZrExecIrBlockId first, TZrExecIrBlockId second)
 * ## Summary
 * 为指定块追加一条或两条后继边，并将返回的 successor range 记录到该块，供
 * predecessors 和验证器读取测试 CFG。
 *
 * ## Parameters
 * @param[in] block 拥有这些后继边的非零块 ID，必须已存在于 function.blocks。
 * @param[in] first 第一条有效目标块 ID，必须是图中的合法块。
 * @param[in] second 为 0 表示只追加 first；非零时作为第二条后继块 ID 一并追加。
 *
 * ## Returns
 * 无返回值；边和范围写入 function。AppendSuccessors 失败时 Unity 断言终止当前用例。
 *
 * ## Constraints and lifecycle
 * 目标块应先于本调用创建。helper 不验证 block 范围，也不回滚已部分发生的夹具修改；
 * 断言失败时后续 tearDown 释放 function。
 *
 * ## References
 * 调用示例见 tests/parser/test_ssa_conditional_cleanup.c:122、:123、:124、
 * tests/parser/test_ssa_state_map_liveness.c:8、
 * tests/parser/test_ssa_state_map_ownership.c:51、:52、
 * tests/parser/test_ssa_oracle_resume.c:366。
 */

/**
 * @fn void predecessors(void)
 * ## Summary
 * 遍历当前 CFG 的 successor ranges，为每个目标块收集指向它的源块 ID，并将追加
 * 得到的 predecessor range 写回对应块，供验证器和 dataflow 构造使用。
 *
 * ## Returns
 * 无返回值；前驱数组及每块 range 写入 function。AppendPredecessors 失败时 Unity
 * 断言终止当前用例。
 *
 * ## Constraints and lifecycle
 * 要求 blockCount、blocks 与 successor ranges 已构造且边目标有效。helper 不清空
 * function.predecessors；重复调用会重新遍历同一 successor 图并再次追加相同的源块 ID
 * 序列，随后把每块的 range 指向本次新追加片段。旧片段仍留在数组前缀中，但不再由
 * 块 range 引用，因此 helper 非幂等；应在一次完整边构造之后调用一次。
 * 每个块的 range.start 取当前 predecessorCount，已有前驱不会被覆盖或回收。
 *
 * ## References
 * build 在验证前调用本 helper，见本文件 build；显式调用见
 * tests/parser/test_ssa_conditional_cleanup.c:100、:585、:726。
 */

/**
 * @fn TZrExecIrInstructionId emit(TZrExecIrBlockId blockId, EZrExecIrOpcode opcode, TZrUInt16 flags, TZrExecIrValueId operand, TZrExecIrValueId result)
 * ## Summary
 * 按 opcode schema 在当前 function 追加一条指令，并按需追加单个 operand/result；
 * 对非零 blockId 同步维护块的指令范围及 terminator ID，供测试构建可验证的 ExecIR。
 *
 * ## Parameters
 * @param[in] blockId 非零时指定已存在的所属块；0 表示仅追加指令与函数级 operand/result，
 * 不将该指令登记到块范围。
 * @param[in] opcode 指令 opcode；其 schema 用于判断终止符及 successor range。
 * @param[in] flags 原样写入指令 flags，由测试按 opcode contract 提供。
 * @param[in] operand 非零时作为唯一 operand 追加到函数 operand 表；0 表示无 operand。
 * @param[in] result 非零时作为唯一 result 追加到函数 result 表；0 表示无 result。
 *
 * ## Returns
 * @return AppendInstruction 提供的指令 ID。任一 Append 操作失败时 Unity 断言中止当前
 * 用例；已追加到 function 的局部数据由 tearDown 随 function 一并释放。
 *
 * ## Constraints and lifecycle
 * 非零 blockId 必须在 function.blocks 范围内；operand/result ID 的结构合法性由后续
 * 验证器检查。terminator 指令复制所属块当前 successorRange；blockId 为 0 时不写块
 * 范围或 terminatorInstructionId。instruction.sourceId 按当前 instructionCount 生成。
 *
 * ## References
 * 调用示例见 tests/parser/test_ssa_conditional_cleanup.c:90、:128、:129、:131、:135、:137、
 * tests/parser/test_ssa_state_map_liveness.c:10、:11、:12、:13、:14、
 * tests/parser/test_ssa_state_map_ownership.c:52、:54、:56、:58、:59、:60、
 * tests/parser/test_ssa_oracle_resume.c:157。
 */

/**
 * @fn const SZrExecIrStateMapEntry *checkpoint(TZrExecIrInstructionId instruction, EZrExecIrStateMapPhase phase)
 * ## Summary
 * 在当前 function 的 state map 中查找 instruction 与 phase 同时匹配的 checkpoint，
 * 供测试断言状态图内容或构造材料化请求。
 *
 * ## Parameters
 * @param[in] instruction 要查找的指令 ID。
 * @param[in] phase 要匹配的 state-map 阶段。
 *
 * ## Returns
 * @return 指向 function.stateMap->entries 内部项的只读借用指针。stateMap 为空或找不到
 * 项时 Unity 断言失败并 longjmp，不以 NULL 作为正常结果返回。
 *
 * ## Constraints and lifecycle
 * 返回值只在当前 function 和对应 stateMap 保持不变且存活期间有效；成功重建/替换
 * stateMap、修改其拥有存储或调用 FreeFunction 后必须丢弃。调用方不得释放或保存为
 * 独立所有对象。
 *
 * ## References
 * 结果立即用于测试断言或复制选择项，例如
 * tests/parser/test_ssa_conditional_cleanup.c:160、:248、
 * tests/parser/test_ssa_state_map_ownership.c:20、:90、
 * tests/parser/test_ssa_oracle_resume.c:105。
 */

/**
 * @fn void assert_only_root(TZrExecIrInstructionId instruction, EZrExecIrStateMapPhase phase, TZrExecIrValueId root)
 * ## Summary
 * 检查指定 checkpoint 的 rootValues 数量及可选唯一 root ID，并调用材料化 API 验证
 * checkpoint 可还原到临时 MaterializedState；主要用于 state-map 测试的组合断言。
 *
 * ## Parameters
 * @param[in] instruction checkpoint 所属指令 ID。
 * @param[in] phase 要检查的 state-map 阶段。
 * @param[in] root 期望唯一 root ID；0 表示期望 rootValues 为空。
 *
 * ## Returns
 * 无返回值；任何断言失败均由 Unity 标记当前测试失败。材料化 state 在正常路径由
 * helper 初始化并释放，function 与 map 仍归当前夹具所有。
 *
 * ## Constraints and lifecycle
 * 调用期间借用 function、其当前 stateMap 与 checkpoint entry；不得在此期间重建或
 * 释放这些对象。root==0 时只检查 rootValues 为空和材料化结果的 rootCount；非零时
 * 还检查 rootPool 项及 target.roots[0]。Unity 断言失败经 longjmp；若成功材料化后的
 * rootCount/root ID 断言失败，会跳过栈上 target 的 Free，但 fixture tearDown 只释放
 * function。当前测试没有触发这个失败分支，因此记录边界而不声称已复现缺陷。
 *
 * ## References
 * 直接调用示例见 tests/parser/test_ssa_state_map_liveness.c:20、:21、
 * tests/parser/test_ssa_state_map_ownership.c:22、:23、
 * tests/parser/test_ssa_oracle_resume.c:162。
 * 材料化准备失败时释放临时 prepared 状态见
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:811-814；提交阶段先释放
 * request->target 再赋入 prepared，见同文件:817-820。本 helper 正常路径在本文件
 * :155 释放局部 target。
 */

/**
 * @fn void build(void)
 * ## Summary
 * 完成测试 CFG 的前驱范围，使用 core verifier 验证当前 function，并在验证成功后
 * 调用 parser BuildStateMaps，为 resume、ownership 和 liveness 断言准备检查点。
 *
 * ## Returns
 * 无返回值；验证失败时通过 diagnostic 拼出 Unity 消息并中止当前用例；map 构建
 * 失败时 Unity 断言失败。成功时 map 归 function 所有并由 tearDown 释放。
 *
 * ## Constraints and lifecycle
 * 调用方须先构造值、块、边和指令；本 helper 会追加 predecessors，因此只适用于
 * 尚未填充前驱数组的夹具。VerifyFunction 与 BuildStateMaps 都可写入 diagnostic。
 * map 构建成功后，调用方可用 checkpoint 借用 entry；替换 map 后须重新查找。
 *
 * ## References
 * 测试调用示例见 tests/parser/test_ssa_conditional_cleanup.c:750、:789、
 * tests/parser/test_ssa_state_map_liveness.c:19、
 * tests/parser/test_ssa_state_map_ownership.c:61、
 * tests/parser/test_ssa_oracle_resume.c:161。
 * 生产 map 入口为 zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:351。
 */
