#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define ZR_TEST_SSA_SOURCE_SCALAR_CONSTANT_NO_REDIRECT
#include "support/ssa_source_scalar_constant_noalloc_shim.h"
#include "unity.h"
#include "support/ssa_literal_script_fixture.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/array.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_source_module_contract.h"
#include "zr_vm_parser/exec_ir_source_scalar_constant.h"

enum { VM_RECORDS = 8192, VM_DIAGNOSTICS = 64, SOURCES = 2 };
typedef struct SAllocation { void *pointer; size_t bytes; TZrInt64 flag; } SAllocation;
typedef struct SAllocatorDiagnostic {
    unsigned reason, slot;
    size_t allocated, original, requested;
    TZrInt64 previousFlag, requestedFlag;
} SAllocatorDiagnostic;
typedef struct SLedger {
    SAllocation rows[VM_RECORDS];
    SAllocatorDiagnostic diagnostics[VM_DIAGNOSTICS];
    unsigned attempts, frees, live, errors, diagnosticCount, dropped;
} SLedger;
typedef struct SObservation {
    TZrUInt64 digest;
    SZrTypeValue compilerRow, runtimeRow;
    unsigned attempts, frees, direct[4];
} SObservation;
static SLedger g_vm;
static unsigned g_direct[4];
static SZrGlobalState *g_global;
static SZrSsaLiteralScriptFixture g_fixture[SOURCES];
static TZrBool g_registered[SOURCES], g_savedStopGc, g_haveSavedStopGc;
static SZrExecIrModule g_fresh;

static void ledger_error(unsigned reason, unsigned slot, size_t original,
                         size_t requested, TZrInt64 flag) {
    SAllocatorDiagnostic *d;
    ++g_vm.errors;
    if (g_vm.diagnosticCount == VM_DIAGNOSTICS) { ++g_vm.dropped; return; }
    d = &g_vm.diagnostics[g_vm.diagnosticCount++];
    d->reason = reason; d->slot = slot; d->original = original;
    d->requested = requested; d->requestedFlag = flag;
    if (slot < VM_RECORDS) {
        d->allocated = g_vm.rows[slot].bytes;
        d->previousFlag = g_vm.rows[slot].flag;
    }
}

/* Strict sizes/types, bounded static bookkeeping, no assertion or I/O in callback. */
static TZrPtr vm_allocator(TZrPtr user, TZrPtr pointer, TZrSize original,
                           TZrSize requested, TZrInt64 flag) {
    unsigned slot = VM_RECORDS;
    void *replacement;
    TZrBool wasNew = pointer == ZR_NULL;
    (void)user;
    ++g_vm.attempts;
    for (unsigned i = 0u; i < VM_RECORDS; ++i) {
        if (pointer != ZR_NULL ? g_vm.rows[i].pointer == pointer :
                                g_vm.rows[i].pointer == ZR_NULL) { slot = i; break; }
    }
    if (slot == VM_RECORDS || (pointer == ZR_NULL && requested == 0u)) {
        ledger_error(1u, slot, original, requested, flag); return ZR_NULL;
    }
    if (pointer != ZR_NULL && (g_vm.rows[slot].bytes != original ||
                              g_vm.rows[slot].flag != flag))
        ledger_error(2u, slot, original, requested, flag);
    if (pointer == ZR_NULL && original != 0u)
        ledger_error(3u, slot, original, requested, flag);
    if (requested == 0u) {
        g_vm.rows[slot].pointer = ZR_NULL; --g_vm.live; ++g_vm.frees;
        free(pointer); return ZR_NULL;
    }
    replacement = realloc(pointer, requested);
    if (replacement == ZR_NULL) {
        ledger_error(4u, slot, original, requested, flag); return ZR_NULL;
    }
    if (wasNew) ++g_vm.live;
    g_vm.rows[slot].pointer = replacement;
    g_vm.rows[slot].bytes = requested; g_vm.rows[slot].flag = flag;
    return replacement;
}

