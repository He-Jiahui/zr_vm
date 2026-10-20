#ifndef ZR_TEST_COMPTIME_FRAME_TRANSACTION_CASES_H
#define ZR_TEST_COMPTIME_FRAME_TRANSACTION_CASES_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/exception.h"

typedef enum EFrameTxnMode {
    FRAME_TXN_OOM,
    FRAME_TXN_GC,
    FRAME_TXN_EXACT,
    FRAME_TXN_INTERLEAVE_REJECT,
    FRAME_TXN_INTERLEAVE_ACCEPT
} EFrameTxnMode;

enum {
    FRAME_TXN_INITIAL = 4,
    FRAME_TXN_GROWN = 9,
    FRAME_TXN_WARM_ROOTS = 24
};

typedef struct SFrameTxnCase {
    SZrCompileTimeFrame frame;
    SZrGcRootHandle warmRoots[FRAME_TXN_WARM_ROOTS];
    union {
        SZrCompileTimeBinding bindings[FRAME_TXN_INITIAL];
        SZrGcRootHandle roots[FRAME_TXN_INITIAL];
    } savedRows;
    FZrAllocator allocator;
    TZrPtr userData;
    EFrameTxnMode mode;
    TZrBool bindings, live, prepared, hooked, armed, busy;
    TZrBool success, independent, freeShapes, interleaved, candidateAllocated;
    TZrBool unchanged, rowsValid, exactValid, reuseValid;
    TZrSize warmCount, elementSize, requestBytes, candidateBytes;
    TZrSize requests, oldFrees, candidateFrees, callbackRoots;
    TZrBytePtr oldHead, candidate;
    SZrString *name;
    SZrTypeValue value;
    SZrParserComptimeBudgetUsage initialUsage, beforeUsage, afterUsage;
    TZrSize baselineRoots, beforeRoots, duringRoots, afterRoots;
    TZrUInt32 baselineDepth, afterDepth;
    TZrUInt64 fullBefore, fullAfter;
} SFrameTxnCase;

static SFrameTxnCase g_frameTxn;

static const TZrChar *g_frameTxnNames[FRAME_TXN_GROWN] = {
        "txnBinding0", "txnBinding1", "txnBinding2",
        "txnBinding3", "txnBinding4", "txnBinding5",
        "txnBinding6", "txnBinding7", "txnBinding8"};
static const TZrChar *g_frameTxnValues[FRAME_TXN_GROWN] = {
        "txn value zero", "txn value one", "txn value two",
        "txn value three", "txn value four", "txn value five",
        "txn value six", "txn value seven", "txn value eight"};
static const SZrAotGcRootSlot g_frameTxnSlots[] = {
        {0U, 0U, 0U, 0U, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0U, 0U},
        {0U, sizeof(SZrRawObject *), 0U, 0U,
         ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0U, 0U}};
static const SZrAotGcRootMap g_frameTxnMap = {2U, g_frameTxnSlots};

static TZrBool frame_txn_usage_equal(
        const SZrParserComptimeBudgetUsage *a,
        const SZrParserComptimeBudgetUsage *b) {
    return a->fuel == b->fuel && a->callDepth == b->callDepth &&
           a->heapBytes == b->heapBytes &&
           a->aggregateCount == b->aggregateCount &&
           a->generatedDeclarationCount == b->generatedDeclarationCount &&
           a->diagnosticCount == b->diagnosticCount &&
           a->tokenCount == b->tokenCount && a->nodeCount == b->nodeCount &&
           a->matchingCount == b->matchingCount &&
           a->expansionCount == b->expansionCount;
}

static SZrArray *frame_txn_array(void) {
    return g_frameTxn.bindings ? &g_frameTxn.frame.bindings
                               : &g_frameTxn.frame.temporaryRoots;
}

static TZrUInt64 frame_txn_full_count(void) {
    SZrGarbageCollectorStatsSnapshot snapshot;
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &snapshot);
    return snapshot.fullCollectionCount;
}

static void frame_txn_restore_allocator(void) {
    if (g_frameTxn.hooked) {
        g_state->global->allocator = g_frameTxn.allocator;
        g_state->global->userAllocationArguments = g_frameTxn.userData;
        g_frameTxn.hooked = ZR_FALSE;
    }
}

