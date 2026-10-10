#include <stdio.h>
#include <string.h>
#include "ssa_source_finalized_module_fixture.h"
#include "unity.h"
SZrExecIrDiagnostic Fm_Error(EZrExecutionDiagnosticCode code) {
    SZrExecIrDiagnostic d = {0};
    d.code = code;
    d.functionToken = g_fm.original.functions[0].functionToken;
    return d;
}
void Fm_Diagnostic(const SZrExecIrDiagnostic *e, const SZrExecIrDiagnostic *a) { ZrTests_ExecOwner_Diagnostic(e, a); }
static void bytes(SFmObservation *o, const void *p, size_t n) {
    if (n > FM_BYTES - o->length || (n && !p)) {
        o->error = 1;
        return;
    }
    if (n)
        memcpy(o->bytes + o->length, p, n);
    o->length += n;
}
static TZrBool pool(const void *p, size_t count, size_t capacity, size_t width, size_t alignment) {
    if (count > capacity || capacity > SIZE_MAX / width || (!p != !capacity))
        return ZR_FALSE;
    if (!capacity)
        return ZR_TRUE;
    size_t n = capacity * width;
    uintptr_t start = (uintptr_t) p;
    return start % alignment == 0 && n <= FM_BYTES && start <= UINTPTR_MAX - n;
}
static TZrBool capture(const SZrExecIrModule *m, SZrOwnerContents *contents) {
    /* Numeric finite-profile proof covers every loop/header followed by the
     * unchanged generic writer. Its per-word assertion cannot fail after this
     * proof; observation itself has no failing Unity assertion or allocation. */
    if (!m || (uintptr_t) m % _Alignof(SZrExecIrModule) || m->constantCount != 1 || m->layoutCount != 1 ||
        m->sourceMapCount || m->functionCount != 1)
        return ZR_FALSE;
#define POOL(obj, field, count, capacity)                                                                              \
    pool((obj)->field, (obj)->count, (obj)->capacity, sizeof(*(obj)->field), sizeof(void *))
    if (!POOL(m, constants, constantCount, constantCapacity) || !POOL(m, layouts, layoutCount, layoutCapacity) ||
        !POOL(m, sourceMaps, sourceMapCount, sourceMapCapacity) || !POOL(m, functions, functionCount, functionCapacity))
        return ZR_FALSE;
    const SZrExecIrFunction *f = &m->functions[0];
    if (f->valueCount != 1 || f->instructionCount != 3 || f->blockCount != 1 || f->operandCount != 1 ||
        f->resultCount != 1 || f->sourceMapCount != 3 || f->gcMap || f->gcMapCount || f->gcMapCapacity)
        return ZR_FALSE;
    if (!POOL(f, values, valueCount, valueCapacity) || !POOL(f, instructions, instructionCount, instructionCapacity) ||
        !POOL(f, blocks, blockCount, blockCapacity) || !POOL(f, operandPool, operandCount, operandCapacity) ||
        !POOL(f, resultPool, resultCount, resultCapacity) || !POOL(f, sourceMaps, sourceMapCount, sourceMapCapacity))
        return ZR_FALSE;
#define EMPTY(field, count, capacity)                                                                                  \
    if (f->field || f->count || f->capacity)                                                                           \
    return ZR_FALSE
    EMPTY(memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    EMPTY(phiPool, phiCount, phiCapacity);
    EMPTY(phiIncoming, phiIncomingCount, phiIncomingCapacity);
    EMPTY(predecessors, predecessorCount, predecessorCapacity);
    EMPTY(successors, successorCount, successorCapacity);
    EMPTY(gcRoots, gcRootCount, gcRootCapacity);
    EMPTY(deoptStates, deoptStateCount, deoptStateCapacity);
    EMPTY(deoptValues, deoptValueCount, deoptValueCapacity);
    EMPTY(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    EMPTY(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    EMPTY(bindingRows, bindingRowCount, bindingRowCapacity);
#undef EMPTY
    if (!f->frameLayout || (uintptr_t) f->frameLayout % _Alignof(SZrExecIrFrameLayout))
        return ZR_FALSE;
    if (f->frameLayout->slotCount != 1 || !POOL(f->frameLayout, slots, slotCount, slotCapacity))
        return ZR_FALSE;
    if (f->stateMap) {
        const SZrExecIrStateMap *s = f->stateMap;
        if ((uintptr_t) s % _Alignof(SZrExecIrStateMap) || s->entries || s->entryCount || s->entryCapacity ||
            s->valuePool || s->valueCount || s->valueCapacity || s->rootPool || s->rootCount || s->rootCapacity ||
            s->ownerStatePool || s->ownerStateCount || s->ownerStateCapacity)
            return ZR_FALSE;
    }
#undef POOL
    TZrUInt64 words = 19u + (TZrUInt64) 3 * m->constantCount + (TZrUInt64) 5 * m->layoutCount +
                      (TZrUInt64) 8 * m->sourceMapCount + 36u + (TZrUInt64) 6 * f->valueCount +
                      (TZrUInt64) 22 * f->instructionCount +
                      (TZrUInt64) (15u + 3u * ZR_EXEC_IR_MEMORY_CLASS_COUNT) * f->blockCount + f->operandCount +
                      f->resultCount + (TZrUInt64) 8 * f->sourceMapCount + 3u;
    if (f->stateMap)
        words += 7u;
    if (f->frameLayout)
        words += 10u + (TZrUInt64) 6 * f->frameLayout->slotCount;
    if (words > ZR_OWNER_WORDS)
        return ZR_FALSE;
    ZrTests_ExecOwner_Capture(m, contents);
    return contents->count == words;
}
static void function(SFmObservation *o, const SZrExecIrFunction *visible, const SZrExecIrFunction *owner) {
    bytes(o, visible, sizeof(*visible));
#define POOL(field, capacity) bytes(o, owner->field, (size_t) owner->capacity * sizeof(*owner->field))
    POOL(values, valueCapacity);
    POOL(instructions, instructionCapacity);
    POOL(blocks, blockCapacity);
    POOL(operandPool, operandCapacity);
    POOL(resultPool, resultCapacity);
    POOL(memoryTokenPool, memoryTokenCapacity);
    POOL(phiPool, phiCapacity);
    POOL(phiIncoming, phiIncomingCapacity);
    POOL(predecessors, predecessorCapacity);
    POOL(successors, successorCapacity);
    POOL(gcRoots, gcRootCapacity);
    POOL(deoptStates, deoptStateCapacity);
    POOL(deoptValues, deoptValueCapacity);
    POOL(deoptAggregates, deoptAggregateCapacity);
    POOL(deoptAggregateFields, deoptAggregateFieldCapacity);
    POOL(sourceMaps, sourceMapCapacity);
    POOL(bindingRows, bindingRowCapacity);
#undef POOL
    if (owner->frameLayout) {
        bytes(o, owner->frameLayout, sizeof(*owner->frameLayout));
        bytes(o, g_fmFrameOwner.slots, (size_t) g_fmFrameOwner.slotCapacity * sizeof(*g_fmFrameOwner.slots));
    }
    if (owner->stateMap)
        bytes(o, owner->stateMap, sizeof(*owner->stateMap));
}
void Fm_Observe(SFmObservation *o) {
    SFmFixture *f = &g_fm;
    memset(o, 0, sizeof(*o));
    o->heap = ZrTests_ExecOwner_Stats();
    /* Check registered numeric release records before any potentially stale deep read. */
    if (!Fm_InputLive()) {
        o->error = 1;
        return;
    }
#define OBJ(field) bytes(o, &f->field, sizeof(f->field))
    OBJ(state);
    OBJ(context);
    OBJ(nodes);
    OBJ(symbols);
    OBJ(semantic);
    OBJ(instructions);
    OBJ(values);
    OBJ(maps);
    OBJ(regions);
    OBJ(places);
    OBJ(blocks);
    OBJ(operands);
    OBJ(script);
    OBJ(returned);
    OBJ(integer);
    OBJ(statements);
    OBJ(statementNodes);
    OBJ(identity);
    OBJ(constants);
    OBJ(original);
#undef OBJ
    bytes(o, f->compilerPool, 2 * sizeof(*f->compilerPool));
    bytes(o, f->functionPool, sizeof(*f->functionPool));
    bytes(o, g_fmModuleOwner.constants, (size_t) g_fmModuleOwner.constantCapacity * sizeof(*g_fmModuleOwner.constants));
    bytes(o, g_fmModuleOwner.layouts, (size_t) g_fmModuleOwner.layoutCapacity * sizeof(*g_fmModuleOwner.layouts));
    bytes(o, g_fmModuleOwner.sourceMaps,
          (size_t) g_fmModuleOwner.sourceMapCapacity * sizeof(*g_fmModuleOwner.sourceMaps));
    bytes(o, g_fmModuleOwner.functions, (size_t) g_fmModuleOwner.functionCapacity * sizeof(*g_fmModuleOwner.functions));
    /* Saved owning headers bound reads independently of deliberately refused visible headers. */
    function(o, &g_fmModuleOwner.functions[0], &g_fmOriginalFunctionOwner);
    function(o, &f->finalized, &g_fmFunctionOwner);
    o->hasOutput = f->output != ZR_NULL;
    o->outputAddress = (uintptr_t) f->output;
    if (!Fm_OutputLive()) {
        o->error = 1;
        return;
    }
    if (o->hasOutput && !capture(ZrCore_ExecIr_OwnedModuleView(f->output), &o->output))
        o->error = 1;
    if (o->heap.records > 32768u) {
        o->error = 1;
        return;
    }
    for (unsigned i = 0; i < o->heap.records; ++i) {
        unsigned frees;
        if (!ZrTests_ExecOwner_TryRecordFrees(i, &frees)) {
            o->error = 1;
            return;
        }
        if (!frees) {
            if (o->liveCount == FM_LIVE) {
                o->error = 1;
                break;
            }
            o->live[o->liveCount++] = i;
        }
    }
}
void Fm_Unchanged(const SFmObservation *a, const SFmObservation *b, TZrBool output) {
    TEST_ASSERT_EQUAL_UINT(0, Fm_DeferredErrors());
    TEST_ASSERT_EQUAL_UINT(0, a->error);
    TEST_ASSERT_EQUAL_UINT(0, b->error);
    TEST_ASSERT_EQUAL_UINT64(a->length, b->length);
    TEST_ASSERT_EQUAL_MEMORY(a->bytes, b->bytes, a->length);
    TEST_ASSERT_EQUAL_UINT(0, a->heap.errors);
    TEST_ASSERT_EQUAL_UINT(0, b->heap.errors);
    if (output) {
        TEST_ASSERT_EQUAL(a->hasOutput, b->hasOutput);
        TEST_ASSERT_EQUAL_UINT(a->output.count, b->output.count);
        TEST_ASSERT_EQUAL_UINT64(a->outputAddress, b->outputAddress);
        TEST_ASSERT_EQUAL_MEMORY(a->output.words, b->output.words, a->output.count * sizeof(*a->output.words));
        TEST_ASSERT_EQUAL_UINT(a->heap.live, b->heap.live);
        ZrTests_ExecOwner_AssertReleased(a->heap.records, b->heap.records);
        for (unsigned i = 0; i < a->liveCount; ++i)
            ZrTests_ExecOwner_AssertLive(a->live[i], a->live[i] + 1);
    }
}
SFmResult Fm_CallWithFault(unsigned site) {
    SFmResult r;
    ZrTests_ExecOwner_Disarm(); /* The helper exclusively owns its bounded fault window. */
    Fm_Observe(&r.before);
    if (r.before.error || r.before.heap.attempts > 65536u || site > 65536u - r.before.heap.attempts) {
        r.before.error = 1;
        r.after = r.before;
        r.success = ZR_FALSE;
        memset(&r.diagnostic, 0, sizeof(r.diagnostic));
        return r;
    }
    memset(&r.diagnostic, 0xA5, sizeof(r.diagnostic));
    if (site)
        ZrTests_ExecOwner_Arm(site);
    r.success = source_snapshot_finalized_primitive_module(&g_fm.semantic, &g_fm.context, &g_fm.script, &g_fm.identity,
                                                           &g_fm.constants, g_fm.functionPool, 1, &g_fm.original,
                                                           &g_fm.finalized, &g_fm.output, &r.diagnostic);
    ZrTests_ExecOwner_Disarm();
    TZrBool released = !r.success || Fm_OutputReleased();
    if (r.success)
        Fm_RegisterOutput(r.before.heap.records, ZrTests_ExecOwner_Stats().records);
    Fm_Observe(&r.after);
    if (!released)
        r.after.error = 1;
    return r;
}
SFmResult Fm_Call(void) { return Fm_CallWithFault(0); }
void Fm_Refused(const SFmResult *r, const SZrExecIrDiagnostic *d) {
    Fm_Unchanged(&r->before, &r->after, ZR_TRUE);
    TEST_ASSERT_FALSE(r->success);
    Fm_Diagnostic(d, &r->diagnostic);
}
void Fm_OutputCapture(SZrOwnerContents *contents) {
    TEST_ASSERT_EQUAL_UINT(0, Fm_DeferredErrors());
    TEST_ASSERT_TRUE_MESSAGE(Fm_OutputLive(), "actual output owner records must be live before reading the view");
    TEST_ASSERT_NOT_NULL(g_fm.output);
    TEST_ASSERT_TRUE_MESSAGE(capture(ZrCore_ExecIr_OwnedModuleView(g_fm.output), contents),
                             "actual output profile and witness bounds must be readable");
}
void Fm_Published(void) {
    SZrExecIrDiagnostic d, zero = {0};
    SZrOwnerContents expected;
    TEST_ASSERT_EQUAL_UINT(0, Fm_DeferredErrors());
    TEST_ASSERT_TRUE_MESSAGE(Fm_InputLive(), "actual input records must be live before publication reads");
    TEST_ASSERT_TRUE_MESSAGE(Fm_OutputLive(), "actual output records must be live before publication reads");
    TEST_ASSERT_NOT_NULL(g_fm.output);
    TEST_ASSERT_EQUAL_UINT(1, g_fm.original.functionCount);
    const SZrExecIrModule *stored = ZrCore_ExecIr_OwnedModuleView(g_fm.output);
    TEST_ASSERT_NOT_NULL(stored);
    SZrOwnerContents output;
    TEST_ASSERT_TRUE(capture(stored, &output));
    /* Both sides stay live until every source pointer check has passed. */
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) &g_fm.original, (uintptr_t) stored);
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) g_fm.original.functions, (uintptr_t) stored->functions);
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) g_fm.original.layouts, (uintptr_t) stored->layouts);
    const SZrExecIrFunction *copy = &stored->functions[0];
    const SZrExecIrFunction *sources[2] = {&g_fm.original.functions[0], &g_fm.finalized};
    for (unsigned i = 0; i < 2; ++i) {
        const SZrExecIrFunction *source = sources[i];
#define DISTINCT(field)                                                                                                \
    do {                                                                                                               \
        if (source->field && copy->field)                                                                              \
            TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) source->field, (uintptr_t) copy->field);                          \
    } while (0)
        DISTINCT(values);
        DISTINCT(instructions);
        DISTINCT(blocks);
        DISTINCT(operandPool);
        DISTINCT(resultPool);
        DISTINCT(memoryTokenPool);
        DISTINCT(phiPool);
        DISTINCT(phiIncoming);
        DISTINCT(predecessors);
        DISTINCT(successors);
        DISTINCT(gcRoots);
        DISTINCT(deoptStates);
        DISTINCT(deoptValues);
        DISTINCT(deoptAggregates);
        DISTINCT(deoptAggregateFields);
        DISTINCT(sourceMaps);
        DISTINCT(bindingRows);
        DISTINCT(gcMap);
        DISTINCT(stateMap);
        DISTINCT(frameLayout);
