#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define ZR_TEST_SSA_AOT_OWNED_MODULE_NO_REDIRECT
#include "support/ssa_aot_owned_module_fault_shim.h"
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_parser/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/exec_ir_host_aot_target.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "zr_vm_parser/exec_ir_source_frame.h"
#include "zr_vm_parser/exec_ir_source_module_contract.h"
#include "support/ssa_literal_script_fixture.h"

/* This ledger sees only direct allocation calls from the actual owner TU. */
enum { LEDGER_CAPACITY = 8, SOURCE_COUNT = 2 };
typedef struct SAllocation {
    void *livePointer;
    size_t bytes;
    unsigned frees;
} SAllocation;
typedef struct SLedger {
    SAllocation records[LEDGER_CAPACITY];
    unsigned attempts, failures, successes, frees, live, errors;
    size_t lastBytes;
    TZrBool armed;
} SLedger;
typedef struct SSource {
    SZrState *state;
    SZrSsaLiteralScriptFixture fixture;
    SZrExecIrFunction compacted;
    SZrAotIrProjection projection;
    SZrAotIrProjectionDescriptor descriptor;
    SZrAotIrTargetContract target;
} SSource;
/* Value-only witness remains usable after self-source replacement. */
typedef struct SWitness {
    TZrUInt64 semanticHash;
    TZrUInt64 contentsHash;
    TZrInt64 literal;
    SZrAotIrTargetContract target;
    SZrExecutionContract moduleContract;
    SZrAotIrFunction function;
    SZrExecIrStateMap stateMap;
} SWitness;
static SLedger g_ledger;
static SSource g_sources[SOURCE_COUNT];
static SZrAotIrOwnedModule *g_owner;

void *ZrTests_AotOwnedModule_Malloc(size_t bytes) {
    void *pointer;
    ++g_ledger.attempts;
    g_ledger.lastBytes = bytes;
    if (g_ledger.armed) {
        g_ledger.armed = ZR_FALSE;
        ++g_ledger.failures;
        return ZR_NULL;
    }
    if (g_ledger.successes == LEDGER_CAPACITY) {
        ++g_ledger.errors;
        return ZR_NULL;
    }
    pointer = malloc(bytes);
    if (pointer != ZR_NULL) {
        SAllocation *record = &g_ledger.records[g_ledger.successes++];
        record->livePointer = pointer;
        record->bytes = bytes;
        ++g_ledger.live;
    }
    return pointer;
}

void ZrTests_AotOwnedModule_Free(void *pointer) {
    unsigned i;
    if (pointer == ZR_NULL) return;
    for (i = 0; i < g_ledger.successes; ++i) {
        SAllocation *record = &g_ledger.records[i];
        if (record->livePointer == pointer) {
            /* No address remains in the ledger after the real free. */
            record->livePointer = ZR_NULL;
            ++record->frees;
            ++g_ledger.frees;
            --g_ledger.live;
            free(pointer);
            return;
        }
    }
    ++g_ledger.errors; /* Suppress an unknown or duplicate real free. */
}

void setUp(void) {
    unsigned i;
    memset(&g_ledger, 0, sizeof(g_ledger));
    memset(g_sources, 0, sizeof(g_sources));
    g_owner = ZR_NULL;
    for (i = 0; i < SOURCE_COUNT; ++i) {
        ZrTests_SsaLiteralScriptFixture_Init(&g_sources[i].fixture, ZR_NULL);
        ZrCore_ExecIr_FunctionInit(&g_sources[i].compacted);
    }
}

