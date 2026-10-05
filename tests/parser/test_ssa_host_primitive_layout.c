#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "support/ssa_literal_script_fixture.h"

static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixtures[2];
static SZrExecIrOracleExecutionResult g_oracle;

void setUp(void) {
    TZrUInt32 index;
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    for (index = 0u; index < 2u; ++index)
        ZrTests_SsaLiteralScriptFixture_Init(&g_fixtures[index], g_state);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    for (index = 0u; index < 2u; ++index)
        ZrTests_SsaLiteralScriptFixture_Free(&g_fixtures[index]);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrSsaLiteralScriptFixture *prepare(TZrUInt32 index) {
    SZrSsaLiteralScriptFixture *fixture = &g_fixtures[index];
    ZrTests_SsaLiteralScriptFixture_Prepare(fixture, index == 0u
            ? ZR_TEST_SSA_LITERAL_SCRIPT_NINE : ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
    ZrTests_SsaLiteralScriptFixture_AssertOracle(fixture,
            ZrTests_SsaLiteralScriptFixture_Function(fixture), 1u, &g_oracle);
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
    return fixture;
}

static TZrTypeId actual_i64(SZrSsaLiteralScriptFixture *fixture) {
    const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(
            &fixture->compiler.preSemanticIr, 0u);
    const SZrCanonicalTypeNode *node;
    TEST_ASSERT_NOT_NULL(instruction);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_CONSTANT, instruction->opcode);
    node = ZrParser_CanonicalType_Find(fixture->compiler.semanticContext, instruction->typeId);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, node->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, node->data.primitive.valueType);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, node->structuralHash);
    return instruction->typeId;
}

/* Arithmetic byte extraction is independent of production shift encoders.
 * No object representation, padding or terminating domain NUL is hashed. */
static void encode_le(TZrByte *bytes, TZrUInt64 value, TZrUInt32 count) {
    TZrUInt32 index;
    for (index = 0u; index < count; ++index) {
        bytes[index] = (TZrByte)(value % UINT64_C(256));
        value /= UINT64_C(256);
    }
}

static TZrUInt64 expected_hash(const SZrCanonicalTypeNode *node) {
    static const char domain[] = "zr.execir.host.primitive-layout";
    TZrByte bytes[63];
    const TZrUInt16 endian = 1u;
    TEST_ASSERT_EQUAL_UINT32(31u, sizeof(domain) - 1u);
    memcpy(bytes, domain, sizeof(domain) - 1u);
    encode_le(bytes + 31u, 1u, 4u);
    encode_le(bytes + 35u, (TZrUInt32)node->data.primitive.valueType, 4u);
    encode_le(bytes + 39u, node->structuralHash, 8u);
    encode_le(bytes + 47u, sizeof(TZrInt64), 4u);
    encode_le(bytes + 51u, alignof(TZrInt64), 4u);
    encode_le(bytes + 55u, CHAR_BIT, 4u);
    encode_le(bytes + 59u, *(const TZrByte *)&endian == 1u ? 0u : 1u, 4u);
    return ZrCore_Hash_CreateStable64(bytes, sizeof(bytes));
}

