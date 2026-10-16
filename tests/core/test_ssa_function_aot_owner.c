#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ZR_TEST_SSA_AOT_OWNED_MODULE_NO_REDIRECT
#include "tests/parser/support/ssa_aot_owned_module_fault_shim.h"
#include "support/ssa_function_aot_owner_fixture.h"
#include "unity.h"
#include "zr_vm_core/function_aot_ir.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"

enum { VM_RECORDS = 8192, VM_DIAGNOSTICS = 64, ARENA_RECORDS = 32,
       PRODUCERS = 4, RAW_FUNCTIONS = 16 };
typedef struct SAllocation {
    void *livePointer;
    size_t bytes;
    TZrInt64 flag;
    unsigned frees, allocatedAt, freedAt;
} SAllocation;
typedef struct SVmDiagnostic {
    unsigned reason, record, operation;
    size_t allocatedBytes, originalSize, newSize;
    TZrInt64 priorFlag, requestFlag;
} SVmDiagnostic;
typedef struct SVmLedger {
    SAllocation records[VM_RECORDS];
    unsigned live, errors, allocations, frees;
    SVmDiagnostic diagnostics[VM_DIAGNOSTICS];
    unsigned diagnosticCount, diagnosticsDropped;
} SVmLedger;
typedef struct SArenaLedger {
    SAllocation records[ARENA_RECORDS];
    unsigned attempts, failures, successes, frees, live, errors, sequence;
    size_t lastBytes;
    TZrBool armed;
} SArenaLedger;
static SVmLedger g_vm;
static SArenaLedger g_arena;
static SZrGlobalState *g_global;
static SZrState *g_state;
static TZrBool g_savedStopGc, g_haveSavedStopGc;
static SZrFunction *g_functions[RAW_FUNCTIONS];
static SZrTestsFunctionAotProducer *g_producers[PRODUCERS];
static SZrAotIrOwnedModule *g_clone;

static void record_vm_error(SVmLedger *ledger, unsigned reason, unsigned record,
                            unsigned operation, size_t originalSize,
                            size_t newSize, TZrInt64 flag) {
    SVmDiagnostic *diagnostic;
    ++ledger->errors;
    if (ledger->diagnosticCount == VM_DIAGNOSTICS) {
        ++ledger->diagnosticsDropped;
        return;
    }
    diagnostic = &ledger->diagnostics[ledger->diagnosticCount++];
    diagnostic->reason = reason;
    diagnostic->record = record;
    diagnostic->operation = operation;
    diagnostic->originalSize = originalSize;
    diagnostic->newSize = newSize;
    diagnostic->requestFlag = flag;
    if (record < VM_RECORDS) {
        diagnostic->allocatedBytes = ledger->records[record].bytes;
        diagnostic->priorFlag = ledger->records[record].flag;
    }
}

/* No Unity assertions or dynamic bookkeeping occur in either allocator. */
static TZrPtr vm_allocator(TZrPtr userData, TZrPtr pointer,
                           TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    SVmLedger *ledger = userData;
    unsigned index = VM_RECORDS;
    unsigned operation = pointer == ZR_NULL ? 0u : (newSize == 0u ? 1u : 2u);
    void *replacement;
    if (pointer != ZR_NULL) {
        for (unsigned i = 0u; i < VM_RECORDS; ++i)
            if (ledger->records[i].livePointer == pointer) { index = i; break; }
        if (index == VM_RECORDS) {
            record_vm_error(ledger, 1u, index, operation, originalSize, newSize, flag);
            return ZR_NULL;
        }
        if (ledger->records[index].bytes != originalSize ||
            ledger->records[index].flag != flag)
            record_vm_error(ledger, 2u, index, operation, originalSize, newSize, flag);
        ledger->records[index].livePointer = ZR_NULL;
        --ledger->live;
        if (newSize == 0u) {
            ++ledger->records[index].frees;
            ++ledger->frees;
            free(pointer);
            return ZR_NULL;
        }
    } else {
        for (unsigned i = 0u; i < VM_RECORDS; ++i)
            if (ledger->records[i].livePointer == ZR_NULL) { index = i; break; }
        if (index == VM_RECORDS || newSize == 0u) {
            record_vm_error(ledger, 3u, index, operation, originalSize, newSize, flag);
            return ZR_NULL;
        }
    }
    replacement = realloc(pointer, newSize);
    if (replacement == ZR_NULL) {
        if (pointer != ZR_NULL) {
            ledger->records[index].livePointer = pointer; /* still live on failure */
            ++ledger->live;
        }
        record_vm_error(ledger, 4u, index, operation, originalSize, newSize, flag);
        return ZR_NULL;
    }
    ledger->records[index].livePointer = replacement;
    ledger->records[index].bytes = newSize;
    ledger->records[index].flag = flag;
    ++ledger->live;
    ++ledger->allocations;
    return replacement;
}

