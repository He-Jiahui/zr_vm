/* Focused COMPARE_BRANCH_INT projection cases.  This include is placed after
 * the shared ExecIR fixture helpers in test_ssa_generated_fusion.c. */

static void set_compare_branch_types(SZrExecIrFunction *function,
                                    TZrUInt32 compareMode) {
    /* The six-pattern fixture places Compare and ConditionalBranch at IDs 3
     * and 4.  Its typed shape is i64 x i64 -> BOOL. */
    function->values[2].typeToken = ZR_VALUE_TYPE_INT64;
    function->values[3].typeToken = ZR_VALUE_TYPE_INT64;
    function->values[14].typeToken = ZR_VALUE_TYPE_BOOL;
    function->instructions[2].typeToken = compareMode;
    function->instructions[3].typeToken = ZR_VALUE_TYPE_BOOL;
}

static const SZrExecBcFusionSideEntry *find_compare_branch_entry(
        const SZrExecBcFusionPlan *plan) {
    TZrUInt32 index;
    if (plan == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < plan->sideEntryCount; ++index) {
        if (plan->sideEntries[index].pattern ==
                ZR_EXEC_BC_FUSION_PATTERN_COMPARE_BRANCH_INT) {
            return &plan->sideEntries[index];
        }
    }
    return ZR_NULL;
}

static void test_compare_branch_int_preserves_modes_and_plan_hash(void) {
    /* ExecBC compare modes are EQ, LT, LE, GT, GE, and NE (0 through 5). */
    static const TZrUInt32 modes[] = {0u, 1u, 2u, 3u, 4u, 5u};
    SZrExecBcPatternOptions options;
    ZrParser_ExecBcPatternOptions_Init(&options);
    for (TZrUInt32 modeIndex = 0u;
         modeIndex < (TZrUInt32)(sizeof(modes) / sizeof(modes[0]));
         ++modeIndex) {
        SZrExecIrFunction function;
        SZrExecBcFusionPlan first;
        SZrExecBcFusionPlan second;
        SZrExecIrDiagnostic diagnostic;
        const SZrExecBcFusionSideEntry *entry;
        const SZrExecBcFusionSideEntry *secondEntry;
        TZrUInt32 entryIndex;
        TZrUInt64 validHash;

        make_six_pattern_function(&function);
        set_compare_branch_types(&function, modes[modeIndex]);
        ZrParser_ExecBcFusionPlan_Init(&first);
        ZrParser_ExecBcFusionPlan_Init(&second);
        assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &first,
                                                 &diagnostic));
        assert(first.fusedCount == 6u);
        entry = find_compare_branch_entry(&first);
        assert(entry != ZR_NULL);
        entryIndex = (TZrUInt32)(entry - first.sideEntries);
        assert(entry->comparisonMode == modes[modeIndex]);
        assert(ZrParser_ExecBcFusion_Validate(&first, &diagnostic));
        validHash = first.generatedHash;
        assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &second,
                                                 &diagnostic));
        secondEntry = find_compare_branch_entry(&second);
        assert(secondEntry != ZR_NULL);
        assert(second.generatedHash == first.generatedHash);
        assert(secondEntry->comparisonMode == modes[modeIndex]);

        /* The retained mode participates in plan integrity, and a
         * non-canonical selector stays invalid even if its hash is rebuilt. */
        first.sideEntries[entryIndex].comparisonMode = 6u;
        first.generatedHash = ZrParser_ExecBcFusion_Hash(&first);
        assert(!ZrParser_ExecBcFusion_Validate(&first, &diagnostic));
        first.sideEntries[entryIndex].comparisonMode = modes[modeIndex];
        first.generatedHash = validHash;
        assert(ZrParser_ExecBcFusion_Validate(&first, &diagnostic));

        ZrParser_ExecBcFusionPlan_Free(&second);
        ZrParser_ExecBcFusionPlan_Free(&first);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_compare_branch_int_rejects_noncanonical_inputs(void) {
    for (TZrUInt32 invalidCase = 0u; invalidCase < 3u; ++invalidCase) {
        SZrExecIrFunction function;
        SZrExecBcFusionPlan plan;
        SZrExecIrDiagnostic diagnostic;
        SZrExecBcPatternOptions options;
        TZrBool sawCompareTypeFallback = ZR_FALSE;

        make_six_pattern_function(&function);
        if (invalidCase == 0u) {
            function.values[2].typeToken = ZR_VALUE_TYPE_INT32;
        } else if (invalidCase == 1u) {
            function.values[14].typeToken = ZR_VALUE_TYPE_INT64;
        } else {
            /* Current Oracle evaluation treats unknown modes like EQ, but
             * only canonical selector values 0..5 may be projected. */
            function.instructions[2].typeToken = 6u;
        }
        ZrParser_ExecBcPatternOptions_Init(&options);
        ZrParser_ExecBcFusionPlan_Init(&plan);
        assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                                 &diagnostic));
        assert(find_compare_branch_entry(&plan) == ZR_NULL);
        assert(plan.fusedCount == 5u);
        for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
            if (plan.fallbacks[index].pattern ==
                    ZR_EXEC_BC_FUSION_PATTERN_COMPARE_BRANCH_INT &&
                plan.fallbacks[index].reason ==
                    ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH) {
                sawCompareTypeFallback = ZR_TRUE;
            }
        }
        assert(sawCompareTypeFallback);
        ZrParser_ExecBcFusionPlan_Free(&plan);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}
