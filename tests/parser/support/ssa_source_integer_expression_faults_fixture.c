#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ZR_TEST_INTEGER_EXPR_NO_REDIRECT
#include "ssa_source_integer_expression_faults_fixture.h"
#include "ssa_source_integer_expression_faults_producer_shim.h"
#include "ssa_source_integer_expression_faults_frame_shim.h"
#include "ssa_source_integer_expression_faults_graph_shim.h"
#include "zr_vm_core/exec_ir_state_map.h"

static const SZrSourceCompileOptions g_options = {ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR};
static SZrIntegerExprObservation g_observed;
static unsigned g_phases[ZR_INTEGER_EXPR_TRACE], g_stack[ZR_INTEGER_EXPR_DEPTH];
static unsigned g_depth, g_active, g_started, g_lifecycleErrors;
static unsigned g_lifecycles, g_unhealthyLifecycles, g_totalObserverErrors;
static SZrIntegerExprClose g_close;
static SZrReturnedExecCleanupRecord g_crtWitnesses[2][ZR_RETURNED_CRT_ROWS];
static SZrReturnedExecCleanupRecord g_ownerWitnesses[ZR_INTEGER_EXPR_RECORDS];
/* Actual live keeper records and typed headers, not replacement producer IR.
 * Contents ignores spare bytes; same-instance header witnesses preserve all
 * pointers/capacities as well. The borrowed addresses never outlive this Global. */
static struct {
    const SZrFunction *function;
    const SZrExecIrOwnedModule *owner;
    const SZrExecIrModule *module;
    SZrExecIrModule moduleHeader;
    SZrExecIrFunction functionHeader;
    SZrExecIrFrameLayout frameHeader;
    SZrExecIrGcMap gcHeader;
    SZrExecIrStateMap stateHeader;
    SZrOwnerContents contents;
    unsigned records[ZR_INTEGER_EXPR_RECORDS], recordCount, live, captured;
} g_keeper;