void *ZrTests_AotOwnedModule_Malloc(size_t bytes) {
    void *pointer;
    unsigned index;
    ++g_arena.attempts;
    g_arena.lastBytes = bytes;
    if (g_arena.armed) {
        g_arena.armed = ZR_FALSE;
        ++g_arena.failures;
        return ZR_NULL;
    }
    index = g_arena.successes;
    if (index == ARENA_RECORDS) { ++g_arena.errors; return ZR_NULL; }
    pointer = malloc(bytes);
    if (pointer == ZR_NULL) { ++g_arena.failures; return ZR_NULL; }
    g_arena.records[index].livePointer = pointer;
    g_arena.records[index].bytes = bytes;
    g_arena.records[index].allocatedAt = ++g_arena.sequence;
    ++g_arena.successes;
    ++g_arena.live;
    return pointer;
}

void ZrTests_AotOwnedModule_Free(void *pointer) {
    unsigned index;
    if (pointer == ZR_NULL) return;
    for (index = 0u; index < g_arena.successes; ++index)
        if (g_arena.records[index].livePointer == pointer) break;
    if (index == g_arena.successes) { ++g_arena.errors; return; }
    g_arena.records[index].livePointer = ZR_NULL;
    ++g_arena.records[index].frees;
    g_arena.records[index].freedAt = ++g_arena.sequence;
    ++g_arena.frees;
    --g_arena.live;
    free(pointer);
}

static void shutdown_global(void) {
    SZrGlobalState *live = g_global;
    if (live == ZR_NULL) return;
    if (g_haveSavedStopGc) live->garbageCollector->stopGcFlag = g_savedStopGc;
    g_haveSavedStopGc = ZR_FALSE;
    memset(g_functions, 0, sizeof(g_functions));
    g_state = ZR_NULL;
    g_global = ZR_NULL;
    ZrCore_GlobalState_Free(live);
    /* Values only, after destruction; no allocator callback performs I/O. */
    for (unsigned i = 0u; i < g_vm.diagnosticCount; ++i) {
        const SVmDiagnostic *d = &g_vm.diagnostics[i];
        fprintf(stderr, "VM_LEDGER reason=%u record=%u operation=%u allocated=%llu original=%llu new=%llu priorFlag=%lld requestFlag=%lld\n",
                d->reason, d->record, d->operation,
                (unsigned long long)d->allocatedBytes,
                (unsigned long long)d->originalSize, (unsigned long long)d->newSize,
                (long long)d->priorFlag, (long long)d->requestFlag);
    }
    if (g_vm.diagnosticsDropped != 0u)
        fprintf(stderr, "VM_LEDGER dropped=%u\n", g_vm.diagnosticsDropped);
}

void setUp(void) {
    /* A failed cleanup must not erase records for still-live allocations. */
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.live);
    memset(&g_vm, 0, sizeof(g_vm));
    memset(&g_arena, 0, sizeof(g_arena));
    memset(g_producers, 0, sizeof(g_producers));
    memset(g_functions, 0, sizeof(g_functions));
    g_global = ZR_NULL;
    g_state = ZR_NULL;
    g_clone = ZR_NULL;
    g_haveSavedStopGc = ZR_FALSE;
}

void tearDown(void) {
    g_arena.armed = ZR_FALSE;
    for (unsigned i = 0u; i < PRODUCERS; ++i)
        ZrTests_FunctionAotProducer_Free(&g_producers[i]);
    ZrCore_AotIr_OwnedModuleFree(&g_clone);
    shutdown_global();
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.errors);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}

static SZrTestsFunctionAotProducer *producer(unsigned slot, TZrInt64 literal) {
    g_producers[slot] = ZrTests_FunctionAotProducer_New(literal);
    TEST_ASSERT_NOT_NULL(g_producers[slot]);
    return g_producers[slot];
}

