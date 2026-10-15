void test_state_map_verifier_rejects_attached_identity_mismatch(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 which;
    function_with_alloc_debug_boundary(&function);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    for (which = 0u; which < 3u; ++which) {
        if (which == 0u) ++function.stateMap->functionToken;
        if (which == 1u) ++function.stateMap->signatureHash;
        if (which == 2u) ++function.stateMap->generation;
        TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
                &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
        function.stateMap->functionToken = function.functionToken;
        function.stateMap->signatureHash = function.signatureHash;
        function.stateMap->generation = function.contract.generation;
    }
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_state_map_verifier_rejects_attached_storage_and_content(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 savedCount;
    TZrExecIrSourceId savedSource;
    function_with_alloc_debug_boundary(&function);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    savedCount = function.stateMap->valueCount;
    function.stateMap->valueCount = function.stateMap->valueCapacity + 1u;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    function.stateMap->valueCount = savedCount;
    savedSource = function.stateMap->entries[0].sourceId;
    ++function.stateMap->entries[0].sourceId;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    function.stateMap->entries[0].sourceId = savedSource;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_state_map_builder_replaces_stale_attached_map(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrStateMap *oldMap;
    const SZrExecIrStateMapEntry *entry;
    function_with_alloc_debug_boundary(&function);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    function.instructions[1].sourceId = 21u;
    oldMap = function.stateMap;
    function.blocks[0].instructionRange.start = UINT32_MAX;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(oldMap, function.stateMap);
    function.blocks[0].instructionRange.start = 0u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    entry = ZrCore_ExecIr_StateMapFind(function.stateMap, 21u, 2u,
            ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&function);
}

/**
 * @fn void test_state_map_builder_replaces_stale_attached_map(void)
 *
 * ## Summary
 * 在已构建 map 后修改 DEBUG_POLL 的 sourceId，使旧 map 内容语义过期；确认一个独立的无效 block 输入失败时保留旧 map 指针，恢复 block 后重建可按新 sourceId 21/resumeId 2 查询 BEFORE_EFFECT entry，且完整 Core verifier 接受结果。
 *
 * ## Returns
 * 本测试无 C 返回值；所有 Unity 断言通过时正常结束。断言失败由 Unity 跳出当前 case，再继续 teardown 和 main 中后续测试，不会立即终止整个进程。
 *
 * ## Constraints and lifecycle
 * 本例不比较成功重建前后的 map 指针，因此只证明新内容可查询且通过 verifier，不把指针不相等作为断言信用；失败分支只覆盖无效 block 的验证拒绝。测试图来自 `function_with_alloc_debug_boundary`；局部 function 在正常路径释放，提前断言失败会跳过末尾释放语句。
 *
 * ## References
 * 本测试定义在该 include 片段中，由 `tests/parser/test_ssa_state_maps.c:1351` 包含并在 `tests/parser/test_ssa_state_maps.c:1357` 注册。它调用 `ZrParser_ExecIr_BuildStateMaps`、`ZrCore_ExecIr_StateMapFind` 和 `ZrCore_ExecIr_VerifyFunction`。
 */