static void observation_error(void) {
    ++g_lifecycleErrors;
    if (g_active) ++g_observed.errors;
}
const char *ZrTests_IntegerExpr_PhaseName(unsigned phase) {
    static const char *const names[ZR_INTEGER_EXPR_PHASE_COUNT] = {
        "none", "producer-other", "bridge", "compactor", "frame",
        "finalizer", "candidate", "owner"
    };
    return phase < ZR_INTEGER_EXPR_PHASE_COUNT ? names[phase] : "invalid";
}
static unsigned enter(unsigned kind, unsigned phase) {
    unsigned index;
    if (!g_active || kind >= ZR_INTEGER_EXPR_CALL_KIND_COUNT ||
            phase == ZR_INTEGER_EXPR_NONE || phase >= ZR_INTEGER_EXPR_PHASE_COUNT ||
            g_depth >= ZR_INTEGER_EXPR_DEPTH || g_observed.callCount >= ZR_INTEGER_EXPR_CALLS) {
        observation_error(); return UINT_MAX;
    }
    index = g_observed.callCount++;
    ++g_observed.calls[kind];
    g_observed.call[index].kind = kind;
    g_observed.call[index].phase = phase;
    g_observed.call[index].depth = g_depth;
    g_observed.call[index].before = ZrTests_ExecOwner_Stats();
    g_stack[g_depth++] = index;
    return index;
}
static void leave(unsigned index, TZrBool result, const SZrExecIrDiagnostic *diagnostic) {
    SZrIntegerExprCall *call;
    if (index == UINT_MAX) return; /* The refusal was recorded before the real call. */
    if (index >= g_observed.callCount || !g_depth || g_stack[g_depth - 1u] != index) {
        observation_error(); return;
    }
    call = &g_observed.call[index];
    call->after = ZrTests_ExecOwner_Stats(); call->result = result;
    if (diagnostic) { call->diagnostic = *diagnostic; call->hasDiagnostic = 1u; }
    if (call->before.attempts < g_observed.before.attempts ||
            call->after.attempts < call->before.attempts ||
            call->after.attempts - g_observed.before.attempts > ZR_INTEGER_EXPR_TRACE) {
        observation_error();
    } else {
        /* Inner windows close first. An outer window only labels unclaimed
         * actual attempts, so candidate/owner and Frame allocations keep their
         * most specific observed provenance without wrapping allocation twice. */
        for (unsigned i = call->before.attempts; i < call->after.attempts; ++i) {
            unsigned relative = i - g_observed.before.attempts;
            if (!g_phases[relative]) g_phases[relative] = call->phase;
        }
    }
    --g_depth;
}
TZrBool ZrTests_IntegerExpr_Prepare(const struct SZrCompilerState *compiler,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *diagnostic) {
    unsigned call = enter(ZR_INTEGER_EXPR_BRIDGE_CALL, ZR_INTEGER_EXPR_BRIDGE);
    TZrBool result = source_integer_constants_prepare(compiler, output, diagnostic);
    leave(call, result, diagnostic); return result;
}
TZrBool ZrTests_IntegerExpr_Compact(const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context, const SZrExecIrFunction *input,
        SZrExecIrFunction *output, SZrExecIrDiagnostic *diagnostic) {
    unsigned call = enter(ZR_INTEGER_EXPR_COMPACTOR_CALL, ZR_INTEGER_EXPR_COMPACTOR);
    TZrBool result = ZrParser_ExecIr_EliminateDeadSourcePlaces(semantic, context, input, output, diagnostic);
    leave(call, result, diagnostic); return result;
}
TZrBool ZrTests_IntegerExpr_AppendLayout(SZrExecIrModule *module,
        const SZrExecIrLayout *layouts, TZrSize count, SZrExecIrRange *range) {
    unsigned call = enter(ZR_INTEGER_EXPR_APPEND_CALL, ZR_INTEGER_EXPR_PRODUCER_OTHER);
    TZrBool result = ZrCore_ExecIr_ModuleAppendLayout(module, layouts, count, range);
    leave(call, result, ZR_NULL); return result;
}
TZrBool ZrTests_IntegerExpr_Attach(SZrExecIrFunction *function,
        const SZrSemanticContext *context, const SZrExecIrLayout *layouts,
        TZrUInt32 layoutCount, TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic) {
    unsigned call = enter(ZR_INTEGER_EXPR_ATTACH_CALL, ZR_INTEGER_EXPR_FRAME);
    TZrBool result = ZrParser_ExecIr_AttachPrimitiveSourceFrame(function, context, layouts,
            layoutCount, frameByteLimit, diagnostic);
    leave(call, result, diagnostic); return result;
}
TZrBool ZrTests_IntegerExpr_Finalized(const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity,
        const SZrArray *compilerConstants, const SZrTypeValue *functionConstants,
        TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination,
        SZrExecIrDiagnostic *diagnostic) {
    unsigned call = enter(ZR_INTEGER_EXPR_FINALIZER_CALL, ZR_INTEGER_EXPR_FINALIZER);
    TZrBool result = source_snapshot_finalized_primitive_module(semantic, context, script,
            sourceIdentity, compilerConstants, functionConstants, functionConstantCount,
            original, finalized, destination, diagnostic);
    leave(call, result, diagnostic); return result;
}
TZrBool ZrTests_IntegerExpr_Candidate(const SZrExecIrModule *source,
        TZrExecIrFunctionId selectedId, const SZrExecIrFunction *replacement,
        SZrExecIrModule *destination, SZrExecIrDiagnostic *diagnostic) {
    unsigned call = enter(ZR_INTEGER_EXPR_CANDIDATE_CALL, ZR_INTEGER_EXPR_CANDIDATE);
    TZrBool result = ZrCore_ExecIr_CloneModuleReplacingFunction(source, selectedId,
            replacement, destination, diagnostic);
    leave(call, result, diagnostic); return result;
}
TZrBool ZrTests_IntegerExpr_Owner(const SZrExecIrModule *source,
        SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic) {
    unsigned call = enter(ZR_INTEGER_EXPR_OWNER_CALL, ZR_INTEGER_EXPR_OWNER);
    TZrBool result = ZrCore_ExecIr_CloneOwnedModule(source, destination, diagnostic);
    leave(call, result, diagnostic); return result;
}
void *ZrTests_ReturnedExec_FrameMalloc(size_t bytes) {
    unsigned call = enter(ZR_INTEGER_EXPR_FRAME_HEADER_CALL, ZR_INTEGER_EXPR_FRAME);
    void *result = ZrTests_ExecOwner_Malloc(bytes);
    if (bytes != sizeof(SZrExecIrFrameLayout)) observation_error();
    leave(call, (TZrBool)(result != ZR_NULL), ZR_NULL); return result;
}
void *ZrTests_ReturnedExec_FrameCalloc(size_t count, size_t width) {
    unsigned call = enter(ZR_INTEGER_EXPR_FRAME_VALUES_CALL, ZR_INTEGER_EXPR_FRAME);
    void *result = ZrTests_ExecOwner_Calloc(count, width);
    if (!count || count > UINT_MAX || width != sizeof(SZrExecIrPackedValue)) observation_error();
    else if (g_active && count > g_observed.frameValuesMax) g_observed.frameValuesMax = (unsigned)count;
    leave(call, (TZrBool)(result != ZR_NULL), ZR_NULL); return result;
}
void ZrTests_ReturnedExec_FrameFree(void *pointer) { ZrTests_ExecOwner_Free(pointer); }
void ZrTests_ReturnedExec_PackedFree(SZrExecIrPackedFrameLayout *packed) {
    const void *pointers[4] = {ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL};
    SZrOwnerLiveSpan spans[4] = {{0}};
    unsigned have[4] = {0u, 0u, 0u, 0u};
    SZrOwnerHeapStats before = ZrTests_ExecOwner_Stats();
    if (packed) {
        pointers[0] = packed->frame.slots; pointers[1] = packed->logicalValueIds;
        pointers[2] = packed->logicalToPhysical; pointers[3] = packed->slotClasses;
        for (unsigned i = 0u; i < 4u; ++i) if (pointers[i]) {
            have[i] = (unsigned)ZrTests_ExecOwner_TryLiveSpan(0u, before.records,
                    pointers[i], 1u, 1u, &spans[i]);
            if (have[i] != 1u) observation_error();
            for (unsigned j = 0u; j < i; ++j)
                if (have[i] && have[j] && spans[i].record == spans[j].record) observation_error();
        }
    }
    /* Exactly one genuine free, even on observation refusal or partial layout. */
    ZrParser_ExecIr_PackedFrameLayoutFree(packed);
    if (g_active) ++g_observed.packedFrees;
    for (unsigned i = 0u; i < 4u; ++i) if (have[i]) {
        unsigned frees = 0u;
        if (ZrTests_ExecOwner_TryRecordFrees(spans[i].record, &frees) != 1 || frees != 1u)
            observation_error();
    }
    if (packed) {
        const unsigned char *bytes = (const unsigned char *)packed;
        for (size_t i = 0u; i < sizeof(*packed); ++i) if (bytes[i]) { observation_error(); break; }
    }
}