static void open_global(void) {
    g_global = ZrCore_GlobalState_New(vm_allocator, &g_vm, 0xA017u, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_global);
    g_state = g_global->mainThreadState;
    TEST_ASSERT_NOT_NULL(g_state);
    TEST_ASSERT_NOT_NULL(g_global->garbageCollector);
    g_savedStopGc = g_global->garbageCollector->stopGcFlag;
    g_haveSavedStopGc = ZR_TRUE;
    g_global->garbageCollector->stopGcFlag = ZR_TRUE;
    TEST_ASSERT_NULL(g_global->profileRuntime);
    TEST_ASSERT_NULL(g_global->sourceLoader);
    TEST_ASSERT_NULL(g_global->aotModuleLoader);
    TEST_ASSERT_NULL(g_global->nativeModuleLoader);
    TEST_ASSERT_NULL(g_global->providerModuleNameResolver);
    TEST_ASSERT_NULL(g_global->callBindingModuleResolver);
    TEST_ASSERT_NULL(g_global->typedCallBindingResolver);
    TEST_ASSERT_NULL(g_global->compileSource);
    TEST_ASSERT_NULL(g_global->nativeRegistryStateCleanup);
    TEST_ASSERT_NULL(g_global->parserModuleInitStateCleanup);
    TEST_ASSERT_NULL(g_global->postGcCleanup);
    TEST_ASSERT_NULL(g_global->callbacks.afterStateInitialized);
    TEST_ASSERT_NULL(g_global->callbacks.beforeStateReleased);
    TEST_ASSERT_NULL(g_global->callbacks.afterThreadInitialized);
    TEST_ASSERT_NULL(g_global->callbacks.beforeThreadReleased);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}

static SZrFunction *new_function(unsigned slot) {
    g_functions[slot] = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_functions[slot]);
    return g_functions[slot];
}

static unsigned attach(SZrFunction *function, const SZrAotIrModule *source) {
    unsigned record = g_arena.successes, attempts = g_arena.attempts;
    SZrAotIrDiagnostic diagnostic = {0};
    EZrAotIrStatus status = ZrCore_Function_SnapshotAotIr(function, source, &diagnostic);
    /* Safe refusal stub fails this actual successful-association assertion. */
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, status);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT(attempts + 1u, g_arena.attempts);
    TEST_ASSERT_EQUAL_UINT(record + 1u, g_arena.successes);
    TEST_ASSERT_NOT_NULL(ZrCore_Function_GetAotIr(function));
    return record;
}

static void assert_freed(unsigned record, unsigned frees) {
    TEST_ASSERT_EQUAL_UINT(frees, g_arena.records[record].frees);
}

static void assert_balanced(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.live);
    TEST_ASSERT_EQUAL_UINT(g_arena.successes, g_arena.frees);
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.errors);
}

static void finish_shutdown(void) {
    unsigned arenaFrees = g_arena.frees;
    shutdown_global();
    TEST_ASSERT_EQUAL_UINT(arenaFrees, g_arena.frees);
    assert_balanced();
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}

static void assert_diagnostic(const SZrAotIrDiagnostic *expected,
                              const SZrAotIrDiagnostic *actual) {
    TEST_ASSERT_EQUAL_INT(expected->status, actual->status);
    TEST_ASSERT_EQUAL_UINT32(expected->functionId, actual->functionId);
    TEST_ASSERT_EQUAL_UINT32(expected->blockId, actual->blockId);
    TEST_ASSERT_EQUAL_UINT32(expected->instructionId, actual->instructionId);
    TEST_ASSERT_EQUAL_UINT32(expected->index, actual->index);
    TEST_ASSERT_EQUAL_UINT64(expected->expected, actual->expected);
    TEST_ASSERT_EQUAL_UINT64(expected->actual, actual->actual);
}

