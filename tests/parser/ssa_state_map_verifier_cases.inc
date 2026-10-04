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
