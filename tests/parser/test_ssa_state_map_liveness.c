#include "ssa_state_map_fixture.h"

static void loop(EZrExecIrOwnership ownership, TZrUInt16 flags) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId reference = value(ownership, ZR_TRUE);
    TZrExecIrValueId copy = value(ownership, ZR_FALSE);
    blocks(3u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_COPY, 0u, reference, copy);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, flags, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, condition, 0u);
}

static void test_backedge_keeps_reference_live_after_last_linear_use(void) {
    loop(ZR_EXEC_IR_OWNERSHIP_GC, ZR_EXEC_IR_FLAG_MAY_GC);
    build();
    assert_only_root(3u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, 2u);
    assert_only_root(3u, ZR_EXEC_IR_STATE_AFTER_EFFECT, 2u);
}

static void test_backedge_borrow_is_rejected_at_suspend(void) {
    SZrExecIrStateMap *published;
    loop(ZR_EXEC_IR_OWNERSHIP_BORROWED, ZR_EXEC_IR_FLAG_MAY_GC);
    build();
    published = function.stateMap;
    function.instructions[2].flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
    TEST_ASSERT_EQUAL_UINT32(3u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(103u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(2u, diagnostic.blockId);
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
}

static void test_sibling_branch_value_does_not_cross_suspend(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId borrowed = value(ZR_EXEC_IR_OWNERSHIP_BORROWED, ZR_TRUE);
    blocks(3u);
    edges(1u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_SUSPEND, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, condition, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, borrowed, 0u);
    build();
    TEST_ASSERT_EQUAL_UINT32(1u,
            checkpoint(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT)->liveValues.count);
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, 0u);
}

static void test_phi_inputs_are_live_only_on_their_incoming_edges(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId left = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId right = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId merged = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_FALSE);
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming[2] = {{2u, left}, {3u, right}};
    blocks(4u);
    edges(1u, 2u, 3u);
    edges(2u, 4u, 0u);
    edges(3u, 4u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(4u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(4u, ZR_EXEC_IR_OPCODE_RETURN, 0u, merged, 0u);
    phi.result = merged;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhiIncoming(
            &function, incoming, 2u, &phi.incomings));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhis(
            &function, &phi, 1u, &function.blocks[3].phis));
    build();
    assert_only_root(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT, left);
    assert_only_root(4u, ZR_EXEC_IR_STATE_AFTER_EFFECT, right);
    assert_only_root(6u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, merged);
}

static void test_definition_order_follows_cfg_instead_of_instruction_ids(void) {
    TZrExecIrValueId reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_FALSE);
    blocks(3u);
    edges(1u, 3u, 0u);
    edges(3u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, reference, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, reference);
    emit(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    build();
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
}

static void test_exception_successor_keeps_its_own_roots(void) {
    TZrExecIrValueId reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId result = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_FALSE);
    TZrExecIrMemoryTokenId before[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)};
    TZrExecIrMemoryTokenId after[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)};
    blocks(3u);
    function.blocks[2].flags |= ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    edges(1u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_INVOKE,
         ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, 0u, result);
    function.instructions[0].effectIn = 1u;
    function.instructions[0].effectOut = 2u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, before, 2u, &function.instructions[0].memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, after, 2u, &function.instructions[0].memoryOut));
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, result, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, reference, 0u);
    build();
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, result);
    assert_only_root(4u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
}

static void test_deopt_values_die_after_their_checkpoint(void) {
    TZrExecIrValueId reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrUInt32 index;
    for (index = 0u; index < 3u; ++index) {
        emit(0u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    }
    function.deoptStates = (SZrExecIrDeoptState *)calloc(1u, sizeof(*function.deoptStates));
    function.deoptValues = (TZrExecIrValueId *)malloc(sizeof(*function.deoptValues));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    TEST_ASSERT_NOT_NULL(function.deoptValues);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptValueCount = function.deoptValueCapacity = 1u;
    function.deoptValues[0] = reference;
    function.instructions[1].deoptId = 91u;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].sourceId = 102u;
    function.deoptStates[0].resumeId = 701u;
    function.deoptStates[0].valueRange.count = 1u;
    build();
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
    assert_only_root(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT, reference);
    assert_only_root(3u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, 0u);
}