/* Publish storage and complete the real copy+Detach protocol before asserting. */
static SZrFunction *move_child(SZrFunction *parent, SZrFunction *donor) {
    SZrFunction *child = ZrCore_Memory_RawMallocWithType(g_global,
            sizeof(*child), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    parent->childFunctionList = child;
    parent->childFunctionLength = child != ZR_NULL ? 1u : 0u;
    if (child != ZR_NULL) {
        *child = *donor;
        child->ownerFunction = parent;
        ZrCore_Function_DetachOwnedBuffers(donor);
    }
    TEST_ASSERT_NOT_NULL(child);
    return child;
}

static void test_raw_scalar_fixture_validates(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrTestsFunctionAotWitness witness;
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,
            ZrCore_AotIr_ValidateModule(&p->module, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,
            ZrCore_AotIr_RequireExecutableAbi(&p->module, 1u, &abi, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_UINT32(17u, abi.returnTypeToken);
    TEST_ASSERT_EQUAL_UINT64(9u, p->constant.bits);
    TEST_ASSERT_EQUAL_UINT32(16u, p->function.frameLayout.frameByteSize);
    TEST_ASSERT_EQUAL_UINT32(8u, p->slot.byteSize);
    TEST_ASSERT_EQUAL_UINT32(0u, p->valueSlot);
    TEST_ASSERT_EQUAL_UINT32(2u, p->block.terminatorInstructionId);
    TEST_ASSERT_EQUAL_UINT32(101u, p->sourceMaps[0].sourceId);
    TEST_ASSERT_EQUAL_UINT32(102u, p->sourceMaps[1].sourceId);
    TEST_ASSERT_EQUAL_UINT32(7u, p->logicalMap.functionToken);
    TEST_ASSERT_EQUAL_UINT64(11u, p->logicalMap.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(1u, p->logicalMap.generation);
    ZrTests_FunctionAotWitness_Capture(&p->module, &witness);
    ZrTests_FunctionAot_AssertView(&p->module, &witness);
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.attempts);
}

static void test_real_clone_survives_producer_release(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrTestsFunctionAotWitness witness;
    SZrAotIrDiagnostic diagnostic = {0};
    const SZrAotIrModule *view;
    ZrTests_FunctionAotWitness_Capture(&p->module, &witness);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK,
            ZrCore_AotIr_CloneOwnedModule(&p->module, &g_clone, &diagnostic));
    TEST_ASSERT_EQUAL_UINT(1u, g_arena.attempts);
    TEST_ASSERT_EQUAL_UINT(1u, g_arena.successes);
    view = ZrCore_AotIr_OwnedModuleView(g_clone);
    ZrTests_FunctionAot_AssertIndependent(&p->module, view);
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    ZrTests_FunctionAot_AssertView(view, &witness);
    ZrCore_AotIr_OwnedModuleFree(&g_clone);
    TEST_ASSERT_NULL(g_clone);
    assert_freed(0u, 1u);
    assert_balanced();
}

static void test_new_absence_and_null_getter(void) {
    SZrFunction *first, *second;
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(ZR_NULL));
    open_global();
    first = new_function(0u);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(first));
    ZrCore_Function_Free(g_state, first);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(first));
    second = new_function(1u);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(second));
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.attempts);
    finish_shutdown();
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.attempts);
}

static void test_attach_survives_producer_release(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrTestsFunctionAotWitness witness;
    SZrFunction *function;
    unsigned record;
    open_global();
    function = new_function(0u);
    ZrTests_FunctionAotWitness_Capture(&p->module, &witness);
    record = attach(function, &p->module);
    ZrTests_FunctionAot_AssertIndependent(&p->module, ZrCore_Function_GetAotIr(function));
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(function), &witness);
    ZrCore_Function_Free(g_state, function);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(function));
    assert_freed(record, 1u);
    assert_balanced();
    finish_shutdown();
}

static void test_independent_replacement_nine_to_eight(void) {
    SZrTestsFunctionAotProducer *a = producer(0u, 9), *b = producer(1u, 8);
    SZrTestsFunctionAotWitness wa, wb;
    SZrFunction *function;
    unsigned old, candidate;
    open_global();
    function = new_function(0u);
    ZrTests_FunctionAotWitness_Capture(&a->module, &wa);
    ZrTests_FunctionAotWitness_Capture(&b->module, &wb);
    old = attach(function, &a->module);
    candidate = attach(function, &b->module);
    assert_freed(old, 1u);
    assert_freed(candidate, 0u);
    TEST_ASSERT_TRUE(g_arena.records[candidate].allocatedAt < g_arena.records[old].freedAt);
    ZrTests_FunctionAot_AssertIndependent(&b->module, ZrCore_Function_GetAotIr(function));
    ZrTests_FunctionAot_AssertView(&a->module, &wa);
    ZrTests_FunctionAot_AssertView(&b->module, &wb);
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    ZrTests_FunctionAotProducer_Free(&g_producers[1]);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(function), &wb);
    ZrCore_Function_Free(g_state, function);
    assert_freed(candidate, 1u);
    assert_balanced();
    finish_shutdown();
}

