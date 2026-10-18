#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ssa_gc_map_source_preflight_fixture.h"
#include "ssa_primitive_frame_validation_fixture.h"
#include "ssa_source_finalized_module_fixture.h"
#include "exec_ir/source/exec_ir_source_frame_internal.h"
#include "unity.h"

/* One lifecycle per independent case; the existing two fixtures share one ledger. */
enum { SP_EXTRAS = 136, SP_SPANS = 160, SP_BYTES = 65536, SP_GROW_PLACES = 128 };
typedef struct SSpSpan {
    const void *pointer;
    size_t bytes;
    unsigned char content[SP_BYTES];
} SSpSpan;
static unsigned g_family, g_ready, g_extraCount, g_spanCount, g_liveCount;
static void *g_extras[SP_EXTRAS];
static unsigned g_live[FM_LIVE];
static SSpSpan g_spans[SP_SPANS];
static SZrExecIrGcMap *g_maps;
static SZrExecIrFunction *g_bound;
static SZrExecIrGcMap *g_savedMap;
static TZrUInt32 g_savedCount, g_savedCapacity;
static SZrExecIrFunction g_functionBefore;
static SZrExecIrModule g_candidateBefore;
static SZrExecIrFunction *g_observed;
static SPfObservation g_pfBefore, g_pfAfter;
static SFmObservation g_fmBefore, g_fmAfter;
static SSourceFinalizedModuleWitness g_witness;
static unsigned g_pending;
static SZrSourceFrameStorage g_storage;
static unsigned g_growing;
static SZrArray g_placesBefore;
static SSpSummary g_summary;

