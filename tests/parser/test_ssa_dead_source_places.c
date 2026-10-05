#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRT declarations precede Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "support/ssa_literal_script_fixture.h"

enum { DEAD_PLACE_FIXTURE_COUNT = 2 };
typedef SZrSsaLiteralScriptFixture SZrDeadPlaceFixture;

static SZrState *g_state;
static SZrDeadPlaceFixture g_fixtures[DEAD_PLACE_FIXTURE_COUNT];
static SZrExecIrFunction g_output;
static SZrExecIrFunction g_mutated;
static SZrExecIrOracleExecutionResult g_oracle;

void setUp(void) {
    TZrUInt32 index;
    ZrCore_ExecIr_FunctionInit(&g_output);
    ZrCore_ExecIr_FunctionInit(&g_mutated);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    for (index = 0u; index < DEAD_PLACE_FIXTURE_COUNT; ++index)
        ZrTests_SsaLiteralScriptFixture_Init(&g_fixtures[index], g_state);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_FreeFunction(&g_output);
    ZrCore_ExecIr_FreeFunction(&g_mutated);
    for (index = 0u; index < DEAD_PLACE_FIXTURE_COUNT; ++index)
        ZrTests_SsaLiteralScriptFixture_Free(&g_fixtures[index]);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static void assert_api(TZrBool success, const SZrExecIrDiagnostic *diagnostic,
                       const char *message) {
    if (!success && diagnostic != ZR_NULL) {
        (void)printf("%s: code=%u token=%u instruction=%u source=%u\n", message,
                (unsigned)diagnostic->code, (unsigned)diagnostic->functionToken,
                (unsigned)diagnostic->instructionId, (unsigned)diagnostic->sourceId);
    }
    TEST_ASSERT_TRUE_MESSAGE(success, message);
}

/* Thin adapters preserve the compaction boundary helpers used by edges.inc. */
static SZrExecIrFunction *fixture_function(SZrDeadPlaceFixture *fixture) {
    return ZrTests_SsaLiteralScriptFixture_Function(fixture);
}

static TZrUInt64 digest_bytes(TZrUInt64 previous, const void *bytes, size_t size) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(previous, bytes, size);
}

static TZrUInt64 function_digest(const SZrExecIrFunction *function) {
    return ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
}

static TZrUInt64 source_digest(const SZrDeadPlaceFixture *fixture) {
    return ZrTests_SsaLiteralScriptFixture_SourceDigest(fixture);
}

static void assert_source_maps(const SZrDeadPlaceFixture *fixture,
        const SZrExecIrFunction *function) {
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(fixture, function);
}

static void assert_oracle(SZrDeadPlaceFixture *fixture,
        const SZrExecIrFunction *function, TZrUInt32 expectedPlaceCalls) {
    ZrTests_SsaLiteralScriptFixture_AssertOracle(fixture, function, expectedPlaceCalls, &g_oracle);
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
}

static SZrDeadPlaceFixture *prepare_fixture(TZrUInt32 index) {
    SZrDeadPlaceFixture *fixture;
    EZrSsaLiteralScriptSource source;
    TEST_ASSERT_TRUE(index < DEAD_PLACE_FIXTURE_COUNT);
    fixture = &g_fixtures[index];
    source = index == 0u ? ZR_TEST_SSA_LITERAL_SCRIPT_NINE : ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT;
    ZrTests_SsaLiteralScriptFixture_Prepare(fixture, source);
    assert_oracle(fixture, fixture_function(fixture), 1u);
    return fixture;
}