static void test_exact_self_source_replacement(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrTestsFunctionAotWitness witness;
    SZrFunction *function;
    unsigned old, candidate;
    open_global();
    function = new_function(0u);
    old = attach(function, &p->module);
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    ZrTests_FunctionAotWitness_Capture(ZrCore_Function_GetAotIr(function), &witness);
    candidate = attach(function, ZrCore_Function_GetAotIr(function));
    assert_freed(old, 1u);
    TEST_ASSERT_TRUE(g_arena.records[candidate].allocatedAt < g_arena.records[old].freedAt);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(function), &witness);
    ZrCore_Function_Free(g_state, function);
    assert_freed(candidate, 1u);
    assert_balanced();
    finish_shutdown();
}

static void test_validation_and_null_refusals_are_atomic(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrTestsFunctionAotProducer sourceBytes;
    SZrAotIrModule oldHeader;
    SZrAotIrFunction oldFunction;
    SZrTestsFunctionAotWitness witness;
    SZrAotIrDiagnostic direct, actual, invalid = {0};
    EZrAotIrStatus directStatus, actualStatus;
    SZrFunction *function;
    const SZrAotIrModule *old;
    unsigned record, attempts;
    open_global();
    function = new_function(0u);
    record = attach(function, &p->module);
    old = ZrCore_Function_GetAotIr(function);
    memcpy(&sourceBytes, p, sizeof(sourceBytes));
    memcpy(&oldHeader, old, sizeof(oldHeader));
    memcpy(&oldFunction, old->functions, sizeof(oldFunction));
    ZrTests_FunctionAotWitness_Capture(&p->module, &witness);
    attempts = g_arena.attempts;
    for (unsigned mutation = 0u; mutation < 2u; ++mutation) {
        memset(&direct, 0xA5, sizeof(direct));
        memset(&actual, 0xA5, sizeof(actual));
        if (mutation == 0u) p->instructions[0].flags = ~((TZrUInt32)ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK);
        else p->function.contract.signatureHash = 12u;
        directStatus = ZrCore_AotIr_ValidateModule(&p->module, &direct);
        g_arena.armed = ZR_TRUE;
        actualStatus = ZrCore_Function_SnapshotAotIr(function, &p->module, &actual);
        g_arena.armed = ZR_FALSE;
        p->instructions[0].flags = 0u;
        p->function.contract.signatureHash = 11u;
        TEST_ASSERT_NOT_EQUAL(ZR_AOT_IR_OK, directStatus);
        TEST_ASSERT_EQUAL_INT(directStatus, actualStatus);
        assert_diagnostic(&direct, &actual);
        TEST_ASSERT_EQUAL_PTR(old, ZrCore_Function_GetAotIr(function));
        TEST_ASSERT_EQUAL_MEMORY(&sourceBytes, p, sizeof(sourceBytes));
        TEST_ASSERT_EQUAL_MEMORY(&oldHeader, old, sizeof(oldHeader));
        TEST_ASSERT_EQUAL_MEMORY(&oldFunction, old->functions, sizeof(oldFunction));
        ZrTests_FunctionAot_AssertView(old, &witness);
        ZrTests_FunctionAot_AssertView(&p->module, &witness);
        TEST_ASSERT_EQUAL_UINT(attempts, g_arena.attempts);
        assert_freed(record, 0u);
    }
    invalid.status = ZR_AOT_IR_INVALID_ARGUMENT;
    memset(&actual, 0xA5, sizeof(actual));
    g_arena.armed = ZR_TRUE;
    actualStatus = ZrCore_Function_SnapshotAotIr(function, ZR_NULL, &actual);
    g_arena.armed = ZR_FALSE;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, actualStatus);
    assert_diagnostic(&invalid, &actual);
    memset(&actual, 0xA5, sizeof(actual));
    g_arena.armed = ZR_TRUE;
    actualStatus = ZrCore_Function_SnapshotAotIr(ZR_NULL, &p->module, &actual);
    g_arena.armed = ZR_FALSE;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, actualStatus);
    assert_diagnostic(&invalid, &actual);
    TEST_ASSERT_EQUAL_PTR(old, ZrCore_Function_GetAotIr(function));
    TEST_ASSERT_EQUAL_MEMORY(&sourceBytes, p, sizeof(sourceBytes));
    TEST_ASSERT_EQUAL_MEMORY(&oldHeader, old, sizeof(oldHeader));
    TEST_ASSERT_EQUAL_MEMORY(&oldFunction, old->functions, sizeof(oldFunction));
    ZrTests_FunctionAot_AssertView(old, &witness);
    ZrTests_FunctionAot_AssertView(&p->module, &witness);
    TEST_ASSERT_EQUAL_UINT(attempts, g_arena.attempts);
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.failures);
    ZrCore_Function_Free(g_state, function);
    assert_freed(record, 1u);
    assert_balanced();
    finish_shutdown();
}