void tearDown(void) {
    unsigned i;
    g_ledger.armed = ZR_FALSE;
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
    for (i = 0; i < SOURCE_COUNT; ++i) {
        SSource *s = &g_sources[i];
        ZrParser_AotIrProjection_FreeDescriptor(&s->descriptor);
        ZrParser_AotIrProjection_Free(&s->projection);
        ZrCore_ExecIr_FreeFunction(&s->compacted);
        ZrTests_SsaLiteralScriptFixture_Free(&s->fixture);
        if (s->state != ZR_NULL) ZrTests_Runtime_State_Destroy(s->state);
        s->state = ZR_NULL;
    }
    /* Fail-safe for an assertion before publication; never counted as proof. */
    for (i = 0; i < g_ledger.successes; ++i) {
        void *pointer = g_ledger.records[i].livePointer;
        if (pointer != ZR_NULL) ZrTests_AotOwnedModule_Free(pointer);
    }
}

static const SZrAotIrModule *prepare(unsigned index, EZrSsaLiteralScriptSource source) {
    SSource *s = &g_sources[index];
    const SZrExecIrFunction *original;
    const SZrCanonicalTypeNode *callable, *primitive;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrAotIrDiagnostic aotDiagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    TZrTypeId type;
    s->state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&s->fixture, s->state);
    TEST_ASSERT_NOT_NULL(s->state);
    ZrTests_SsaLiteralScriptFixture_Prepare(&s->fixture, source);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BindSourceModuleContract(
            &s->fixture.compiler, &s->fixture.module, &diagnostic));
    original = ZrTests_SsaLiteralScriptFixture_Function(&s->fixture);
    callable = ZrParser_CanonicalType_Find(s->fixture.compiler.semanticContext,
            s->fixture.compiler.preSemanticIr.callableTypeId);
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    type = callable->data.function.returnTypeId;
    primitive = ZrParser_CanonicalType_Find(s->fixture.compiler.semanticContext, type);
    TEST_ASSERT_NOT_NULL(primitive);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(original, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &s->fixture.compiler.preSemanticIr, s->fixture.compiler.semanticContext,
            original, &s->compacted, &diagnostic));
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&s->fixture, &s->compacted);
    TEST_ASSERT_EQUAL_UINT32(3u, s->compacted.instructionCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, s->compacted.instructions[1].opcode);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostPrimitiveLayout(
            s->fixture.compiler.semanticContext, type, 1u, &row, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&s->fixture.module, &row, 1u, &range));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_AttachPrimitiveSourceFrame(&s->compacted,
            s->fixture.compiler.semanticContext, s->fixture.module.layouts,
            s->fixture.module.layoutCount, 0u, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&s->compacted, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LowerAotWithCanonicalCallable(&s->compacted,
            s->fixture.module.constants, s->fixture.module.constantCount,
            s->fixture.module.layouts, s->fixture.module.layoutCount,
            s->fixture.compiler.semanticContext, s->fixture.compiler.preSemanticIr.callableTypeId,
            &s->projection, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
            s->fixture.compiler.semanticContext, s->fixture.compiler.preSemanticIr.callableTypeId,
            &s->fixture.module.layouts[0], &s->target, &aotDiagnostic));
    TEST_ASSERT_TRUE(ZrParser_AotIrProjection_BuildDescriptor(&s->projection, &s->target,
            &s->fixture.module.contract, &s->descriptor, &aotDiagnostic));
    TEST_ASSERT_EQUAL_PTR(&s->projection, s->descriptor.owner);
    TEST_ASSERT_EQUAL_PTR(&s->descriptor.function, s->descriptor.module.functions);
    TEST_ASSERT_EQUAL_MEMORY(&s->fixture.module.contract, &s->descriptor.module.contract,
            sizeof(s->fixture.module.contract));
    TEST_ASSERT_EQUAL_MEMORY(&s->projection.contract, &s->descriptor.function.contract,
            sizeof(s->projection.contract));
    TEST_ASSERT_EQUAL_UINT32(3u, s->descriptor.function.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(s->projection.sourceMapCount, s->descriptor.function.sourceMapCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,
            ZrCore_AotIr_ValidateModule(&s->descriptor.module, &aotDiagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, s->descriptor.module.functionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->descriptor.module.constantCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->descriptor.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->descriptor.function.frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->descriptor.function.valueSlotCount);
    TEST_ASSERT_EQUAL_INT64(s->fixture.expectedReturn, (TZrInt64)s->descriptor.constants[0].bits);
    TEST_ASSERT_NULL(s->descriptor.function.gcMap);
    TEST_ASSERT_NOT_NULL(s->descriptor.function.logicalStateMap);
    TEST_ASSERT_EQUAL_UINT32(0u, s->descriptor.function.logicalStateMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->descriptor.function.logicalStateMap->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->descriptor.function.logicalStateMap->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->descriptor.function.logicalStateMap->ownerStateCount);
    return &s->descriptor.module;
}

/* Observe every advertised span, including live addresses/capacities in headers.
 * Actual finite fixtures have a present empty logical map and no GC map. */
static TZrUInt64 bounded_digest(const SZrAotIrModule *module, TZrBool headers) {
    const SZrAotIrFunction *f = &module->functions[0];
    TZrUInt64 hash = headers ? ZrTests_SsaLiteralScriptFixture_DigestBytes(0u, module, sizeof(*module)) : 0u;
#define OBSERVE(p, n) hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, p, n)
#define POOL(field, count) if (f->count != 0u) OBSERVE(f->field, (size_t)f->count * sizeof(*f->field))
    if (headers) OBSERVE(module->functions, (size_t)module->functionCount * sizeof(*module->functions));
    OBSERVE(module->constantPool, (size_t)module->constantCount * sizeof(*module->constantPool));
    OBSERVE(module->layoutPool, (size_t)module->layoutCount * sizeof(*module->layoutPool));
    POOL(frameSlots, frameSlotCount); POOL(valueSlotPool, valueSlotCount);
    POOL(blocks, blockCount); POOL(instructions, instructionCount);
    POOL(operandPool, operandCount); POOL(resultPool, resultCount);
    POOL(phiIncomingPool, phiIncomingCount); POOL(successorPool, successorCount);
    POOL(memoryTokenPool, memoryTokenCount); POOL(sourceMaps, sourceMapCount);
    POOL(gcRootPool, gcRootCount); POOL(deoptStates, deoptStateCount);
    POOL(deoptValuePool, deoptValueCount); POOL(deoptAggregates, deoptAggregateCount);
    POOL(deoptAggregateFields, deoptAggregateFieldCount);
    if (headers) OBSERVE(f->logicalStateMap, sizeof(*f->logicalStateMap));
#undef POOL
#undef OBSERVE
    return hash;
}
static TZrUInt64 observation(const SZrAotIrModule *module) {
    return bounded_digest(module, ZR_TRUE);
}

static SWitness witness(const SZrAotIrModule *m) {
    SWitness w;
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    memset(&w, 0, sizeof(w));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(m, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,
            ZrCore_AotIr_RequireExecutableAbi(m, m->functions[0].id, &abi, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    w.semanticHash = ZrCore_AotIr_HashModule(m);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, w.semanticHash);
    w.contentsHash = bounded_digest(m, ZR_FALSE);
    w.literal = (TZrInt64)m->constantPool[0].bits;
    w.target = m->target;
    w.moduleContract = m->contract;
    w.function.id = m->functions[0].id;
    w.function.functionToken = m->functions[0].functionToken;
    w.function.signatureHash = m->functions[0].signatureHash;
    w.function.contract = m->functions[0].contract;
    w.function.frameLayout = m->functions[0].frameLayout;
    w.function.callableAbi = m->functions[0].callableAbi;
    w.stateMap.functionToken = m->functions[0].logicalStateMap->functionToken;
    w.stateMap.signatureHash = m->functions[0].logicalStateMap->signatureHash;
    w.stateMap.generation = m->functions[0].logicalStateMap->generation;
    return w;
}

static void assert_candidate(const SWitness *w) {
    const SZrAotIrModule *m = ZrCore_AotIr_OwnedModuleView(g_owner);
    const SZrAotIrFunction *f;
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(m, &diagnostic));
    f = &m->functions[0];
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(m, f->id, &abi, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_MEMORY(&w->function.callableAbi, &abi, sizeof(abi));
    TEST_ASSERT_EQUAL_UINT64(w->semanticHash, ZrCore_AotIr_HashModule(m));
    TEST_ASSERT_EQUAL_UINT64(w->contentsHash, bounded_digest(m, ZR_FALSE));
    TEST_ASSERT_EQUAL_INT64(w->literal, (TZrInt64)m->constantPool[0].bits);
    TEST_ASSERT_EQUAL_MEMORY(&w->target, &m->target, sizeof(m->target));
    TEST_ASSERT_EQUAL_MEMORY(&w->moduleContract, &m->contract, sizeof(m->contract));
    TEST_ASSERT_EQUAL_UINT32(w->function.id, f->id);
    TEST_ASSERT_EQUAL_UINT32(w->function.functionToken, f->functionToken);
    TEST_ASSERT_EQUAL_UINT64(w->function.signatureHash, f->signatureHash);
    TEST_ASSERT_EQUAL_MEMORY(&w->function.contract, &f->contract, sizeof(f->contract));
    TEST_ASSERT_EQUAL_MEMORY(&w->function.frameLayout, &f->frameLayout, sizeof(f->frameLayout));
    TEST_ASSERT_NOT_NULL(f->logicalStateMap);
    TEST_ASSERT_EQUAL_UINT32(w->stateMap.functionToken, f->logicalStateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT64(w->stateMap.signatureHash, f->logicalStateMap->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(w->stateMap.generation, f->logicalStateMap->generation);
    TEST_ASSERT_NULL(f->gcMap);
#define EMPTY(field, count) do { TEST_ASSERT_EQUAL_UINT32(0u, f->count); TEST_ASSERT_NULL(f->field); } while (0)
    EMPTY(phiIncomingPool, phiIncomingCount); EMPTY(memoryTokenPool, memoryTokenCount);
    EMPTY(gcRootPool, gcRootCount); EMPTY(deoptStates, deoptStateCount);
    EMPTY(deoptValuePool, deoptValueCount); EMPTY(deoptAggregates, deoptAggregateCount);
    EMPTY(deoptAggregateFields, deoptAggregateFieldCount);
#undef EMPTY
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->entryCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->valueCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->rootCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->ownerStateCount);
    TEST_ASSERT_EQUAL_UINT32(0u, f->logicalStateMap->ownerStateCapacity);
    TEST_ASSERT_NULL(f->logicalStateMap->entries);
    TEST_ASSERT_NULL(f->logicalStateMap->valuePool);
    TEST_ASSERT_NULL(f->logicalStateMap->rootPool);
    TEST_ASSERT_NULL(f->logicalStateMap->ownerStatePool);
}

static void assert_copies(const SZrAotIrModule *source) {
    const SZrAotIrModule *m = ZrCore_AotIr_OwnedModuleView(g_owner);
    const SZrAotIrFunction *a = &source->functions[0], *b = &m->functions[0];
    TEST_ASSERT_TRUE(m != source);
    TEST_ASSERT_TRUE(m->functions != source->functions);
    TEST_ASSERT_TRUE(m->constantPool != source->constantPool);
    TEST_ASSERT_TRUE(m->layoutPool != source->layoutPool);
    TEST_ASSERT_EQUAL_MEMORY(source->constantPool, m->constantPool,
            (size_t)m->constantCount * sizeof(*m->constantPool));
    TEST_ASSERT_EQUAL_MEMORY(source->layoutPool, m->layoutPool,
            (size_t)m->layoutCount * sizeof(*m->layoutPool));
#define COPY(field, count) do { \
    TEST_ASSERT_EQUAL_UINT32(a->count, b->count); \
    if (a->count != 0u) { \
        TEST_ASSERT_TRUE(a->field != b->field); \
        TEST_ASSERT_EQUAL_MEMORY(a->field, b->field, (size_t)a->count * sizeof(*a->field)); \
    } else TEST_ASSERT_NULL(b->field); \
} while (0)
    COPY(frameSlots, frameSlotCount); COPY(valueSlotPool, valueSlotCount);
    COPY(blocks, blockCount); COPY(instructions, instructionCount);
    COPY(operandPool, operandCount); COPY(resultPool, resultCount);
    COPY(phiIncomingPool, phiIncomingCount); COPY(successorPool, successorCount);
    COPY(memoryTokenPool, memoryTokenCount); COPY(sourceMaps, sourceMapCount);
    COPY(gcRootPool, gcRootCount); COPY(deoptStates, deoptStateCount);
    COPY(deoptValuePool, deoptValueCount); COPY(deoptAggregates, deoptAggregateCount);
    COPY(deoptAggregateFields, deoptAggregateFieldCount);
#undef COPY
    TEST_ASSERT_NULL(b->gcMap);
    TEST_ASSERT_TRUE(a->logicalStateMap != b->logicalStateMap);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->entryCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->valueCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->rootCapacity);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->ownerStateCount);
    TEST_ASSERT_EQUAL_UINT32(0u, b->logicalStateMap->ownerStateCapacity);
    TEST_ASSERT_NULL(b->logicalStateMap->entries);
    TEST_ASSERT_NULL(b->logicalStateMap->valuePool);
    TEST_ASSERT_NULL(b->logicalStateMap->rootPool);
    TEST_ASSERT_NULL(b->logicalStateMap->ownerStatePool);
}