/* A direct bridge allocation is a contract violation; these never allocate. */
void *ZrTests_SourceScalarConstant_Malloc(size_t bytes) {
    (void)bytes; ++g_direct[0]; return ZR_NULL;
}
void *ZrTests_SourceScalarConstant_Calloc(size_t count, size_t bytes) {
    (void)count; (void)bytes; ++g_direct[1]; return ZR_NULL;
}
void *ZrTests_SourceScalarConstant_Realloc(void *pointer, size_t bytes) {
    (void)pointer; (void)bytes; ++g_direct[2]; return ZR_NULL;
}
void ZrTests_SourceScalarConstant_Free(void *pointer) {
    (void)pointer; ++g_direct[3];
}

void setUp(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    memset(&g_vm, 0, sizeof(g_vm)); memset(g_direct, 0, sizeof(g_direct));
    memset(g_fixture, 0, sizeof(g_fixture)); memset(g_registered, 0, sizeof(g_registered));
    g_global = ZR_NULL; g_savedStopGc = ZR_FALSE; g_haveSavedStopGc = ZR_FALSE;
    ZrCore_ExecIr_ModuleInit(&g_fresh);
}
void tearDown(void) {
    ZrCore_ExecIr_FreeModule(&g_fresh);
    for (unsigned i = 0u; i < SOURCES; ++i)
        if (g_registered[i]) ZrTests_SsaLiteralScriptFixture_Free(&g_fixture[i]);
    if (g_global != ZR_NULL) {
        SZrGlobalState *live = g_global;
        if (g_haveSavedStopGc && live->garbageCollector != ZR_NULL)
            live->garbageCollector->stopGcFlag = g_savedStopGc;
        g_haveSavedStopGc = ZR_FALSE;
        g_global = ZR_NULL;
        ZrCore_GlobalState_Free(live);
    }
    /* Only independent numeric diagnostics are read after owner destruction. */
    for (unsigned i = 0u; i < g_vm.diagnosticCount; ++i) {
        const SAllocatorDiagnostic *d = &g_vm.diagnostics[i];
        fprintf(stderr, "VM_LEDGER reason=%u slot=%u allocated=%llu original=%llu requested=%llu priorFlag=%lld requestFlag=%lld\n",
                d->reason, d->slot, (unsigned long long)d->allocated,
                (unsigned long long)d->original, (unsigned long long)d->requested,
                (long long)d->previousFlag, (long long)d->requestedFlag);
    }
    if (g_vm.dropped != 0u) fprintf(stderr, "VM_LEDGER dropped=%u\n", g_vm.dropped);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
    for (unsigned i = 0u; i < 4u; ++i) TEST_ASSERT_EQUAL_UINT(0u, g_direct[i]);
}