static void test_snapshot_arena_oom_then_retry(void) {
    SZrTestsFunctionAotProducer *a = producer(0u, 9), *b = producer(1u, 8);
    SZrTestsFunctionAotProducer sourceABytes, sourceBBytes;
    SZrAotIrModule oldHeader;
    SZrAotIrFunction oldFunction;
    SZrTestsFunctionAotWitness wa, wb;
    SZrAotIrDiagnostic actual, expected = {0};
    EZrAotIrStatus status;
    const SZrAotIrModule *old;
    SZrFunction *function;
    unsigned record, attempts, candidate;
    size_t requestBytes;
    open_global();
    function = new_function(0u);
    record = attach(function, &a->module);
    old = ZrCore_Function_GetAotIr(function);
    memcpy(&sourceABytes, a, sizeof(sourceABytes));
    memcpy(&sourceBBytes, b, sizeof(sourceBBytes));
    memcpy(&oldHeader, old, sizeof(oldHeader));
    memcpy(&oldFunction, old->functions, sizeof(oldFunction));
    ZrTests_FunctionAotWitness_Capture(old, &wa);
    ZrTests_FunctionAotWitness_Capture(&b->module, &wb);
    attempts = g_arena.attempts;
    memset(&actual, 0xA5, sizeof(actual));
    g_arena.armed = ZR_TRUE;
    status = ZrCore_Function_SnapshotAotIr(function, &b->module, &actual);
    g_arena.armed = ZR_FALSE;
    requestBytes = g_arena.lastBytes;
    expected.status = ZR_AOT_IR_OUT_OF_MEMORY;
    expected.expected = requestBytes;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OUT_OF_MEMORY, status);
    assert_diagnostic(&expected, &actual);
    TEST_ASSERT_EQUAL_UINT(attempts + 1u, g_arena.attempts);
    TEST_ASSERT_EQUAL_UINT(1u, g_arena.failures);
    TEST_ASSERT_EQUAL_UINT(1u, g_arena.successes);
    TEST_ASSERT_EQUAL_UINT(1u, g_arena.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_arena.frees);
    TEST_ASSERT_TRUE(requestBytes > 0u);
    TEST_ASSERT_EQUAL_PTR(old, ZrCore_Function_GetAotIr(function));
    TEST_ASSERT_EQUAL_MEMORY(&sourceABytes, a, sizeof(sourceABytes));
    TEST_ASSERT_EQUAL_MEMORY(&sourceBBytes, b, sizeof(sourceBBytes));
    TEST_ASSERT_EQUAL_MEMORY(&oldHeader, old, sizeof(oldHeader));
    TEST_ASSERT_EQUAL_MEMORY(&oldFunction, old->functions, sizeof(oldFunction));
    ZrTests_FunctionAot_AssertView(old, &wa);
    ZrTests_FunctionAot_AssertView(&a->module, &wa);
    ZrTests_FunctionAot_AssertView(&b->module, &wb);
    candidate = attach(function, &b->module);
    TEST_ASSERT_EQUAL_UINT64(requestBytes, g_arena.lastBytes);
    TEST_ASSERT_EQUAL_UINT(attempts + 2u, g_arena.attempts);
    assert_freed(record, 1u);
    TEST_ASSERT_EQUAL_MEMORY(&sourceABytes, a, sizeof(sourceABytes));
    TEST_ASSERT_EQUAL_MEMORY(&sourceBBytes, b, sizeof(sourceBBytes));
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(function), &wb);
    ZrCore_Function_Free(g_state, function);
    assert_freed(candidate, 1u);
    assert_balanced();
    finish_shutdown();
}