static SZrExecIrLayout make_row(SZrSsaLiteralScriptFixture *fixture,
        TZrTypeId typeId, TZrUInt32 layoutId, TZrBool withDiagnostic) {
    SZrExecIrLayout row;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrDiagnostic cleared = {0};
    TZrUInt64 before = ZrTests_SsaLiteralScriptFixture_SourceDigest(fixture);
    TZrBool success;
    memset(&row, 0xa5, sizeof(row));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    success = ZrParser_ExecIr_MakeHostPrimitiveLayout(fixture->compiler.semanticContext,
            typeId, layoutId, &row, withDiagnostic ? &diagnostic : ZR_NULL);
    TEST_ASSERT_EQUAL_UINT64(before, ZrTests_SsaLiteralScriptFixture_SourceDigest(fixture));
    TEST_ASSERT_TRUE_MESSAGE(success, "FEATURE: host canonical INT64 layout row");
    if (withDiagnostic) TEST_ASSERT_EQUAL_MEMORY(&cleared, &diagnostic, sizeof(diagnostic));
    TEST_ASSERT_EQUAL_UINT32(layoutId, row.id);
    TEST_ASSERT_EQUAL_UINT32(typeId, row.typeToken);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_EQUAL_UINT64(expected_hash(ZrParser_CanonicalType_Find(
            fixture->compiler.semanticContext, typeId)), row.layoutHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    return row;
}

static TZrUInt32 next_layout_id(const SZrExecIrModule *module) {
    TEST_ASSERT_TRUE(module->layoutCount <= module->layoutCapacity);
    TEST_ASSERT_TRUE(module->layoutCount < UINT32_MAX);
    return module->layoutCount + 1u;
}

static void append_row(SZrExecIrModule *module, const SZrExecIrLayout *row) {
    SZrExecIrRange range;
    TZrUInt32 previous = module->layoutCount;
    TEST_ASSERT_EQUAL_UINT32(next_layout_id(module), row->id);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(module, row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(previous, range.offset);
    TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_EQUAL_UINT32(previous + 1u, module->layoutCount);
    TEST_ASSERT_TRUE(module->layoutCapacity >= module->layoutCount);
    TEST_ASSERT_NOT_NULL(module->layouts);
    TEST_ASSERT_EQUAL_UINT32(row->id, module->layouts[range.offset].id);
    TEST_ASSERT_EQUAL_UINT32(row->typeToken, module->layouts[range.offset].typeToken);
    TEST_ASSERT_EQUAL_UINT32(row->byteSize, module->layouts[range.offset].byteSize);
    TEST_ASSERT_EQUAL_UINT32(row->byteAlign, module->layouts[range.offset].byteAlign);
    TEST_ASSERT_EQUAL_UINT64(row->layoutHash, module->layouts[range.offset].layoutHash);
}

static void test_prerequisite_nine(void) { (void)actual_i64(prepare(0u)); }
static void test_prerequisite_eight(void) { (void)actual_i64(prepare(1u)); }

static void test_host_i64_rows_append_and_layout_id_does_not_change_hash(void) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    TZrTypeId typeId = actual_i64(fixture);
    SZrExecIrLayout first, second;
    TEST_ASSERT_EQUAL_UINT32(0u, fixture->module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture->module.layoutCapacity);
    TEST_ASSERT_NULL(fixture->module.layouts);
    first = make_row(fixture, typeId, next_layout_id(&fixture->module), ZR_TRUE);
    append_row(&fixture->module, &first);
    second = make_row(fixture, typeId, next_layout_id(&fixture->module), ZR_TRUE);
    append_row(&fixture->module, &second);
    TEST_ASSERT_NOT_EQUAL_UINT32(first.id, second.id);
    TEST_ASSERT_EQUAL_UINT64(first.layoutHash, second.layoutHash);
}

static void test_two_real_contexts_produce_same_host_hash(void) {
    SZrSsaLiteralScriptFixture *nine = prepare(0u), *eight = prepare(1u);
    TZrTypeId nineId = actual_i64(nine), eightId = actual_i64(eight);
    SZrExecIrLayout first, second;
    TEST_ASSERT_TRUE(nine->compiler.semanticContext != eight->compiler.semanticContext);
    first = make_row(nine, nineId, next_layout_id(&nine->module), ZR_TRUE);
    second = make_row(eight, eightId, next_layout_id(&eight->module), ZR_TRUE);
    append_row(&nine->module, &first);
    append_row(&eight->module, &second);
    TEST_ASSERT_EQUAL_UINT64(first.layoutHash, second.layoutHash);
    /* TypeIds and runtime salts may coincide: this demonstrates only the actual
     * distinct context addresses, not a dynamic exclusion of those other inputs. */
}

static void test_success_accepts_null_diagnostic(void) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    SZrExecIrLayout row = make_row(fixture, actual_i64(fixture),
            next_layout_id(&fixture->module), ZR_FALSE);
    append_row(&fixture->module, &row);
}

static void assert_failure(const SZrSemanticContext *context, TZrTypeId typeId,
        TZrUInt32 layoutId, TZrBool outputPresent, EZrExecutionDiagnosticCode code) {
    SZrExecIrLayout row, before;
    SZrExecIrDiagnostic diagnostic;
    TZrBool success;
    memset(&row, 0xa5, sizeof(row));
    memcpy(&before, &row, sizeof(row));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    success = ZrParser_ExecIr_MakeHostPrimitiveLayout(context, typeId, layoutId,
            outputPresent ? &row : ZR_NULL, &diagnostic);
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_MEMORY(&before, &row, sizeof(row));
    TEST_ASSERT_EQUAL_INT(code, diagnostic.code);
}

