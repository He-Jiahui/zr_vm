#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define ZR_TEST_SSA_SOURCE_DESCRIPTOR_OOM_NO_REDIRECT
#include "support/ssa_source_descriptor_oom_shim.h"
#include "support/ssa_literal_script_fixture.h"
#include "unity.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/source/exec_ir_source_module_contract.h"
#include "exec_ir/exec_ir_host_primitive_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "exec_ir/exec_ir_host_aot_target.h"

enum { RECORDS = 256, VM_RECORDS = 8192, SITES = 6, SOURCES = 2 };
typedef struct SRecord {
    void *livePointer;
    size_t count, width, bytes;
    unsigned site, frees, allocatedAt, freedAt;
} SRecord;
typedef struct SAttempt {
    size_t count, width, bytes;
    unsigned site, record;
    TZrBool injected;
} SAttempt;
typedef struct SLedger {
    SRecord records[RECORDS];
    SAttempt events[RECORDS];
    unsigned attempts, successes, failures, frees, live, errors, sequence;
    unsigned callBase, failAt;
} SLedger;
typedef struct SVmRecord {
    void *livePointer;
    size_t bytes;
    TZrInt64 flag;
} SVmRecord;
typedef struct SVmLedger {
    SVmRecord records[VM_RECORDS];
    unsigned live, errors;
} SVmLedger;
typedef struct SSource {
    SZrSsaLiteralScriptFixture fixture;
    SZrExecIrFunction compacted;
    SZrAotIrProjection projection;
    SZrAotIrTargetContract target;
} SSource;
/* Numeric observations only; this never retains a descriptor span address. */
typedef struct SWitness {
    TZrUInt64 hash, signatureHash, moduleHash;
    TZrUInt32 functionId, functionToken, schemaVersion;
    TZrInt64 literal;
    SZrAotIrCallableAbi abi;
} SWitness;
static SLedger g_ledger;
static SVmLedger g_vm;
static SSource g_sources[SOURCES];
static SZrAotIrProjectionDescriptor g_descriptor;
static SZrGlobalState *g_global;
static TZrBool g_savedStopGc, g_haveStopGc;
static const size_t g_counts[SITES] = {3u, 1u, 1u, 3u, 1u, 1u};
static const size_t g_widths[SITES] = {
    sizeof(SZrAotIrInstruction), sizeof(SZrAotIrBlock),
    sizeof(SZrAotIrFrameSlot), sizeof(SZrAotIrSourceMap),
    sizeof(SZrExecIrConstant), sizeof(SZrExecIrLayout)
};