static int owner_stats_same(SZrOwnerHeapStats a, SZrOwnerHeapStats b) {
    return a.attempts == b.attempts && a.records == b.records && a.live == b.live &&
            a.frees == b.frees && a.errors == b.errors && a.failures == b.failures;
}
static int crt_stats_same(SZrReturnedHeapStats a, SZrReturnedHeapStats b) {
    return a.attempts == b.attempts && a.successes == b.successes && a.live == b.live &&
            a.frees == b.frees && a.errors == b.errors && a.failures == b.failures;
}
static int vm_stats_same(SZrReturnedVmStats a, SZrReturnedVmStats b) {
    return a.attempts == b.attempts && a.frees == b.frees && a.live == b.live &&
            a.errors == b.errors && a.diagnosticCount == b.diagnosticCount && a.dropped == b.dropped;
}
int ZrTests_IntegerExpr_ClosedHealthy(void) {
    SZrOwnerHeapStats owner = ZrTests_ExecOwner_Stats();
    SZrReturnedVmStats vm = {0};
    if (!g_started || !g_close.closed || g_active || g_depth ||
            g_close.beginResult != 1 || g_close.vmResult != 1 || g_close.observerErrors ||
            g_close.ledgers.closed != 1u || g_close.ledgers.completed != 1u ||
            g_close.ledgers.healthyBeforeRecovery != 1u ||
            g_close.ledgers.refusedQueries || g_close.ledgers.observationErrors ||
            ZrTests_Returned_TryVmStats(&vm) != 1 || vm.live || vm.errors || vm.diagnosticCount || vm.dropped ||
            !vm_stats_same(vm, g_close.vm) || owner.live || owner.errors ||
            !owner_stats_same(owner, g_close.ledgers.ownerAfter) ||
            owner.records != g_close.ledgers.ownerRecordCount || owner.records > ZR_INTEGER_EXPR_RECORDS) return 0;
    for (unsigned i = 0u; i < owner.records; ++i) {
        const SZrReturnedExecCleanupRecord *w = &g_ownerWitnesses[i]; unsigned frees = 0u;
        if (w->beforeResult != 1 || w->afterResult != 1 || w->beforeFrees != 1u || w->afterFrees != 1u ||
                ZrTests_ExecOwner_TryRecordFrees(i, &frees) != 1 || frees != 1u) return 0;
    }
    for (unsigned k = 0u; k < 2u; ++k) {
        SZrReturnedHeapStats crt = {0};
        if (g_close.ledgers.crtBeforeResult[k] != 1 || g_close.ledgers.crtAfterResult[k] != 1 ||
                ZrTests_Returned_TryStats(k, &crt) != 1 || crt.live || crt.errors ||
                !crt_stats_same(crt, g_close.ledgers.crtAfter[k]) ||
                crt.successes != g_close.ledgers.crtRecordCount[k] || crt.successes > ZR_RETURNED_CRT_ROWS) return 0;
        for (unsigned i = 0u; i < crt.successes; ++i) {
            const SZrReturnedExecCleanupRecord *w = &g_crtWitnesses[k][i]; unsigned frees = 0u;
            if (w->beforeResult != 1 || w->afterResult != 1 || w->beforeFrees != 1u || w->afterFrees != 1u ||
                    ZrTests_Returned_TryRecordFrees(k, i, &frees) != 1 || frees != 1u) return 0;
        }
    }
    return 1;
}
int ZrTests_IntegerExpr_Init(void) {
    SZrOwnerHeapStats owner = ZrTests_ExecOwner_Stats();
    SZrReturnedVmStats vm = {0};
    SZrReturnedExecCleanupStorage storage = {{g_crtWitnesses[0], g_crtWitnesses[1]},
        {ZR_RETURNED_CRT_ROWS, ZR_RETURNED_CRT_ROWS}, g_ownerWitnesses, ZR_INTEGER_EXPR_RECORDS};
    int prior = g_started ? ZrTests_IntegerExpr_ClosedHealthy() :
            (!owner.attempts && !owner.records && !owner.live && !owner.frees && !owner.errors && !owner.failures &&
             ZrTests_Returned_TryVmStats(&vm) == 1 && !vm.attempts && !vm.live && !vm.frees &&
             !vm.errors && !vm.diagnosticCount && !vm.dropped && ZrTests_IntegerExpr_CrtQuiet());
    if (!prior) return 0; /* Keep the first close and every witness intact. */
    ZrTests_Returned_Init(); ZrTests_ExecOwner_Reset();
    memset(&g_observed, 0, sizeof(g_observed)); memset(&g_close, 0, sizeof(g_close));
    memset(&g_keeper, 0, sizeof(g_keeper)); memset(g_crtWitnesses, 0, sizeof(g_crtWitnesses));
    memset(g_ownerWitnesses, 0, sizeof(g_ownerWitnesses));
    g_depth = 0u; g_active = 0u; g_lifecycleErrors = 0u; g_started = 1u; ++g_lifecycles;
    g_close.beginResult = ZrTests_ReturnedExec_CleanupBegin(&g_close.ledgers, &storage, prior);
    if (g_close.beginResult != 1) { observation_error(); return 0; }
    return 1;
}
void ZrTests_IntegerExpr_Open(void) { ZrTests_Returned_Open(); }
SZrFunction *ZrTests_IntegerExpr_Compile(unsigned slot, const char *source,
        SZrSourceCompileDiagnostic *diagnostic) {
    unsigned call;
    SZrFunction *result;
    SZrSourceCompileDiagnostic actual = {0};
    if (!g_started || g_close.closed || g_active || g_depth) { observation_error(); return ZR_NULL; }
    memset(&g_observed, 0, sizeof(g_observed)); memset(g_phases, 0, sizeof(g_phases));
    g_observed.before = ZrTests_ExecOwner_Stats(); g_active = 1u;
    call = enter(ZR_INTEGER_EXPR_SOURCE_CALL, ZR_INTEGER_EXPR_PRODUCER_OTHER);
    result = ZrTests_Returned_Compile(slot, source, ZR_FALSE, &g_options, &actual);
    leave(call, (TZrBool)(result != ZR_NULL), ZR_NULL);
    g_observed.after = ZrTests_ExecOwner_Stats(); g_observed.source = actual;
    if (g_depth) observation_error();
    g_active = 0u;
    if (diagnostic) *diagnostic = actual;
    return result;
}
const SZrIntegerExprObservation *ZrTests_IntegerExpr_Observation(void) { return &g_observed; }
int ZrTests_IntegerExpr_TryTrace(unsigned relativeIndex, SZrIntegerExprEvent *output) {
    SZrIntegerExprEvent actual = {{0}, 0u};
    if (!output || g_active || g_depth || g_observed.after.attempts < g_observed.before.attempts ||
            relativeIndex >= g_observed.after.attempts - g_observed.before.attempts ||
            relativeIndex >= ZR_INTEGER_EXPR_TRACE) return 0;
    if (ZrTests_ExecOwner_TryEvent(g_observed.before.attempts + relativeIndex, &actual.actual) != 1 ||
            !g_phases[relativeIndex] || g_phases[relativeIndex] >= ZR_INTEGER_EXPR_PHASE_COUNT) {
        observation_error(); return 0;
    }
    actual.phase = g_phases[relativeIndex]; *output = actual; return 1;
}
int ZrTests_IntegerExpr_CrtQuiet(void) {
    for (unsigned k = 0u; k < 2u; ++k) {
        SZrReturnedHeapStats s = {0};
        if (ZrTests_Returned_TryStats(k, &s) != 1 || s.attempts || s.successes || s.frees ||
                s.live || s.errors || s.failures) return 0;
    }
    return 1;
}
int ZrTests_IntegerExpr_Released(unsigned first, unsigned last) {
    SZrOwnerHeapStats s = ZrTests_ExecOwner_Stats();
    if (first > last || last > s.records || last > ZR_INTEGER_EXPR_RECORDS) return 0;
    for (unsigned i = first; i < last; ++i) {
        unsigned frees = 0u;
        if (ZrTests_ExecOwner_TryRecordFrees(i, &frees) != 1 || frees != 1u) return 0;
    }
    return 1;
}
static int span(unsigned first, unsigned last, const void *pointer, size_t count, size_t width) {
    SZrOwnerLiveSpan live = {0};
    if (!pointer) return count == 0u;
    return ZrTests_ExecOwner_TryLiveSpan(first, last, pointer, count ? count : 1u, width, &live) == 1;
}
static int graph_spans(const SZrExecIrOwnedModule *owner, const SZrExecIrModule *module,
        unsigned first, unsigned last) {
    const SZrExecIrFunction *f;
    if (!span(first, last, owner, 1u, 1u) || !span(first, last, module, 1u, sizeof(*module)) ||
            !module || module->functionCount != 1u) return 0;
#define S(object, field, count) if (!span(first, last, (object)->field, (object)->count, sizeof(*(object)->field))) return 0
    S(module, constants, constantCount); S(module, layouts, layoutCount);
    S(module, sourceMaps, sourceMapCount); S(module, functions, functionCount);
    f = &module->functions[0];
    S(f, values, valueCount); S(f, instructions, instructionCount); S(f, blocks, blockCount);
    S(f, operandPool, operandCount); S(f, resultPool, resultCount); S(f, memoryTokenPool, memoryTokenCount);
    S(f, phiPool, phiCount); S(f, phiIncoming, phiIncomingCount); S(f, predecessors, predecessorCount);
    S(f, successors, successorCount); S(f, gcRoots, gcRootCount); S(f, deoptStates, deoptStateCount);
    S(f, deoptValues, deoptValueCount); S(f, deoptAggregates, deoptAggregateCount);
    S(f, deoptAggregateFields, deoptAggregateFieldCount); S(f, sourceMaps, sourceMapCount);
    S(f, bindingRows, bindingRowCount);
    if (f->frameLayout) {
        if (!span(first, last, f->frameLayout, 1u, sizeof(*f->frameLayout))) return 0;
        S(f->frameLayout, slots, slotCount);
    }
    if (f->stateMap) {
        if (!span(first, last, f->stateMap, 1u, sizeof(*f->stateMap))) return 0;
        S(f->stateMap, entries, entryCount); S(f->stateMap, valuePool, valueCount);
        S(f->stateMap, rootPool, rootCount); S(f->stateMap, ownerStatePool, ownerStateCount);
    }
    if (f->gcMap) {
        if (!span(first, last, f->gcMap, 1u, sizeof(*f->gcMap))) return 0;
        S(f->gcMap, entries, entryCount); S(f->gcMap, slotIndexPool, slotIndexCount);
        S(f->gcMap, inlineRefOffsetPool, inlineRefOffsetCount);
    }
#undef S
    return 1;
}
static int diagnostic_clear(SZrExecIrDiagnostic d) {
    return !d.code && !d.functionToken && !d.blockId && !d.instructionId && !d.sourceId &&
            !d.expectedVersion && !d.actualVersion && !d.expectedHash && !d.actualHash;
}
int ZrTests_IntegerExpr_Published(const SZrFunction *function,
        const SZrIntegerExprObservation *o) {
    const SZrExecIrModule *module;
    SZrExecIrDiagnostic d = {0};
    unsigned first = 0u, last = 0u, found = 0u;
    if (!function || !o || !function->ownedExecIr || function->ownedAotIr || o->errors ||
            o->source.stage != ZR_SOURCE_STAGE_NONE || o->source.result != ZR_SOURCE_RESULT_OK ||
            o->source.hasExecIr || o->source.hasAotIr || o->source.hasRange ||
            !diagnostic_clear(o->source.execIr) || o->callCount > ZR_INTEGER_EXPR_CALLS) return 0;
    for (unsigned i = 0u; i < o->callCount; ++i) if (o->call[i].kind == ZR_INTEGER_EXPR_OWNER_CALL) {
        if (!o->call[i].result || !o->call[i].hasDiagnostic || !diagnostic_clear(o->call[i].diagnostic)) return 0;
        first = o->call[i].before.records; last = o->call[i].after.records; ++found;
    }
    if (found != 1u || first >= last || first < o->before.records || last > o->after.records ||
            !ZrTests_IntegerExpr_Released(o->before.records, first) ||
            !ZrTests_IntegerExpr_Released(last, o->after.records)) return 0;
    module = ZrCore_Function_GetExecIr(function);
    if (module != ZrCore_ExecIr_OwnedModuleView(function->ownedExecIr) ||
            !graph_spans(function->ownedExecIr, module, first, last) ||
            !module->functions[0].frameLayout || module->functions[0].valueCount <= 1u ||
            module->functions[0].frameLayout->logicalSlotCount != module->functions[0].valueCount ||
            !ZrCore_ExecIr_VerifyModule(module, &d) || !diagnostic_clear(d) ||
            !ZrCore_ExecIr_VerifyFunction(&module->functions[0], ZR_EXEC_IR_VERIFY_ALL, &d) ||
            !diagnostic_clear(d)) return 0;
    return 1;
}
static int same_words(const SZrOwnerContents *a, const SZrOwnerContents *b) {
    return a->count <= ZR_OWNER_WORDS && a->count == b->count &&
            memcmp(a->words, b->words, a->count * sizeof(a->words[0])) == 0;
}
int ZrTests_IntegerExpr_CaptureKeeper(void) {
    const SZrFunction *function = ZrTests_Returned_Function(0u);
    SZrOwnerHeapStats s = ZrTests_ExecOwner_Stats();
    const SZrExecIrModule *module;
    const SZrExecIrFunction *f;
    if (g_keeper.captured || !ZrTests_IntegerExpr_Published(function, &g_observed) ||
            s.records > ZR_INTEGER_EXPR_RECORDS) return 0;
    module = ZrCore_Function_GetExecIr(function); f = &module->functions[0];
    if (ZrTests_ExecOwner_TryCapture(module, &g_keeper.contents) != 1) return 0;
    g_keeper.function = function; g_keeper.owner = function->ownedExecIr; g_keeper.module = module;
    memcpy(&g_keeper.moduleHeader, module, sizeof(*module));
    memcpy(&g_keeper.functionHeader, f, sizeof(*f));
    if (f->frameLayout) memcpy(&g_keeper.frameHeader, f->frameLayout, sizeof(*f->frameLayout));
    if (f->stateMap) memcpy(&g_keeper.stateHeader, f->stateMap, sizeof(*f->stateMap));
    if (f->gcMap) memcpy(&g_keeper.gcHeader, f->gcMap, sizeof(*f->gcMap));
    for (unsigned i = 0u; i < s.records; ++i) {
        unsigned frees = 0u;
        if (ZrTests_ExecOwner_TryRecordFrees(i, &frees) != 1 || frees > 1u) return 0;
        if (!frees) g_keeper.records[g_keeper.recordCount++] = i;
    }
    g_keeper.live = s.live;
    if (!g_keeper.live || g_keeper.recordCount != s.live) return 0;
    g_keeper.captured = 1u; return 1;
}
int ZrTests_IntegerExpr_KeeperUnchanged(void) {
    const SZrFunction *function = ZrTests_Returned_Function(0u);
    const SZrExecIrModule *module;
    const SZrExecIrFunction *f;
    SZrOwnerContents contents;
    if (!g_keeper.captured || function != g_keeper.function || !function ||
            function->ownedExecIr != g_keeper.owner || function->ownedAotIr) return 0;
    /* Refuse a released keeper before borrowing/dereferencing its owner graph.
     * Numeric lookup alone is not typed authentication: these are the actual
     * records captured from this successful, still-rooted Source Function. */
    for (unsigned i = 0u; i < g_keeper.recordCount; ++i) {
        unsigned frees = 0u;
        if (ZrTests_ExecOwner_TryRecordFrees(g_keeper.records[i], &frees) != 1 || frees != 0u) return 0;
    }
    module = ZrCore_Function_GetExecIr(function);
    if (module != g_keeper.module || !module || memcmp(module, &g_keeper.moduleHeader, sizeof(*module))) return 0;
    f = &module->functions[0];
    if (memcmp(f, &g_keeper.functionHeader, sizeof(*f)) ||
            (f->frameLayout && memcmp(f->frameLayout, &g_keeper.frameHeader, sizeof(*f->frameLayout))) ||
            (f->stateMap && memcmp(f->stateMap, &g_keeper.stateHeader, sizeof(*f->stateMap))) ||
            (f->gcMap && memcmp(f->gcMap, &g_keeper.gcHeader, sizeof(*f->gcMap))) ||
            ZrTests_ExecOwner_TryCapture(module, &contents) != 1 || !same_words(&contents, &g_keeper.contents)) return 0;
    return 1;
}
unsigned ZrTests_IntegerExpr_KeeperLive(void) { return g_keeper.live; }
void ZrTests_IntegerExpr_Close(void) {
    if (!g_started || g_close.closed) return;
    if (g_active || g_depth) observation_error();
    g_active = 0u; g_depth = 0u;
    ZrTests_ExecOwner_Disarm(); ZrTests_Returned_Disarm();
    ZrTests_Returned_FreeFunction(1u); ZrTests_Returned_FreeFunction(0u);
    ZrTests_Returned_Shutdown();
    g_close.vmResult = ZrTests_Returned_TryVmStats(&g_close.vm);
    g_close.observerErrors = g_lifecycleErrors;
    ZrTests_ReturnedExec_CleanupLedgers(&g_close.ledgers);
    g_close.closed = 1u; g_totalObserverErrors += g_lifecycleErrors;
    if (!ZrTests_IntegerExpr_ClosedHealthy()) {
        ++g_unhealthyLifecycles;
        fprintf(stderr, "INTEGER_EXPR_LEDGER closed=%u healthy=%u vm_result=%d vm_live=%u vm_errors=%u owner_live=%u owner_errors=%u observer=%u refused=%u observation_errors=%u\n",
                g_close.ledgers.closed, g_close.ledgers.healthyBeforeRecovery, g_close.vmResult,
                g_close.vm.live, g_close.vm.errors, g_close.ledgers.ownerBefore.live,
                g_close.ledgers.ownerBefore.errors, g_close.observerErrors,
                g_close.ledgers.refusedQueries, g_close.ledgers.observationErrors);
    }
    /* Erase borrowed addresses only after the actual metadata/Global close. */
    memset(&g_keeper, 0, sizeof(g_keeper));
}
const SZrIntegerExprClose *ZrTests_IntegerExpr_CloseReport(void) { return &g_close; }
SZrIntegerExprFinal ZrTests_IntegerExpr_Final(void) {
    SZrIntegerExprFinal result = {0};
    result.lifecycles = g_lifecycles; result.closed = g_close.closed;
    result.healthy = (unsigned)(ZrTests_IntegerExpr_ClosedHealthy() && !g_unhealthyLifecycles);
    result.unhealthyLifecycles = g_unhealthyLifecycles; result.observerErrors = g_totalObserverErrors;
    result.owner = ZrTests_ExecOwner_Stats(); result.vmResult = ZrTests_Returned_TryVmStats(&result.vm);
    result.ownerBeforeRecovery = g_close.ledgers.ownerBefore;
    for (unsigned k = 0u; k < 2u; ++k) {
        result.crtResult[k] = ZrTests_Returned_TryStats(k, &result.crt[k]);
        result.crtBeforeRecovery[k] = g_close.ledgers.crtBefore[k];
        result.crtBeforeResult[k] = g_close.ledgers.crtBeforeResult[k];
    }
    return result;
}
