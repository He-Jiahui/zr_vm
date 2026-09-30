#include <stdlib.h>

typedef struct ZrGcBudgetConstructorAllocatorContext {
    TZrUInt32 injectedCount;
    SZrGarbageCollector *injectedCollector;
    SZrGcBudget seededBudget;
} ZrGcBudgetConstructorAllocatorContext;

typedef struct ZrGcBudgetConstructorObservation {
    TZrBool configured;
    EZrGcBudgetStepStatus lastStatus;
    EZrGcBudgetPhase phase;
    EZrGcBudgetPauseReason pauseReason;
    TZrUInt64 cursor;
    TZrUInt64 workDone;
    TZrUInt64 elapsedUs;
    TZrInt64 debtBytes;
    TZrUInt64 overBudgetCount;
    TZrUInt64 compactDeferredCount;
    TZrBool pressure;
    TZrBool fallback;
    TZrBool snapshotConfigured;
    EZrGcBudgetStepStatus snapshotLastStatus;
    EZrGcBudgetPhase snapshotPhase;
    EZrGcBudgetPauseReason snapshotPauseReason;
    TZrUInt64 snapshotCursor;
    TZrUInt64 snapshotWorkDone;
    TZrUInt64 snapshotElapsedUs;
    TZrInt64 snapshotDebtBytes;
    TZrUInt64 snapshotOverBudgetCount;
    TZrUInt64 snapshotCompactDeferredCount;
    TZrBool snapshotPressure;
    TZrBool snapshotFallback;
} ZrGcBudgetConstructorObservation;

/* Write legal typed values into the exact manager allocation before the real
 * collector constructor receives it. This models allocator reuse without
 * reading indeterminate bytes or manufacturing invalid TZrBool values. */
static void gc_budget_constructor_seed_old_fields(
        ZrGcBudgetConstructorAllocatorContext *context,
        SZrGarbageCollector *collector) {
    collector->budget = context->seededBudget;
    collector->budgetConfigured = ZR_TRUE;
    collector->budgetLastStatus = ZR_GC_BUDGET_STEP_OVER_BUDGET;
    collector->budgetPhase = ZR_GC_BUDGET_PHASE_MARK_CONCURRENT;
    collector->budgetPauseReason = ZR_GC_BUDGET_PAUSE_BUDGET;
    collector->budgetCursor = 71u;
    collector->budgetWorkDone = 73u;
    collector->budgetElapsedUs = 79u;
    collector->budgetDebtBytes = 83;
    collector->budgetOverBudgetCount = 89u;
    collector->budgetCompactDeferredCount = 97u;
    collector->budgetPressure = ZR_TRUE;
    collector->budgetFallback = ZR_TRUE;

    collector->statsSnapshot.budgetConfigured = ZR_TRUE;
    collector->statsSnapshot.budgetLastStatus = ZR_GC_BUDGET_STEP_REJECTED;
    collector->statsSnapshot.budgetPhase = ZR_GC_BUDGET_PHASE_SWEEP;
    collector->statsSnapshot.budgetPauseReason = ZR_GC_BUDGET_PAUSE_OOM;
    collector->statsSnapshot.budgetCursor = 101u;
    collector->statsSnapshot.budgetWorkDone = 103u;
    collector->statsSnapshot.budgetElapsedUs = 107u;
    collector->statsSnapshot.budgetDebtBytes = 109;
    collector->statsSnapshot.budgetOverBudgetCount = 113u;
    collector->statsSnapshot.budgetCompactDeferredCount = 127u;
    collector->statsSnapshot.budgetPressure = ZR_TRUE;
    collector->statsSnapshot.budgetFallback = ZR_TRUE;
}

static TZrPtr gc_budget_constructor_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    ZrGcBudgetConstructorAllocatorContext *context =
            (ZrGcBudgetConstructorAllocatorContext *)userData;
    TZrPtr allocation;

    (void)originalSize;
    if (newSize == 0u) {
        free(pointer);
        return ZR_NULL;
    }

    allocation = pointer == ZR_NULL
                         ? malloc(newSize)
                         : realloc(pointer, newSize);
    if (allocation == ZR_NULL) {
        return ZR_NULL;
    }

    if (pointer == ZR_NULL && context != ZR_NULL &&
        context->injectedCount == 0u &&
        flag == (TZrInt64)ZR_MEMORY_NATIVE_TYPE_MANAGER &&
        newSize == sizeof(SZrGarbageCollector)) {
        SZrGarbageCollector *collector = (SZrGarbageCollector *)allocation;

        context->injectedCount++;
        context->injectedCollector = collector;
        gc_budget_constructor_seed_old_fields(context, collector);
    }
    return allocation;
}

static void gc_budget_constructor_capture_observation(
        const SZrGarbageCollector *collector,
        const SZrGarbageCollectorStatsSnapshot *snapshot,
        ZrGcBudgetConstructorObservation *observation) {
    observation->configured = collector->budgetConfigured;
    observation->lastStatus = collector->budgetLastStatus;
    observation->phase = collector->budgetPhase;
    observation->pauseReason = collector->budgetPauseReason;
    observation->cursor = collector->budgetCursor;
    observation->workDone = collector->budgetWorkDone;
    observation->elapsedUs = collector->budgetElapsedUs;
    observation->debtBytes = collector->budgetDebtBytes;
    observation->overBudgetCount = collector->budgetOverBudgetCount;
    observation->compactDeferredCount = collector->budgetCompactDeferredCount;
    observation->pressure = collector->budgetPressure;
    observation->fallback = collector->budgetFallback;
    observation->snapshotConfigured = snapshot->budgetConfigured;
    observation->snapshotLastStatus = snapshot->budgetLastStatus;
    observation->snapshotPhase = snapshot->budgetPhase;
    observation->snapshotPauseReason = snapshot->budgetPauseReason;
    observation->snapshotCursor = snapshot->budgetCursor;
    observation->snapshotWorkDone = snapshot->budgetWorkDone;
    observation->snapshotElapsedUs = snapshot->budgetElapsedUs;
    observation->snapshotDebtBytes = snapshot->budgetDebtBytes;
    observation->snapshotOverBudgetCount = snapshot->budgetOverBudgetCount;
    observation->snapshotCompactDeferredCount =
            snapshot->budgetCompactDeferredCount;
    observation->snapshotPressure = snapshot->budgetPressure;
    observation->snapshotFallback = snapshot->budgetFallback;
}