#undef DISTINCT
        if (source->frameLayout && copy->frameLayout)
            TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) source->frameLayout->slots, (uintptr_t) copy->frameLayout->slots);
    }
    TEST_ASSERT_TRUE(
            ZrCore_ExecIr_CloneModuleReplacingFunction(&g_fm.original, 1, &g_fm.finalized, &g_fm.candidate, &d));
    Fm_Diagnostic(&zero, &d);
    TEST_ASSERT_TRUE(capture(&g_fm.candidate, &expected));
    ZrTests_ExecOwner_Equal(&expected, stored);
    ZrTests_ExecOwner_Independent(&g_fm.candidate, stored);
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) g_fm.original.constants, (uintptr_t) stored->constants);
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) g_fm.compilerPool, (uintptr_t) stored->constants);
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) g_fm.functionPool, (uintptr_t) stored->constants);
    TEST_ASSERT_NOT_EQUAL_UINT64((uintptr_t) g_fm.finalized.values, (uintptr_t) stored->functions[0].values);
    TEST_ASSERT_TRUE(stored->functions[0].valueCapacity >= stored->functions[0].valueCount);
    TEST_ASSERT_TRUE(stored->functions[0].frameLayout->slotCapacity >= stored->functions[0].frameLayout->slotCount);
    ZrCore_ExecIr_FreeModule(&g_fm.candidate);
}
void Fm_Feature(void) {
    SFmResult r = Fm_Call();
    SZrExecIrDiagnostic zero = {0};
    Fm_Unchanged(&r.before, &r.after, !r.success);
    if (!r.success) {
        SZrExecIrDiagnostic expected = Fm_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        Fm_Diagnostic(&expected, &r.diagnostic);
        TEST_ASSERT_EQUAL_UINT(r.before.heap.attempts, r.after.heap.attempts);
        TEST_ASSERT_EQUAL_UINT(r.before.heap.frees, r.after.heap.frees);
        fprintf(stderr, "SOURCE_FINALIZED_FEATURE code=28 attempts=0 frees=0\n");
    }
    TEST_ASSERT_TRUE_MESSAGE(r.success, "actual finalized primitive source module snapshot must succeed");
    Fm_Diagnostic(&zero, &r.diagnostic);
    Fm_Published();
}
void Fm_EmptyMaps(void) {
    SZrExecIrDiagnostic d, zero = {0};
    for (unsigned i = 0; i < 2; ++i) {
        SZrExecIrFunction *f = i ? &g_fm.finalized : &g_fm.original.functions[0];
        SZrExecIrStateMap map;
        ZrCore_ExecIr_StateMapInit(&map);
        map.functionToken = f->functionToken;
        map.signatureHash = f->signatureHash;
        map.generation = f->contract.generation;
        SZrExecIrFunction borrowed = *f;
        borrowed.stateMap = &map;
        TZrBool ok = ZrCore_ExecIr_CloneFunction(&borrowed, &g_fm.temporary, &d);
        if (ok) {
            SZrExecIrFunction prior = *f;
            *f = g_fm.temporary;
            g_fm.temporary = prior;
            ZrCore_ExecIr_FreeFunction(&g_fm.temporary);
        }
        Fm_Refresh();
        TEST_ASSERT_TRUE(ok);
        Fm_Diagnostic(&zero, &d);
        Fm_Spare(f);
    }
    Fm_Refresh();
}