static void release_and_assert(void) {
    unsigned i, frees;
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
    frees = g_ledger.frees;
    TEST_ASSERT_NULL(g_owner);
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.errors);
    TEST_ASSERT_EQUAL_UINT(g_ledger.successes, g_ledger.frees);
    for (i = 0; i < g_ledger.successes; ++i)
        TEST_ASSERT_EQUAL_UINT(1u, g_ledger.records[i].frees);
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
    TEST_ASSERT_EQUAL_UINT(frees, g_ledger.frees);
}

static void clone_initial(const SZrAotIrModule *source) {
    SZrAotIrDiagnostic diagnostic = {0};
    EZrAotIrStatus status = ZrCore_AotIr_CloneOwnedModule(source, &g_owner, &diagnostic);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, status);
    TEST_ASSERT_NOT_NULL(g_owner);
    TEST_ASSERT_EQUAL_UINT(1u, g_ledger.successes);
    TEST_ASSERT_EQUAL_UINT(1u, g_ledger.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.errors);
}

/* mode 0: initial; 1: independent 9 -> 8; 2: exact owner view self-source. */
static void oom_then_retry(unsigned mode) {
    const SZrAotIrModule *source = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    const SZrAotIrModule *oldView = ZR_NULL;
    SZrAotIrOwnedModule *oldOwner = ZR_NULL;
    TZrUInt64 before, oldBefore = 0u, nineBefore = observation(source);
    const SZrAotIrModule *nine = source;
    SWitness saved;
    SZrAotIrDiagnostic diagnostic, expected = {0};
    EZrAotIrStatus status;
    unsigned attempts, failures, successes, frees, oldRecord = 0u;
    size_t bytes;
    if (mode != 0u) {
        clone_initial(source);
        oldOwner = g_owner;
        oldView = ZrCore_AotIr_OwnedModuleView(g_owner);
        oldBefore = observation(oldView);
        if (mode == 1u) source = prepare(1u, ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
        else source = oldView;
    }
    saved = witness(source);
    before = observation(source);
    attempts = g_ledger.attempts; failures = g_ledger.failures;
    successes = g_ledger.successes; frees = g_ledger.frees;
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    g_ledger.armed = ZR_TRUE;
    status = ZrCore_AotIr_CloneOwnedModule(source, &g_owner, &diagnostic);
    g_ledger.armed = ZR_FALSE;
    bytes = g_ledger.lastBytes;
    expected.status = ZR_AOT_IR_OUT_OF_MEMORY;
    expected.expected = (TZrUInt64)bytes;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OUT_OF_MEMORY, status);
    TEST_ASSERT_EQUAL_MEMORY(&expected, &diagnostic, sizeof(expected));
    TEST_ASSERT_TRUE(bytes > 0u);
    TEST_ASSERT_EQUAL_UINT(attempts + 1u, g_ledger.attempts);
    TEST_ASSERT_EQUAL_UINT(failures + 1u, g_ledger.failures);
    TEST_ASSERT_EQUAL_UINT(successes, g_ledger.successes);
    TEST_ASSERT_EQUAL_UINT(frees, g_ledger.frees);
    TEST_ASSERT_EQUAL_UINT(successes, g_ledger.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.errors);
    TEST_ASSERT_EQUAL_PTR(oldOwner, g_owner);
    TEST_ASSERT_EQUAL_PTR(oldView, ZrCore_AotIr_OwnedModuleView(g_owner));
    TEST_ASSERT_EQUAL_UINT64(before, observation(source));
    TEST_ASSERT_EQUAL_UINT64(nineBefore, observation(nine));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(source, &diagnostic));
    if (mode != 0u) {
        TEST_ASSERT_EQUAL_UINT64(oldBefore, observation(oldView));
        TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(oldView, &diagnostic));
        TEST_ASSERT_EQUAL_UINT(0u, g_ledger.records[oldRecord].frees);
    }
    status = ZrCore_AotIr_CloneOwnedModule(source, &g_owner, &diagnostic);
    /* oldOwner/oldView/source are never used after successful self-publication. */
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, status);
    TEST_ASSERT_EQUAL_UINT(attempts + 2u, g_ledger.attempts);
    TEST_ASSERT_EQUAL_UINT(failures + 1u, g_ledger.failures);
    TEST_ASSERT_EQUAL_UINT(successes + 1u, g_ledger.successes);
    TEST_ASSERT_EQUAL_UINT(frees + (mode != 0u ? 1u : 0u), g_ledger.frees);
    TEST_ASSERT_EQUAL_UINT64(bytes, g_ledger.lastBytes);
    TEST_ASSERT_EQUAL_UINT(1u, g_ledger.live);
    if (mode != 0u) TEST_ASSERT_EQUAL_UINT(1u, g_ledger.records[oldRecord].frees);
    assert_candidate(&saved);
    if (mode != 2u) {
        assert_copies(source);
        TEST_ASSERT_EQUAL_UINT64(before, observation(source));
    }
    TEST_ASSERT_EQUAL_UINT64(nineBefore, observation(nine));
    release_and_assert();
}
static void test_initial_oom_retry_nine(void) { oom_then_retry(0u); }
static void test_replacement_oom_retry_nine_to_eight(void) { oom_then_retry(1u); }
static void test_self_source_oom_retry(void) { oom_then_retry(2u); }

