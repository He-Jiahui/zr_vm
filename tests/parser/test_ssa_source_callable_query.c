#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ZR_TEST_SOURCE_CALLABLE_NO_REDIRECT
#include "support/ssa_source_callable_query_shim.h"
#include "unity.h"
#include "exec_ir/source/exec_ir_source_callable_query_internal.h"
#include "zr_vm_parser/semantic.h"

enum { CALLABLE_ID = 271u, RETURN_ID = 113u, TOKEN = 0x06000031u };
typedef struct SFixture {
    SZrSemanticContext context;
    SZrCanonicalTypeNode nodes[4];
    SZrCanonicalParameterContract parameters[2];
} SFixture;
typedef struct SCall {
    SFixture before, after;
    SSourceCallableFacts beforeOutput, afterOutput;
    SZrExecIrDiagnostic diagnostic;
    TZrBool success, hasOutput, hasDiagnostic;
} SCall;
static SFixture g_fixture;
static unsigned g_attempts, g_frees;

/* Only production query/lookup objects are redirected. No callback asserts,
 * allocates bookkeeping, retains pointers, or observes pointers after realloc/free. */
void *ZrTests_SourceCallable_Malloc(size_t bytes) {
    ++g_attempts;
    return malloc(bytes);
}
void *ZrTests_SourceCallable_Calloc(size_t count, size_t width) {
    ++g_attempts;
    return calloc(count, width);
}
void *ZrTests_SourceCallable_Realloc(void *pointer, size_t bytes) {
    ++g_attempts;
    return realloc(pointer, bytes);
}
void ZrTests_SourceCallable_Free(void *pointer) {
    ++g_frees;
    free(pointer);
}