static const char *entrance_name(unsigned e) {
    static const char *const names[] = {"frame", "selected", "finalized", "candidate"};
    return names[e];
}
static const char *shape_name(unsigned shape) {
    static const char *const names[] = {"absent", "empty", "multi", "real2_2", "real1_2",
                                      "nonnull0_0", "null1_1", "null0_1", "real2_1"};
    return names[shape];
}
static int diagnostic_is(const SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code, TZrMetadataToken token) {
    return d->code == code && d->functionToken == token && d->blockId == 0u &&
           d->instructionId == 0u && d->sourceId == 0u && d->expectedVersion == 0u &&
           d->actualVersion == 0u && d->expectedHash == 0u && d->actualHash == 0u;
}
static void restore(void) {
    if (g_bound != NULL) {
        g_bound->gcMap = g_savedMap;
        g_bound->gcMapCount = g_savedCount;
        g_bound->gcMapCapacity = g_savedCapacity;
        g_bound = NULL;
    }
    if (g_growing) {
        g_fm.semantic.places.places = g_placesBefore;
        g_growing = 0u;
    }
}
static void release(void) {
    if (g_pending) {
        source_finalized_module_release(&g_witness);
        g_pending = 0u;
    }
}
static void free_extras(void) {
    while (g_extraCount != 0u) ZrTests_ExecOwner_Free(g_extras[--g_extraCount]);
    g_maps = NULL;
}
static void *extra(size_t count, size_t width) {
    void *p;
    TEST_ASSERT_LESS_THAN_UINT(SP_EXTRAS, g_extraCount);
    p = ZrTests_ExecOwner_Calloc(count, width);
    g_extras[g_extraCount++] = p;
    TEST_ASSERT_NOT_NULL(p);
    return p;
}
void Sp_SetFamily(unsigned family) { g_family = family; }
void Sp_SetUp(void) {
    g_extraCount = g_spanCount = g_liveCount = g_pending = g_growing = 0u;
    g_maps = NULL;
    g_bound = NULL;
    g_ready = 1u;
    if (g_family == SP_PF) { Pf_SetUp(); Pf_Feature(); }
    else { Fm_SetUp(); Fm_Feature(); }
}
void Sp_TearDown(void) {
    SZrOwnerHeapStats s;
    restore();
    release();
    ZrTests_ExecOwner_Disarm();
    free_extras();
    if (g_ready) {
        g_ready = 0u;
        if (g_family == SP_PF) Pf_TearDown(); else Fm_TearDown();
    }
    s = ZrTests_ExecOwner_Stats();
    g_summary.attempts += s.attempts;
    g_summary.live += s.live;
    g_summary.errors += s.errors;
}
SSpSummary Sp_Summary(void) { return g_summary; }
static SZrExecIrFunction *function_for(unsigned entrance) {
    if (entrance == SP_FRAME) return &g_pfFunctions[0];
    if (entrance == SP_SELECTED) return &g_fm.original.functions[0];
    if (entrance == SP_FINALIZED) return &g_fm.finalized;
    if (g_fm.candidate.functionCount == 0u) {
        SZrExecIrDiagnostic d;
        TZrBool ok = ZrCore_ExecIr_CloneModuleReplacingFunction(
                &g_fm.original, 1u, &g_fm.finalized, &g_fm.candidate, &d);
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_TRUE(diagnostic_is(&d, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u));
    }
    return &g_fm.candidate.functions[0];
}
static void maps(void) {
    g_maps = extra(2u, sizeof(*g_maps));
    g_maps[0].entries = extra(2u, sizeof(*g_maps[0].entries));
    g_maps[0].slotIndexPool = extra(2u, sizeof(*g_maps[0].slotIndexPool));
    g_maps[0].inlineRefOffsetPool = extra(2u, sizeof(*g_maps[0].inlineRefOffsetPool));
    g_maps[0].entryCount = g_maps[0].entryCapacity = 2u;
    g_maps[0].slotIndexCount = g_maps[0].slotIndexCapacity = 2u;
    g_maps[0].inlineRefOffsetCount = g_maps[0].inlineRefOffsetCapacity = 2u;
    for (unsigned i = 0u; i < 2u; ++i) {
        g_maps[0].entries[i].site = i + 1u;
        g_maps[0].entries[i].liveRefSlots = (SZrExecIrRange){.start = i, .count = 1u};
        g_maps[0].entries[i].inlineRefOffsets = (SZrExecIrRange){.start = i, .count = 1u};
        /* Real scalar fixtures have slot 0; offset 0 is within their I64 slot. */
        g_maps[0].slotIndexPool[i] = 0u;
        g_maps[0].inlineRefOffsetPool[i] = 0u;
    }
}
static void bind(SZrExecIrFunction *f, unsigned shape) {
    g_bound = f;
    g_savedMap = f->gcMap; g_savedCount = f->gcMapCount; g_savedCapacity = f->gcMapCapacity;
    f->gcMap = g_maps; f->gcMapCount = f->gcMapCapacity = 1u;
    switch (shape) {
        case SP_ABSENT: f->gcMap = NULL; f->gcMapCount = f->gcMapCapacity = 0u; break;
        case SP_EMPTY: case SP_MULTI: break;
        case SP_REAL2_2: f->gcMapCount = f->gcMapCapacity = 2u; break;
        case SP_REAL1_2: f->gcMapCapacity = 2u; break;
        case SP_NONNULL0_0: f->gcMapCount = f->gcMapCapacity = 0u; break;
        case SP_NULL1_1: f->gcMap = NULL; break;
        case SP_NULL0_1: f->gcMap = NULL; f->gcMapCount = 0u; break;
        case SP_REAL2_1: f->gcMapCount = 2u; break;
    }
}
static void witness(SZrExecIrDiagnostic *d) {
    memset(&g_witness, 0, sizeof(g_witness));
    g_witness.semantic = &g_fm.semantic; g_witness.context = &g_fm.context;
    g_witness.script = &g_fm.script; g_witness.identity = &g_fm.identity;
    g_witness.compilerConstants = &g_fm.constants;
    g_witness.functionConstants = g_fm.functionPool; g_witness.functionConstantCount = 1u;
    g_witness.original = &g_fm.original; g_witness.finalized = &g_fm.finalized;
    g_witness.destination = &g_fm.output; g_witness.diagnostic = d;
    source_integer_proof_init(&g_witness.integerProof);
    g_pending = 1u;
}
static TZrBool invoke(unsigned entrance, SZrExecIrDiagnostic *d) {
    if (entrance == SP_FRAME) {
        memset(&g_storage, 0, sizeof(g_storage));
        return source_frame_storage(&g_pfFunctions[0], &g_pfContext, g_pfRows, 2u, d, &g_storage);
    }
    witness(d);
    if (entrance == SP_CANDIDATE) {
        g_witness.spans = g_witness.inlineSpans;
        g_witness.spanLimit = SOURCE_FINALIZED_INLINE_SPANS;
        g_witness.token = g_fm.candidate.functions[0].functionToken;
        return source_finalized_module_candidate_storage(&g_witness, &g_fm.candidate);
    }
    return source_finalized_module_preflight(&g_witness);
}
static int remember(const void *p) {
    SZrOwnerLiveSpan live;
    if (p == NULL) return 1;
    if (!ZrTests_ExecOwner_TryLiveSpan(0u, ZrTests_ExecOwner_Stats().records, p, 1u, 1u, &live)) return 0;
    for (unsigned i = 0u; i < g_spanCount; ++i) if (g_spans[i].pointer == live.allocation) return 1;
    if (g_spanCount == SP_SPANS || live.bytes > SP_BYTES) return 0;
    g_spans[g_spanCount].pointer = live.allocation;
    g_spans[g_spanCount].bytes = live.bytes;
    memcpy(g_spans[g_spanCount].content, live.allocation, live.bytes);
    ++g_spanCount;
    return 1;
}
static int before(SZrExecIrFunction *f) {
    int ok = 1;
    g_observed = f; g_functionBefore = *f; g_candidateBefore = g_fm.candidate;
    g_spanCount = g_liveCount = 0u;
    if (g_family == SP_PF) Pf_Observe(&g_pfBefore); else Fm_Observe(&g_fmBefore);
#define POOL(field) ok &= remember(f->field)
    POOL(values); POOL(instructions); POOL(blocks); POOL(operandPool); POOL(resultPool);
    POOL(memoryTokenPool); POOL(phiPool); POOL(phiIncoming); POOL(predecessors); POOL(successors);
    POOL(gcRoots); POOL(deoptStates); POOL(deoptValues); POOL(deoptAggregates);
    POOL(deoptAggregateFields); POOL(sourceMaps); POOL(bindingRows);
#undef POOL
    ok &= remember(f->frameLayout);
    if (f->frameLayout != NULL) ok &= remember(f->frameLayout->slots);
    ok &= remember(f->stateMap);
    if (g_family == SP_FM && f == g_fm.candidate.functions) {
        ok &= remember(g_fm.candidate.functions); ok &= remember(g_fm.candidate.constants);
        ok &= remember(g_fm.candidate.layouts); ok &= remember(g_fm.candidate.sourceMaps);
    }
    for (unsigned i = 0u; i < g_extraCount; ++i) ok &= remember(g_extras[i]);
    for (unsigned i = 0u; i < ZrTests_ExecOwner_Stats().records; ++i) {
        unsigned frees;
        if (!ZrTests_ExecOwner_TryRecordFrees(i, &frees)) return 0;
        if (!frees) {
            if (g_liveCount == FM_LIVE) return 0;
            g_live[g_liveCount++] = i;
        }
    }
    return ok;
}
static int stable(void) {
    SZrOwnerHeapStats a = g_family == SP_PF ? g_pfBefore.heap : g_fmBefore.heap;
    SZrOwnerHeapStats b = ZrTests_ExecOwner_Stats();
    int ok = a.errors == 0u && b.errors == 0u && a.live == b.live;
    ok &= memcmp(g_observed, &g_functionBefore, sizeof(*g_observed)) == 0;
    if (g_family == SP_PF) {
        Pf_Observe(&g_pfAfter);
        ok &= !g_pfBefore.error && !g_pfAfter.error && g_pfBefore.length == g_pfAfter.length;
        ok &= memcmp(g_pfBefore.bytes, g_pfAfter.bytes, g_pfBefore.length) == 0;
    } else {
        Fm_Observe(&g_fmAfter);
        ok &= !g_fmBefore.error && !g_fmAfter.error && g_fmBefore.length == g_fmAfter.length;
        ok &= memcmp(g_fmBefore.bytes, g_fmAfter.bytes, g_fmBefore.length) == 0;
        ok &= g_fmBefore.hasOutput == g_fmAfter.hasOutput &&
              g_fmBefore.outputAddress == g_fmAfter.outputAddress &&
              g_fmBefore.output.count == g_fmAfter.output.count;
        ok &= memcmp(g_fmBefore.output.words, g_fmAfter.output.words,
                     g_fmBefore.output.count * sizeof(*g_fmBefore.output.words)) == 0;
        ok &= Fm_DeferredErrors() == 0u;
        ok &= memcmp(&g_fm.candidate, &g_candidateBefore, sizeof(g_candidateBefore)) == 0;
    }
    for (unsigned i = 0u; i < g_spanCount; ++i) {
        SZrOwnerLiveSpan live;
        if (!ZrTests_ExecOwner_TryLiveSpan(0u, b.records, g_spans[i].pointer, 1u, 1u, &live) ||
            live.allocation != g_spans[i].pointer || live.bytes != g_spans[i].bytes) return 0;
        ok &= memcmp(g_spans[i].content, g_spans[i].pointer, g_spans[i].bytes) == 0;
    }
    for (unsigned i = 0u; i < g_liveCount; ++i) {
        unsigned frees;
        ok &= ZrTests_ExecOwner_TryRecordFrees(g_live[i], &frees) && frees == 0u;
    }
    for (unsigned i = a.records; i < b.records; ++i) {
        unsigned frees;
        ok &= ZrTests_ExecOwner_TryRecordFrees(i, &frees) && frees == 1u;
    }
    return ok;
}
static TZrBool snapshot(const SZrSemanticIrFunction *s, const SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    unsigned first = ZrTests_ExecOwner_Stats().records;
    TZrBool ok = source_snapshot_finalized_primitive_module(s, &g_fm.context, &g_fm.script, &g_fm.identity,
            &g_fm.constants, g_fm.functionPool, 1u, &g_fm.original, f, &g_fm.output, d);
    /* An unexpected success may release the old destination. Observe actual output,
     * never inspect the saved old owner after successful publication. */
    if (ok) Fm_RegisterOutput(first, ZrTests_ExecOwner_Stats().records);
    return ok;
}
void Sp_Matrix(unsigned entrance, unsigned shape, unsigned writable) {
    SZrExecIrFunction *f = function_for(entrance);
    SZrExecIrDiagnostic d, poison;
    SZrOwnerHeapStats a, b;
    TZrBool accepted, baseline;
    unsigned error;
    int baselineStable, unchanged, ok;
    maps();
    memset(&d, 0xA5, sizeof(d)); poison = d;
    ok = before(f);
    bind(f, SP_MULTI);
    baseline = invoke(entrance, writable ? &d : NULL);
    restore(); release();
    baselineStable = stable();
    ok &= baseline && baselineStable;
    if (shape == SP_EMPTY) memset(&g_maps[0], 0, sizeof(g_maps[0]));
    memset(&d, 0xA5, sizeof(d));
    ok &= before(f);
    a = ZrTests_ExecOwner_Stats();
    bind(f, shape);
    accepted = invoke(entrance, writable ? &d : NULL);
    error = entrance == SP_FRAME ? (unsigned)g_storage.error : (unsigned)d.code;
    restore(); release();
    b = ZrTests_ExecOwner_Stats();
    unchanged = stable();
    ok &= unchanged && b.attempts == a.attempts;
    if (shape <= SP_MULTI) {
        ok &= accepted != ZR_FALSE;
        if (writable) ok &= memcmp(&d, &poison, sizeof(d)) == 0;
    } else {
        ok &= accepted == ZR_FALSE;
        if (entrance == SP_FRAME) {
            ok &= g_storage.error == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
            if (writable) ok &= memcmp(&d, &poison, sizeof(d)) == 0;
        } else if (writable) ok &= diagnostic_is(&d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f->functionToken);
    }
    printf("SOURCE_PREFLIGHT_STORAGE entrance=%s shape=%s diag=%s accepted=%u error=%u attempts=%u stable=%u restored=1 released=1 baseline=%u\n",
           entrance_name(entrance), shape_name(shape), writable ? "writable" : "null", (unsigned)accepted,
           error, b.attempts - a.attempts, (unsigned)unchanged, (unsigned)(baseline && baselineStable));
    free_extras();
    TEST_ASSERT_TRUE_MESSAGE(ok, "real storage admission / diagnostic / allocation / restoration contract");
}
void Sp_PublicMalformed(unsigned entrance, unsigned writable) {
    SZrExecIrFunction *f = function_for(entrance);
    SZrExecIrDiagnostic d;
    SZrOwnerHeapStats a, b;
    int ok;
    TZrBool result;
    maps(); ok = before(f); memset(&d, 0xA5, sizeof(d));
    a = ZrTests_ExecOwner_Stats(); bind(f, SP_REAL2_2);
    if (entrance == SP_FRAME)
        result = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(f, &g_pfContext, g_pfRows, 2u, 0u,
                                                            writable ? &d : NULL);
    else result = snapshot(&g_fm.semantic, &g_fm.finalized, writable ? &d : NULL);
    restore(); release(); b = ZrTests_ExecOwner_Stats();
    ok &= stable() && !result && a.attempts == b.attempts;
    if (writable) ok &= diagnostic_is(&d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f->functionToken);
    printf("SOURCE_PREFLIGHT_PUBLIC entrance=%s diag=%s accepted=%u code=%u attempts=%u\n",
           entrance_name(entrance), writable ? "writable" : "null", (unsigned)result,
           (unsigned)d.code, b.attempts - a.attempts);
    free_extras();
    TEST_ASSERT_TRUE_MESSAGE(ok, "public/private snapshot admission before lower verifier/profile");
}
void Sp_Profile(unsigned entrance, unsigned shape, unsigned writable) {
    SZrExecIrFunction *f = function_for(entrance);
    SZrExecIrDiagnostic d, coreDiagnostic, metadataDiagnostic;
    TZrBool core, preflight = ZR_TRUE, profile, result;
    int ok;
    maps(); if (shape == SP_EMPTY) memset(&g_maps[0], 0, sizeof(g_maps[0]));
    ok = before(f); bind(f, shape); memset(&d, 0xA5, sizeof(d));
    core = ZrCore_ExecIr_VerifyFunction(f, ZR_EXEC_IR_VERIFY_ALL, &coreDiagnostic);
    ok &= core && diagnostic_is(&coreDiagnostic, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u);
    if (entrance == SP_FRAME) {
        profile = !source_frame_body_metadata(f);
        result = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(f, &g_pfContext, g_pfRows, 2u, 0u,
                                                            writable ? &d : NULL);
    } else {
        TZrBool module = ZrCore_ExecIr_VerifyModule(&g_fm.original, &coreDiagnostic);
        ok &= module && diagnostic_is(&coreDiagnostic, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u);
        preflight = invoke(entrance, writable ? &metadataDiagnostic : NULL);
        profile = preflight && !source_finalized_module_metadata(&g_witness, f);
        if (writable) ok &= diagnostic_is(&metadataDiagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, f->functionToken);
        /* The healthy unchanged contract fields were proved by Fm_Feature in setUp;
         * direct preflight and metadata demonstrate the exact no-map refusal seam. */
        result = snapshot(&g_fm.semantic, &g_fm.finalized, writable ? &d : NULL);
    }
    restore(); release();
    ok &= preflight && profile && !result && stable();
    if (writable) ok &= diagnostic_is(&d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, f->functionToken);
    printf("SOURCE_PREFLIGHT_PROFILE entrance=%s shape=%s diag=%s core=%u preflight=%u profile=%u accepted=%u code=%u\n",
           entrance_name(entrance), shape_name(shape), writable ? "writable" : "null", (unsigned)core,
           (unsigned)preflight, (unsigned)profile, (unsigned)result, (unsigned)d.code);
    free_extras();
    TEST_ASSERT_TRUE_MESSAGE(ok, "actual Core-valid present map reaches existing no-map profile only");
}
void Sp_Priority(unsigned which, unsigned writable) {
    SZrExecIrFunction *f = function_for(g_family == SP_PF ? SP_FRAME : SP_SELECTED);
    SZrExecIrDiagnostic d;
    EZrExecutionDiagnosticCode code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    TZrMetadataToken token = 0u;
    TZrBool result;
    int ok;
    maps(); ok = before(f); bind(f, SP_REAL2_2); memset(&d, 0xA5, sizeof(d));
    if (g_family == SP_PF) {
        SZrArray saved = g_pfContext.canonicalTypes;
        if (which == 3u) {
            g_pfContext.canonicalTypes.capacity = SIZE_MAX;
            code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW; token = f->functionToken;
        }
        result = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(which == 0u ? NULL : f,
                which == 1u ? NULL : &g_pfContext, which == 2u ? NULL : g_pfRows,
                2u, 0u, writable ? &d : NULL);
        g_pfContext.canonicalTypes = saved;
    } else {
        TZrUInt32 saved = g_fm.original.functionCount;
        if (which == 1u) { g_fm.original.functionCount = 0u; code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED; }
        if (which == 2u) {
            g_fm.original.functionCount = g_fm.original.functionCapacity + 1u;
            code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        }
        if (which == 3u) { code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED; token = f->functionToken; }
        result = snapshot(which == 0u ? NULL : &g_fm.semantic,
                          which == 3u ? f : &g_fm.finalized, writable ? &d : NULL);
        g_fm.original.functionCount = saved;
    }
    restore(); release();
    ok &= !result && stable();
    if (writable) ok &= diagnostic_is(&d, code, token);
    printf("SOURCE_PREFLIGHT_PRIORITY family=%s which=%u diag=%s accepted=%u expected=%u code=%u\n",
           g_family == SP_PF ? "frame" : "snapshot", which, writable ? "writable" : "null",
           (unsigned)result, (unsigned)code, (unsigned)d.code);
    free_extras();
    TEST_ASSERT_TRUE_MESSAGE(ok, "earlier required argument / storage / owner priority remains exact");
}
void Sp_Healthy(void) {
    if (g_family == SP_PF) Pf_Feature(); else Fm_Feature();
}
void Sp_Grow(unsigned writable) {
    SZrParserPlace *places;
    SZrExecIrFunction *f = &g_fm.original.functions[0];
    SZrExecIrDiagnostic d;
    SZrOwnerHeapStats a, b;
    SZrOwnerHeapEvent grow = {0}, failure = {0};
    unsigned site = 0u, measured = 0u, spanCount, spanLimit;
    TZrBool healthy, refused, retry, fullRetry;
    int ok, oldOwner;
    places = extra(SP_GROW_PLACES, sizeof(*places));
    for (unsigned i = 0u; i < SP_GROW_PLACES; ++i) {
        SZrParserPlaceProjection *projection = extra(1u, sizeof(*projection));
        places[i] = g_fm.places[0];
        places[i].id = i + 1u;
        places[i].projections = Fm_Array(projection, sizeof(*projection), 1u, 1u);
    }
    g_placesBefore = g_fm.semantic.places.places;
    ok = before(f);
    g_fm.semantic.places.places = Fm_Array(places, sizeof(*places), SP_GROW_PLACES, SP_GROW_PLACES);
    g_growing = 1u;
    a = ZrTests_ExecOwner_Stats();
    memset(&d, 0xA5, sizeof(d));
    healthy = invoke(SP_SELECTED, writable ? &d : NULL);
    spanCount = (unsigned)g_witness.spanCount; spanLimit = (unsigned)g_witness.spanLimit;
    b = ZrTests_ExecOwner_Stats();
    for (unsigned i = a.attempts; i < b.attempts; ++i) {
        SZrOwnerHeapEvent e;
        if (!ZrTests_ExecOwner_TryEvent(i, &e)) { ok = 0; continue; }
        if (e.kind == 1u && e.bytes == 2u * SOURCE_FINALIZED_INLINE_SPANS * sizeof(SSourceFinalizedSpan)) {
            ++measured; site = i - a.attempts + 1u; grow = e;
        }
    }
    restore(); release();
    ok &= healthy && measured == 1u && spanCount > SOURCE_FINALIZED_INLINE_SPANS &&
          spanLimit == 2u * SOURCE_FINALIZED_INLINE_SPANS && stable();
    /* Refuse to invent a site if the actual healthy storage traversal did not grow. */
    if (!healthy || measured != 1u || !site) {
        printf("SOURCE_PREFLIGHT_GROW diag=%s measured=%u healthy=%u spans=%u site=%u completed=0\n",
               writable ? "writable" : "null", measured, (unsigned)healthy, spanCount, site);
        free_extras();
        TEST_FAIL_MESSAGE("real independent projection spans must cross actual inline127 and expose one growth request");
        return;
    }
    g_summary.faultMeasured += measured;
    ok &= before(f);
    g_fm.semantic.places.places = Fm_Array(places, sizeof(*places), SP_GROW_PLACES, SP_GROW_PLACES);
    g_growing = 1u;
    a = ZrTests_ExecOwner_Stats();
    ok &= ZrTests_ExecOwner_TryArm(site);
    memset(&d, 0xA5, sizeof(d));
    refused = snapshot(&g_fm.semantic, &g_fm.finalized, writable ? &d : NULL);
    ZrTests_ExecOwner_Disarm();
    b = ZrTests_ExecOwner_Stats();
    ok &= ZrTests_ExecOwner_TryEvent(a.attempts + site - 1u, &failure);
    restore(); release();
    oldOwner = stable();
    ok &= !refused && oldOwner && b.failures == a.failures + 1u && failure.failed &&
          failure.kind == grow.kind && failure.count == grow.count && failure.width == grow.width &&
          failure.bytes == grow.bytes;
    if (writable) ok &= diagnostic_is(&d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, f->functionToken);
    ok &= before(f);
    g_fm.semantic.places.places = Fm_Array(places, sizeof(*places), SP_GROW_PLACES, SP_GROW_PLACES);
    g_growing = 1u;
    retry = invoke(SP_SELECTED, writable ? &d : NULL);
    restore(); release();
    ok &= retry && stable();
    /* Restore the complete healthy Source profile before retrying full snapshot.
     * Expanded storage geometry is not claimed as an admitted Source program. */
    fullRetry = snapshot(&g_fm.semantic, &g_fm.finalized, writable ? &d : NULL);
    ok &= fullRetry && Fm_InputLive() && Fm_OutputLive() && Fm_DeferredErrors() == 0u;
    if (writable) ok &= diagnostic_is(&d, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u);
    if (ok) ++g_summary.faultCompleted;
    printf("SOURCE_PREFLIGHT_GROW diag=%s measured=%u spans=%u limit=%u site=%u failed=%u oldowner=%u storage_retry=%u snapshot_retry=%u completed=%u\n",
           writable ? "writable" : "null", measured, spanCount, spanLimit, site, failure.failed,
           (unsigned)oldOwner, (unsigned)retry, (unsigned)fullRetry, (unsigned)(ok != 0));
    free_extras();
    TEST_ASSERT_TRUE_MESSAGE(ok, "real span-growth OOM releases scratch, retains old output, and retries after restoration");
}