static void assert_candidate(SZrDeadPlaceFixture *fixture, const SZrExecIrFunction *candidate) {
    const SZrExecIrFunction *input = fixture_function(fixture);
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt32 index, operandOffset = 0u, resultOffset = 0u;
    TEST_ASSERT_EQUAL_UINT32(input->id, candidate->id);
    TEST_ASSERT_EQUAL_UINT32(input->functionToken, candidate->functionToken);
    TEST_ASSERT_EQUAL_UINT64(input->signatureHash, candidate->signatureHash);
    TEST_ASSERT_EQUAL_MEMORY(&input->contract, &candidate->contract, sizeof(input->contract));
    TEST_ASSERT_EQUAL_UINT32(input->instructionCount, candidate->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(input->blockCount, candidate->blockCount);
    TEST_ASSERT_EQUAL_MEMORY(input->blocks, candidate->blocks, input->blockCount * sizeof(*input->blocks));
    TEST_ASSERT_EQUAL_UINT32(input->sourceMapCount, candidate->sourceMapCount);
    TEST_ASSERT_EQUAL_MEMORY(input->sourceMaps, candidate->sourceMaps,
            input->sourceMapCount * sizeof(*input->sourceMaps));
    TEST_ASSERT_EQUAL_UINT32(1u, candidate->valueCount);
    TEST_ASSERT_EQUAL_UINT32(input->valueCount - 2u, candidate->valueCount);
    TEST_ASSERT_EQUAL_MEMORY(&input->values[0], &candidate->values[0], sizeof(*input->values));
    for (index = 0u; index < candidate->valueCount; ++index) {
        TEST_ASSERT_EQUAL_UINT32(index + 1u, candidate->values[index].id);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->values[index].flags &
                (ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS | ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY));
    }
    for (index = 0u; index < candidate->instructionCount; ++index) {
        SZrExecIrInstruction before = input->instructions[index];
        SZrExecIrInstruction after = candidate->instructions[index];
        TZrUInt32 poolIndex;
        TEST_ASSERT_EQUAL_UINT32(before.sourceId, after.sourceId);
        TEST_ASSERT_EQUAL_UINT32(operandOffset, after.operands.offset);
        TEST_ASSERT_EQUAL_UINT32(resultOffset, after.results.offset);
        operandOffset += after.operands.count;
        resultOffset += after.results.count;
        for (poolIndex = 0u; poolIndex < after.operands.count; ++poolIndex) {
            TZrExecIrValueId value = candidate->operandPool[after.operands.offset + poolIndex];
            TEST_ASSERT_TRUE(value > 0u && value <= candidate->valueCount);
        }
        for (poolIndex = 0u; poolIndex < after.results.count; ++poolIndex) {
            TZrExecIrValueId value = candidate->resultPool[after.results.offset + poolIndex];
            TEST_ASSERT_TRUE(value > 0u && value <= candidate->valueCount);
            TEST_ASSERT_EQUAL_UINT32(index + 1u, candidate->values[value - 1u].definition);
        }
        if (before.opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE) {
            TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, after.opcode);
            TEST_ASSERT_EQUAL_UINT32(0u, after.operands.count);
            TEST_ASSERT_EQUAL_UINT32(0u, after.results.count);
            TEST_ASSERT_EQUAL_UINT16(0u, after.flags);
            TEST_ASSERT_EQUAL_UINT32(0u, after.effectIn);
            TEST_ASSERT_EQUAL_UINT32(0u, after.effectOut);
            TEST_ASSERT_EQUAL_UINT32(0u, after.memoryIn.count);
            TEST_ASSERT_EQUAL_UINT32(0u, after.memoryOut.count);
            TEST_ASSERT_EQUAL_UINT32(0u, after.deoptId);
            TEST_ASSERT_EQUAL_UINT32(0u, after.bindingRow);
        } else {
            memset(&before.operands, 0, sizeof(before.operands));
            memset(&before.results, 0, sizeof(before.results));
            memset(&after.operands, 0, sizeof(after.operands));
            memset(&after.results, 0, sizeof(after.results));
            TEST_ASSERT_EQUAL_MEMORY(&before, &after, sizeof(before));
        }
    }
    TEST_ASSERT_EQUAL_UINT32(operandOffset, candidate->operandCount);
    TEST_ASSERT_EQUAL_UINT32(resultOffset, candidate->resultCount);
    TEST_ASSERT_EQUAL_UINT32(1u, candidate->operandCount);
    TEST_ASSERT_EQUAL_UINT32(1u, candidate->resultCount);
    if (input->stateMap != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(candidate->stateMap);
        TEST_ASSERT_EQUAL_UINT32(input->stateMap->functionToken, candidate->stateMap->functionToken);
        TEST_ASSERT_EQUAL_UINT64(input->stateMap->signatureHash, candidate->stateMap->signatureHash);
        TEST_ASSERT_EQUAL_UINT64(input->stateMap->generation, candidate->stateMap->generation);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->entryCount);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->valueCount);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->rootCount);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->ownerStateCount);
    }
    assert_api(ZrCore_ExecIr_VerifyFunction(candidate, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "candidate VERIFY_ALL after publication");
    assert_source_maps(fixture, candidate);
    assert_oracle(fixture, candidate, 0u);
}

