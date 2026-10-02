#include "zr_vm_core/exec_ir_state_map.h"

static void assert_scalar_state_map_materialization(
        TZrUInt32 entries, TZrBool wrongIdentity, TZrBool sidePool,
        TZrBool expectSuccess) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecBcVmEmission emission = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(build_add_sub_function(&function, ZR_NULL));
    function.stateMap = (SZrExecIrStateMap *)calloc(1u, sizeof(*function.stateMap));
    TEST_ASSERT_NOT_NULL(function.stateMap);
    function.stateMap->functionToken = function.functionToken;
    function.stateMap->signatureHash = function.signatureHash;
    function.stateMap->generation = function.contract.generation;
    if (wrongIdentity) ++function.stateMap->generation;
    if (entries != 0u) {
        function.stateMap->entries = (SZrExecIrStateMapEntry *)calloc(
                entries, sizeof(*function.stateMap->entries));
        TEST_ASSERT_NOT_NULL(function.stateMap->entries);
        function.stateMap->entryCount = entries;
        function.stateMap->entryCapacity = entries;
    }
    if (sidePool) {
        function.stateMap->valuePool = (TZrExecIrValueId *)calloc(
                1u, sizeof(*function.stateMap->valuePool));
        TEST_ASSERT_NOT_NULL(function.stateMap->valuePool);
        function.stateMap->valueCount = 1u;
        function.stateMap->valueCapacity = 1u;
    }
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildProjection(
            &function, &projection, &diagnostic));
    TEST_ASSERT_TRUE(projection.stateMapPresent);
    TEST_ASSERT_EQUAL(expectSuccess,
            ZrParser_ExecBcProjection_MaterializeVmFunction(
                    state, &projection, &emission, &diagnostic));
    if (expectSuccess) {
        TEST_ASSERT_NOT_NULL(emission.function);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, diagnostic.code);
        TEST_ASSERT_NULL(emission.function);
        TEST_ASSERT_NULL(emission.pcMap);
        TEST_ASSERT_EQUAL_UINT32(0u, emission.pcMapCount);
    }
    ZrParser_ExecBcVmEmission_Free(state, &emission);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_empty_state_map_does_not_require_vm_recovery(void) {
    assert_scalar_state_map_materialization(0u, ZR_FALSE, ZR_FALSE, ZR_TRUE);
}

static void test_nonempty_state_map_remains_unsupported(void) {
    assert_scalar_state_map_materialization(1u, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

static void test_empty_state_map_with_wrong_identity_remains_unsupported(void) {
    assert_scalar_state_map_materialization(0u, ZR_TRUE, ZR_FALSE, ZR_FALSE);
}

static void test_state_map_side_pool_without_entries_remains_unsupported(void) {
    assert_scalar_state_map_materialization(0u, ZR_FALSE, ZR_TRUE, ZR_FALSE);
}