/* Deferred errors only: no allocation, assertion or output in callbacks. */
void *ZrTests_SourceDescriptor_Calloc(size_t count, size_t width) {
    SAttempt *event;
    SRecord *record;
    void *pointer;
    unsigned site = g_ledger.attempts - g_ledger.callBase;
    if (g_ledger.attempts == RECORDS || site >= SITES || count == 0u ||
        width == 0u || count > SIZE_MAX / width) {
        ++g_ledger.errors;
        return ZR_NULL;
    }
    event = &g_ledger.events[g_ledger.attempts++];
    event->count = count; event->width = width; event->bytes = count * width;
    event->site = site + 1u; event->record = RECORDS;
    if (count != g_counts[site] || width != g_widths[site]) {
        ++g_ledger.errors;
        return ZR_NULL;
    }
    if (g_ledger.failAt == g_ledger.attempts) {
        event->injected = ZR_TRUE;
        ++g_ledger.failures;
        return ZR_NULL;
    }
    if (g_ledger.successes == RECORDS) { ++g_ledger.errors; return ZR_NULL; }
    pointer = calloc(count, width);
    if (pointer == ZR_NULL) { ++g_ledger.errors; return ZR_NULL; }
    event->record = g_ledger.successes;
    record = &g_ledger.records[g_ledger.successes++];
    record->livePointer = pointer;
    record->count = count; record->width = width; record->bytes = event->bytes;
    record->site = site + 1u; record->allocatedAt = ++g_ledger.sequence;
    ++g_ledger.live;
    return pointer;
}
void ZrTests_SourceDescriptor_Free(void *pointer) {
    unsigned i;
    if (pointer == ZR_NULL) return;
    for (i = 0u; i < g_ledger.successes; ++i) {
        SRecord *r = &g_ledger.records[i];
        if (r->livePointer == pointer) {
            r->livePointer = ZR_NULL;
            ++r->frees; r->freedAt = ++g_ledger.sequence;
            ++g_ledger.frees; --g_ledger.live;
            free(pointer);
            return;
        }
    }
    ++g_ledger.errors;
}
static TZrPtr vm_allocator(TZrPtr user, TZrPtr pointer, TZrSize original,
                           TZrSize bytes, TZrInt64 flag) {
    SVmLedger *ledger = user;
    unsigned i;
    void *replacement;
    TZrBool wasNew = (TZrBool)(pointer == ZR_NULL);
    if (pointer != ZR_NULL) {
        for (i = 0u; i < VM_RECORDS; ++i)
            if (ledger->records[i].livePointer == pointer) break;
        if (i == VM_RECORDS) { ++ledger->errors; return ZR_NULL; }
        if (ledger->records[i].bytes != original || ledger->records[i].flag != flag)
            ++ledger->errors;
        ledger->records[i].livePointer = ZR_NULL;
        --ledger->live;
        if (bytes == 0u) { free(pointer); return ZR_NULL; }
    } else {
        for (i = 0u; i < VM_RECORDS; ++i)
            if (ledger->records[i].livePointer == ZR_NULL) break;
        if (i == VM_RECORDS || bytes == 0u || original != 0u) {
            ++ledger->errors; return ZR_NULL;
        }
    }
    replacement = realloc(pointer, bytes);
    if (replacement == ZR_NULL) {
        if (!wasNew) { ledger->records[i].livePointer = pointer; ++ledger->live; }
        ++ledger->errors;
        return ZR_NULL;
    }
    ledger->records[i].livePointer = replacement;
    ledger->records[i].bytes = bytes; ledger->records[i].flag = flag;
    ++ledger->live;
    return replacement;
}
static void close_sources(void) {
    unsigned i;
    for (i = 0u; i < SOURCES; ++i) ZrParser_AotIrProjection_Free(&g_sources[i].projection);
    for (i = 0u; i < SOURCES; ++i) ZrCore_ExecIr_FreeFunction(&g_sources[i].compacted);
    for (i = 0u; i < SOURCES; ++i) ZrTests_SsaLiteralScriptFixture_Free(&g_sources[i].fixture);
    if (g_global != ZR_NULL) {
        SZrGlobalState *live = g_global;
        if (g_haveStopGc) live->garbageCollector->stopGcFlag = g_savedStopGc;
        g_haveStopGc = ZR_FALSE; g_global = ZR_NULL;
        ZrCore_GlobalState_Free(live);
    }
}
void setUp(void) {
    unsigned i;
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    memset(&g_ledger, 0, sizeof(g_ledger)); memset(&g_vm, 0, sizeof(g_vm));
    memset(g_sources, 0, sizeof(g_sources)); memset(&g_descriptor, 0, sizeof(g_descriptor));
    g_global = ZR_NULL; g_haveStopGc = ZR_FALSE;
    for (i = 0u; i < SOURCES; ++i) {
        ZrTests_SsaLiteralScriptFixture_Init(&g_sources[i].fixture, ZR_NULL);
        ZrCore_ExecIr_FunctionInit(&g_sources[i].compacted);
    }
}
void tearDown(void) {
    unsigned i;
    g_ledger.failAt = 0u;
    ZrParser_AotIrProjection_FreeDescriptor(&g_descriptor);
    close_sources();
    /* Recovery is not candidate-cleanup evidence. Normal cases check beforehand. */
    for (i = 0u; i < g_ledger.successes; ++i)
        if (g_ledger.records[i].livePointer != ZR_NULL)
            ZrTests_SourceDescriptor_Free(g_ledger.records[i].livePointer);
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.live); TEST_ASSERT_EQUAL_UINT(0u, g_ledger.errors);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static void open_global(void) {
    if (g_global != ZR_NULL) return;
    g_global = ZrCore_GlobalState_New(vm_allocator, &g_vm, 0xA017u, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_global); TEST_ASSERT_NOT_NULL(g_global->mainThreadState);
    TEST_ASSERT_NOT_NULL(g_global->garbageCollector);
    g_savedStopGc = g_global->garbageCollector->stopGcFlag; g_haveStopGc = ZR_TRUE;
    g_global->garbageCollector->stopGcFlag = ZR_TRUE;
    TEST_ASSERT_NULL(g_global->profileRuntime); TEST_ASSERT_NULL(g_global->sourceLoader);
    TEST_ASSERT_NULL(g_global->aotModuleLoader); TEST_ASSERT_NULL(g_global->nativeModuleLoader);
    TEST_ASSERT_NULL(g_global->providerModuleNameResolver); TEST_ASSERT_NULL(g_global->compileSource);
    TEST_ASSERT_NULL(g_global->callBindingModuleResolver); TEST_ASSERT_NULL(g_global->typedCallBindingResolver);
    TEST_ASSERT_NULL(g_global->callbacks.afterStateInitialized);
    TEST_ASSERT_NULL(g_global->callbacks.beforeStateReleased);
    TEST_ASSERT_NULL(g_global->callbacks.afterThreadInitialized);
    TEST_ASSERT_NULL(g_global->callbacks.beforeThreadReleased);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static SSource *prepare(unsigned index) {
    SSource *s = &g_sources[index];
    const SZrCanonicalTypeNode *callable, *primitive;
    const SZrExecIrFunction *original;
    SZrExecIrDiagnostic d = {0}; SZrAotIrDiagnostic a = {0};
    SZrExecIrLayout row; SZrExecIrRange range;
    TZrTypeId type;
    open_global();
    ZrTests_SsaLiteralScriptFixture_Init(&s->fixture, g_global->mainThreadState);
    ZrTests_SsaLiteralScriptFixture_Prepare(&s->fixture, index == 0u
            ? ZR_TEST_SSA_LITERAL_SCRIPT_NINE : ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BindSourceModuleContract(&s->fixture.compiler, &s->fixture.module, &d));
    original = ZrTests_SsaLiteralScriptFixture_Function(&s->fixture);
    callable = ZrParser_CanonicalType_Find(s->fixture.compiler.semanticContext,
            s->fixture.compiler.preSemanticIr.callableTypeId);
    TEST_ASSERT_NOT_NULL(callable); TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    type = callable->data.function.returnTypeId;
    primitive = ZrParser_CanonicalType_Find(s->fixture.compiler.semanticContext, type);
    TEST_ASSERT_NOT_NULL(primitive); TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(original, ZR_EXEC_IR_VERIFY_ALL, &d));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_EliminateDeadSourcePlaces(&s->fixture.compiler.preSemanticIr,
            s->fixture.compiler.semanticContext, original, &s->compacted, &d));
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&s->fixture, &s->compacted);
    TEST_ASSERT_EQUAL_UINT32(3u, s->compacted.instructionCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, s->compacted.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, s->compacted.instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, s->compacted.instructions[2].opcode);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostPrimitiveLayout(s->fixture.compiler.semanticContext, type, 1u, &row, &d));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&s->fixture.module, &row, 1u, &range));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_AttachPrimitiveSourceFrame(&s->compacted,
            s->fixture.compiler.semanticContext, s->fixture.module.layouts, s->fixture.module.layoutCount, 0u, &d));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&s->compacted, ZR_EXEC_IR_VERIFY_ALL, &d));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LowerAotWithCanonicalCallable(&s->compacted,
            s->fixture.module.constants, s->fixture.module.constantCount,
            s->fixture.module.layouts, s->fixture.module.layoutCount,
            s->fixture.compiler.semanticContext, s->fixture.compiler.preSemanticIr.callableTypeId, &s->projection, &d));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(s->fixture.compiler.semanticContext,
            s->fixture.compiler.preSemanticIr.callableTypeId, &s->fixture.module.layouts[0], &s->target, &a));
    TEST_ASSERT_EQUAL_UINT32(3u, s->projection.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->projection.blockCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->projection.frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(3u, s->projection.sourceMapCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->projection.constantCount);
    TEST_ASSERT_EQUAL_UINT32(1u, s->projection.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.successorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.phiIncomingCount);
    TEST_ASSERT_TRUE(s->projection.stateMapPresent);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.stateMap.entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.stateMap.valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.stateMap.rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, s->projection.stateMap.ownerStateCount);
    TEST_ASSERT_EQUAL_INT64(index == 0u ? 9 : 8, (TZrInt64)s->fixture.module.constants[0].bits);
    TEST_ASSERT_EQUAL_MEMORY(s->fixture.module.constants, s->projection.constants, sizeof(*s->projection.constants));
    TEST_ASSERT_EQUAL_MEMORY(s->fixture.module.layouts, s->projection.layouts, sizeof(*s->projection.layouts));
    {
        SZrFunction *runtime = s->fixture.compiler.currentFunction;
        SZrAstNode *ast = s->fixture.ast, *statement, *literal;
        TEST_ASSERT_NOT_NULL(runtime); TEST_ASSERT_EQUAL_UINT32(1u, runtime->constantValueLength);
        TEST_ASSERT_NOT_NULL(runtime->constantValueList);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, runtime->constantValueList[0].type);
        TEST_ASSERT_EQUAL_INT64(index == 0u ? 9 : 8, runtime->constantValueList[0].value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(runtime->metadataTokenRecordLength > 0u && runtime->metadataTokenRecordLength <= RECORDS);
        TEST_ASSERT_NOT_NULL(runtime->metadataTokenRecords);
        TEST_ASSERT_NOT_NULL(ast); TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_UINT64(1u, ast->data.script.statements->count);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes);
        statement = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(statement); TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
        literal = statement->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(literal); TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, literal->type);
        TEST_ASSERT_EQUAL_INT64(index == 0u ? 9 : 8, literal->data.integerLiteral.value);
    }
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
    return s;
}
static TZrBool build(SSource *s, SZrAotIrDiagnostic *d) {
    g_ledger.callBase = g_ledger.attempts;
    return ZrParser_AotIrProjection_BuildDescriptor(&s->projection, &s->target,
            &s->fixture.module.contract, &g_descriptor, d);
}
static void diagnostic_fields(const SZrAotIrDiagnostic *d, EZrAotIrStatus status, size_t bytes) {
    TEST_ASSERT_EQUAL_INT(status, d->status); TEST_ASSERT_EQUAL_UINT64(bytes, d->expected);
    TEST_ASSERT_EQUAL_UINT64(0u, d->actual); TEST_ASSERT_EQUAL_UINT32(0u, d->functionId);
    TEST_ASSERT_EQUAL_UINT32(0u, d->blockId); TEST_ASSERT_EQUAL_UINT32(0u, d->instructionId);
    TEST_ASSERT_EQUAL_UINT32(0u, d->index);
}
static TZrUInt64 source_observation(SSource *s) {
    const SZrAotIrProjection *p = &s->projection;
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_SourceDigest(&s->fixture);
#define OBSERVE(ptr, bytes) hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, ptr, bytes)
#define POOL(field, count) do { if (p->count != 0u) OBSERVE(p->field, (size_t)p->count * sizeof(*p->field)); } while (0)
    OBSERVE(&s->fixture.compiler, sizeof(s->fixture.compiler));
    {
        const SZrFunction *runtime = s->fixture.compiler.currentFunction;
        const SZrAstNode *ast = s->fixture.ast;
        const SZrAstNode *statement = ast->data.script.statements->nodes[0];
        OBSERVE(runtime, sizeof(*runtime));
        OBSERVE(runtime->metadataTokenRecords, (size_t)runtime->metadataTokenRecordLength * sizeof(*runtime->metadataTokenRecords));
        OBSERVE(runtime->constantValueList, sizeof(*runtime->constantValueList));
        OBSERVE(ast, sizeof(*ast)); OBSERVE(ast->data.script.statements, sizeof(*ast->data.script.statements));
        OBSERVE(ast->data.script.statements->nodes, sizeof(*ast->data.script.statements->nodes));
        OBSERVE(statement, sizeof(*statement)); OBSERVE(statement->data.returnStatement.expr, sizeof(SZrAstNode));
    }
    OBSERVE(&s->fixture.module, sizeof(s->fixture.module)); OBSERVE(&s->target, sizeof(s->target));
    OBSERVE(s->fixture.module.constants, (size_t)s->fixture.module.constantCount * sizeof(*s->fixture.module.constants));
    OBSERVE(s->fixture.module.layouts, (size_t)s->fixture.module.layoutCount * sizeof(*s->fixture.module.layouts));
    {
        TZrUInt64 original = ZrTests_SsaLiteralScriptFixture_FunctionDigest(
                ZrTests_SsaLiteralScriptFixture_Function(&s->fixture));
        TZrUInt64 compacted = ZrTests_SsaLiteralScriptFixture_FunctionDigest(&s->compacted);
        OBSERVE(&original, sizeof(original)); OBSERVE(&compacted, sizeof(compacted));
    }
    OBSERVE(p, sizeof(*p));
    POOL(opcodes, instructionCount); POOL(instructions, instructionCount); POOL(frameSlots, frameSlotCount);
    POOL(operands, operandCount); POOL(results, resultCount); POOL(memoryTokens, memoryTokenCount);
    POOL(valueSlots, valueSlotCount); POOL(slotValues, physicalSlotCount); POOL(blocks, blockCount);
    POOL(predecessors, predecessorCount); POOL(successors, successorCount); POOL(phis, phiCount);
    POOL(phiIncomings, phiIncomingCount); POOL(phiCopySources, phiCopyCount); POOL(phiCopyDestinations, phiCopyCount);
    POOL(phiCopyEdges, phiCopyCount); POOL(phiMoves, phiMoveCount); POOL(sourceMaps, sourceMapCount);
    POOL(constants, constantCount); POOL(layouts, layoutCount); POOL(gcRoots, gcRootCount);
    POOL(deoptStates, deoptStateCount); POOL(deoptValues, deoptValueCount);
    POOL(deoptAggregates, deoptAggregateCount); POOL(deoptAggregateFields, deoptAggregateFieldCount);
#undef POOL
#undef OBSERVE
    return hash;
}
static TZrUInt64 descriptor_observation(void) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(0u, &g_descriptor, sizeof(g_descriptor));
#define SPAN(field, count) hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, g_descriptor.field, (size_t)(count) * sizeof(*g_descriptor.field))
    SPAN(instructions, 3u); SPAN(blocks, 1u); SPAN(frameSlots, 1u);
    SPAN(sourceMaps, 3u); SPAN(constants, 1u); SPAN(layouts, 1u);