static TZrPtr frame_txn_allocator(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    SFrameTxnCase *p = (SFrameTxnCase *)userData;
    TZrPtr result;
    TZrBool selected = p->armed && !p->busy &&
            flag == ZR_MEMORY_NATIVE_TYPE_ARRAY && newSize == p->requestBytes &&
            ((pointer == ZR_NULL && originalSize == 0U) ||
             (pointer == p->oldHead && p->oldHead != ZR_NULL &&
              originalSize == FRAME_TXN_INITIAL * p->elementSize));

    if (flag == ZR_MEMORY_NATIVE_TYPE_ARRAY && newSize == 0U) {
        if (pointer != ZR_NULL && pointer == p->oldHead) {
            p->oldFrees++;
            p->freeShapes = p->freeShapes &&
                    originalSize == FRAME_TXN_INITIAL * p->elementSize;
        }
        if (pointer != ZR_NULL && pointer == p->candidate) {
            p->candidateFrees++;
            p->freeShapes = p->freeShapes && originalSize == p->candidateBytes;
        }
    }
    if (!selected) {
        return p->allocator(p->userData, pointer, originalSize, newSize, flag);
    }
    p->requests++;
    p->independent = p->independent &&
            pointer == ZR_NULL && originalSize == 0U;
    p->callbackRoots = ZrCore_GcDomain_GetRootCount(g_state);
    p->armed = ZR_FALSE;
    p->busy = ZR_TRUE;
    if (p->mode == FRAME_TXN_OOM) {
        p->busy = ZR_FALSE;
        return ZR_NULL;
    }
    if (p->mode == FRAME_TXN_GC) {
        ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
        p->fullAfter = frame_txn_full_count();
    } else if (p->mode == FRAME_TXN_INTERLEAVE_REJECT ||
               p->mode == FRAME_TXN_INTERLEAVE_ACCEPT) {
        p->interleaved = ZrParser_ComptimeBudget_TryConsume(
                &g_compiler.comptimeBudget,
                ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, 1U);
    }
    result = p->allocator(p->userData, pointer, originalSize, newSize, flag);
    p->candidate = (TZrBytePtr)result;
    p->candidateBytes = newSize;
    p->busy = ZR_FALSE;
    return result;
}

static void frame_txn_install_allocator(TZrSize requestBytes) {
    g_frameTxn.requestBytes = requestBytes;
    g_frameTxn.armed = ZR_TRUE;
    g_state->global->allocator = frame_txn_allocator;
    g_state->global->userAllocationArguments = &g_frameTxn;
    g_frameTxn.hooked = ZR_TRUE;
}

static void frame_txn_release_warm_roots(void) {
    while (g_frameTxn.warmCount != 0U) {
        g_frameTxn.warmCount--;
        ZrCore_GcRootHandle_Release(
                g_state, &g_frameTxn.warmRoots[g_frameTxn.warmCount]);
    }
}

/* Reserve real, reusable root slots before arming the candidate probe. */
static TZrBool frame_txn_warm_roots(void) {
    SZrRawObject *objects[2] = {ZR_NULL, ZR_NULL};
    SZrAotGcRootFrame rootFrame;
    SZrString *anchor;
    TZrBool success = ZR_TRUE;
    if (!ZrCore_Gc_AotRootFramePush(
                g_state, &rootFrame, (TZrStackValuePointer)objects,
                &g_frameTxnMap)) {
        return ZR_FALSE;
    }
    anchor = ZrCore_String_CreateFromNative(g_state, "txn root-table warmup");
    if (anchor == ZR_NULL) {
        success = ZR_FALSE;
    } else {
        objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(anchor);
    }
    for (TZrSize index = 0U;
         success && index < FRAME_TXN_WARM_ROOTS; index++) {
        SZrGcRootHandle *root = &g_frameTxn.warmRoots[index];
        if (!ZrCore_GcRootHandle_Create(g_state, objects[0], root)) {
            success = ZR_FALSE;
            break;
        }
        g_frameTxn.warmCount++;
        if (!ZrCore_GcRootHandle_Update(g_state, root, objects[0])) {
            success = ZR_FALSE;
        }
    }
    frame_txn_release_warm_roots();
    if (!ZrCore_Gc_AotRootFramePop(g_state, &rootFrame)) {
        success = ZR_FALSE;
    }
    return success &&
           ZrCore_GcDomain_GetRootCount(g_state) == g_frameTxn.baselineRoots;
}

