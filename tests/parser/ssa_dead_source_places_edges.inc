/* Source-derived edges for the finite dead temporary-place contract.
 * Metadata added to cloned real graphs below is negative guard evidence,
 * never a substitute for the production Parse/BuildModule prerequisites. */

static void assert_compressed_as_input(TZrUInt32 fixtureIndex) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(fixtureIndex);
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 originalBefore, sourceBefore, compressedBefore;
    eliminate_and_assert(fixture);
    originalBefore = function_digest(fixture_function(fixture));
    sourceBefore = source_digest(fixture);
    compressedBefore = function_digest(&g_output);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, g_output.instructions[1].opcode);
    assert_api(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            &g_output, &g_mutated, &diagnostic), &diagnostic,
            "compressed PLACE_BASE-to-NOP output must itself be accepted");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(compressedBefore, function_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(originalBefore, function_digest(fixture_function(fixture)));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    assert_candidate(fixture, &g_mutated);
    TEST_ASSERT_EQUAL_MEMORY(g_output.instructions, g_mutated.instructions,
            g_output.instructionCount * sizeof(*g_output.instructions));
    TEST_ASSERT_EQUAL_MEMORY(g_output.values, g_mutated.values,
            g_output.valueCount * sizeof(*g_output.values));
    TEST_ASSERT_EQUAL_MEMORY(g_output.operandPool, g_mutated.operandPool,
            g_output.operandCount * sizeof(*g_output.operandPool));
    TEST_ASSERT_EQUAL_MEMORY(g_output.resultPool, g_mutated.resultPool,
            g_output.resultCount * sizeof(*g_output.resultPool));
    TEST_ASSERT_TRUE(g_output.values != g_mutated.values);
    TEST_ASSERT_TRUE(g_output.instructions != g_mutated.instructions);
    assert_oracle(fixture, fixture_function(fixture), 1u);
    assert_oracle(fixture, &g_output, 0u);
}

static void test_compressed_nine_as_input(void) { assert_compressed_as_input(0u); }
static void test_compressed_eight_as_input(void) { assert_compressed_as_input(1u); }

static void test_null_diagnostic_success(void) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(0u);
    SZrExecIrFunction *input = fixture_function(fixture);
    TZrUInt64 before = function_digest(input), sourceBefore = source_digest(fixture);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, &g_output, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    assert_candidate(fixture, &g_output);
}