static void test_gc_budget_constructor_initializes_optional_state(void) {
    ZrGcBudgetConstructorAllocatorContext allocatorContext = {0};
    ZrGcBudgetConstructorObservation observed = {0};
    SZrCallbackGlobal callbacks = {0};
    SZrGarbageCollectorStatsSnapshot snapshot = {0};
    SZrGcBudget budgetOutput = {0};
    SZrGcBudgetStepResult globalStatsOutput = {0};
    SZrGcBudgetStepResult stateStatsOutput = {0};
    SZrGlobalState *global;
    SZrGarbageCollector *collector;
    SZrState *state;
    TZrBool globalCreated;
    TZrBool stateCreated = ZR_FALSE;
    TZrBool injectedActualCollector = ZR_FALSE;
    TZrBool getBudgetResult = ZR_FALSE;
    TZrBool getGlobalStatsResult = ZR_FALSE;
    TZrBool getStateStatsResult = ZR_FALSE;

    ZrCore_GcBudget_Init(&allocatorContext.seededBudget);
    allocatorContext.seededBudget.maxWorkUnits = 37u;
    ZrCore_GcBudget_Init(&budgetOutput);
    budgetOutput.maxWorkUnits = 41u;

    global = ZrCore_GlobalState_New(
            gc_budget_constructor_allocator,
            &allocatorContext,
            0x474342u,
            &callbacks);
    globalCreated = global != ZR_NULL;
    if (global != ZR_NULL) {
        state = global->mainThreadState;
        stateCreated = state != ZR_NULL;
        collector = global->garbageCollector;
        injectedActualCollector =
                collector != ZR_NULL &&
                allocatorContext.injectedCount == 1u &&
                allocatorContext.injectedCollector == collector;

        if (state != ZR_NULL) {
            ZrCore_GlobalState_InitRegistry(state, global);
        }

        /* Do not read the production API gate if the hook did not seed the
         * actual collector: that would turn a fixture miss into UB/false RED. */
        if (injectedActualCollector && state != ZR_NULL) {
            ZrCore_GarbageCollector_GetStatsSnapshot(global, &snapshot);
            gc_budget_constructor_capture_observation(
                    collector, &snapshot, &observed);

            getBudgetResult =
                    ZrCore_GarbageCollector_GetBudget(global, &budgetOutput);
            getGlobalStatsResult = ZrCore_GarbageCollector_GetBudgetStats(
                    global, &globalStatsOutput);
            getStateStatsResult = ZrCore_Gc_GetStats(state, &stateStatsOutput);
        }

        ZrCore_GlobalState_Free(global);
    }

    /* Assert only after the custom global and every allocator-owned block
     * have been released, so a RED assertion cannot strand test state. */
    TEST_ASSERT_TRUE(globalCreated);
    TEST_ASSERT_TRUE(stateCreated);
    TEST_ASSERT_EQUAL_UINT32(1u, allocatorContext.injectedCount);
    TEST_ASSERT_TRUE(injectedActualCollector);
    TEST_ASSERT_FALSE(getBudgetResult);
    TEST_ASSERT_FALSE(getGlobalStatsResult);
    TEST_ASSERT_FALSE(getStateStatsResult);

    TEST_ASSERT_FALSE(observed.configured);
    TEST_ASSERT_EQUAL_INT(
            ZR_GC_BUDGET_STEP_ACCEPTED, observed.lastStatus);
    TEST_ASSERT_EQUAL_INT(ZR_GC_BUDGET_PHASE_IDLE, observed.phase);
    TEST_ASSERT_EQUAL_INT(ZR_GC_BUDGET_PAUSE_NONE, observed.pauseReason);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.cursor);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.workDone);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.elapsedUs);
    TEST_ASSERT_EQUAL_INT64(0, observed.debtBytes);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.overBudgetCount);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.compactDeferredCount);
    TEST_ASSERT_FALSE(observed.pressure);
    TEST_ASSERT_FALSE(observed.fallback);

    TEST_ASSERT_FALSE(observed.snapshotConfigured);
    TEST_ASSERT_EQUAL_INT(
            ZR_GC_BUDGET_STEP_ACCEPTED, observed.snapshotLastStatus);
    TEST_ASSERT_EQUAL_INT(ZR_GC_BUDGET_PHASE_IDLE, observed.snapshotPhase);
    TEST_ASSERT_EQUAL_INT(
            ZR_GC_BUDGET_PAUSE_NONE, observed.snapshotPauseReason);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.snapshotCursor);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.snapshotWorkDone);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.snapshotElapsedUs);
    TEST_ASSERT_EQUAL_INT64(0, observed.snapshotDebtBytes);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.snapshotOverBudgetCount);
    TEST_ASSERT_EQUAL_UINT64(0u, observed.snapshotCompactDeferredCount);
    TEST_ASSERT_FALSE(observed.snapshotPressure);
    TEST_ASSERT_FALSE(observed.snapshotFallback);
}