static void eliminate_and_assert(SZrDeadPlaceFixture *fixture) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrFunction *input = fixture_function(fixture);
    TZrUInt64 before = function_digest(input), sourceBefore = source_digest(fixture);
    TZrUInt64 constantsBefore = digest_bytes(0u, fixture->module.constants,
            fixture->module.constantCount * sizeof(*fixture->module.constants));
    TZrBool success = ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, &g_output, &diagnostic);
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    TEST_ASSERT_EQUAL_UINT64(constantsBefore, digest_bytes(0u, fixture->module.constants,
            fixture->module.constantCount * sizeof(*fixture->module.constants)));
    assert_api(success, &diagnostic,
            "FEATURE RED: verified literal source dead temporary place must eliminate");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    assert_candidate(fixture, &g_output);
}

static void test_prerequisite_return_nine(void) { (void)prepare_fixture(0u); }
static void test_prerequisite_return_eight(void) { (void)prepare_fixture(1u); }
static void test_eliminate_return_nine(void) { eliminate_and_assert(prepare_fixture(0u)); }
static void test_eliminate_return_eight(void) { eliminate_and_assert(prepare_fixture(1u)); }

static void test_repeat_original_input(void) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(0u);
    eliminate_and_assert(fixture);
    eliminate_and_assert(fixture);
}

static void test_replace_actual_eight_output(void) {
    SZrDeadPlaceFixture *nine = prepare_fixture(0u), *eight = prepare_fixture(1u);
    SZrExecIrDiagnostic diagnostic = {0};
    assert_api(ZrCore_ExecIr_CloneFunction(fixture_function(eight), &g_output, &diagnostic),
            &diagnostic, "PRECONDITION: actual eight output");
    eliminate_and_assert(nine);
}

static void assert_refused(SZrDeadPlaceFixture *fixture,
        const SZrExecIrFunction *input, EZrExecutionDiagnosticCode expectedCode) {
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 inputBefore = function_digest(input);
    TZrUInt64 outputBefore = function_digest(&g_output);
    TZrUInt64 sourceBefore = source_digest(fixture);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(expectedCode, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(inputBefore, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
}

static SZrDeadPlaceFixture *prepare_guard(void) {
    SZrDeadPlaceFixture *nine = prepare_fixture(0u), *eight = prepare_fixture(1u);
    SZrExecIrDiagnostic diagnostic = {0};
    assert_api(ZrCore_ExecIr_CloneFunction(fixture_function(nine), &g_mutated, &diagnostic),
            &diagnostic, "PRECONDITION: source-produced Core clone");
    assert_api(ZrCore_ExecIr_CloneFunction(fixture_function(eight), &g_output, &diagnostic),
            &diagnostic, "PRECONDITION: retained actual eight output");
    return nine;
}

/* Single-field perturbations below retain production IDs/types/AST/context.
 * Valid unsupported graphs are explicitly VERIFY_ALL checked; the invalid
 * range case is kept separate from those semantic refusal boundaries. */
static void test_guard_actual_address_use_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.operandPool[g_mutated.instructions[2].operands.offset] =
            g_mutated.resultPool[g_mutated.instructions[1].results.offset];
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: actual address use remains valid SSA");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_unknown_provenance_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.operandPool[g_mutated.instructions[1].operands.offset] =
            g_mutated.resultPool[g_mutated.instructions[0].results.offset];
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: non-builder provenance remains valid SSA");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_sealed_metadata_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.sealed = ZR_TRUE;
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: sealed graph remains valid");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_SEALED);
}

static void test_guard_invalid_range_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 before, outputBefore;
    g_mutated.instructions[1].operands.offset = g_mutated.operandCount;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    before = function_digest(&g_mutated);
    outputBefore = function_digest(&g_output);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            &g_mutated, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(&g_mutated));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
}