#undef SPAN
    return hash;
}
static void requests(unsigned base, unsigned count) {
    unsigned i;
    for (i = 0u; i < count; ++i) {
        const SAttempt *e = &g_ledger.events[base + i];
        TEST_ASSERT_EQUAL_UINT(i + 1u, e->site);
        TEST_ASSERT_EQUAL_UINT64(g_counts[i], e->count);
        TEST_ASSERT_EQUAL_UINT64(g_widths[i], e->width);
        TEST_ASSERT_EQUAL_UINT64(g_counts[i] * g_widths[i], e->bytes);
        if (!e->injected) {
            const SRecord *r;
            TEST_ASSERT_TRUE(e->record < g_ledger.successes);
            r = &g_ledger.records[e->record];
            TEST_ASSERT_EQUAL_UINT(e->site, r->site);
            TEST_ASSERT_EQUAL_UINT64(e->count, r->count);
            TEST_ASSERT_EQUAL_UINT64(e->width, r->width);
            TEST_ASSERT_EQUAL_UINT64(e->bytes, r->bytes);
        } else TEST_ASSERT_EQUAL_UINT(RECORDS, e->record);
    }
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.errors);
}
static void validate(SSource *s, TZrInt64 literal) {
    const SZrAotIrProjection *p = &s->projection;
    const SZrAotIrFunction *f = &g_descriptor.function;
    SZrAotIrDiagnostic d = {0}; SZrAotIrCallableAbi abi = {0};
    unsigned i;
    TEST_ASSERT_EQUAL_PTR(p, g_descriptor.owner);
    TEST_ASSERT_EQUAL_PTR(f, g_descriptor.module.functions);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(&g_descriptor.module, &d));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(&g_descriptor.module, f->id, &abi, &d));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_MEMORY(&p->callableAbi, &abi, sizeof(abi));
    TEST_ASSERT_EQUAL_MEMORY(&s->target, &g_descriptor.module.target, sizeof(s->target));
    TEST_ASSERT_EQUAL_MEMORY(&s->fixture.module.contract, &g_descriptor.module.contract, sizeof(s->fixture.module.contract));
    TEST_ASSERT_EQUAL_MEMORY(&p->contract, &f->contract, sizeof(p->contract));
    TEST_ASSERT_EQUAL_UINT32(p->functionId, f->id); TEST_ASSERT_EQUAL_UINT32(p->functionToken, f->functionToken);
    TEST_ASSERT_EQUAL_UINT64(p->signatureHash, f->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(p->frameLayoutHash, f->frameLayout.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(p->logicalSlotCount, f->frameLayout.logicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(p->storageSlotCount, f->frameLayout.storageSlotCount);
    TEST_ASSERT_EQUAL_UINT32(p->parameterPrefixBytes, f->frameLayout.parameterPrefixBytes);
    TEST_ASSERT_EQUAL_UINT32(p->returnAreaOffset, f->frameLayout.returnAreaOffset);
    TEST_ASSERT_EQUAL_UINT32(p->frameByteSize, f->frameLayout.frameByteSize);
    TEST_ASSERT_EQUAL_UINT32(p->frameByteAlign, f->frameLayout.frameByteAlign);
    TEST_ASSERT_EQUAL_PTR(p->operands, f->operandPool); TEST_ASSERT_EQUAL_PTR(p->results, f->resultPool);
    TEST_ASSERT_EQUAL_PTR(p->valueSlots, f->valueSlotPool); TEST_ASSERT_EQUAL_PTR(p->memoryTokens, f->memoryTokenPool);
    TEST_ASSERT_EQUAL_PTR(&p->stateMap, f->logicalStateMap);
    TEST_ASSERT_EQUAL_PTR(p->gcRoots, f->gcRootPool);
    TEST_ASSERT_EQUAL_PTR(p->deoptStates, f->deoptStates); TEST_ASSERT_EQUAL_PTR(p->deoptValues, f->deoptValuePool);
    TEST_ASSERT_EQUAL_PTR(p->deoptAggregates, f->deoptAggregates);
    TEST_ASSERT_EQUAL_PTR(p->deoptAggregateFields, f->deoptAggregateFields);
    TEST_ASSERT_NULL(f->gcMap); TEST_ASSERT_NULL(f->successorPool); TEST_ASSERT_NULL(g_descriptor.phiIncoming);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.functionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.constantCount); TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(3u, f->instructionCount); TEST_ASSERT_EQUAL_UINT32(1u, f->blockCount);
    TEST_ASSERT_EQUAL_UINT32(1u, f->frameSlotCount); TEST_ASSERT_EQUAL_UINT32(3u, f->sourceMapCount);
    TEST_ASSERT_EQUAL_MEMORY(p->constants, g_descriptor.constants, sizeof(*p->constants));
    TEST_ASSERT_EQUAL_MEMORY(p->layouts, g_descriptor.layouts, sizeof(*p->layouts));
    TEST_ASSERT_EQUAL_INT64(literal, (TZrInt64)g_descriptor.constants[0].bits);
    for (i = 0u; i < 3u; ++i) {
#define INS(field) TEST_ASSERT_EQUAL_UINT32(p->instructions[i].field, g_descriptor.instructions[i].field)
        INS(flags); INS(effectIn); INS(effectOut); INS(deoptId); INS(layoutId);
        INS(bindingRow); INS(typeToken); INS(matchTypeToken);
        INS(results.offset); INS(results.count); INS(operands.offset); INS(operands.count);
        INS(memoryIn.offset); INS(memoryIn.count); INS(memoryOut.offset); INS(memoryOut.count);
        TEST_ASSERT_EQUAL_UINT32(p->instructions[i].successorRange.offset, g_descriptor.instructions[i].successors.offset);
        TEST_ASSERT_EQUAL_UINT32(p->instructions[i].successorRange.count, g_descriptor.instructions[i].successors.count);
        TEST_ASSERT_EQUAL_UINT32(p->instructions[i].phiRange.offset, g_descriptor.instructions[i].phiIncoming.offset);
        TEST_ASSERT_EQUAL_UINT32(p->instructions[i].phiRange.count, g_descriptor.instructions[i].phiIncoming.count);
#undef INS
        TEST_ASSERT_EQUAL_UINT16(p->instructions[i].opcode, g_descriptor.instructions[i].opcode);
        TEST_ASSERT_EQUAL_UINT32(p->instructions[i].sourceId, g_descriptor.instructions[i].sourceId);
        TEST_ASSERT_EQUAL_UINT32(p->instructions[i].pc + 1u, g_descriptor.instructions[i].id);
        TEST_ASSERT_EQUAL_UINT32(p->sourceMaps[i].sourceId, g_descriptor.sourceMaps[i].sourceId);
        TEST_ASSERT_EQUAL_UINT32(p->sourceMaps[i].pc + 1u, g_descriptor.sourceMaps[i].instructionId);
        TEST_ASSERT_EQUAL_UINT32(p->sourceMaps[i].startOffset, g_descriptor.sourceMaps[i].startOffset);
        TEST_ASSERT_EQUAL_UINT32(p->sourceMaps[i].endOffset, g_descriptor.sourceMaps[i].endOffset);
#define MAP(field) TEST_ASSERT_EQUAL_UINT32(p->sourceMaps[i].field, g_descriptor.sourceMaps[i].field)
        MAP(startLine); MAP(startColumn); MAP(endLine); MAP(endColumn);
#undef MAP
    }
    TEST_ASSERT_EQUAL_UINT32(p->blocks[0].id, g_descriptor.blocks[0].id);
    TEST_ASSERT_EQUAL_UINT32(p->blocks[0].instructions.count, g_descriptor.blocks[0].instructions.count);
    TEST_ASSERT_EQUAL_UINT32(p->blocks[0].instructions.offset, g_descriptor.blocks[0].instructions.offset);
    TEST_ASSERT_EQUAL_UINT32(p->blocks[0].flags, g_descriptor.blocks[0].flags);
    TEST_ASSERT_EQUAL_UINT32(p->blocks[0].terminatorInstructionId, g_descriptor.blocks[0].terminatorInstructionId);
    TEST_ASSERT_EQUAL_UINT32(0u, g_descriptor.blocks[0].predecessors.offset);
    TEST_ASSERT_EQUAL_UINT32(0u, g_descriptor.blocks[0].predecessors.count);
    TEST_ASSERT_EQUAL_UINT32(0u, g_descriptor.blocks[0].successors.count);
    TEST_ASSERT_EQUAL_UINT32(p->frameSlots[0].slotId, g_descriptor.frameSlots[0].slotId);
    TEST_ASSERT_EQUAL_UINT32(p->frameSlots[0].byteSize, g_descriptor.frameSlots[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(p->frameSlots[0].byteOffset, g_descriptor.frameSlots[0].byteOffset);
    TEST_ASSERT_EQUAL_UINT32(p->frameSlots[0].byteAlign, g_descriptor.frameSlots[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT32(p->frameSlots[0].typeToken, g_descriptor.frameSlots[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(p->frameSlots[0].kind, g_descriptor.frameSlots[0].kind);
    {
        const void *spans[SITES] = {g_descriptor.instructions, g_descriptor.blocks,
            g_descriptor.frameSlots, g_descriptor.sourceMaps, g_descriptor.constants, g_descriptor.layouts};
        unsigned liveRecords = 0u;
        for (i = 0u; i < g_ledger.successes; ++i) {
            const SRecord *r = &g_ledger.records[i];
            if (r->livePointer == ZR_NULL) continue;
            TEST_ASSERT_TRUE(r->site >= 1u && r->site <= SITES);
            TEST_ASSERT_EQUAL_PTR(spans[r->site - 1u], r->livePointer);
            TEST_ASSERT_EQUAL_UINT(0u, r->frees); ++liveRecords;
        }
        TEST_ASSERT_EQUAL_UINT(SITES, liveRecords);
    }
}
static void release(void) {
    unsigned i, frees;
    SZrAotIrProjectionDescriptor empty = {0};
    ZrParser_AotIrProjection_FreeDescriptor(&g_descriptor);
    TEST_ASSERT_EQUAL_MEMORY(&empty, &g_descriptor, sizeof(empty));
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.live); TEST_ASSERT_EQUAL_UINT(0u, g_ledger.errors);
    TEST_ASSERT_EQUAL_UINT(g_ledger.successes, g_ledger.frees);
    for (i = 0u; i < g_ledger.successes; ++i) TEST_ASSERT_EQUAL_UINT(1u, g_ledger.records[i].frees);
    frees = g_ledger.frees;
    ZrParser_AotIrProjection_FreeDescriptor(&g_descriptor);
    TEST_ASSERT_EQUAL_UINT(frees, g_ledger.frees);
    close_sources();
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static void prerequisite(unsigned index) {
    SSource *s = prepare(index);
    TZrUInt64 before = source_observation(s);
    SZrAotIrDiagnostic d;
    memset(&d, 0xa5, sizeof(d));
    TEST_ASSERT_TRUE(build(s, &d)); diagnostic_fields(&d, ZR_AOT_IR_OK, 0u);
    TEST_ASSERT_EQUAL_UINT(6u, g_ledger.attempts); TEST_ASSERT_EQUAL_UINT(6u, g_ledger.live);
    TEST_ASSERT_EQUAL_UINT(6u, g_ledger.successes); TEST_ASSERT_EQUAL_UINT(0u, g_ledger.frees);
    TEST_ASSERT_EQUAL_UINT(0u, g_ledger.failures); requests(0u, 6u);
    validate(s, index == 0u ? 9 : 8);
    TEST_ASSERT_EQUAL_UINT64(before, source_observation(s)); release();
}
static void fault(unsigned site, TZrBool replace, TZrBool optional) {
    SSource *nine = prepare(0u), *source = nine;
    SZrAotIrDiagnostic d;
    SZrAotIrProjectionDescriptor old = {0};
    SWitness oldWitness = {0};
    TZrUInt64 nineBefore, sourceBefore, oldBefore = 0u;
    unsigned attempts, successes, frees, live, i, retryBase, retryRecord;
    TZrBool result;
    if (replace) {
        TEST_ASSERT_TRUE(build(nine, &d)); validate(nine, 9);
        source = prepare(1u);
        old = g_descriptor; oldBefore = descriptor_observation();
        oldWitness.hash = ZrCore_AotIr_HashModule(&g_descriptor.module);
        oldWitness.literal = (TZrInt64)g_descriptor.constants[0].bits;
        oldWitness.functionId = g_descriptor.function.id;
        oldWitness.functionToken = g_descriptor.function.functionToken;
        oldWitness.signatureHash = g_descriptor.function.signatureHash;
        oldWitness.moduleHash = g_descriptor.module.moduleHash;
        oldWitness.schemaVersion = g_descriptor.module.schemaVersion;
        oldWitness.abi = g_descriptor.function.callableAbi;
    }
    nineBefore = source_observation(nine); sourceBefore = source_observation(source);
    attempts = g_ledger.attempts; successes = g_ledger.successes;
    frees = g_ledger.frees; live = g_ledger.live;
    g_ledger.failAt = attempts + site;
    memset(&d, 0xa5, sizeof(d));
    result = build(source, optional ? ZR_NULL : &d);
    g_ledger.failAt = 0u; /* Disarmed before every assertion and cleanup. */
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT(attempts + site, g_ledger.attempts); requests(attempts, site);
    TEST_ASSERT_TRUE(g_ledger.events[attempts + site - 1u].injected);
    TEST_ASSERT_EQUAL_UINT(1u, g_ledger.failures);
    TEST_ASSERT_EQUAL_UINT(successes + site - 1u, g_ledger.successes);
    TEST_ASSERT_EQUAL_UINT(frees + site - 1u, g_ledger.frees);
    TEST_ASSERT_EQUAL_UINT(live, g_ledger.live);
    for (i = successes; i < g_ledger.successes; ++i) {
        TEST_ASSERT_EQUAL_UINT(1u, g_ledger.records[i].frees);
        TEST_ASSERT_NULL(g_ledger.records[i].livePointer);
    }
    TEST_ASSERT_EQUAL_MEMORY(&old, &g_descriptor, sizeof(old));
    TEST_ASSERT_EQUAL_UINT64(nineBefore, source_observation(nine));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation(source));
    if (replace) {
        TEST_ASSERT_EQUAL_UINT64(oldBefore, descriptor_observation()); validate(nine, 9);
        for (i = 0u; i < 6u; ++i) TEST_ASSERT_EQUAL_UINT(0u, g_ledger.records[i].frees);
    }
    if (!optional) diagnostic_fields(&d, ZR_AOT_IR_OUT_OF_MEMORY, g_ledger.events[attempts + site - 1u].bytes);
    retryBase = g_ledger.attempts; retryRecord = g_ledger.successes;
    memset(&d, 0xa5, sizeof(d));
    result = build(source, &d);
    /* old contains pointers to released spans: never read it after this call. */
    TEST_ASSERT_TRUE(result); diagnostic_fields(&d, ZR_AOT_IR_OK, 0u);
    TEST_ASSERT_EQUAL_UINT(retryBase + 6u, g_ledger.attempts); requests(retryBase, 6u);
    TEST_ASSERT_EQUAL_UINT(retryRecord + 6u, g_ledger.successes);
    TEST_ASSERT_EQUAL_UINT(6u, g_ledger.live); TEST_ASSERT_EQUAL_UINT(1u, g_ledger.failures);
    TEST_ASSERT_EQUAL_UINT(frees + site - 1u + (replace ? 6u : 0u), g_ledger.frees);
    if (replace) {
        unsigned finalAllocation = g_ledger.records[retryRecord + 5u].allocatedAt;
        for (i = 0u; i < 6u; ++i) {
            TEST_ASSERT_EQUAL_UINT(1u, g_ledger.records[i].frees);
            TEST_ASSERT_TRUE(finalAllocation < g_ledger.records[i].freedAt);
        }
    }
    validate(source, replace ? 8 : 9);
    if (replace) {
        TEST_ASSERT_EQUAL_INT64(9, oldWitness.literal);
        TEST_ASSERT_EQUAL_INT64(8, (TZrInt64)g_descriptor.constants[0].bits);
        TEST_ASSERT_NOT_EQUAL_UINT64(0u, oldWitness.hash);
        TEST_ASSERT_NOT_EQUAL_UINT64(0u, oldWitness.moduleHash);
        TEST_ASSERT_NOT_EQUAL_UINT64(0u, oldWitness.signatureHash);
        TEST_ASSERT_NOT_EQUAL_UINT32(0u, oldWitness.functionId);
        TEST_ASSERT_NOT_EQUAL_UINT32(0u, oldWitness.functionToken);
        TEST_ASSERT_EQUAL_UINT32(oldWitness.schemaVersion, g_descriptor.module.schemaVersion);
        TEST_ASSERT_EQUAL_INT(oldWitness.abi.kind, g_descriptor.function.callableAbi.kind);
    }
    TEST_ASSERT_EQUAL_UINT64(nineBefore, source_observation(nine));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation(source)); release();
}
static void test_actual_nine_descriptor_allocation_prerequisite(void) { prerequisite(0u); }
static void test_actual_eight_descriptor_allocation_prerequisite(void) { prerequisite(1u); }
static void test_first_descriptor_instruction_oom_retry(void) { fault(1u, ZR_FALSE, ZR_FALSE); }
static void test_first_descriptor_block_oom_retry(void) { fault(2u, ZR_FALSE, ZR_FALSE); }
static void test_first_descriptor_frame_slot_oom_retry(void) { fault(3u, ZR_FALSE, ZR_FALSE); }
static void test_first_descriptor_source_map_oom_retry(void) { fault(4u, ZR_FALSE, ZR_FALSE); }
static void test_first_descriptor_constant_oom_retry(void) { fault(5u, ZR_FALSE, ZR_FALSE); }
static void test_first_descriptor_layout_oom_retry(void) { fault(6u, ZR_FALSE, ZR_FALSE); }
static void test_replace_descriptor_instruction_oom_retry(void) { fault(1u, ZR_TRUE, ZR_FALSE); }
static void test_replace_descriptor_block_oom_retry(void) { fault(2u, ZR_TRUE, ZR_FALSE); }
static void test_replace_descriptor_frame_slot_oom_retry(void) { fault(3u, ZR_TRUE, ZR_FALSE); }
static void test_replace_descriptor_source_map_oom_retry(void) { fault(4u, ZR_TRUE, ZR_FALSE); }
static void test_replace_descriptor_constant_oom_retry(void) { fault(5u, ZR_TRUE, ZR_FALSE); }
static void test_replace_descriptor_layout_oom_retry(void) { fault(6u, ZR_TRUE, ZR_FALSE); }
static void test_null_descriptor_arguments_preserve_existing_owner(void) {
    SSource *s = prepare(0u);
    SZrAotIrDiagnostic d;
    TZrUInt64 before, sourceBefore;
    unsigned attempts, i;
    TEST_ASSERT_TRUE(build(s, &d)); validate(s, 9);
    before = descriptor_observation(); sourceBefore = source_observation(s); attempts = g_ledger.attempts;
    for (i = 0u; i < 4u; ++i) {
        TZrBool result;
        memset(&d, 0xa5, sizeof(d));
        result = ZrParser_AotIrProjection_BuildDescriptor(i == 0u ? ZR_NULL : &s->projection,
                i == 1u ? ZR_NULL : &s->target, i == 2u ? ZR_NULL : &s->fixture.module.contract,
                i == 3u ? ZR_NULL : &g_descriptor, &d);
        TEST_ASSERT_FALSE(result); diagnostic_fields(&d, ZR_AOT_IR_INVALID_ARGUMENT, 0u);
        TEST_ASSERT_EQUAL_UINT(attempts, g_ledger.attempts);
        TEST_ASSERT_EQUAL_UINT(6u, g_ledger.live); TEST_ASSERT_EQUAL_UINT(0u, g_ledger.frees);
        TEST_ASSERT_EQUAL_UINT64(before, descriptor_observation());
        TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation(s)); validate(s, 9);
    }
    release();
}
static void test_optional_diagnostic_oom_cleanup_then_retry(void) { fault(6u, ZR_TRUE, ZR_TRUE); }
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_actual_nine_descriptor_allocation_prerequisite);
    RUN_TEST(test_actual_eight_descriptor_allocation_prerequisite);
    RUN_TEST(test_first_descriptor_instruction_oom_retry);
    RUN_TEST(test_first_descriptor_block_oom_retry);
    RUN_TEST(test_first_descriptor_frame_slot_oom_retry);
    RUN_TEST(test_first_descriptor_source_map_oom_retry);
    RUN_TEST(test_first_descriptor_constant_oom_retry);
    RUN_TEST(test_first_descriptor_layout_oom_retry);
    RUN_TEST(test_replace_descriptor_instruction_oom_retry);
    RUN_TEST(test_replace_descriptor_block_oom_retry);
    RUN_TEST(test_replace_descriptor_frame_slot_oom_retry);
    RUN_TEST(test_replace_descriptor_source_map_oom_retry);
    RUN_TEST(test_replace_descriptor_constant_oom_retry);
    RUN_TEST(test_replace_descriptor_layout_oom_retry);
    RUN_TEST(test_null_descriptor_arguments_preserve_existing_owner);
    RUN_TEST(test_optional_diagnostic_oom_cleanup_then_retry);
    return UNITY_END();
}