static void test_real_by_value_move_detaches_donor(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrTestsFunctionAotWitness witness;
    SZrFunction *parent, *donor, *child;
    const SZrAotIrModule *liveView;
    unsigned record;
    open_global();
    parent = new_function(0u);
    donor = new_function(1u);
    record = attach(donor, &p->module);
    liveView = ZrCore_Function_GetAotIr(donor);
    ZrTests_FunctionAotWitness_Capture(liveView, &witness);
    child = move_child(parent, donor);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(donor));
    TEST_ASSERT_EQUAL_PTR(liveView, ZrCore_Function_GetAotIr(child));
    TEST_ASSERT_EQUAL_PTR(parent, child->ownerFunction);
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    ZrCore_Function_Free(g_state, donor);
    assert_freed(record, 0u);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(child), &witness);
    ZrCore_Function_Free(g_state, parent);
    assert_freed(record, 1u);
    assert_balanced();
    finish_shutdown();
}

static void prepare_tree(SZrFunction **root, SZrFunction **child, SZrFunction **grand,
                         unsigned records[3], SZrTestsFunctionAotWitness witnesses[3]) {
    SZrFunction *childDonor, *grandDonor;
    *root = new_function(0u);
    childDonor = new_function(1u);
    grandDonor = new_function(2u);
    for (unsigned i = 0u; i < 3u; ++i) {
        SZrTestsFunctionAotProducer *p = producer(i, (TZrInt64)(9u - i));
        SZrFunction *function = i == 0u ? *root : (i == 1u ? childDonor : grandDonor);
        ZrTests_FunctionAotWitness_Capture(&p->module, &witnesses[i]);
        records[i] = attach(function, &p->module);
    }
    move_child(childDonor, grandDonor);
    *child = move_child(*root, childDonor);
    *grand = (*child)->childFunctionList;
    (*grand)->ownerFunction = *child;
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(childDonor));
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(grandDonor));
    TEST_ASSERT_EQUAL_UINT(3u, g_arena.live);
    for (unsigned i = 0u; i < 3u; ++i)
        for (unsigned j = i + 1u; j < 3u; ++j)
            TEST_ASSERT_TRUE(g_arena.records[records[i]].livePointer !=
                             g_arena.records[records[j]].livePointer);
    for (unsigned i = 0u; i < 3u; ++i) ZrTests_FunctionAotProducer_Free(&g_producers[i]);
}

static void test_owned_root_child_grandchild_release_once(void) {
    SZrFunction *root, *child, *grand;
    unsigned records[3];
    SZrTestsFunctionAotWitness witnesses[3];
    open_global();
    prepare_tree(&root, &child, &grand, records, witnesses);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(root), &witnesses[0]);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(child), &witnesses[1]);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(grand), &witnesses[2]);
    ZrCore_Function_Free(g_state, root);
    for (unsigned i = 0u; i < 3u; ++i) assert_freed(records[i], 1u);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(root));
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(g_functions[1]));
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(g_functions[2]));
    assert_balanced();
    finish_shutdown();
}