static void test_actual_empty_state_header_preserved(void) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(0u);
    const SZrExecIrStateMap *map = fixture_function(fixture)->stateMap;
    TEST_ASSERT_NOT_NULL_MESSAGE(map, "PRECONDITION: production empty state-map header");
    TEST_ASSERT_EQUAL_UINT32(0u, map->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, map->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, map->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, map->ownerStateCount);
    eliminate_and_assert(fixture);
    TEST_ASSERT_TRUE(map != g_output.stateMap);
    TEST_ASSERT_EQUAL_UINT32(map->functionToken, g_output.stateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT64(map->signatureHash, g_output.stateMap->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(map->generation, g_output.stateMap->generation);
}

/* VERIFY_ALL deliberately checks source-map storage, not locations/identity.
 * These are valid Core graphs with unsupported source correspondence. */
static void test_guard_source_map_location_mismatch(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    ++g_mutated.sourceMaps[1].startColumn;
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: map location mismatch is VERIFY_ALL-valid");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_duplicate_source_map_identity(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.sourceMaps[1].instructionId = g_mutated.sourceMaps[0].instructionId;
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: duplicate map identity is VERIFY_ALL-valid");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

enum EZrDeadPlaceSharedStorage {
    DEAD_PLACE_SHARED_VALUES,
    DEAD_PLACE_SHARED_OPERANDS,
    DEAD_PLACE_SHARED_STATE_MAP,
    DEAD_PLACE_INTERIOR_VALUES,
    DEAD_PLACE_INTERIOR_OPERANDS,
    DEAD_PLACE_INTERIOR_STATE_VALUES
};

static void assert_shared_output_refused(enum EZrDeadPlaceSharedStorage storage) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrValue *ownedValues = g_output.values;
    TZrExecIrValueId *ownedOperands = g_output.operandPool;
    SZrExecIrStateMap *ownedStateMap = g_output.stateMap;
    TZrExecIrValueId *ownedStateValues = ownedStateMap != ZR_NULL
            ? ownedStateMap->valuePool : ZR_NULL;
    TZrUInt32 ownedCount = 0u, ownedCapacity = 0u;
    const void *borrowedPointer;
    TZrUInt32 borrowedCount = 0u, borrowedCapacity = 0u;
    TZrUInt64 inputBefore = function_digest(&g_mutated);
    TZrUInt64 outputBefore = function_digest(&g_output), sourceBefore = source_digest(fixture);
    TZrBool success, borrowedUnchanged;
    TZrBool values = (TZrBool)(storage == DEAD_PLACE_SHARED_VALUES ||
            storage == DEAD_PLACE_INTERIOR_VALUES);
    TZrBool operands = (TZrBool)(storage == DEAD_PLACE_SHARED_OPERANDS ||
            storage == DEAD_PLACE_INTERIOR_OPERANDS);
    TZrUInt32 interior = (storage == DEAD_PLACE_INTERIOR_VALUES ||
            storage == DEAD_PLACE_INTERIOR_OPERANDS) ? 1u : 0u;
    TEST_ASSERT_NOT_NULL(ownedStateMap);
    TEST_ASSERT_NOT_NULL(g_mutated.stateMap);
    if (values) {
        TEST_ASSERT_GREATER_THAN_UINT32(interior, g_mutated.valueCount);
        ownedCount = g_output.valueCount;
        ownedCapacity = g_output.valueCapacity;
        g_output.values = g_mutated.values + interior;
        g_output.valueCount = g_mutated.valueCount - interior;
        g_output.valueCapacity = g_mutated.valueCapacity - interior;
        borrowedPointer = g_output.values;
        borrowedCount = g_output.valueCount;
        borrowedCapacity = g_output.valueCapacity;
    } else if (operands) {
        TEST_ASSERT_GREATER_THAN_UINT32(interior, g_mutated.operandCount);
        ownedCount = g_output.operandCount;
        ownedCapacity = g_output.operandCapacity;
        g_output.operandPool = g_mutated.operandPool + interior;
        g_output.operandCount = g_mutated.operandCount - interior;
        g_output.operandCapacity = g_mutated.operandCapacity - interior;
        borrowedPointer = g_output.operandPool;
        borrowedCount = g_output.operandCount;
        borrowedCapacity = g_output.operandCapacity;
    } else if (storage == DEAD_PLACE_SHARED_STATE_MAP) {
        g_output.stateMap = g_mutated.stateMap;
        borrowedPointer = g_output.stateMap;
    } else {
        TEST_ASSERT_GREATER_THAN_UINT32(1u, g_mutated.valueCount);
        ownedCount = ownedStateMap->valueCount;
        ownedCapacity = ownedStateMap->valueCapacity;
        /* Only ownership spans are inspected. This borrowed address is never
         * read as a value-ID pool and never freed by the fixture. */
        ownedStateMap->valuePool = (TZrExecIrValueId *)(void *)(g_mutated.values + 1u);
        ownedStateMap->valueCount = ownedStateMap->valueCapacity = 1u;
        borrowedPointer = ownedStateMap->valuePool;
        borrowedCount = ownedStateMap->valueCount;
        borrowedCapacity = ownedStateMap->valueCapacity;
    }
    success = ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            &g_mutated, &g_output, &diagnostic);
    if (values) {
        borrowedUnchanged = (TZrBool)(g_output.values == borrowedPointer &&
                g_output.valueCount == borrowedCount &&
                g_output.valueCapacity == borrowedCapacity);
    } else if (operands) {
        borrowedUnchanged = (TZrBool)(g_output.operandPool == borrowedPointer &&
                g_output.operandCount == borrowedCount &&
                g_output.operandCapacity == borrowedCapacity);
    } else if (storage == DEAD_PLACE_SHARED_STATE_MAP) {
        borrowedUnchanged = (TZrBool)(g_output.stateMap == borrowedPointer);
    } else {
        borrowedUnchanged = (TZrBool)(g_output.stateMap == ownedStateMap &&
                g_output.stateMap->valuePool == borrowedPointer &&
                g_output.stateMap->valueCount == borrowedCount &&
                g_output.stateMap->valueCapacity == borrowedCapacity);
    }
    /* Restore ownership before Unity can longjmp. If a broken implementation
     * publishes, it has freed the borrowed allocation: detach it from input
     * and release the replaced output allocation to avoid teardown doublefree. */
    if (!success) {
        if (values) {
            g_output.values = ownedValues;
            g_output.valueCount = ownedCount;
            g_output.valueCapacity = ownedCapacity;
        } else if (operands) {
            g_output.operandPool = ownedOperands;
            g_output.operandCount = ownedCount;
            g_output.operandCapacity = ownedCapacity;
        } else if (storage == DEAD_PLACE_SHARED_STATE_MAP) {
            g_output.stateMap = ownedStateMap;
        } else {
            g_output.stateMap = ownedStateMap;
            ownedStateMap->valuePool = ownedStateValues;
            ownedStateMap->valueCount = ownedCount;
            ownedStateMap->valueCapacity = ownedCapacity;
        }
    } else if (values) {
        g_mutated.values = ZR_NULL;
        g_mutated.valueCount = g_mutated.valueCapacity = 0u;
        free(ownedValues);
    } else if (operands) {
        g_mutated.operandPool = ZR_NULL;
        g_mutated.operandCount = g_mutated.operandCapacity = 0u;
        free(ownedOperands);
    } else if (storage == DEAD_PLACE_SHARED_STATE_MAP) {
        g_mutated.stateMap = ZR_NULL;
        ZrCore_ExecIr_StateMapFree(ownedStateMap);
        free(ownedStateMap);
    } else {
        g_mutated.values = ZR_NULL;
        g_mutated.valueCount = g_mutated.valueCapacity = 0u;
        free(ownedStateValues);
    }
    TEST_ASSERT_FALSE_MESSAGE(success, "independent output record must own disjoint storage");
    TEST_ASSERT_TRUE_MESSAGE(borrowedUnchanged,
            "refusal must preserve selected borrowed pointer/count/capacity before restoration");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(inputBefore, function_digest(&g_mutated));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_shared_value_storage(void) {
    assert_shared_output_refused(DEAD_PLACE_SHARED_VALUES);
}
static void test_guard_shared_operand_storage(void) {
    assert_shared_output_refused(DEAD_PLACE_SHARED_OPERANDS);
}
static void test_guard_shared_state_map_storage(void) {
    assert_shared_output_refused(DEAD_PLACE_SHARED_STATE_MAP);
}
static void test_guard_interior_value_storage(void) {
    assert_shared_output_refused(DEAD_PLACE_INTERIOR_VALUES);
}
static void test_guard_interior_operand_storage(void) {
    assert_shared_output_refused(DEAD_PLACE_INTERIOR_OPERANDS);
}
static void test_guard_interior_state_value_storage(void) {
    assert_shared_output_refused(DEAD_PLACE_INTERIOR_STATE_VALUES);
}

static void assert_missing_semantic_identity_refused(TZrBool missingSymbol,
        TZrBool missingCallable) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrSemanticIrFunction *semantic = &fixture->compiler.preSemanticIr;
    TZrSymbolId symbol = semantic->symbolId;
    TZrTypeId callable = semantic->callableTypeId;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 inputBefore = function_digest(&g_mutated);
    TZrUInt64 outputBefore = function_digest(&g_output), sourceBefore = source_digest(fixture);
    TZrUInt64 missingSourceBefore;
    TZrBool success, sourceUnchanged;
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, symbol);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, callable);
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: real Core graph before missing SemIR identity");
    if (missingSymbol) semantic->symbolId = 0u;
    if (missingCallable) semantic->callableTypeId = 0u;
    missingSourceBefore = source_digest(fixture);
    success = ZrParser_ExecIr_EliminateDeadSourcePlaces(semantic,
            fixture->compiler.semanticContext, &g_mutated, &g_output, &diagnostic);
    sourceUnchanged = (TZrBool)(missingSourceBefore == source_digest(fixture));
    /* Restore the still-live production source identity before any assertion
     * can longjmp. Missing provenance is unsupported, not positive evidence. */
    semantic->symbolId = symbol;
    semantic->callableTypeId = callable;
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, diagnostic.code);
    TEST_ASSERT_TRUE(sourceUnchanged);
    TEST_ASSERT_EQUAL_UINT64(inputBefore, function_digest(&g_mutated));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_missing_semantic_symbol(void) {
    assert_missing_semantic_identity_refused(ZR_TRUE, ZR_FALSE);
}
static void test_guard_missing_semantic_callable(void) {
    assert_missing_semantic_identity_refused(ZR_FALSE, ZR_TRUE);
}
static void test_guard_missing_semantic_identity(void) {
    assert_missing_semantic_identity_refused(ZR_TRUE, ZR_TRUE);
}