static void invalid_source(TZrBool opcode) {
    const SZrAotIrModule *source = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SSource *s = &g_sources[0];
    SZrAotIrDiagnostic direct = {0}, clone = {0};
    SZrAotIrInstruction instruction = s->descriptor.instructions[1];
    TZrUInt64 signature = s->descriptor.function.contract.signatureHash;
    SZrAotIrOwnedModule *oldOwner;
    const SZrAotIrModule *oldView;
    TZrUInt64 before, oldBefore;
    EZrAotIrStatus directStatus, cloneStatus;
    unsigned attempts, failures, frees;
    clone_initial(source);
    oldOwner = g_owner; oldView = ZrCore_AotIr_OwnedModuleView(g_owner);
    before = observation(source); oldBefore = observation(oldView);
    attempts = g_ledger.attempts; failures = g_ledger.failures; frees = g_ledger.frees;
    if (opcode) s->descriptor.instructions[1].flags |= ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK + 1u;
    else s->descriptor.function.contract.signatureHash ^= UINT64_C(1);
    directStatus = ZrCore_AotIr_ValidateModule(source, &direct);
    g_ledger.armed = ZR_TRUE;
    cloneStatus = ZrCore_AotIr_CloneOwnedModule(source, &g_owner, &clone);
    {
        TZrBool stillArmed = g_ledger.armed;
        g_ledger.armed = ZR_FALSE;
        s->descriptor.instructions[1] = instruction;
        s->descriptor.function.contract.signatureHash = signature;
        TEST_ASSERT_TRUE(stillArmed);
    }
    TEST_ASSERT_NOT_EQUAL(ZR_AOT_IR_OK, directStatus);
    TEST_ASSERT_NOT_EQUAL(ZR_AOT_IR_OUT_OF_MEMORY, directStatus);
    if (opcode) TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_OPCODE, directStatus);
    TEST_ASSERT_EQUAL_INT(directStatus, cloneStatus);
    TEST_ASSERT_EQUAL_MEMORY(&direct, &clone, sizeof(direct));
    TEST_ASSERT_EQUAL_UINT(attempts, g_ledger.attempts);
    TEST_ASSERT_EQUAL_UINT(failures, g_ledger.failures);
    TEST_ASSERT_EQUAL_UINT(frees, g_ledger.frees);
    TEST_ASSERT_EQUAL_PTR(oldOwner, g_owner);
    TEST_ASSERT_EQUAL_PTR(oldView, ZrCore_AotIr_OwnedModuleView(g_owner));
    TEST_ASSERT_EQUAL_UINT64(before, observation(source));
    TEST_ASSERT_EQUAL_UINT64(oldBefore, observation(oldView));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(source, &direct));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(oldView, &direct));
    release_and_assert();
}
static void test_invalid_flags_precede_allocation(void) { invalid_source(ZR_TRUE); }
static void test_invalid_contract_precedes_allocation(void) { invalid_source(ZR_FALSE); }