static SZrSsaLiteralScriptFixture *prepare(unsigned slot, EZrSsaLiteralScriptSource source) {
    if (g_global == ZR_NULL) {
        g_global = ZrCore_GlobalState_New(vm_allocator, &g_vm, 0xA018u, ZR_NULL);
        TEST_ASSERT_NOT_NULL(g_global); TEST_ASSERT_NOT_NULL(g_global->mainThreadState);
        TEST_ASSERT_NOT_NULL(g_global->garbageCollector);
        g_savedStopGc = g_global->garbageCollector->stopGcFlag;
        g_haveSavedStopGc = ZR_TRUE;
        g_global->garbageCollector->stopGcFlag = ZR_TRUE;
        TEST_ASSERT_NULL(g_global->profileRuntime); TEST_ASSERT_NULL(g_global->compileSource);
        TEST_ASSERT_NULL(g_global->sourceLoader); TEST_ASSERT_NULL(g_global->nativeModuleLoader);
        TEST_ASSERT_NULL(g_global->aotModuleLoader); TEST_ASSERT_NULL(g_global->providerModuleNameResolver);
    }
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture[slot], g_global->mainThreadState);
    g_registered[slot] = ZR_TRUE;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture[slot], source);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
    return &g_fixture[slot];
}
static SZrSemanticIrInstruction *instruction(SZrSsaLiteralScriptFixture *f, unsigned index) {
    return (SZrSemanticIrInstruction *)ZrCore_Array_Get(&f->compiler.preSemanticIr.instructions, index);
}
static SZrTypeValue *compiler_row(SZrSsaLiteralScriptFixture *f) {
    return (SZrTypeValue *)ZrCore_Array_Get(&f->compiler.constants, 0u);
}
static TZrUInt64 source_digest(SZrSsaLiteralScriptFixture *f) {
    TZrUInt64 h = ZrTests_SsaLiteralScriptFixture_SourceDigest(f);
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h, &f->compiler, sizeof(f->compiler));
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h, f->ast, sizeof(*f->ast));
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h, f->compiler.currentFunction,
                                                  sizeof(*f->compiler.currentFunction));
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h,
            f->compiler.currentFunction->constantValueList,
            f->compiler.currentFunction->constantValueLength * sizeof(SZrTypeValue));
    return h;
}
static SObservation observe(SZrSsaLiteralScriptFixture *f) {
    SObservation o;
    o.digest = source_digest(f);
    memcpy(&o.compilerRow, compiler_row(f), sizeof(o.compilerRow));
    memcpy(&o.runtimeRow, f->compiler.currentFunction->constantValueList, sizeof(o.runtimeRow));
    o.attempts = g_vm.attempts; o.frees = g_vm.frees;
    memcpy(o.direct, g_direct, sizeof(o.direct)); return o;
}
static void assert_observation(SZrSsaLiteralScriptFixture *f, const SObservation *o) {
    TEST_ASSERT_EQUAL_UINT64(o->digest, source_digest(f));
    TEST_ASSERT_EQUAL_MEMORY(&o->compilerRow, compiler_row(f), sizeof(o->compilerRow));
    TEST_ASSERT_EQUAL_MEMORY(&o->runtimeRow, f->compiler.currentFunction->constantValueList,
                             sizeof(o->runtimeRow));
    TEST_ASSERT_EQUAL_UINT(o->attempts, g_vm.attempts); TEST_ASSERT_EQUAL_UINT(o->frees, g_vm.frees);
    for (unsigned i = 0u; i < 4u; ++i) {
        TEST_ASSERT_EQUAL_UINT(o->direct[i], g_direct[i]); TEST_ASSERT_EQUAL_UINT(0u, g_direct[i]);
    }
}
static void assert_diagnostic(const SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
                              TZrUInt32 source, TZrUInt64 expected, TZrUInt64 actual) {
    TEST_ASSERT_EQUAL_INT(code, d->code);
    TEST_ASSERT_EQUAL_UINT32(0u, d->functionToken); TEST_ASSERT_EQUAL_UINT32(0u, d->blockId);
    TEST_ASSERT_EQUAL_UINT32(0u, d->instructionId); TEST_ASSERT_EQUAL_UINT32(source, d->sourceId);
    TEST_ASSERT_EQUAL_UINT32(0u, d->expectedVersion); TEST_ASSERT_EQUAL_UINT32(0u, d->actualVersion);
    TEST_ASSERT_EQUAL_UINT64(expected, d->expectedHash); TEST_ASSERT_EQUAL_UINT64(actual, d->actualHash);
}
static SZrExecIrConstant expected_constant(SZrSsaLiteralScriptFixture *f) {
    SZrExecIrConstant c = {0};
    SZrSemanticIrInstruction *s = instruction(f, 0u);
    const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(f->compiler.semanticContext, s->typeId);
    TEST_ASSERT_NOT_NULL(type); TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, type->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, type->data.primitive.valueType);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, compiler_row(f)->type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, f->compiler.currentFunction->constantValueList[0].type);
    TEST_ASSERT_EQUAL_INT64(compiler_row(f)->value.nativeObject.nativeInt64,
            f->compiler.currentFunction->constantValueList[0].value.nativeObject.nativeInt64);
    c.typeToken = s->typeId; c.bits = (TZrUInt64)compiler_row(f)->value.nativeObject.nativeInt64;
    return c;
}
static SZrExecIrConstant extract(SZrSsaLiteralScriptFixture *f, TZrBool withDiagnostic) {
    SZrExecIrConstant output, expected = expected_constant(f);
    SZrExecIrDiagnostic d;
    SObservation o = observe(f);
    TZrBool ok;
    memset(&output, 0xA5, sizeof(output)); memset(&d, 0xB6, sizeof(d));
    ok = ZrParser_ExecIr_MakeSourceScalarConstant(&f->compiler, &output, withDiagnostic ? &d : ZR_NULL);
    fprintf(stderr, "SCALAR_BRIDGE result=%u diagnostic=%u source=%u\n",
            (unsigned)ok, withDiagnostic ? (unsigned)d.code : 0u,
            withDiagnostic ? (unsigned)d.sourceId : 0u);
    assert_observation(f, &o);
    if (!ok) {
        SZrExecIrConstant unchanged; memset(&unchanged, 0xA5, sizeof(unchanged));
        TEST_ASSERT_EQUAL_MEMORY(&unchanged, &output, sizeof(output));
    }
    TEST_ASSERT_TRUE_MESSAGE(ok, "actual linked bridge must accept finalized literal SCRIPT");
    if (withDiagnostic) assert_diagnostic(&d, 0, 0u, 0u, 0u);
    TEST_ASSERT_EQUAL_UINT32(expected.typeToken, output.typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, output.flags); TEST_ASSERT_EQUAL_UINT64(expected.bits, output.bits);
    TEST_ASSERT_EQUAL_MEMORY(&expected, &output, sizeof(output)); return output;
}
/* Mutations are live bounded scalar/header fields, restored before any assertion. */
static void refuse(SZrSsaLiteralScriptFixture *f, void *field, const void *replacement, size_t bytes,
                    EZrExecutionDiagnosticCode code, TZrUInt32 source,
                    TZrUInt64 expected, TZrUInt64 actual) {
    unsigned char saved[sizeof(SZrTypeValue) + sizeof(SZrArray)];
    SZrExecIrConstant output, unchanged;
    SZrExecIrDiagnostic d;
    SObservation o = observe(f);
    SZrTypeValue compilerAfter, runtimeAfter;
    TZrUInt64 mutatedDigest, afterDigest;
    TZrBool ok;
    TEST_ASSERT_LESS_OR_EQUAL_UINT(sizeof(saved), bytes);
    memcpy(saved, field, bytes); memset(&output, 0xA5, sizeof(output)); unchanged = output;
    memset(&d, 0xB6, sizeof(d)); memcpy(field, replacement, bytes);
    mutatedDigest = source_digest(f);
    ok = ZrParser_ExecIr_MakeSourceScalarConstant(&f->compiler, &output, &d);
    afterDigest = source_digest(f);
    memcpy(&compilerAfter, f->compiler.constants.head, sizeof(compilerAfter));
    memcpy(&runtimeAfter, f->compiler.currentFunction->constantValueList, sizeof(runtimeAfter));
    memcpy(field, saved, bytes);
    /* Compare while-call observations only after the fault is restored. The
     * selected header/row cannot hide a bridge write behind that restoration. */
    TEST_ASSERT_EQUAL_UINT64(mutatedDigest, afterDigest);
    if (field != (void *)&compiler_row(f)->type)
        TEST_ASSERT_EQUAL_MEMORY(&o.compilerRow, &compilerAfter, sizeof(compilerAfter));
    if (field != (void *)&f->compiler.currentFunction->constantValueList[0].type &&
        field != (void *)&f->compiler.currentFunction->constantValueList[0].value.nativeObject.nativeInt64)
        TEST_ASSERT_EQUAL_MEMORY(&o.runtimeRow, &runtimeAfter, sizeof(runtimeAfter));
    assert_observation(f, &o); TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_MEMORY(&unchanged, &output, sizeof(output));
    assert_diagnostic(&d, code, source, expected, actual);
}
#define REFUSE(f, field, replacement, code, source, expected, actual) \
    refuse((f), &(field), &(replacement), sizeof(field), (code), (source), (expected), (actual))