static void test_required_arguments_and_zero_ids(void) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    const SZrSemanticContext *context = fixture->compiler.semanticContext;
    TZrTypeId typeId = actual_i64(fixture);
    assert_failure(ZR_NULL, typeId, 1u, ZR_TRUE, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert_failure(context, typeId, 1u, ZR_FALSE, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert_failure(context, 0u, 1u, ZR_TRUE, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert_failure(context, typeId, 0u, ZR_TRUE, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
}

static void test_unknown_type_id(void) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    SZrSemanticContext *context = fixture->compiler.semanticContext;
    TZrTypeId unknownId;
    TEST_ASSERT_TRUE(context->canonicalTypes.length < UINT32_MAX);
    unknownId = (TZrTypeId)context->canonicalTypes.length + 1u;
    TEST_ASSERT_NULL_MESSAGE(ZrParser_CanonicalType_Find(context, unknownId),
            "PRECONDITION: derived ID is absent from the actual context");
    assert_failure(context, unknownId, 1u, ZR_TRUE, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}

enum EArrayMutation { BAD_VALID, BAD_ELEMENT_SIZE, BAD_LENGTH, BAD_HEAD,
    CAPACITY_PRODUCT_OVERFLOW, ADDRESS_SPAN_OVERFLOW };

static void assert_array_failure(enum EArrayMutation mutation,
        EZrExecutionDiagnosticCode code) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    TZrTypeId typeId = actual_i64(fixture);
    SZrSemanticContext *context = fixture->compiler.semanticContext;
    SZrArray saved = context->canonicalTypes;
    SZrExecIrLayout row, before;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrBool success;
    memset(&row, 0xa5, sizeof(row));
    memcpy(&before, &row, sizeof(row));
    switch (mutation) {
        case BAD_VALID: context->canonicalTypes.isValid = ZR_FALSE; break;
        case BAD_ELEMENT_SIZE: context->canonicalTypes.elementSize = 1u; break;
        case BAD_LENGTH: context->canonicalTypes.length = saved.capacity + 1u; break;
        case BAD_HEAD: context->canonicalTypes.head = ZR_NULL; break;
        case CAPACITY_PRODUCT_OVERFLOW:
            context->canonicalTypes.capacity = SIZE_MAX / sizeof(SZrCanonicalTypeNode) + 1u;
            break;
        case ADDRESS_SPAN_OVERFLOW:
            context->canonicalTypes.head = (TZrUInt8 *)(uintptr_t)(UINTPTR_MAX - 1u);
            break;
    }
    /* No assertion or fake-address dereference until real storage is restored. */
    success = ZrParser_ExecIr_MakeHostPrimitiveLayout(context, typeId, 1u, &row, &diagnostic);
    context->canonicalTypes = saved;
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_MEMORY(&before, &row, sizeof(row));
    TEST_ASSERT_EQUAL_INT(code, diagnostic.code);
}

static void test_invalid_canonical_array_valid_flag(void) {
    assert_array_failure(BAD_VALID, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}
static void test_invalid_canonical_array_element_size(void) {
    assert_array_failure(BAD_ELEMENT_SIZE, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}
static void test_invalid_canonical_array_length(void) {
    assert_array_failure(BAD_LENGTH, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}
static void test_invalid_canonical_array_null_head(void) {
    assert_array_failure(BAD_HEAD, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}
static void test_canonical_capacity_product_overflow(void) {
    assert_array_failure(CAPACITY_PRODUCT_OVERFLOW, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
}
static void test_canonical_address_span_overflow(void) {
    assert_array_failure(ADDRESS_SPAN_OVERFLOW, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
}

static void test_actual_callable_is_unsupported(void) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    TZrTypeId callableId = fixture->compiler.preSemanticIr.callableTypeId;
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(
            fixture->compiler.semanticContext, callableId);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, node->kind);
    assert_failure(fixture->compiler.semanticContext, callableId, 1u,
            ZR_TRUE, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_actual_node_zero_hash_preserves_output(void) {
    SZrSsaLiteralScriptFixture *fixture = prepare(0u);
    TZrTypeId typeId = actual_i64(fixture);
    SZrCanonicalTypeNode *node = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(
            fixture->compiler.semanticContext, typeId);
    TZrUInt64 saved = node->structuralHash;
    SZrExecIrLayout row, before;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrBool success;
    memset(&row, 0xa5, sizeof(row));
    memcpy(&before, &row, sizeof(row));
    node->structuralHash = 0u;
    success = ZrParser_ExecIr_MakeHostPrimitiveLayout(fixture->compiler.semanticContext,
            typeId, 1u, &row, &diagnostic);
    node->structuralHash = saved;
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_MEMORY(&before, &row, sizeof(row));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, diagnostic.code);
}

int main(int argc, char **argv) {
    TZrBool prerequisitesOnly = ZR_FALSE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) prerequisitesOnly = ZR_TRUE;
    else if (argc != 1) {
        (void)fprintf(stderr, "usage: %s [--prerequisites-only]\n", argv[0]);
        return EXIT_FAILURE;
    }
    UNITY_BEGIN();
    RUN_TEST(test_prerequisite_nine);
    RUN_TEST(test_prerequisite_eight);
    if (!prerequisitesOnly) {
        RUN_TEST(test_host_i64_rows_append_and_layout_id_does_not_change_hash);
        RUN_TEST(test_two_real_contexts_produce_same_host_hash);
        RUN_TEST(test_success_accepts_null_diagnostic);
        RUN_TEST(test_required_arguments_and_zero_ids);
        RUN_TEST(test_unknown_type_id);
        RUN_TEST(test_invalid_canonical_array_valid_flag);
        RUN_TEST(test_invalid_canonical_array_element_size);
        RUN_TEST(test_invalid_canonical_array_length);
        RUN_TEST(test_invalid_canonical_array_null_head);
        RUN_TEST(test_canonical_capacity_product_overflow);
        RUN_TEST(test_canonical_address_span_overflow);
        RUN_TEST(test_actual_callable_is_unsupported);
        RUN_TEST(test_actual_node_zero_hash_preserves_output);
    }
    return UNITY_END();
}