static void rejected_span(size_t start, size_t count, size_t width, size_t alignment) {
    size_t cursor = start, offset = 17u;
    TEST_ASSERT_FALSE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, count, width, alignment, &offset));
    TEST_ASSERT_EQUAL_UINT64(start, cursor);
    TEST_ASSERT_EQUAL_UINT64(17u, offset);
}
static void test_exact_size_max_fit(void) {
    size_t cursor = SIZE_MAX - 8u, offset = 0u;
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 8u, 1u, 1u, &offset));
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX - 8u, offset);
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, cursor);
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_AlignmentBound() >= 2u);
    cursor = 1u;
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 1u, 1u, 2u, &offset));
    TEST_ASSERT_EQUAL_UINT64(2u, offset);
    TEST_ASSERT_EQUAL_UINT64(3u, cursor);
}
static void test_multiplication_overflow(void) { rejected_span(0u, SIZE_MAX, 2u, 1u); }
static void test_padding_overflow(void) {
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_AlignmentBound() >= 2u);
    rejected_span(SIZE_MAX, 1u, 1u, 2u);
}
static void test_extent_addition_overflow(void) { rejected_span(SIZE_MAX - 1u, 2u, 1u, 1u); }
static void test_zero_count_without_padding(void) {
    size_t cursor = SIZE_MAX, offset = 0u;
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_AlignmentBound() >= 2u);
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 0u, 1u, 2u, &offset));
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, cursor);
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, offset);
    cursor = 3u;
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 0u, 1u, 2u, &offset));
    TEST_ASSERT_EQUAL_UINT64(3u, cursor);
    TEST_ASSERT_EQUAL_UINT64(3u, offset);
}
static void test_invalid_width(void) { rejected_span(3u, 1u, 0u, 1u); rejected_span(3u, 0u, 0u, 1u); }
static void test_zero_alignment(void) { rejected_span(3u, 1u, 1u, 0u); rejected_span(3u, 0u, 1u, 0u); }
static void test_nonpower_alignment(void) { rejected_span(3u, 1u, 1u, 3u); rejected_span(3u, 0u, 1u, 3u); }
static void test_null_cursor(void) {
    size_t offset = 17u;
    TEST_ASSERT_FALSE(ZrTests_AotOwnedModule_CheckedSpan(ZR_NULL, 1u, 1u, 1u, &offset));
    TEST_ASSERT_EQUAL_UINT64(17u, offset);
}
static void test_null_offset(void) {
    size_t cursor = 3u;
    TEST_ASSERT_FALSE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 1u, 1u, 1u, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(3u, cursor);
}
static void test_status_stability(void) {
    static const EZrAotIrStatus statuses[] = {
        ZR_AOT_IR_OK, ZR_AOT_IR_INVALID_ARGUMENT, ZR_AOT_IR_VERSION_MISMATCH,
        ZR_AOT_IR_INVALID_TARGET, ZR_AOT_IR_INVALID_CONTRACT, ZR_AOT_IR_INVALID_ID,
        ZR_AOT_IR_DUPLICATE_ID, ZR_AOT_IR_INVALID_RANGE, ZR_AOT_IR_INVALID_CFG,
        ZR_AOT_IR_INVALID_OPCODE, ZR_AOT_IR_INVALID_EFFECT, ZR_AOT_IR_INVALID_LAYOUT,
        ZR_AOT_IR_INVALID_SIGNATURE, ZR_AOT_IR_RELOCATION, ZR_AOT_IR_UNSUPPORTED,
        ZR_AOT_IR_OUT_OF_MEMORY
    };
    static const char *names[] = {
        "ok", "invalid_argument", "version_mismatch", "invalid_target", "invalid_contract",
        "invalid_id", "duplicate_id", "invalid_range", "invalid_cfg", "invalid_opcode",
        "invalid_effect", "invalid_layout", "invalid_signature", "relocation", "unsupported",
        "out_of_memory"
    };
    unsigned i;
    for (i = 0u; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(i, statuses[i]);
        TEST_ASSERT_EQUAL_STRING(names[i], ZrCore_AotIr_StatusName(statuses[i]));
    }
    TEST_ASSERT_EQUAL_STRING("unknown", ZrCore_AotIr_StatusName((EZrAotIrStatus)999));
}
static void test_actual_alignment_bound(void) {
    size_t bound = ZrTests_AotOwnedModule_AlignmentBound(), cursor = 0u, offset = 17u;
    TEST_ASSERT_TRUE(bound > 0u);
    TEST_ASSERT_EQUAL_UINT64(0u, bound & (bound - 1u));
    TEST_ASSERT_TRUE(bound <= SIZE_MAX / 2u);
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 1u, 1u, bound, &offset));
    TEST_ASSERT_EQUAL_UINT64(0u, offset);
    TEST_ASSERT_EQUAL_UINT64(1u, cursor);
    cursor = 3u;
    TEST_ASSERT_TRUE(ZrTests_AotOwnedModule_CheckedSpan(&cursor, 0u, 1u, bound, &offset));
    TEST_ASSERT_EQUAL_UINT64(3u, cursor);
    TEST_ASSERT_EQUAL_UINT64(3u, offset);
    rejected_span(3u, 1u, 1u, 2u * bound);
    rejected_span(3u, 0u, 1u, 2u * bound);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_oom_retry_nine);
    RUN_TEST(test_replacement_oom_retry_nine_to_eight);
    RUN_TEST(test_self_source_oom_retry);
    RUN_TEST(test_invalid_flags_precede_allocation);
    RUN_TEST(test_invalid_contract_precedes_allocation);
    RUN_TEST(test_exact_size_max_fit);
    RUN_TEST(test_multiplication_overflow);
    RUN_TEST(test_padding_overflow);
    RUN_TEST(test_extent_addition_overflow);
    RUN_TEST(test_zero_count_without_padding);
    RUN_TEST(test_invalid_width);
    RUN_TEST(test_zero_alignment);
    RUN_TEST(test_nonpower_alignment);
    RUN_TEST(test_null_cursor);
    RUN_TEST(test_null_offset);
    RUN_TEST(test_status_stability);
    RUN_TEST(test_actual_alignment_bound);
    return UNITY_END();
}