static void prerequisite(EZrSsaLiteralScriptSource source, TZrInt64 literal) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, source);
    SZrCompilerState *c = &f->compiler;
    SZrExecIrConstant expected = expected_constant(f);
    SZrExecIrDiagnostic d = {0};
    const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(c->semanticContext,
                                                        c->preSemanticIr.callableTypeId);
    TEST_ASSERT_TRUE(c->preSemanticIrInitialized); TEST_ASSERT_TRUE(c->preSemanticIrValidated);
    TEST_ASSERT_TRUE(c->preSemanticIrCfgActive); TEST_ASSERT_TRUE(c->preSemanticIrCfgTerminated);
    TEST_ASSERT_EQUAL_PTR(c->currentAst, c->scriptAst); TEST_ASSERT_FALSE(c->hasError);
    TEST_ASSERT_EQUAL_UINT32(3u, c->preSemanticIr.instructions.length);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_CONSTANT, instruction(f, 0u)->opcode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_PLACE_BASE, instruction(f, 1u)->opcode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_RETURN, instruction(f, 2u)->opcode);
    TEST_ASSERT_TRUE(instruction(f, 0u)->hasConstantPoolIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, instruction(f, 0u)->constantPoolIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, c->constants.length);
    TEST_ASSERT_EQUAL_UINT32(sizeof(SZrTypeValue), c->constants.elementSize);
    TEST_ASSERT_EQUAL_UINT32(1u, c->currentFunction->constantValueLength);
    TEST_ASSERT_EQUAL_PTR(c->state, c->preSemanticIr.state);
    TEST_ASSERT_EQUAL_PTR(c->state, c->semanticContext->state);
    TEST_ASSERT_TRUE(c->preSemanticIr.values.isValid);
    TEST_ASSERT_EQUAL_UINT32(sizeof(SZrSemanticIrValue), c->preSemanticIr.values.elementSize);
    TEST_ASSERT_TRUE(c->preSemanticIr.values.length <= c->preSemanticIr.values.capacity);
    TEST_ASSERT_TRUE(c->preSemanticIr.values.length <= 3u);
    TEST_ASSERT_NOT_NULL(c->preSemanticIr.values.head);
    TEST_ASSERT_TRUE(c->preSemanticIr.valueOperands.isValid);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrValueId), c->preSemanticIr.valueOperands.elementSize);
    TEST_ASSERT_TRUE(c->preSemanticIr.valueOperands.length <= c->preSemanticIr.valueOperands.capacity);
    TEST_ASSERT_NOT_NULL(c->preSemanticIr.valueOperands.head);
    TEST_ASSERT_EQUAL_UINT32(1u, instruction(f, 2u)->operandCount);
    TEST_ASSERT_TRUE(instruction(f, 2u)->operandStart < c->preSemanticIr.valueOperands.length);
    fprintf(stderr, "SCALAR_SEMIR source=%u values=%llu constantResult=%u placeResult=%u returnOperand=%u\n",
            (unsigned)source, (unsigned long long)c->preSemanticIr.values.length,
            (unsigned)instruction(f, 0u)->resultValueId, (unsigned)instruction(f, 1u)->resultValueId,
            (unsigned)*(TZrValueId *)ZrCore_Array_Get(&c->preSemanticIr.valueOperands,
                                                    instruction(f, 2u)->operandStart));
    for (TZrSize i = 0u; i < c->preSemanticIr.values.length; ++i) {
        const SZrSemanticIrValue *v = (const SZrSemanticIrValue *)ZrCore_Array_Get(&c->preSemanticIr.values, i);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)i + 1u, v->id);
        fprintf(stderr, "SCALAR_SEMIR_VALUE source=%u row=%llu id=%u typeId=%u definition=%u\n",
                (unsigned)source, (unsigned long long)i, (unsigned)v->id,
                (unsigned)v->typeId, (unsigned)v->definitionInstructionId);
    }
    TEST_ASSERT_EQUAL_UINT32(instruction(f, 0u)->resultValueId,
            *(TZrValueId *)ZrCore_Array_Get(&c->preSemanticIr.valueOperands,
                                           instruction(f, 2u)->operandStart));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)literal, expected.bits);
    TEST_ASSERT_NOT_NULL(callable); TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash,
                            c->currentFunction->sourceCallableIdentity.canonicalSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(c->preSemanticIr.callableTypeId,
                            c->currentFunction->sourceCallableIdentity.typeId);
    TEST_ASSERT_EQUAL_UINT32(c->preSemanticIr.symbolId,
                            c->currentFunction->sourceCallableIdentity.symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1,
                            c->currentFunction->sourceCallableIdentity.schemaVersion);
    TEST_ASSERT_TRUE(c->currentFunction->sourceCallableIdentity.hasExplicitNoArgsI64);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(ZrTests_SsaLiteralScriptFixture_Function(f),
                                                ZR_EXEC_IR_VERIFY_ALL, &d));
}
static void test_actual_nine_lower_fixture_prerequisite(void) {
    prerequisite(ZR_TEST_SSA_LITERAL_SCRIPT_NINE, 9);
}
static void test_actual_eight_lower_fixture_prerequisite(void) {
    prerequisite(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT, 8);
}
static void test_bridge_extracts_actual_nine_without_allocation(void) {
    (void)extract(prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE), ZR_TRUE);
}
static void test_bridge_extracts_actual_eight_without_allocation(void) {
    (void)extract(prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT), ZR_TRUE);
}
static void test_two_live_sources_remain_independent(void) {
    SZrSsaLiteralScriptFixture *nine = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SZrSsaLiteralScriptFixture *eight = prepare(1u, ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
    SZrExecIrConstant a, b, savedA, savedB;
    TEST_ASSERT_TRUE(nine->compiler.constants.head != eight->compiler.constants.head);
    TEST_ASSERT_TRUE(nine->compiler.currentFunction->constantValueList !=
                     eight->compiler.currentFunction->constantValueList);
    a = extract(nine, ZR_TRUE); b = extract(eight, ZR_TRUE); savedA = a; savedB = b;
    ZrTests_SsaLiteralScriptFixture_Free(nine); g_registered[0] = ZR_FALSE;
    ZrTests_SsaLiteralScriptFixture_Free(eight); g_registered[1] = ZR_FALSE;
    TEST_ASSERT_EQUAL_MEMORY(&savedA, &a, sizeof(a)); TEST_ASSERT_EQUAL_MEMORY(&savedB, &b, sizeof(b));
    TEST_ASSERT_EQUAL_UINT64(9u, a.bits); TEST_ASSERT_EQUAL_UINT64(8u, b.bits);
}
static void test_null_arguments_preserve_output(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SZrExecIrConstant output, saved;
    SZrExecIrDiagnostic d;
    SObservation o = observe(f);
    TZrBool ok;
    memset(&output, 0xA5, sizeof(output)); saved = output; memset(&d, 0xB6, sizeof(d));
    ok = ZrParser_ExecIr_MakeSourceScalarConstant(ZR_NULL, &output, &d);
    assert_observation(f, &o); TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &output, sizeof(output));
    assert_diagnostic(&d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u, 0u, 0u);
    memset(&d, 0xB6, sizeof(d));
    ok = ZrParser_ExecIr_MakeSourceScalarConstant(&f->compiler, ZR_NULL, &d);
    assert_observation(f, &o); TEST_ASSERT_FALSE(ok);
    assert_diagnostic(&d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u, 0u, 0u);
    ok = ZrParser_ExecIr_MakeSourceScalarConstant(ZR_NULL, &output, ZR_NULL);
    assert_observation(f, &o); TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &output, sizeof(output));
}
static void test_optional_diagnostic_does_not_change_extraction(void) {
    (void)extract(prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE), ZR_FALSE);
}
static void test_uncertified_or_unterminated_source_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrBool no = ZR_FALSE;
    REFUSE(f, f->compiler.preSemanticIrInitialized, no, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u, 1u, 0u);
    REFUSE(f, f->compiler.preSemanticIrValidated, no, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u, 1u, 0u);
    REFUSE(f, f->compiler.preSemanticIrCfgActive, no, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u, 1u, 0u);
    REFUSE(f, f->compiler.preSemanticIrCfgTerminated, no, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u, 1u, 0u);
}
static void test_script_and_snapshot_eligibility_is_preserved(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SZrAstNode *none = ZR_NULL;
    TZrBool yes = ZR_TRUE;
    REFUSE(f, f->compiler.currentAst, none, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u, 1u, 0u);
    REFUSE(f, f->compiler.hasError, yes, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u, 1u, 0u);
    /* A second State constructor is deliberately outside this finite profile. */
}
static void test_declared_callable_effect_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrUInt32 effect = 1u;
    REFUSE(f, f->compiler.currentFunction->sourceCallableIdentity.effectFlags, effect,
            ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0u, 1u, 0u);
}
static void test_missing_constant_index_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrBool no = ZR_FALSE;
    REFUSE(f, instruction(f, 0u)->hasConstantPoolIndex, no,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, instruction(f, 0u)->id, 1u, 0u);
}
static void test_constant_index_outside_live_pool_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrUInt32 onePast = (TZrUInt32)f->compiler.constants.length;
    REFUSE(f, instruction(f, 0u)->constantPoolIndex, onePast,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, instruction(f, 0u)->id, onePast, onePast);
}
static void test_compiler_pool_count_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrSize zero = 0u;
    REFUSE(f, f->compiler.constants.length, zero, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u, 0u, 1u);
}
static void test_compiler_pool_geometry_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrSize wrongWidth = sizeof(SZrTypeValue) - 1u, zero = 0u;
    REFUSE(f, f->compiler.constants.elementSize, wrongWidth,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u, sizeof(SZrTypeValue), wrongWidth);
    REFUSE(f, f->compiler.constants.capacity, zero, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u, 0u, 1u);
}
static void test_runtime_pool_count_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrUInt32 zero = 0u;
    REFUSE(f, f->compiler.currentFunction->constantValueLength, zero,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u, 1u, 0u);
}
static void test_compiler_pool_type_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    EZrValueType boolean = ZR_VALUE_TYPE_BOOL;
    REFUSE(f, compiler_row(f)->type, boolean, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
            instruction(f, 0u)->id, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL);
}
static void test_runtime_pool_type_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    EZrValueType boolean = ZR_VALUE_TYPE_BOOL;
    REFUSE(f, f->compiler.currentFunction->constantValueList[0].type, boolean,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, instruction(f, 0u)->id, ZR_VALUE_TYPE_INT64, ZR_VALUE_TYPE_BOOL);
}
static void test_live_pool_value_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrInt64 eight = 8;
    REFUSE(f, f->compiler.currentFunction->constantValueList[0].value.nativeObject.nativeInt64,
            eight, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, instruction(f, 0u)->id, 9u, 8u);
}
static void test_missing_source_canonical_type_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrTypeId absent = 0u;
    REFUSE(f, instruction(f, 0u)->typeId, absent, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
            instruction(f, 0u)->id, 1u, 0u);
}
static void test_nonprimitive_source_canonical_type_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrTypeId callable = f->compiler.preSemanticIr.callableTypeId;
    REFUSE(f, instruction(f, 0u)->typeId, callable, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
            instruction(f, 0u)->id, 1u, 0u);
}
static void test_result_type_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrTypeId callable = f->compiler.preSemanticIr.callableTypeId;
    TZrValueId result = instruction(f, 0u)->resultValueId;
    SZrSemanticIrValue *v = (SZrSemanticIrValue *)ZrCore_Array_Get(&f->compiler.preSemanticIr.values, result - 1u);
    REFUSE(f, v->typeId, callable, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
            instruction(f, 0u)->id, instruction(f, 0u)->typeId, callable);
}
static void test_invalid_return_operand_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SZrSemanticIrInstruction *ret = instruction(f, 2u);
    TZrValueId invalid = ZR_VALUE_ID_INVALID;
    TZrValueId *operand;
    TEST_ASSERT_TRUE(f->compiler.preSemanticIr.valueOperands.isValid);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrValueId), f->compiler.preSemanticIr.valueOperands.elementSize);
    TEST_ASSERT_TRUE(f->compiler.preSemanticIr.valueOperands.length <=
                     f->compiler.preSemanticIr.valueOperands.capacity);
    TEST_ASSERT_NOT_NULL(f->compiler.preSemanticIr.valueOperands.head);
    TEST_ASSERT_EQUAL_UINT32(1u, ret->operandCount);
    TEST_ASSERT_TRUE(ret->operandStart < f->compiler.preSemanticIr.valueOperands.length);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_VALUE_ID_INVALID, instruction(f, 0u)->resultValueId);
    TEST_ASSERT_TRUE(instruction(f, 0u)->resultValueId <= f->compiler.preSemanticIr.values.length);
    TEST_ASSERT_NOT_NULL(ZrParser_SemanticIr_Value(&f->compiler.preSemanticIr,
                                                 instruction(f, 0u)->resultValueId));
    operand = (TZrValueId *)ZrCore_Array_Get(&f->compiler.preSemanticIr.valueOperands, ret->operandStart);
    REFUSE(f, *operand, invalid, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, ret->id,
            instruction(f, 0u)->resultValueId, invalid);
}
static void test_finalized_signature_witness_disagreement_is_refused(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TZrUInt64 hash = f->compiler.currentFunction->sourceCallableIdentity.canonicalSignatureHash;
    TZrUInt64 different = hash == 1u ? 2u : 1u;
    REFUSE(f, f->compiler.currentFunction->sourceCallableIdentity.canonicalSignatureHash,
            different, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0u, hash, different);
}
static void test_fresh_build_uses_only_production_bridge_constant(void) {
    SZrSsaLiteralScriptFixture *f = prepare(0u, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SZrCompilerState *c = &f->compiler;
    SZrFunction *source = c->currentFunction;
    const SZrMetadataTokenRecord *entry = ZR_NULL, *module = ZR_NULL;
    const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(c->semanticContext,
                                                         c->preSemanticIr.callableTypeId);
    SZrExecIrBuildInput build = {0};
    SZrExecIrDiagnostic d = {0};
    SZrExecIrConstant constant;
    SZrExecIrRange range;
    SZrExecIrFunction *function;
    TZrUInt64 digest = source_digest(f);
    unsigned liveBefore = g_vm.live;
    for (TZrUInt32 i = 0u; i < source->metadataTokenRecordLength; ++i) {
        const SZrMetadataTokenRecord *r = &source->metadataTokenRecords[i];
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MODULE) {
            TEST_ASSERT_NULL(module); module = r;
        }
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            r->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) { TEST_ASSERT_NULL(entry); entry = r; }
    }
    TEST_ASSERT_NOT_NULL(module); TEST_ASSERT_NOT_NULL(entry); TEST_ASSERT_NOT_NULL(callable);
    g_fresh.moduleToken = module->token; g_fresh.moduleHash = source->moduleSignatureHash;
    g_fresh.contract.moduleHash = source->moduleSignatureHash;
    build.semanticFunction = &c->preSemanticIr; build.functionToken = entry->token;
    build.signatureHash = callable->structuralHash;
    build.options.preserveSourceMaps = ZR_TRUE; build.options.preserveDeoptStates = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildModule(&build, &g_fresh, &d));
    TEST_ASSERT_EQUAL_UINT32(0u, g_fresh.constantCount);
    constant = extract(f, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendConstant(&g_fresh, &constant, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(0u, range.offset); TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_fresh.constantCount);
    function = ZrCore_ExecIr_ModuleFunctionAt(&g_fresh, 1u); TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, function->instructions[0].layoutId);
    /* Binder is a separate allocating interval, outside bridge nonallocation observations. */
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BindSourceModuleContract(c, &g_fresh, &d));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyModule(&g_fresh, &d));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Verify(&g_fresh, &d));
    TEST_ASSERT_EQUAL_UINT64(digest, source_digest(f));
    ZrCore_ExecIr_FreeModule(&g_fresh);
    {
        SZrExecIrModule empty = {0};
        TEST_ASSERT_EQUAL_MEMORY(&empty, &g_fresh, sizeof(empty));
    }
    TEST_ASSERT_EQUAL_UINT(liveBefore, g_vm.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_actual_nine_lower_fixture_prerequisite);
    RUN_TEST(test_actual_eight_lower_fixture_prerequisite);
    RUN_TEST(test_bridge_extracts_actual_nine_without_allocation);
    RUN_TEST(test_bridge_extracts_actual_eight_without_allocation);
    RUN_TEST(test_two_live_sources_remain_independent);
    RUN_TEST(test_null_arguments_preserve_output);
    RUN_TEST(test_optional_diagnostic_does_not_change_extraction);
    RUN_TEST(test_uncertified_or_unterminated_source_is_refused);
    RUN_TEST(test_script_and_snapshot_eligibility_is_preserved);
    RUN_TEST(test_declared_callable_effect_is_refused);
    RUN_TEST(test_missing_constant_index_is_refused);
    RUN_TEST(test_constant_index_outside_live_pool_is_refused);
    RUN_TEST(test_compiler_pool_count_disagreement_is_refused);
    RUN_TEST(test_compiler_pool_geometry_is_refused);
    RUN_TEST(test_runtime_pool_count_disagreement_is_refused);
    RUN_TEST(test_compiler_pool_type_disagreement_is_refused);
    RUN_TEST(test_runtime_pool_type_disagreement_is_refused);
    RUN_TEST(test_live_pool_value_disagreement_is_refused);
    RUN_TEST(test_missing_source_canonical_type_is_refused);
    RUN_TEST(test_nonprimitive_source_canonical_type_is_refused);
    RUN_TEST(test_result_type_disagreement_is_refused);
    RUN_TEST(test_invalid_return_operand_is_refused);
    RUN_TEST(test_finalized_signature_witness_disagreement_is_refused);
    RUN_TEST(test_fresh_build_uses_only_production_bridge_constant);
    return UNITY_END();
}