/* The construction map is popped before the operation under test. */
static TZrBool frame_txn_make_input(TZrSize index) {
    SZrRawObject *objects[2] = {ZR_NULL, ZR_NULL};
    SZrAotGcRootFrame rootFrame;
    SZrString *created;
    TZrBool success = ZR_TRUE;
    if (!ZrCore_Gc_AotRootFramePush(
                g_state, &rootFrame, (TZrStackValuePointer)objects,
                &g_frameTxnMap)) {
        return ZR_FALSE;
    }
    if (g_frameTxn.bindings) {
        created = ZrCore_String_CreateFromNative(
                g_state, (TZrNativeString)g_frameTxnNames[index]);
        success = created != ZR_NULL;
        if (success) {
            objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(created);
        }
    }
    if (success) {
        created = ZrCore_String_CreateFromNative(
                g_state, (TZrNativeString)g_frameTxnValues[index]);
        success = created != ZR_NULL;
        if (success) {
            objects[1] = ZR_CAST_RAW_OBJECT_AS_SUPER(created);
        }
    }
    if (success) {
        g_frameTxn.name = (SZrString *)objects[0];
        ZrCore_Value_InitAsRawObject(g_state, &g_frameTxn.value, objects[1]);
        g_frameTxn.value.type = ZR_VALUE_TYPE_STRING;
    }
    if (!ZrCore_Gc_AotRootFramePop(g_state, &rootFrame)) {
        success = ZR_FALSE;
    }
    return success;
}

static TZrBool frame_txn_append(void) {
    return g_frameTxn.bindings
            ? ct_frame_set(&g_compiler, &g_frameTxn.frame,
                           g_frameTxn.name, &g_frameTxn.value)
            : ct_frame_root_value(&g_compiler, &g_frameTxn.frame,
                                  &g_frameTxn.value);
}

static TZrBool frame_txn_resolves(
        const SZrGcRootHandle *root, const TZrChar *text,
        SZrRawObject **resolved) {
    *resolved = ZR_NULL;
    return ZrCore_GcRootHandle_Resolve(g_state, root, resolved) &&
           *resolved != ZR_NULL &&
           strcmp(ZrCore_String_GetNativeString((SZrString *)*resolved), text) == 0;
}