static void test_guard_input_output_alias(void) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(0u);
    SZrExecIrFunction *input = fixture_function(fixture);
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 before = function_digest(input), sourceBefore = source_digest(fixture);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, input, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
}

#include "ssa_dead_source_places_edges.inc"

int main(int argc, char **argv) {
    TZrBool prerequisites = ZR_TRUE, features = ZR_TRUE, guards = ZR_TRUE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) {
        features = guards = ZR_FALSE;
    } else if (argc == 2 && strcmp(argv[1], "--features-only") == 0) {
        prerequisites = guards = ZR_FALSE;
    } else if (argc == 2 && strcmp(argv[1], "--guards-only") == 0) {
        prerequisites = features = ZR_FALSE;
    } else if (argc != 1) {
        (void)fprintf(stderr, "usage: %s [--prerequisites-only|--features-only|--guards-only]\n", argv[0]);
        return 2;
    }
    UNITY_BEGIN();
    if (prerequisites) {
        RUN_TEST(test_prerequisite_return_nine);
        RUN_TEST(test_prerequisite_return_eight);
    }
    if (features) {
        RUN_TEST(test_eliminate_return_nine);
        RUN_TEST(test_eliminate_return_eight);
        RUN_TEST(test_repeat_original_input);
        RUN_TEST(test_replace_actual_eight_output);
        RUN_TEST(test_compressed_nine_as_input);
        RUN_TEST(test_compressed_eight_as_input);
        RUN_TEST(test_null_diagnostic_success);
        RUN_TEST(test_actual_empty_state_header_preserved);
    }
    if (guards) {
        RUN_TEST(test_guard_actual_address_use_preserves_eight);
        RUN_TEST(test_guard_unknown_provenance_preserves_eight);
        RUN_TEST(test_guard_sealed_metadata_preserves_eight);
        RUN_TEST(test_guard_invalid_range_preserves_eight);
        RUN_TEST(test_guard_input_output_alias);
        RUN_TEST(test_guard_source_map_location_mismatch);
        RUN_TEST(test_guard_duplicate_source_map_identity);
        RUN_TEST(test_guard_shared_value_storage);
        RUN_TEST(test_guard_shared_operand_storage);
        RUN_TEST(test_guard_shared_state_map_storage);
        RUN_TEST(test_guard_interior_value_storage);
        RUN_TEST(test_guard_interior_operand_storage);
        RUN_TEST(test_guard_interior_state_value_storage);
        RUN_TEST(test_guard_missing_semantic_symbol);
        RUN_TEST(test_guard_missing_semantic_callable);
        RUN_TEST(test_guard_missing_semantic_identity);
        RUN_TEST(test_guard_empty_state_header_mismatch);
        RUN_TEST(test_guard_owned_state_value_pool);
        RUN_TEST(test_guard_owned_deopt_reconstruction);
        RUN_TEST(test_guard_null_required_arguments);
    }
    return UNITY_END();
}
