#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "zr_vm_parser/exec_ir_source_frame.h"
#include "support/ssa_literal_script_fixture.h"

static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrExecIrFunction g_output;
static SZrExecIrOracleExecutionResult g_oracle;
static SZrExecIrFrameLayout *g_unexpectedFrame;

void setUp(void) {
    g_unexpectedFrame = ZR_NULL;
    ZrCore_ExecIr_FunctionInit(&g_output);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_state);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    if (g_unexpectedFrame != ZR_NULL) {
        ZrCore_ExecIr_FrameLayoutFree(g_unexpectedFrame);
        free(g_unexpectedFrame);
        g_unexpectedFrame = ZR_NULL;
    }
    ZrCore_ExecIr_FreeFunction(&g_output);
    ZrTests_SsaLiteralScriptFixture_Free(&g_fixture);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static TZrUInt64 digest_bytes(TZrUInt64 hash, const void *data, size_t size) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, data, size);
}

static TZrUInt64 function_digest(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
    if (function->frameLayout != ZR_NULL) {
        const SZrExecIrFrameLayout *frame = function->frameLayout;
        hash = digest_bytes(hash, frame, sizeof(*frame));
        hash = digest_bytes(hash, frame->slots,
                (size_t)frame->slotCount * sizeof(*frame->slots));
    }
    return hash;
}

static TZrUInt64 module_digest(void) {
    TZrUInt64 hash = digest_bytes(0u, &g_fixture.module, sizeof(g_fixture.module));
    hash = digest_bytes(hash, g_fixture.module.constants,
            (size_t)g_fixture.module.constantCount * sizeof(*g_fixture.module.constants));
    return digest_bytes(hash, g_fixture.module.layouts,
            (size_t)g_fixture.module.layoutCount * sizeof(*g_fixture.module.layouts));
}

static void verify(const SZrExecIrFunction *function) {
    SZrExecIrDiagnostic diagnostic = {0};
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(function,
            ZR_EXEC_IR_VERIFY_ALL, &diagnostic), "PRECONDITION: actual source graph VERIFY_ALL");
}

static void oracle(const SZrExecIrFunction *function, TZrUInt32 placeCalls) {
    ZrTests_SsaLiteralScriptFixture_AssertOracle(&g_fixture, function, placeCalls, &g_oracle);
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
}

/* Thin composition of the unique source fixture and production prerequisites.
 * Frame attachment is called only inside the four fault observations. */