/* This identity mismatch is an INVALID graph; preserve Core's diagnostic. */
static void test_guard_empty_state_header_mismatch(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    TEST_ASSERT_NOT_NULL(g_mutated.stateMap);
    ++g_mutated.stateMap->generation;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

/* No checkpoint is invented. Core accepts retained logical pool storage even
 * with zero entries; deletion may not silently discard its real address ID. */
static void test_guard_owned_state_value_pool(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrStateMap *map = g_mutated.stateMap;
    TEST_ASSERT_NOT_NULL(map);
    TEST_ASSERT_NULL(map->valuePool);
    map->valuePool = (TZrExecIrValueId *)calloc(1u, sizeof(*map->valuePool));
    TEST_ASSERT_NOT_NULL(map->valuePool);
    map->valueCount = map->valueCapacity = 1u;
    map->valuePool[0] = g_mutated.resultPool[g_mutated.instructions[1].results.offset];
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: owned real-address state pool is VERIFY_ALL-valid");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_owned_deopt_reconstruction(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    TEST_ASSERT_NULL(g_mutated.deoptStates);
    TEST_ASSERT_NULL(g_mutated.deoptValues);
    g_mutated.deoptStates = (SZrExecIrDeoptState *)calloc(1u, sizeof(*g_mutated.deoptStates));
    TEST_ASSERT_NOT_NULL(g_mutated.deoptStates);
    g_mutated.deoptStateCount = g_mutated.deoptStateCapacity = 1u;
    g_mutated.deoptValues = (TZrExecIrValueId *)calloc(1u, sizeof(*g_mutated.deoptValues));
    TEST_ASSERT_NOT_NULL(g_mutated.deoptValues);
    g_mutated.deoptValueCount = g_mutated.deoptValueCapacity = 1u;
    g_mutated.deoptStates[0].id = g_mutated.instructions[1].sourceId;
    g_mutated.deoptStates[0].sourceId = g_mutated.instructions[1].sourceId;
    g_mutated.deoptStates[0].resumeId = g_mutated.instructions[1].sourceId;
    g_mutated.deoptStates[0].valueRange.count = 1u;
    g_mutated.deoptValues[0] = g_mutated.resultPool[g_mutated.instructions[1].results.offset];
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: real-address deopt reconstruction is VERIFY_ALL-valid");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_null_required_arguments(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 inputBefore = function_digest(&g_mutated);
    TZrUInt64 outputBefore = function_digest(&g_output), sourceBefore = source_digest(fixture);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(ZR_NULL,
            fixture->compiler.semanticContext, &g_mutated, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, ZR_NULL, &g_mutated, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            ZR_NULL, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            &g_mutated, ZR_NULL, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(inputBefore, function_digest(&g_mutated));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}