static TZrBool frame_txn_verify_rows(
        TZrSize count, TZrBool checkNewCachedValue) {
    SZrArray *array = frame_txn_array();
    if (array->length != count || array->head == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < count; index++) {
        SZrRawObject *name = ZR_NULL;
        SZrRawObject *value = ZR_NULL;
        if (g_frameTxn.bindings) {
            SZrCompileTimeBinding *binding =
                    (SZrCompileTimeBinding *)ZrCore_Array_Get(array, index);
            SZrTypeValue result;
            if (binding == ZR_NULL || !binding->hasNameRoot ||
                !binding->hasValueRoot || binding->isConst ||
                !frame_txn_resolves(&binding->nameRoot,
                                   g_frameTxnNames[index], &name) ||
                !frame_txn_resolves(&binding->valueRoot,
                                   g_frameTxnValues[index], &value)) {
                return ZR_FALSE;
            }
            if (checkNewCachedValue && index + 1U == count &&
                (binding->name != (SZrString *)name ||
                 binding->value.value.object != value)) {
                return ZR_FALSE;
            }
            if (!ct_frame_get(&g_frameTxn.frame, (SZrString *)name, &result) ||
                result.type != ZR_VALUE_TYPE_STRING ||
                result.value.object != value) {
                return ZR_FALSE;
            }
        } else {
            SZrGcRootHandle *root =
                    (SZrGcRootHandle *)ZrCore_Array_Get(array, index);
            if (root == ZR_NULL ||
                !frame_txn_resolves(root, g_frameTxnValues[index], &value) ||
                (checkNewCachedValue && index + 1U == count &&
                 g_frameTxn.value.value.object != value)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static void frame_txn_exact_body(void) {
    SFrameTxnCase *p = &g_frameTxn;
    SZrArray *array = frame_txn_array();
    SZrParserComptimeBudgetUsage expected = p->initialUsage;
    p->exactValid = ZR_TRUE;
    g_compiler.comptimeBudget.limits.heapBytes =
            expected.heapBytes +
            (FRAME_TXN_INITIAL + FRAME_TXN_GROWN) * p->elementSize;
    g_compiler.comptimeBudget.limits.aggregateCount =
            expected.aggregateCount + FRAME_TXN_GROWN;
    p->prepared = ZR_TRUE;
    for (TZrSize index = 0U; index < FRAME_TXN_GROWN; index++) {
        TZrBytePtr previousHead = array->head;
        TZrSize capacity = index < FRAME_TXN_INITIAL
                ? FRAME_TXN_INITIAL : FRAME_TXN_GROWN;
        if (!frame_txn_make_input(index)) {
            p->prepared = ZR_FALSE;
            return;
        }
        if (index == FRAME_TXN_INITIAL) {
            p->oldHead = array->head;
        }
        frame_txn_install_allocator(capacity * p->elementSize);
        p->success = frame_txn_append();
        frame_txn_restore_allocator();
        if (!p->success || array->length != index + 1U) {
            p->exactValid = ZR_FALSE;
            return;
        }
        expected.heapBytes = p->initialUsage.heapBytes +
                (index < FRAME_TXN_INITIAL ? FRAME_TXN_INITIAL :
                 FRAME_TXN_INITIAL + FRAME_TXN_GROWN) * p->elementSize;
        expected.aggregateCount = p->initialUsage.aggregateCount + index + 1U;
        p->exactValid = p->exactValid && array->capacity == capacity &&
                frame_txn_usage_equal(&expected, &g_compiler.comptimeBudget.usage) &&
                ((index == 0U || index == FRAME_TXN_INITIAL)
                 ? array->head != previousHead : array->head == previousHead) &&
                frame_txn_verify_rows(index + 1U, ZR_FALSE);
    }
    p->beforeUsage = g_compiler.comptimeBudget.usage;
    p->beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    if (p->bindings) {
        SZrCompileTimeBinding *last = (SZrCompileTimeBinding *)ZrCore_Array_Get(
                array, FRAME_TXN_GROWN - 1U);
        SZrRawObject *name = ZR_NULL;
        SZrRawObject *value = ZR_NULL;
        if (!frame_txn_resolves(&last->nameRoot,
                               g_frameTxnNames[FRAME_TXN_GROWN - 1U], &name) ||
            !frame_txn_resolves(&last->valueRoot,
                               g_frameTxnValues[FRAME_TXN_GROWN - 1U], &value)) {
            p->rowsValid = ZR_FALSE;
            return;
        }
        p->name = (SZrString *)name;
        ZrCore_Value_InitAsRawObject(g_state, &p->value, value);
        p->value.type = ZR_VALUE_TYPE_STRING;
    }
    {
        TZrBytePtr previousHead = array->head;
        frame_txn_install_allocator(FRAME_TXN_GROWN * p->elementSize);
        p->reuseValid = frame_txn_append();
        frame_txn_restore_allocator();
        p->reuseValid = p->reuseValid && array->head == previousHead &&
                array->length == FRAME_TXN_GROWN &&
                array->capacity == FRAME_TXN_GROWN &&
                frame_txn_usage_equal(&p->beforeUsage,
                                     &g_compiler.comptimeBudget.usage) &&
                ZrCore_GcDomain_GetRootCount(g_state) == p->beforeRoots;
    }
    p->rowsValid = frame_txn_verify_rows(FRAME_TXN_GROWN, ZR_FALSE);
}

static void frame_txn_body(SZrState *state, TZrPtr arguments) {
    SFrameTxnCase *p = (SFrameTxnCase *)arguments;
    SZrArray *array;
    (void)state;
    if (ZR_PARSER_INITIAL_CAPACITY_TINY != FRAME_TXN_INITIAL ||
        ZR_ARRAY_INCREASEMENT_MULTIPLIER_PERCENT != 200U ||
        !frame_txn_warm_roots()) {
        return;
    }
    p->live = ZR_TRUE;
    ct_frame_init(&g_compiler, &p->frame, ZR_NULL);
    if (p->mode == FRAME_TXN_EXACT) {
        frame_txn_exact_body();
        return;
    }
    for (TZrSize index = 0U; index < FRAME_TXN_INITIAL; index++) {
        if (!frame_txn_make_input(index) || !frame_txn_append()) {
            return;
        }
    }
    if (!frame_txn_make_input(FRAME_TXN_INITIAL) ||
        !frame_txn_verify_rows(FRAME_TXN_INITIAL, ZR_FALSE)) {
        return;
    }
    array = frame_txn_array();
    if (array->capacity != FRAME_TXN_INITIAL) {
        return;
    }
    p->oldHead = array->head;
    memcpy(&p->savedRows, array->head, FRAME_TXN_INITIAL * p->elementSize);
    p->beforeUsage = g_compiler.comptimeBudget.usage;
    p->beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    g_compiler.comptimeBudget.limits.heapBytes = p->beforeUsage.heapBytes +
            FRAME_TXN_GROWN * p->elementSize +
            (p->mode == FRAME_TXN_INTERLEAVE_ACCEPT ? 1U : 0U);
    g_compiler.comptimeBudget.limits.aggregateCount =
            p->beforeUsage.aggregateCount + 1U;
    p->fullBefore = frame_txn_full_count();
    p->prepared = ZR_TRUE;
    frame_txn_install_allocator(FRAME_TXN_GROWN * p->elementSize);
    p->success = frame_txn_append();
    frame_txn_restore_allocator();
    p->unchanged = array->head == p->oldHead &&
            array->length == FRAME_TXN_INITIAL &&
            array->capacity == FRAME_TXN_INITIAL &&
            memcmp(array->head, &p->savedRows,
                   FRAME_TXN_INITIAL * p->elementSize) == 0;
    p->rowsValid = frame_txn_verify_rows(
            p->success ? FRAME_TXN_INITIAL + 1U : FRAME_TXN_INITIAL,
            p->mode == FRAME_TXN_GC && p->success);
}

static void run_frame_txn_case(TZrBool bindings, EFrameTxnMode mode) {
    SFrameTxnCase *p = &g_frameTxn;
    SZrParserComptimeBudgetUsage expected;
    EZrThreadStatus status;
    TZrBool expectedSuccess = mode != FRAME_TXN_OOM &&
            mode != FRAME_TXN_INTERLEAVE_REJECT;
    TZrSize rootsPerEntry = bindings ? 2U : 1U;
    memset(p, 0, sizeof(*p));
    p->bindings = bindings;
    p->mode = mode;
    p->allocator = g_state->global->allocator;
    p->userData = g_state->global->userAllocationArguments;
    p->elementSize = bindings
            ? sizeof(SZrCompileTimeBinding) : sizeof(SZrGcRootHandle);
    p->independent = ZR_TRUE;
    p->freeShapes = ZR_TRUE;
    p->baselineRoots = ZrCore_GcDomain_GetRootCount(g_state);
    p->baselineDepth = ZrCore_Gc_AotRootFrameDepth(g_state);
    p->initialUsage = g_compiler.comptimeBudget.usage;
    status = ZrCore_Exception_TryRun(g_state, frame_txn_body, p);
    frame_txn_restore_allocator();
    frame_txn_release_warm_roots();
    p->afterUsage = g_compiler.comptimeBudget.usage;
    p->candidateAllocated = p->candidate != ZR_NULL;
    p->duringRoots = ZrCore_GcDomain_GetRootCount(g_state);
    p->afterDepth = ZrCore_Gc_AotRootFrameDepth(g_state);
    if (p->live) {
        ct_frame_free(&g_compiler, &p->frame);
        p->live = ZR_FALSE;
    }
    p->afterRoots = ZrCore_GcDomain_GetRootCount(g_state);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_TRUE_MESSAGE(p->prepared, "Real frame transaction fixture failed");
    TEST_ASSERT_TRUE(p->independent);
    TEST_ASSERT_TRUE(p->freeShapes);
    TEST_ASSERT_TRUE(p->rowsValid);
    TEST_ASSERT_EQUAL_UINT64(p->baselineRoots, p->afterRoots);
    TEST_ASSERT_EQUAL_UINT32(p->baselineDepth, p->afterDepth);
    TEST_ASSERT_EQUAL_UINT32(p->baselineDepth,
                            ZrCore_Gc_AotRootFrameDepth(g_state));
    TEST_ASSERT_TRUE(frame_txn_usage_equal(
            &p->afterUsage, &g_compiler.comptimeBudget.usage));
    if (mode == FRAME_TXN_EXACT) {
        expected = p->initialUsage;
        expected.heapBytes +=
                (FRAME_TXN_INITIAL + FRAME_TXN_GROWN) * p->elementSize;
        expected.aggregateCount += FRAME_TXN_GROWN;
        TEST_ASSERT_TRUE(p->success);
        TEST_ASSERT_TRUE(p->exactValid);
        TEST_ASSERT_TRUE(p->reuseValid);
        TEST_ASSERT_EQUAL_UINT64(2U, p->requests);
        TEST_ASSERT_EQUAL_UINT64(1U, p->oldFrees);
        TEST_ASSERT_EQUAL_UINT64(0U, p->candidateFrees);
        TEST_ASSERT_EQUAL_UINT64(
                p->baselineRoots + rootsPerEntry * FRAME_TXN_GROWN,
                p->duringRoots);
    } else {
        expected = p->beforeUsage;
        if (mode == FRAME_TXN_INTERLEAVE_REJECT ||
            mode == FRAME_TXN_INTERLEAVE_ACCEPT) {
            expected.heapBytes++;
            TEST_ASSERT_TRUE(p->interleaved);
        }
        if (expectedSuccess) {
            expected.heapBytes += FRAME_TXN_GROWN * p->elementSize;
            expected.aggregateCount++;
        }
        TEST_ASSERT_EQUAL_INT(expectedSuccess, p->success);
        TEST_ASSERT_EQUAL_UINT64(1U, p->requests);
        TEST_ASSERT_EQUAL_UINT64(
                p->beforeRoots + rootsPerEntry, p->callbackRoots);
        TEST_ASSERT_EQUAL_UINT64(
                p->beforeRoots + (expectedSuccess ? rootsPerEntry : 0U),
                p->duringRoots);
        TEST_ASSERT_EQUAL_UINT64(expectedSuccess ? 1U : 0U, p->oldFrees);
        TEST_ASSERT_EQUAL_UINT64(
                mode == FRAME_TXN_INTERLEAVE_REJECT ? 1U : 0U,
                p->candidateFrees);
        if (!expectedSuccess) {
            TEST_ASSERT_TRUE(p->unchanged);
            TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
            TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        }
        if (mode == FRAME_TXN_OOM) {
            TEST_ASSERT_FALSE(p->candidateAllocated);
            TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage,
                                       "Cannot allocate compile-time frame storage"));
        } else {
            TEST_ASSERT_TRUE(p->candidateAllocated);
        }
        if (mode == FRAME_TXN_GC) {
            TEST_ASSERT_TRUE_MESSAGE(
                    p->fullAfter > p->fullBefore,
                    "Candidate allocator did not complete a real full GC");
        }
    }
    TEST_ASSERT_TRUE(frame_txn_usage_equal(&expected, &p->afterUsage));
    if (mode == FRAME_TXN_INTERLEAVE_REJECT) {
        TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                              g_compiler.comptimeBudget.exceededResource);
        TEST_ASSERT_EQUAL_UINT64(
                p->beforeUsage.heapBytes + FRAME_TXN_GROWN * p->elementSize,
                g_compiler.comptimeBudget.exceededLimit);
        TEST_ASSERT_EQUAL_UINT64(
                p->beforeUsage.heapBytes +
                FRAME_TXN_GROWN * p->elementSize + 1U,
                g_compiler.comptimeBudget.requestedUsage);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_NONE,
                              g_compiler.comptimeBudget.exceededResource);
        if (mode != FRAME_TXN_OOM) {
            TEST_ASSERT_FALSE(g_compiler.hasError);
            TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
        }
    }
}