static SZrArray declared_array(void *head, size_t width, size_t length, size_t capacity) {
    SZrArray a;
    memset(&a, 0, sizeof(a));
    a.head = (TZrByte *) head;
    a.elementSize = width;
    a.length = length;
    a.capacity = capacity;
    a.isValid = ZR_TRUE;
    return a;
}
static void initialize(SFixture *f) {
    memset(f, 0, sizeof(*f));
    f->nodes[0].id = RETURN_ID;
    f->nodes[0].kind = ZR_CANONICAL_TYPE_PRIMITIVE;
    f->nodes[0].structuralHash = 701u;
    f->nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_INT64;
    f->nodes[1].id = CALLABLE_ID;
    f->nodes[1].kind = ZR_CANONICAL_TYPE_FUNCTION;
    f->nodes[1].structuralHash = 907u;
    f->nodes[1].data.function.returnTypeId = RETURN_ID;
    f->nodes[1].data.function.parameterContracts = declared_array(ZR_NULL, sizeof(*f->parameters), 0u, 0u);
    f->context.canonicalTypes = declared_array(f->nodes, sizeof(*f->nodes), 2u, 4u);
}
void setUp(void) {
    g_attempts = g_frees = 0u;
    initialize(&g_fixture);
}
void tearDown(void) {
    if (g_attempts || g_frees)
        fprintf(stderr, "SOURCE_CALLABLE_LEDGER attempts=%u frees=%u\n", g_attempts, g_frees);
    TEST_ASSERT_EQUAL_UINT(0u, g_attempts);
    TEST_ASSERT_EQUAL_UINT(0u, g_frees);
}
static SZrExecIrDiagnostic error(EZrExecutionDiagnosticCode code) {
    SZrExecIrDiagnostic d = {0};
    d.code = code;
    d.functionToken = TOKEN;
    return d;
}
static SZrExecIrDiagnostic versions(EZrExecutionDiagnosticCode code, TZrUInt32 expected, TZrUInt32 actual) {
    SZrExecIrDiagnostic d = error(code);
    d.expectedVersion = expected;
    d.actualVersion = actual;
    return d;
}
static SZrExecIrDiagnostic hashes(TZrUInt64 expected, TZrUInt64 actual) {
    SZrExecIrDiagnostic d = error(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    d.expectedHash = expected;
    d.actualHash = actual;
    return d;
}
static void diagnostic_equal(const SZrExecIrDiagnostic *e, const SZrExecIrDiagnostic *a) {
    TEST_ASSERT_EQUAL_UINT(e->code, a->code);
    TEST_ASSERT_EQUAL_UINT32(e->functionToken, a->functionToken);
    TEST_ASSERT_EQUAL_UINT32(e->blockId, a->blockId);
    TEST_ASSERT_EQUAL_UINT32(e->instructionId, a->instructionId);
    TEST_ASSERT_EQUAL_UINT32(e->sourceId, a->sourceId);
    TEST_ASSERT_EQUAL_UINT32(e->expectedVersion, a->expectedVersion);
    TEST_ASSERT_EQUAL_UINT32(e->actualVersion, a->actualVersion);
    TEST_ASSERT_EQUAL_UINT64(e->expectedHash, a->expectedHash);
    TEST_ASSERT_EQUAL_UINT64(e->actualHash, a->actualHash);
}
static SCall call(SFixture *f, const SZrSemanticContext *context, TZrTypeId id, TZrMetadataToken token,
                  SSourceCallableFacts *output, TZrBool withDiagnostic) {
    SCall r;
    memset(&r, 0, sizeof(r));
    memcpy(&r.before, f, sizeof(*f));
    r.hasOutput = output != ZR_NULL;
    r.hasDiagnostic = withDiagnostic;
    if (output != ZR_NULL)
        memcpy(&r.beforeOutput, output, sizeof(*output));
    memset(&r.diagnostic, 0xA5, sizeof(r.diagnostic));
    r.success = source_callable_query(context, id, token, output, withDiagnostic ? &r.diagnostic : ZR_NULL);
    memcpy(&r.after, f, sizeof(*f));
    if (output != ZR_NULL)
        memcpy(&r.afterOutput, output, sizeof(*output));
    return r;
}
static void unchanged(const SCall *r) {
    TEST_ASSERT_EQUAL_MEMORY(&r->before, &r->after, sizeof(r->before));
    TEST_ASSERT_EQUAL_UINT(0u, g_attempts);
    TEST_ASSERT_EQUAL_UINT(0u, g_frees);
}
static void refused(const SCall *r, SZrExecIrDiagnostic expected) {
    unchanged(r);
    TEST_ASSERT_FALSE(r->success);
    if (r->hasOutput)
        TEST_ASSERT_EQUAL_MEMORY(&r->beforeOutput, &r->afterOutput, sizeof(r->beforeOutput));
    if (r->hasDiagnostic)
        diagnostic_equal(&expected, &r->diagnostic);
}
static void accepted(const SCall *r, const SFixture *f, unsigned callable, unsigned result) {
    SZrExecIrDiagnostic zero = {0};
    unchanged(r);
    TEST_ASSERT_TRUE(r->success);
    if (r->hasDiagnostic)
        diagnostic_equal(&zero, &r->diagnostic);
    TEST_ASSERT_EQUAL_UINT32(f->nodes[callable].id, r->afterOutput.callableTypeId);
    TEST_ASSERT_EQUAL_UINT32(f->nodes[result].id, r->afterOutput.returnTypeId);
    TEST_ASSERT_EQUAL_UINT64(f->nodes[callable].structuralHash, r->afterOutput.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(f->nodes[result].structuralHash, r->afterOutput.returnStructuralHash);
    TEST_ASSERT_EQUAL_INT(f->nodes[result].data.primitive.valueType, r->afterOutput.returnPrimitive);
}
static SCall invoke(TZrTypeId id) {
    SSourceCallableFacts output;
    memset(&output, 0xA5, sizeof(output));
    return call(&g_fixture, &g_fixture.context, id, TOKEN, &output, ZR_TRUE);
}
static void feature(void) {
    TEST_ASSERT_EQUAL_PTR(&g_fixture.nodes[1], ZrParser_CanonicalType_Find(&g_fixture.context, CALLABLE_ID));
    TEST_ASSERT_EQUAL_PTR(&g_fixture.nodes[0], ZrParser_CanonicalType_Find(&g_fixture.context, RETURN_ID));
    TEST_ASSERT_EQUAL_UINT(0u, g_attempts);
    TEST_ASSERT_EQUAL_UINT(0u, g_frees);
    SCall r = invoke(CALLABLE_ID);
    unchanged(&r);
    if (!r.success) {
        refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
        fprintf(stderr, "SOURCE_CALLABLE_FEATURE code=28 attempts=0 frees=0\n");
    }
    TEST_ASSERT_TRUE_MESSAGE(r.success, "actual const source callable query must succeed");
    accepted(&r, &g_fixture, 1u, 0u);
}
static void test_current_declared_callable_returns_exact_numeric_facts(void) {
    feature();
    SSourceCallableFacts output;
    memset(&output, 0xA5, sizeof(output));
    SCall r = call(&g_fixture, &g_fixture.context, CALLABLE_ID, 0u, &output, ZR_TRUE);
    accepted(&r, &g_fixture, 1u, 0u);
}
static void test_null_arguments_reset_all_nine_fields(void) {
    for (unsigned mask = 1u; mask < 4u; ++mask) {
        SSourceCallableFacts output;
        memset(&output, 0xA5, sizeof(output));
        SCall r = call(&g_fixture, (mask & 1u) ? ZR_NULL : &g_fixture.context, CALLABLE_ID, TOKEN,
                       (mask & 2u) ? ZR_NULL : &output, ZR_TRUE);
        SZrExecIrDiagnostic e = {0};
        e.code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        refused(&r, e);
    }
}
static void test_null_diagnostic_preserves_result_and_refusals(void) {
    feature();
    SSourceCallableFacts output;
    memset(&output, 0xA5, sizeof(output));
    SCall r = call(&g_fixture, &g_fixture.context, CALLABLE_ID, TOKEN, &output, ZR_FALSE);
    accepted(&r, &g_fixture, 1u, 0u);
    memset(&output, 0xA5, sizeof(output));
    r = call(&g_fixture, &g_fixture.context, 0u, TOKEN, &output, ZR_FALSE);
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 1u, 0u));
    g_fixture.nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_BOOL;
    memset(&output, 0xA5, sizeof(output));
    r = call(&g_fixture, &g_fixture.context, CALLABLE_ID, TOKEN, &output, ZR_FALSE);
    g_fixture.nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_INT64;
    refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
}
static void test_zero_callable_id_precedes_storage_checks(void) {
    feature();
    SZrArray saved = g_fixture.context.canonicalTypes;
    g_fixture.context.canonicalTypes.isValid = ZR_FALSE;
    SCall r = invoke(0u);
    g_fixture.context.canonicalTypes = saved;
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 1u, 0u));
}
static void test_canonical_header_validity_width_and_capacity_are_checked(void) {
    feature();
    SZrArray saved = g_fixture.context.canonicalTypes;
    for (unsigned kind = 0u; kind < 3u; ++kind) {
        if (kind == 0u)
            g_fixture.context.canonicalTypes.isValid = ZR_FALSE;
        if (kind == 1u)
            g_fixture.context.canonicalTypes.elementSize = 1u;
        if (kind == 2u)
            g_fixture.context.canonicalTypes.capacity = 1u;
        SCall r = invoke(CALLABLE_ID);
        g_fixture.context.canonicalTypes = saved;
        refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    }
}
static void test_canonical_pointer_capacity_and_checked_spans_are_checked(void) {
    feature();
    SZrArray saved = g_fixture.context.canonicalTypes;
    for (unsigned kind = 0u; kind < 4u; ++kind) {
        if (kind == 0u)
            g_fixture.context.canonicalTypes.head = ZR_NULL;
        if (kind == 1u) {
            g_fixture.context.canonicalTypes.length = 0u;
            g_fixture.context.canonicalTypes.capacity = 0u;
        }
        /* Readable malformed header; reject product before any alleged giant traversal. */
        if (kind == 2u)
            g_fixture.context.canonicalTypes.capacity = SIZE_MAX / sizeof(*g_fixture.nodes) + 1u;
        /* Real pointer, representable product, overflowing numeric end: no traversal. */
        if (kind == 3u)
            g_fixture.context.canonicalTypes.capacity = UINTPTR_MAX / sizeof(*g_fixture.nodes);
        SCall r = invoke(CALLABLE_ID);
        g_fixture.context.canonicalTypes = saved;
        refused(&r, error(kind >= 2u ? ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW : ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    }
}
static void test_output_overlap_is_refused_without_publication(void) {
    feature();
    union SAlignedOverlap {
        SZrSemanticContext context;
        SZrCanonicalTypeNode nodes[4];
        SZrCanonicalParameterContract parameters[2];
        SSourceCallableFacts output;
    } u;
    unsigned char before[sizeof(u)];
    for (unsigned kind = 0u; kind < 3u; ++kind) {
        initialize(&g_fixture);
        memset(&u, 0, sizeof(u));
        if (kind == 0u)
            u.context = g_fixture.context;
        if (kind == 1u) {
            memcpy(u.nodes, g_fixture.nodes, sizeof(u.nodes));
            g_fixture.context.canonicalTypes.head = (TZrByte *) u.nodes;
        }
        if (kind == 2u) {
            g_fixture.nodes[1].data.function.parameterContracts =
                    declared_array(u.parameters, sizeof(*u.parameters), 0u, 2u);
        }
        memcpy(before, &u, sizeof(u)); /* Preserve observed bytes, not a corrupting result poison. */
        SZrExecIrDiagnostic d;
        memset(&d, 0xA5, sizeof(d));
        SFixture observed;
        memcpy(&observed, &g_fixture, sizeof(observed));
        TZrBool ok =
                source_callable_query(kind == 0u ? &u.context : &g_fixture.context, CALLABLE_ID, TOKEN, &u.output, &d);
        TZrBool same = memcmp(before, &u, sizeof(u)) == 0 && memcmp(&observed, &g_fixture, sizeof(observed)) == 0;
        initialize(&g_fixture);
        TEST_ASSERT_TRUE(same);
        TEST_ASSERT_FALSE(ok);
        SZrExecIrDiagnostic e = error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
        diagnostic_equal(&e, &d);
        TEST_ASSERT_EQUAL_UINT(0u, g_attempts);
        TEST_ASSERT_EQUAL_UINT(0u, g_frees);
    }
}
static void test_canonical_ids_are_nonzero_strictly_sorted(void) {
    feature();
    const TZrTypeId bad[3] = {0u, RETURN_ID, RETURN_ID - 1u};
    for (unsigned i = 0u; i < 3u; ++i) {
        g_fixture.nodes[1].id = bad[i];
        SCall r = invoke(CALLABLE_ID);
        g_fixture.nodes[1].id = CALLABLE_ID;
        refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, i ? RETURN_ID : 1u, bad[i]));
    }
}
static void test_empty_sparse_and_missing_callable_tables_refuse_exactly(void) {
    feature();
    SZrArray saved = g_fixture.context.canonicalTypes;
    g_fixture.context.canonicalTypes = declared_array(ZR_NULL, sizeof(*g_fixture.nodes), 0u, 0u);
    SCall r = invoke(CALLABLE_ID);
    g_fixture.context.canonicalTypes = saved;
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, CALLABLE_ID, 0u));
    const TZrTypeId ids[4] = {1u, RETURN_ID + 1u, CALLABLE_ID + 1u, UINT32_MAX};
    for (unsigned i = 0u; i < 4u; ++i) {
        r = invoke(ids[i]);
        refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ids[i], 0u));
    }
}
static void test_callable_kind_and_hash_precede_parameter_union_reads(void) {
    feature();
    SZrCanonicalTypeNode saved = g_fixture.nodes[1];
    g_fixture.nodes[1].data.function.parameterContracts.isValid = ZR_FALSE;
    g_fixture.nodes[1].kind = ZR_CANONICAL_TYPE_PRIMITIVE;
    SCall r = invoke(CALLABLE_ID);
    g_fixture.nodes[1] = saved;
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_CANONICAL_TYPE_FUNCTION,
                         ZR_CANONICAL_TYPE_PRIMITIVE));
    g_fixture.nodes[1].structuralHash = 0u;
    g_fixture.nodes[1].data.function.parameterContracts.isValid = ZR_FALSE;
    r = invoke(CALLABLE_ID);
    g_fixture.nodes[1] = saved;
    refused(&r, hashes(1u, 0u));
}
static void test_parameter_header_and_allocation_geometry_precede_profile(void) {
    feature();
    SZrCanonicalFunctionType *f = &g_fixture.nodes[1].data.function;
    SZrCanonicalFunctionType saved = *f;
    for (unsigned kind = 0u; kind < 7u; ++kind) {
        f->parameterContracts = declared_array(g_fixture.parameters, sizeof(*g_fixture.parameters), 1u, 2u);
        if (kind == 0u)
            f->parameterContracts.isValid = ZR_FALSE;
        if (kind == 1u)
            f->parameterContracts.elementSize = 1u;
        if (kind == 2u)
            f->parameterContracts.capacity = 0u;
        if (kind == 3u)
            f->parameterContracts.head = ZR_NULL;
        if (kind == 4u) {
            f->parameterContracts.length = 0u;
            f->parameterContracts.capacity = 0u;
        }
        if (kind == 5u)
            f->parameterContracts.capacity = SIZE_MAX / sizeof(*g_fixture.parameters) + 1u;
        if (kind == 6u)
            f->parameterContracts.capacity = UINTPTR_MAX / sizeof(*g_fixture.parameters);
        SCall r = invoke(CALLABLE_ID);
        *f = saved;
        refused(&r, error(kind >= 5u ? ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW : ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    }
}
static void test_noargs_receiver_and_effect_profile_order_is_exact(void) {
    feature();
    SZrCanonicalFunctionType *f = &g_fixture.nodes[1].data.function, saved = *f;
    /* These three payloads are identical. Pair each with a distinct later return refusal. */
    for (unsigned kind = 0u; kind < 3u; ++kind) {
        for (unsigned paired = 0u; paired < 2u; ++paired) {
            if (kind == 0u)
                f->parameterContracts = declared_array(g_fixture.parameters, sizeof(*g_fixture.parameters), 1u, 2u);
            if (kind == 1u)
                f->receiverEffect = ZR_CANONICAL_RECEIVER_READONLY;
            if (kind == 2u)
                f->effectFlags = ZR_CANONICAL_CALLABLE_EFFECT_THROWS;
            if (paired)
                f->returnTypeId = 0u;
            SCall r = invoke(CALLABLE_ID);
            *f = saved;
            refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
        }
    }
}
static void test_zero_missing_and_wrong_kind_return_nodes_refuse_exactly(void) {
    feature();
    const TZrTypeId bad[2] = {0u, 114u};
    for (unsigned i = 0u; i < 2u; ++i) {
        g_fixture.nodes[1].data.function.returnTypeId = bad[i];
        SCall r = invoke(CALLABLE_ID);
        g_fixture.nodes[1].data.function.returnTypeId = RETURN_ID;
        refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, i ? bad[i] : 1u, 0u));
    }
    g_fixture.nodes[0].kind = ZR_CANONICAL_TYPE_FUNCTION;
    SCall r = invoke(CALLABLE_ID);
    g_fixture.nodes[0].kind = ZR_CANONICAL_TYPE_PRIMITIVE;
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_CANONICAL_TYPE_PRIMITIVE,
                         ZR_CANONICAL_TYPE_FUNCTION));
}
static void test_return_hash_precedes_primitive_profile(void) {
    feature();
    g_fixture.nodes[0].structuralHash = 0u;
    g_fixture.nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_BOOL;
    SCall r = invoke(CALLABLE_ID);
    initialize(&g_fixture);
    refused(&r, hashes(1u, 0u));
}
static void test_non_i64_return_is_unsupported(void) {
    feature();
    g_fixture.nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_BOOL;
    SCall r = invoke(CALLABLE_ID);
    g_fixture.nodes[0].data.primitive.valueType = ZR_VALUE_TYPE_INT64;
    refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED));
}
static void test_live_spare_parameter_storage_and_maximum_ids_are_supported(void) {
    feature();
    g_fixture.nodes[1].data.function.parameterContracts =
            declared_array(g_fixture.parameters, sizeof(*g_fixture.parameters), 0u, 2u);
    g_fixture.nodes[1].id = UINT32_MAX;
    SCall r = invoke(UINT32_MAX);
    initialize(&g_fixture);
    accepted(&r, &r.before, 1u, 0u);
    g_fixture.nodes[1].data.function.parameterContracts =
            declared_array(g_fixture.parameters, sizeof(*g_fixture.parameters), 0u, 2u);
    g_fixture.nodes[1].id = UINT32_MAX;
    g_fixture.nodes[0].id = UINT32_MAX - 1u;
    g_fixture.nodes[1].data.function.returnTypeId = g_fixture.nodes[0].id;
    r = invoke(UINT32_MAX);
    initialize(&g_fixture);
    accepted(&r, &r.before, 1u, 0u);
}
static void test_independent_equal_ids_and_repeat_calls_preserve_inputs(void) {
    feature();
    SFixture second;
    initialize(&second);
    second.nodes[0].structuralHash = 809u;
    second.nodes[1].structuralHash = 1103u;
    for (unsigned i = 0u; i < 16u; ++i) {
        SFixture *f = i & 1u ? &second : &g_fixture;
        SSourceCallableFacts output;
        memset(&output, 0xA5, sizeof(output));
        SCall r = call(f, &f->context, CALLABLE_ID, TOKEN, &output, ZR_TRUE);
        accepted(&r, f, 1u, 0u);
    }
}
static void test_paired_errors_and_every_failure_preserve_output(void) {
    feature();
    /* Earlier geometry, ID-order, callable, parameter and return-kind errors have
     * distinct payloads from the later errors paired with them here. */
    g_fixture.context.canonicalTypes.elementSize = 1u;
    g_fixture.nodes[1].id = 0u;
    SCall r = invoke(CALLABLE_ID);
    initialize(&g_fixture);
    refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    g_fixture.nodes[1].id = RETURN_ID;
    g_fixture.nodes[1].structuralHash = 0u;
    r = invoke(CALLABLE_ID);
    initialize(&g_fixture);
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, RETURN_ID, RETURN_ID));
    g_fixture.nodes[1].data.function.parameterContracts.isValid = ZR_FALSE;
    g_fixture.nodes[1].data.function.returnTypeId = 0u;
    r = invoke(CALLABLE_ID);
    initialize(&g_fixture);
    refused(&r, error(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE));
    g_fixture.nodes[0].kind = ZR_CANONICAL_TYPE_FUNCTION;
    g_fixture.nodes[0].structuralHash = 0u;
    r = invoke(CALLABLE_ID);
    initialize(&g_fixture);
    refused(&r, versions(ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_CANONICAL_TYPE_PRIMITIVE,
                         ZR_CANONICAL_TYPE_FUNCTION));
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_current_declared_callable_returns_exact_numeric_facts);
    RUN_TEST(test_null_arguments_reset_all_nine_fields);
    RUN_TEST(test_null_diagnostic_preserves_result_and_refusals);
    RUN_TEST(test_zero_callable_id_precedes_storage_checks);
    RUN_TEST(test_canonical_header_validity_width_and_capacity_are_checked);
    RUN_TEST(test_canonical_pointer_capacity_and_checked_spans_are_checked);
    RUN_TEST(test_output_overlap_is_refused_without_publication);
    RUN_TEST(test_canonical_ids_are_nonzero_strictly_sorted);
    RUN_TEST(test_empty_sparse_and_missing_callable_tables_refuse_exactly);
    RUN_TEST(test_callable_kind_and_hash_precede_parameter_union_reads);
    RUN_TEST(test_parameter_header_and_allocation_geometry_precede_profile);
    RUN_TEST(test_noargs_receiver_and_effect_profile_order_is_exact);
    RUN_TEST(test_zero_missing_and_wrong_kind_return_nodes_refuse_exactly);
    RUN_TEST(test_return_hash_precedes_primitive_profile);
    RUN_TEST(test_non_i64_return_is_unsupported);
    RUN_TEST(test_live_spare_parameter_storage_and_maximum_ids_are_supported);
    RUN_TEST(test_independent_equal_ids_and_repeat_calls_preserve_inputs);
    RUN_TEST(test_paired_errors_and_every_failure_preserve_output);
    int result = UNITY_END();
    if (result == 0 && Unity.NumberOfTests == 18u && Unity.TestIgnores == 0u && g_attempts == 0u && g_frees == 0u)
        fprintf(stderr, "SOURCE_CALLABLE_ZERO_ALLOC cases=18 attempts=0 frees=0\n");
    return result;
}