static void test_reverse_layout_converges_and_preserves_high_value_id(void) {
    TZrUInt32 index;
    TZrExecIrValueId reference = 0u;
    for (index = 0u; index < 65u; ++index) {
        reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    }
    blocks(64u);
    edges(1u, 64u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, reference, 0u);
    for (index = 3u; index <= 64u; ++index) {
        edges(index, index - 1u, 0u);
        emit(index, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
        emit(index, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    }
    build();
    assert_only_root(function.instructionCount - 1u,
                     ZR_EXEC_IR_STATE_AFTER_EFFECT, reference);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
}

static void test_empty_function_has_empty_state_map(void) {
    build();
    TEST_ASSERT_NOT_NULL(function.stateMap);
    TEST_ASSERT_EQUAL_UINT32(0u, function.stateMap->entryCount);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_backedge_keeps_reference_live_after_last_linear_use);
    RUN_TEST(test_backedge_borrow_is_rejected_at_suspend);
    RUN_TEST(test_sibling_branch_value_does_not_cross_suspend);
    RUN_TEST(test_phi_inputs_are_live_only_on_their_incoming_edges);
    RUN_TEST(test_definition_order_follows_cfg_instead_of_instruction_ids);
    RUN_TEST(test_exception_successor_keeps_its_own_roots);
    RUN_TEST(test_deopt_values_die_after_their_checkpoint);
    RUN_TEST(test_reverse_layout_converges_and_preserves_high_value_id);
    RUN_TEST(test_empty_function_has_empty_state_map);
    return UNITY_END();
}

/**
 * @fn static void loop(EZrExecIrOwnership ownership, TZrUInt16 flags)
 *
 * ## Summary
 * 在新鲜 fixture 中建立 entry、自环、exit 三块 CFG，供回边活性与 borrowed/suspend 两个 case 复用。
 *
 * ## Parameters
 * @param[in] ownership reference 与 COPY result 的所有权种类；当前两处 caller 分别传 GC 和 BORROWED。
 * @param[in] flags 赋给循环块 NOP 的指令 flags；当前两处 caller 初始都传 MAY_GC。
 *
 * ## Returns
 * 无 C 返回值；正常完成后共享 function 中含三个 value、三个 block 和五条指令。构造 helper 的断言失败会退出当前 case，未完成结构仍挂到共享 fixture。
 *
 * ## Constraints and lifecycle
 * 调用前由本 TU setUp 初始化空 function。blocks 和 edges 使用固定一基 ID，不是向任意已有 CFG 追加的通用 builder；两个当前 caller 每 case 只调用一次。
 * condition 为外部 UNKNOWN value1，reference 为指定 ownership 的外部 value2，copy 为非外部 value3；block2 在 COPY 后经过带 flags 的 NOP 和条件分支回到自身或进入 exit。
 * 只构图，不建立 predecessor/map 或执行循环；调用方随后用 build 计算 predecessors、Verify 并建 map。ownership/flags 要使整个构造满足后续验证。
 * 动态池归共享 function，tearDown 通过 FreeFunction 释放；没有参数所有权转移或返回资源，也不承担其他 helper 的局部 target 清理。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:3
 * - tests/parser/test_ssa_state_map_liveness.c:4
 * - tests/parser/test_ssa_state_map_liveness.c:5
 * - tests/parser/test_ssa_state_map_liveness.c:6
 * - tests/parser/test_ssa_state_map_liveness.c:7
 * - tests/parser/test_ssa_state_map_liveness.c:8
 * - tests/parser/test_ssa_state_map_liveness.c:9
 * - tests/parser/test_ssa_state_map_liveness.c:10
 * - tests/parser/test_ssa_state_map_liveness.c:11
 * - tests/parser/test_ssa_state_map_liveness.c:12
 * - tests/parser/test_ssa_state_map_liveness.c:13
 * - tests/parser/test_ssa_state_map_liveness.c:14
 * - tests/parser/test_ssa_state_map_liveness.c:18
 * - tests/parser/test_ssa_state_map_liveness.c:26
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:28
 * - tests/parser/ssa_state_map_fixture.h:38
 * - tests/parser/ssa_state_map_fixture.h:46
 * - tests/parser/ssa_state_map_fixture.h:77
 * - tests/parser/ssa_state_map_fixture.h:158
 * - tests/parser/ssa_state_map_fixture.h:161
 * - tests/parser/ssa_state_map_fixture.h:162
 * - tests/parser/ssa_state_map_fixture.h:170
 */

/**
 * @fn static void test_backedge_keeps_reference_live_after_last_linear_use(void)
 *
 * ## Summary
 * 构造 GC value 2 的自环 CFG，Build 成功；helper 断言指令3的 BEFORE 和 AFTER 均仅有根 ID2，并执行有限的逻辑物化根检查。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 使用 GC reference 的固定自环，指令3 BEFORE 与 AFTER 通过 assert_only_root 分别比较根数1、根 ID2，并验证逻辑物化后的同一 ID。
 * 这是两个checkpoint的根断言，不直接读取完整存活矩阵、其他value每一位、迭代次数或性能。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:17
 * - tests/parser/test_ssa_state_map_liveness.c:183
 * - tests/parser/test_ssa_state_map_liveness.c:18
 * - tests/parser/test_ssa_state_map_liveness.c:19
 * - tests/parser/test_ssa_state_map_liveness.c:20
 * - tests/parser/test_ssa_state_map_liveness.c:21
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:77
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:103
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:171
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:183
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:188
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:197
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:273
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:276
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_backedge_borrow_is_rejected_at_suspend(void)
 *
 * ## Summary
 * 先成功建立 MAY_GC map，再将指令3改为 MAY_SUSPEND；Builder FALSE，诊断 BORROWED_ACROSS_SUSPEND，token99/instruction3/source103/block2，并保留原 map 精确指针。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 先以 BORROWED reference 和 MAY_GC NOP 成功 build 并保存 map 指针；随后把指令3 flags 替换为 MAY_SUSPEND，再直接调用 Builder。
 * 显式断言 FALSE、BORROWED_ACROSS_SUSPEND、functionToken99、instructionId3、sourceId103、blockId2和旧map精确指针不变。
 * Builder 的 verificationView 去掉旧stateMap后先Verify，再建liveness和owner；预期拒绝在 add_checkpoint 的存活BORROWED与SUSPEND检查，candidate和临时分析随后释放。
 * 指针保留不说明旧map仍与修改后的IR语义一致；本case没有再次用旧map物化，也未逐池比较内容或测试 generation/分配故障。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:24
 * - tests/parser/test_ssa_state_map_liveness.c:184
 * - tests/parser/test_ssa_state_map_liveness.c:26
 * - tests/parser/test_ssa_state_map_liveness.c:27
 * - tests/parser/test_ssa_state_map_liveness.c:28
 * - tests/parser/test_ssa_state_map_liveness.c:29
 * - tests/parser/test_ssa_state_map_liveness.c:30
 * - tests/parser/test_ssa_state_map_liveness.c:31
 * - tests/parser/test_ssa_state_map_liveness.c:32
 * - tests/parser/test_ssa_state_map_liveness.c:33
 * - tests/parser/test_ssa_state_map_liveness.c:34
 * - tests/parser/test_ssa_state_map_liveness.c:35
 * - tests/parser/test_ssa_state_map_liveness.c:36
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:375
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:379
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:384
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:265
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:267
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:268
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:269
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:271
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:445
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:464
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:465
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:466
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:474
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:475
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:183
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_sibling_branch_value_does_not_cross_suspend(void)
 *
 * ## Summary
 * 互斥分支仅在另一条返回分支读取 borrowed value；Builder 成功，suspend checkpoint liveValues.count 为1，根数为0。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * condition与borrowed为两个外部value，分支2含suspend NOP并返回condition，分支3返回borrowed。
 * Build成功后仅直接比较指令2 BEFORE 的 liveValues.count==1，并断言该处根数0和逻辑物化空根。
 * 没有比较唯一live ValueId；结论限制到固定互斥分支和checkpoint，不泛化所有sibling或异常边。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:39
 * - tests/parser/test_ssa_state_map_liveness.c:185
 * - tests/parser/test_ssa_state_map_liveness.c:40
 * - tests/parser/test_ssa_state_map_liveness.c:41
 * - tests/parser/test_ssa_state_map_liveness.c:43
 * - tests/parser/test_ssa_state_map_liveness.c:45
 * - tests/parser/test_ssa_state_map_liveness.c:46
 * - tests/parser/test_ssa_state_map_liveness.c:47
 * - tests/parser/test_ssa_state_map_liveness.c:48
 * - tests/parser/test_ssa_state_map_liveness.c:49
 * - tests/parser/test_ssa_state_map_liveness.c:50
 * - tests/parser/test_ssa_state_map_liveness.c:51
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:77
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:83
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:91
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:188
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:265
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:103
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_phi_inputs_are_live_only_on_their_incoming_edges(void)
 *
 * ## Summary
 * diamond PHI 在左右前驱 checkpoint AFTER 分别仅有 left/right 根，在 join 指令6 BEFORE 仅有 merged 根。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 局部incoming[2]绑定predecessor2/3的left/right GC值，将incoming和PHI追加到function池后build；两次追加各有显式TRUE断言。
 * 左前驱指令2 AFTER唯一根为left，右前驱指令4 AFTER唯一根为right，join指令6 BEFORE唯一根为merged；helper逐次验证逻辑物化。
 * 局部phi/incoming是追加API的构造输入，函数池归fixture清理；case不另持owning map，不声称已枚举全部incoming、live位或owner状态。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:54
 * - tests/parser/test_ssa_state_map_liveness.c:186
 * - tests/parser/test_ssa_state_map_liveness.c:59
 * - tests/parser/test_ssa_state_map_liveness.c:60
 * - tests/parser/test_ssa_state_map_liveness.c:73
 * - tests/parser/test_ssa_state_map_liveness.c:75
 * - tests/parser/test_ssa_state_map_liveness.c:77
 * - tests/parser/test_ssa_state_map_liveness.c:78
 * - tests/parser/test_ssa_state_map_liveness.c:79
 * - tests/parser/test_ssa_state_map_liveness.c:80
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:682
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:689
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:892
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:900
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:94
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:99
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:102
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:103
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:191
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:193
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_definition_order_follows_cfg_instead_of_instruction_ids(void)
 *
 * ## Summary
 * CFG 从 block1 经物理较后的定义 block3 到 block2，Builder 成功，指令2 BEFORE 根为 ref。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * reference为非外部GC value，CFG为block1→block3→block2，物理较后的block3 CONSTANT定义被block2 RETURN使用。
 * build成功后仅比较指令2 BEFORE唯一根reference及逻辑物化同一ID；没有执行CONSTANT/RETURN或比较运行时值。
 * fixture先Verify ALL；结论不代表任意损坏SSA或CFG会被liveness接受。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:83
 * - tests/parser/test_ssa_state_map_liveness.c:187
 * - tests/parser/test_ssa_state_map_liveness.c:84
 * - tests/parser/test_ssa_state_map_liveness.c:86
 * - tests/parser/test_ssa_state_map_liveness.c:87
 * - tests/parser/test_ssa_state_map_liveness.c:89
 * - tests/parser/test_ssa_state_map_liveness.c:90
 * - tests/parser/test_ssa_state_map_liveness.c:91
 * - tests/parser/test_ssa_state_map_liveness.c:93
 * - tests/parser/test_ssa_state_map_liveness.c:94
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:171
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:174
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:183
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:188
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:197
 * - tests/parser/ssa_state_map_fixture.h:162
 * - tests/parser/ssa_state_map_fixture.h:170
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_exception_successor_keeps_its_own_roots(void)
 *
 * ## Summary
 * INVOKE 前根为 ref，正常后继 NOP 前根为 result，异常后继 NOP 前根为 ref。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 构造MAY_THROW|MAY_ALLOCATE的INVOKE result，正常与EXCEPTION后继分别返回result/reference；给effect和两memory domain追加1→2 token。
 * 仅比较三个BEFORE checkpoint：INVOKE指令1前reference、正常NOP指令2前result、异常NOP指令4前reference，各为唯一根。
 * 没有直接断言INVOKE AFTER/CLEANUP的OwnerMaskAt mask。result为GC且有普通用途，case名称不能为cleanup-only UNIQUE/SHARED或post-phase UNINITIALIZED补位授信用。
 * 只构造并验证逻辑CFG/map，不调用Oracle或执行实际INVOKE/throw；局部token数组是追加输入，动态池归function。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:97
 * - tests/parser/test_ssa_state_map_liveness.c:188
 * - tests/parser/test_ssa_state_map_liveness.c:98
 * - tests/parser/test_ssa_state_map_liveness.c:99
 * - tests/parser/test_ssa_state_map_liveness.c:100
 * - tests/parser/test_ssa_state_map_liveness.c:103
 * - tests/parser/test_ssa_state_map_liveness.c:107
 * - tests/parser/test_ssa_state_map_liveness.c:109
 * - tests/parser/test_ssa_state_map_liveness.c:111
 * - tests/parser/test_ssa_state_map_liveness.c:113
 * - tests/parser/test_ssa_state_map_liveness.c:115
 * - tests/parser/test_ssa_state_map_liveness.c:117
 * - tests/parser/test_ssa_state_map_liveness.c:118
 * - tests/parser/test_ssa_state_map_liveness.c:119
 * - tests/parser/test_ssa_state_map_liveness.c:120
 * - tests/parser/test_ssa_state_map_liveness.c:121
 * - tests/parser/test_ssa_state_map_liveness.c:122
 * - tests/parser/test_ssa_state_map_liveness.c:123
 * - tests/parser/test_ssa_state_map_liveness.c:124
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:875
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:883
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:83
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:91
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:267
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:269
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:272
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:273
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:276
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_deopt_values_die_after_their_checkpoint(void)
 *
 * ## Summary
 * 普通 deoptValues 中的 GC ref 在指令1 BEFORE 与 checkpoint2 BEFORE/AFTER 为唯一根，下一指令3 BEFORE 根数0。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 构造三个DEBUG_POLL NOP，将一个calloc state和一个malloc ValueId池挂到function，检查非空后设置count/capacity1、state91/source102/resume701和valueRange。
 * 指令1 BEFORE、checkpoint2 BEFORE/AFTER唯一根均为reference，下一指令3 BEFORE根数0，每次helper还比较逻辑物化根。
 * 逻辑ID存活/根断言不表示运行时对象已释放或真实GC保活；该配方是普通deopt valueRange，不是aggregate图。
 * 两次原始分配先写function字段，FreeFunction会释放这些池；自然OOM会先在135/136触发测试断言，不能计为LivenessBuild逐分配故障注入。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:127
 * - tests/parser/test_ssa_state_map_liveness.c:189
 * - tests/parser/test_ssa_state_map_liveness.c:128
 * - tests/parser/test_ssa_state_map_liveness.c:131
 * - tests/parser/test_ssa_state_map_liveness.c:133
 * - tests/parser/test_ssa_state_map_liveness.c:134
 * - tests/parser/test_ssa_state_map_liveness.c:135
 * - tests/parser/test_ssa_state_map_liveness.c:136
 * - tests/parser/test_ssa_state_map_liveness.c:137
 * - tests/parser/test_ssa_state_map_liveness.c:138
 * - tests/parser/test_ssa_state_map_liveness.c:139
 * - tests/parser/test_ssa_state_map_liveness.c:140
 * - tests/parser/test_ssa_state_map_liveness.c:141
 * - tests/parser/test_ssa_state_map_liveness.c:142
 * - tests/parser/test_ssa_state_map_liveness.c:143
 * - tests/parser/test_ssa_state_map_liveness.c:144
 * - tests/parser/test_ssa_state_map_liveness.c:145
 * - tests/parser/test_ssa_state_map_liveness.c:146
 * - tests/parser/test_ssa_state_map_liveness.c:147
 * - tests/parser/test_ssa_state_map_liveness.c:148
 * - tests/parser/test_ssa_state_map_liveness.c:149
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:41
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:44
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:50
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:53
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:123
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:132
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:139
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:175
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:176
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_reverse_layout_converges_and_preserves_high_value_id(void)
 *
 * ## Summary
 * 65个值与64块逆布局链成功建立；末段所选 AFTER checkpoint 和再次 Build 后指令1 BEFORE 的唯一根为 ID65。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 创建65个外部GC value，reference为最后ID65；64块CFG按entry1→64→63逐步到2，物理block2 RETURN使用reference。
 * 首次build后比较function.instructionCount-1的AFTER唯一根reference，第二次直接Builder TRUE后再比较指令1 BEFORE唯一根reference。
 * 这是ID65及两个指定checkpoint的最终根结果，不直接枚举前64bit、raw位图、固定点迭代轮数或性能。
 * 第一次helper正常返回前已Free局部target，没有跨第二次成功Build保存旧entry或池借用；重建后重新查询新map。这里重用的是Builder的map替换，不是向非空Liveness记录再次Build。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 * assert_only_root 只检查逻辑 ValueId 根清单并调用 MaterializeState；不执行 ExecIR、Oracle、实际 GC 或运行时对象重建。其局部 target 在正常路径 Free，Unity 失败早退可能越过该尾部；tearDown 不代收该局部 target。
 * checkpoint 返回的 entry 与池元素是 map 借用，成功重建或释放 map 后不得继续使用；case 本身不增加跨调用的持有引用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:152
 * - tests/parser/test_ssa_state_map_liveness.c:190
 * - tests/parser/test_ssa_state_map_liveness.c:155
 * - tests/parser/test_ssa_state_map_liveness.c:156
 * - tests/parser/test_ssa_state_map_liveness.c:158
 * - tests/parser/test_ssa_state_map_liveness.c:159
 * - tests/parser/test_ssa_state_map_liveness.c:162
 * - tests/parser/test_ssa_state_map_liveness.c:163
 * - tests/parser/test_ssa_state_map_liveness.c:164
 * - tests/parser/test_ssa_state_map_liveness.c:165
 * - tests/parser/test_ssa_state_map_liveness.c:168
 * - tests/parser/test_ssa_state_map_liveness.c:169
 * - tests/parser/test_ssa_state_map_liveness.c:170
 * - tests/parser/test_ssa_state_map_liveness.c:171
 * - tests/parser/test_ssa_state_map_liveness.c:172
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:7
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:154
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:171
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:174
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:183
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:197
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:202
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:138
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 */

/**
 * @fn static void test_empty_function_has_empty_state_map(void)
 *
 * ## Summary
 * 空函数 Builder 成功，stateMap 非NULL 且 entryCount0。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * 无 C 返回值；预期断言全部满足时正常返回。Unity 失败记录当前 case 失败并退出该 case，runner 随后执行 tearDown、继续后续 case，最终由 UNITY_END 汇总失败计数。
 *
 * ## Constraints and lifecycle
 * 不添加value、block或instruction，只调用共享build，显式断言stateMap非NULL且entryCount0。
 * 本case不调用assert_only_root或MaterializeState，不读Liveness四指针、rowBytes或分配次数。
 * 源码有空分析成功分支，显式断言仍只针对发布map记录与entry数；不证明NULL参数容错、无分配或所有零尺寸组合。
 * 由本 TU 的共享 setUp 初始化空 function，按 Unity 串行 case 使用；构造出的函数池和已发布 map 由共享 tearDown 的 FreeFunction 释放，不把共享定义重复计为本文件单元。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:175
 * - tests/parser/test_ssa_state_map_liveness.c:191
 * - tests/parser/test_ssa_state_map_liveness.c:176
 * - tests/parser/test_ssa_state_map_liveness.c:177
 * - tests/parser/test_ssa_state_map_liveness.c:178
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:150
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:151
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:152
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:230
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:232
 * - tests/parser/ssa_state_map_fixture.h:162
 * - tests/parser/ssa_state_map_fixture.h:170
 * - tests/parser/ssa_state_map_fixture.h:25
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:379
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 启动独立 liveness Unity 程序，按源码顺序注册九个 case，并将 Unity 汇总失败数返回给进程调用方。
 *
 * ## Parameters
 * 无显式参数；使用本 TU 的共享 fixture 和 Unity case 上下文。
 *
 * ## Returns
 * @return 返回 UNITY_END 的 int 失败计数；0 表示 runner 未记录失败，非零表示记录到失败。九个 void case 自身没有 C 返回值。
 *
 * ## Constraints and lifecycle
 * RUN_TEST 把各case函数指针交给 UnityDefaultTestRun；runner 对每case执行共享 setUp、case、tearDown。本文件不定义独立hooks，不把头文件物理定义重复计入本文件。
 * 默认 setjmp/longjmp 路径的断言失败退出当前 case，runner 再经受保护的 tearDown 并继续后续注册；不会补执行断言之后的 case 局部清理。
 * 这些case生成、验证和逻辑物化状态图，不执行 Oracle 或待测 ExecIR，也没有本TU的 liveness 分配故障替身。共享本TU状态，须串行运行。
 * 现有 CMake 独立目标与 CTest COMMAND 提供进程入口；代码没有线程场景、平台条件定义或跨平台运行认证。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_liveness.c:181
 * - tests/parser/test_ssa_state_map_liveness.c:182
 * - tests/parser/test_ssa_state_map_liveness.c:183
 * - tests/parser/test_ssa_state_map_liveness.c:184
 * - tests/parser/test_ssa_state_map_liveness.c:185
 * - tests/parser/test_ssa_state_map_liveness.c:186
 * - tests/parser/test_ssa_state_map_liveness.c:187
 * - tests/parser/test_ssa_state_map_liveness.c:188
 * - tests/parser/test_ssa_state_map_liveness.c:189
 * - tests/parser/test_ssa_state_map_liveness.c:190
 * - tests/parser/test_ssa_state_map_liveness.c:191
 * - tests/parser/test_ssa_state_map_liveness.c:192
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:900
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:909
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:910
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:934
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:881
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 * - tests/third_party/zr_unity/Unity/src/unity.c:2283
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/cmake/ssa-tests.cmake:274
 * - tests/cmake/ssa-tests.cmake:275
 * - tests/cmake/ssa-tests.cmake:299
 */