static void test_borrowed_immediate_shell_preserves_original_descendants(void) {
    SZrFunction *root, *child, *grand, *borrowed, *shell;
    const SZrAotIrModule *childView, *grandView;
    SZrTestsFunctionAotWitness witnesses[3], borrowedWitness;
    unsigned records[3], borrowedRecord, storageRecord = VM_RECORDS, storageFrees;
    open_global();
    prepare_tree(&root, &child, &grand, records, witnesses);
    borrowed = new_function(3u);
    producer(3u, 8);
    ZrTests_FunctionAotWitness_Capture(&g_producers[3]->module, &borrowedWitness);
    borrowedRecord = attach(borrowed, &g_producers[3]->module);
    childView = ZrCore_Function_GetAotIr(child);
    grandView = ZrCore_Function_GetAotIr(grand);
    shell = ZrCore_Memory_RawMallocWithType(g_global, sizeof(*shell), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    borrowed->childFunctionList = shell;
    borrowed->childFunctionLength = shell != ZR_NULL ? 1u : 0u;
    if (shell != ZR_NULL) {
        memcpy(shell, child, sizeof(*shell));
        ZrCore_Function_ForgetCopiedAotIrOwnerAliases(shell, 1u);
        borrowed->childFunctionGraphIsBorrowed = ZR_TRUE;
        shell->ownerFunction = borrowed;
    }
    TEST_ASSERT_NOT_NULL(shell);
    for (unsigned i = 0u; i < VM_RECORDS; ++i)
        if (g_vm.records[i].livePointer == shell) { storageRecord = i; break; }
    TEST_ASSERT_TRUE(storageRecord < VM_RECORDS);
    storageFrees = g_vm.records[storageRecord].frees;
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(shell));
    TEST_ASSERT_EQUAL_PTR(grand, shell->childFunctionList);
    TEST_ASSERT_EQUAL_PTR(childView, ZrCore_Function_GetAotIr(child));
    TEST_ASSERT_EQUAL_PTR(grandView, ZrCore_Function_GetAotIr(grand));
    ZrTests_FunctionAot_AssertView(childView, &witnesses[1]);
    ZrTests_FunctionAot_AssertView(grandView, &witnesses[2]);
    ZrTests_FunctionAotProducer_Free(&g_producers[3]);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(borrowed), &borrowedWitness);
    ZrCore_Function_Free(g_state, borrowed);
    assert_freed(borrowedRecord, 1u);
    TEST_ASSERT_EQUAL_UINT(storageFrees + 1u, g_vm.records[storageRecord].frees);
    TEST_ASSERT_EQUAL_UINT(3u, g_arena.live);
    for (unsigned i = 0u; i < 3u; ++i) assert_freed(records[i], 0u);
    TEST_ASSERT_EQUAL_PTR(childView, ZrCore_Function_GetAotIr(child));
    TEST_ASSERT_EQUAL_PTR(grandView, ZrCore_Function_GetAotIr(grand));
    TEST_ASSERT_EQUAL_PTR(grand, child->childFunctionList);
    ZrTests_FunctionAot_AssertView(ZrCore_Function_GetAotIr(root), &witnesses[0]);
    ZrTests_FunctionAot_AssertView(childView, &witnesses[1]);
    ZrTests_FunctionAot_AssertView(grandView, &witnesses[2]);
    ZrCore_Function_Free(g_state, root);
    for (unsigned i = 0u; i < 3u; ++i) assert_freed(records[i], 1u);
    assert_balanced();
    finish_shutdown();
}

static void test_explicit_free_then_actual_global_shutdown(void) {
    SZrTestsFunctionAotProducer *p = producer(0u, 9);
    SZrFunction *a, *b, *c;
    unsigned aRecord, bRecord;
    open_global();
    a = new_function(0u);
    b = new_function(1u);
    c = new_function(2u);
    aRecord = attach(a, &p->module);
    bRecord = attach(b, &p->module);
    ZrTests_FunctionAotProducer_Free(&g_producers[0]);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(c));
    ZrCore_Function_Free(g_state, a);
    TEST_ASSERT_NULL(ZrCore_Function_GetAotIr(a));
    assert_freed(aRecord, 1u);
    assert_freed(bRecord, 0u);
    TEST_ASSERT_EQUAL_UINT(1u, g_arena.live);
    TEST_ASSERT_EQUAL_UINT(2u, g_arena.attempts);
    shutdown_global();
    /* Only independent ledger values are inspected after real GC shutdown. */
    assert_freed(aRecord, 1u);
    assert_freed(bRecord, 1u);
    TEST_ASSERT_EQUAL_UINT(2u, g_arena.attempts);
    assert_balanced();
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_raw_scalar_fixture_validates);
    RUN_TEST(test_real_clone_survives_producer_release);
    RUN_TEST(test_new_absence_and_null_getter);
    RUN_TEST(test_attach_survives_producer_release);
    RUN_TEST(test_independent_replacement_nine_to_eight);
    RUN_TEST(test_exact_self_source_replacement);
    RUN_TEST(test_validation_and_null_refusals_are_atomic);
    RUN_TEST(test_snapshot_arena_oom_then_retry);
    RUN_TEST(test_real_by_value_move_detaches_donor);
    RUN_TEST(test_owned_root_child_grandchild_release_once);
    RUN_TEST(test_borrowed_immediate_shell_preserves_original_descendants);
    RUN_TEST(test_explicit_free_then_actual_global_shutdown);
    return UNITY_END();
}