static void prepare(void) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    const SZrCanonicalTypeNode *callable, *primitive;
    SZrExecIrFunction *original;
    TZrTypeId typeId;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    original = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    verify(original);
    oracle(original, 1u);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &g_fixture.compiler.preSemanticIr, g_fixture.compiler.semanticContext,
            original, &g_output, &diagnostic), "PRECONDITION: actual same-context compaction");
    verify(&g_output);
    oracle(&g_output, 0u);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_output);
    callable = ZrParser_CanonicalType_Find(g_fixture.compiler.semanticContext,
            g_fixture.compiler.preSemanticIr.callableTypeId);
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    typeId = callable->data.function.returnTypeId;
    primitive = ZrParser_CanonicalType_Find(g_fixture.compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(primitive);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    TEST_ASSERT_EQUAL_UINT32(1u, g_output.valueCount);
    TEST_ASSERT_EQUAL_UINT32(typeId, g_output.values[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, g_output.values[0].flags);
    TEST_ASSERT_NULL(g_output.frameLayout);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCapacity);
    TEST_ASSERT_NULL(g_fixture.module.layouts);
    TEST_ASSERT_TRUE(g_fixture.module.layoutCount < UINT32_MAX);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostPrimitiveLayout(
            g_fixture.compiler.semanticContext, typeId, g_fixture.module.layoutCount + 1u,
            &row, &diagnostic), "PRECONDITION: actual primitive host layout");
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(0u, range.offset);
    TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_fixture.module.layoutCount);
    TEST_ASSERT_NOT_NULL(g_fixture.module.layouts);
    TEST_ASSERT_EQUAL_UINT32(row.id, g_fixture.module.layouts[0].id);
    TEST_ASSERT_EQUAL_UINT32(row.typeToken, g_fixture.module.layouts[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(row.byteSize, g_fixture.module.layouts[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(row.byteAlign, g_fixture.module.layouts[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT64(row.layoutHash, g_fixture.module.layouts[0].layoutHash);
}

typedef struct SStorageObservation {
    TZrUInt64 function, original, source, module;
} SStorageObservation;

/* Called only while all real array headers are valid, never under a fault. */
static SStorageObservation observe_valid(void) {
    SStorageObservation observation;
    observation.function = function_digest(&g_output);
    observation.original = function_digest(ZrTests_SsaLiteralScriptFixture_Function(&g_fixture));
    observation.source = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    observation.module = module_digest();
    return observation;
}

static void assert_preserved(SStorageObservation before, TZrBool success,
        const SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code) {
    SStorageObservation after = observe_valid();
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_INT(code, diagnostic->code);
    TEST_ASSERT_EQUAL_UINT32(g_output.functionToken, diagnostic->functionToken);
    TEST_ASSERT_EQUAL_UINT64(before.function, after.function);
    TEST_ASSERT_EQUAL_UINT64(before.original, after.original);
    TEST_ASSERT_EQUAL_UINT64(before.source, after.source);
    TEST_ASSERT_EQUAL_UINT64(before.module, after.module);
}

static TZrBool call_frame(SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_output,
            g_fixture.compiler.semanticContext, g_fixture.module.layouts,
            g_fixture.module.layoutCount, 0u, diagnostic);
}

static void test_value_count_exceeds_capacity(void) {
    SStorageObservation before;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrFunction saved, faulted, afterCall;
    TZrBool success;
    prepare();
    TEST_ASSERT_EQUAL_UINT32(1u, g_output.valueCount);
    TEST_ASSERT_TRUE(g_output.valueCapacity >= g_output.valueCount);
    before = observe_valid();
    memcpy(&saved, &g_output, sizeof(saved));
    g_output.valueCapacity = g_output.valueCount - 1u;
    memcpy(&faulted, &g_output, sizeof(faulted));
    success = call_frame(&diagnostic);
    memcpy(&afterCall, &g_output, sizeof(afterCall));
    /* A broken producer may publish an owned frame before returning. Preserve
     * that owner before restoring the header, including assertion longjmp. */
    if (afterCall.frameLayout != ZR_NULL && afterCall.frameLayout != saved.frameLayout)
        g_unexpectedFrame = afterCall.frameLayout;
    memcpy(&g_output, &saved, sizeof(saved));
    /* Capture before restoration, which must not hide a producer mutation. */
    TEST_ASSERT_EQUAL_MEMORY(&faulted, &afterCall, sizeof(faulted));
    assert_preserved(before, success, &diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}

static void test_canonical_length_exceeds_capacity(void) {
    SStorageObservation before;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrArray saved, faulted, afterCall;
    TZrBool success;
    prepare();
    memcpy(&saved, &g_fixture.compiler.semanticContext->canonicalTypes, sizeof(saved));
    TEST_ASSERT_TRUE(saved.capacity < SIZE_MAX);
    before = observe_valid();
    g_fixture.compiler.semanticContext->canonicalTypes.length = saved.capacity + 1u;
    memcpy(&faulted, &g_fixture.compiler.semanticContext->canonicalTypes, sizeof(faulted));
    success = call_frame(&diagnostic);
    memcpy(&afterCall, &g_fixture.compiler.semanticContext->canonicalTypes, sizeof(afterCall));
    memcpy(&g_fixture.compiler.semanticContext->canonicalTypes, &saved, sizeof(saved));
    /* SourceDigest uses length: no digest or lookup until this full restore. */
    TEST_ASSERT_EQUAL_MEMORY(&faulted, &afterCall, sizeof(faulted));
    assert_preserved(before, success, &diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}

static void test_canonical_capacity_product_overflow(void) {
    SStorageObservation before;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrArray saved, faulted, afterCall;
    TZrBool success;
    prepare();
    TEST_ASSERT_TRUE(sizeof(SZrCanonicalTypeNode) > 1u);
    memcpy(&saved, &g_fixture.compiler.semanticContext->canonicalTypes, sizeof(saved));
    before = observe_valid();
    g_fixture.compiler.semanticContext->canonicalTypes.capacity =
            SIZE_MAX / sizeof(SZrCanonicalTypeNode) + 1u;
    memcpy(&faulted, &g_fixture.compiler.semanticContext->canonicalTypes, sizeof(faulted));
    success = call_frame(&diagnostic);
    memcpy(&afterCall, &g_fixture.compiler.semanticContext->canonicalTypes, sizeof(afterCall));
    memcpy(&g_fixture.compiler.semanticContext->canonicalTypes, &saved, sizeof(saved));
    TEST_ASSERT_EQUAL_MEMORY(&faulted, &afterCall, sizeof(faulted));
    assert_preserved(before, success, &diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
}

static void test_final_frame_alignment_overflow(void) {
    SStorageObservation before;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout saved, faulted, afterCall;
    TZrBool success;
    prepare();
    TEST_ASSERT_EQUAL_UINT32(1u, g_output.valueCount);
    memcpy(&saved, &g_fixture.module.layouts[0], sizeof(saved));
    TEST_ASSERT_TRUE(saved.byteAlign > 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, saved.byteAlign & (saved.byteAlign - 1u));
    before = observe_valid();
    /* Arithmetic failure contract, not a claim about a host ABI target row.
     * One value permits cursor=UINT32_MAX; the final alignment then overflows. */
    g_fixture.module.layouts[0].byteSize = UINT32_MAX;
    memcpy(&faulted, &g_fixture.module.layouts[0], sizeof(faulted));
    success = call_frame(&diagnostic);
    memcpy(&afterCall, &g_fixture.module.layouts[0], sizeof(afterCall));
    memcpy(&g_fixture.module.layouts[0], &saved, sizeof(saved));
    TEST_ASSERT_EQUAL_MEMORY(&faulted, &afterCall, sizeof(faulted));
    assert_preserved(before, success, &diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_value_count_exceeds_capacity);
    RUN_TEST(test_canonical_length_exceeds_capacity);
    RUN_TEST(test_canonical_capacity_product_overflow);
    RUN_TEST(test_final_frame_alignment_overflow);
    return UNITY_END();
}