static void test_binding_candidate_oom_is_atomic(void) {
    run_frame_txn_case(ZR_TRUE, FRAME_TXN_OOM);
}
static void test_root_candidate_oom_is_atomic(void) {
    run_frame_txn_case(ZR_FALSE, FRAME_TXN_OOM);
}
static void test_binding_candidate_gc_preserves_inputs(void) {
    run_frame_txn_case(ZR_TRUE, FRAME_TXN_GC);
}
static void test_root_candidate_gc_preserves_inputs(void) {
    run_frame_txn_case(ZR_FALSE, FRAME_TXN_GC);
}
static void test_binding_capacity_growth_and_reuse_are_exact(void) {
    run_frame_txn_case(ZR_TRUE, FRAME_TXN_EXACT);
}
static void test_root_capacity_growth_and_reuse_are_exact(void) {
    run_frame_txn_case(ZR_FALSE, FRAME_TXN_EXACT);
}
static void test_binding_commit_rejects_live_heap_interleave(void) {
    run_frame_txn_case(ZR_TRUE, FRAME_TXN_INTERLEAVE_REJECT);
}
static void test_root_commit_rejects_live_heap_interleave(void) {
    run_frame_txn_case(ZR_FALSE, FRAME_TXN_INTERLEAVE_REJECT);
}
static void test_binding_commit_preserves_live_heap_interleave(void) {
    run_frame_txn_case(ZR_TRUE, FRAME_TXN_INTERLEAVE_ACCEPT);
}
static void test_root_commit_preserves_live_heap_interleave(void) {
    run_frame_txn_case(ZR_FALSE, FRAME_TXN_INTERLEAVE_ACCEPT);
}

#endif
