#include "zr_vm_core/execution_contract.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_common/zr_aot_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrExecutionContract make_contract(void) {
    SZrExecutionContract contract;

    memset(&contract, 0, sizeof(contract));
    contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract.logicalVersion = 11u;
    contract.generation = 4u;
    contract.targetToken = 0x02000007u;
    contract.signatureHash = UINT64_C(0x1122334455667788);
    contract.layoutHash = UINT64_C(0x8877665544332211);
    contract.moduleHash = UINT64_C(0xaabbccddeeff0011);
    contract.requiredCapabilities = ZR_EXECUTION_CAPABILITY_ARITHMETIC;
    contract.declaredEffects = ZR_EXECUTION_EFFECT_READ_MEMORY;
    return contract;
}

static void test_identical_contract_is_accepted(void) {
    const SZrExecutionContract expected = make_contract();
    const SZrExecutionContract actual = make_contract();
    SZrExecIrDiagnostic diagnostic;

    memset(&diagnostic, 0xff, sizeof(diagnostic));
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_OK,
                "identical contract was rejected");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE,
                "successful check left a diagnostic code");
}

static void test_signature_and_layout_mismatch_are_structured(void) {
    SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;
    SZrExecIrDiagnostic diagnostic;

    actual.signatureHash++;
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_SIGNATURE_MISMATCH,
                "signature mismatch was not rejected");
    expect_true(diagnostic.expectedHash == expected.signatureHash &&
                    diagnostic.actualHash == actual.signatureHash,
                "signature mismatch diagnostic lost expected/actual hashes");

    actual = expected;
    actual.layoutHash++;
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_LAYOUT_MISMATCH,
                "layout mismatch was not rejected");
}

static void test_version_and_generation_fail_before_identity_resolution(void) {
    SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;
    SZrExecIrDiagnostic diagnostic;

    actual.schemaVersion++;
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                "schema mismatch did not request recompilation");
    expect_true(diagnostic.expectedVersion == expected.schemaVersion &&
                    diagnostic.actualVersion == actual.schemaVersion,
                "schema mismatch diagnostic lost versions");

    actual = expected;
    actual.generation++;
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_STALE_GENERATION,
                "stale generation was not classified");
}

static void test_capability_is_not_inferred_from_effects(void) {
    SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;
    SZrExecIrDiagnostic diagnostic;

    actual.requiredCapabilities |= ZR_EXECUTION_CAPABILITY_NATIVE_CALL;
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_CAPABILITY_MISMATCH,
                "extra capability requirement was accepted");

    actual = expected;
    actual.declaredEffects |= ZR_EXECUTION_EFFECT_WRITE_MEMORY;
    expect_true(ZrCore_ExecutionContract_Check(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_EFFECT_MISMATCH,
                "extra effect was accepted");
}

static EZrExecutionContractStatus check_preserving_inputs(
        const SZrExecutionContract *expected,
        const SZrExecutionContract *actual,
        SZrExecIrDiagnostic *diagnostic) {
    unsigned char expectedBefore[sizeof(*expected)];
    unsigned char actualBefore[sizeof(*actual)];
    EZrExecutionContractStatus status;

    memcpy(expectedBefore, expected, sizeof(expectedBefore));
    memcpy(actualBefore, actual, sizeof(actualBefore));
    status = ZrCore_ExecutionContract_Check(expected, actual, diagnostic);
    expect_true(memcmp(expectedBefore, expected, sizeof(expectedBefore)) == 0,
                "contract check modified the expected input");
    expect_true(memcmp(actualBefore, actual, sizeof(actualBefore)) == 0,
                "contract check modified the actual input");
    return status;
}

static void expect_diagnostic(
        const SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        TZrUInt32 functionToken,
        TZrUInt32 expectedVersion,
        TZrUInt32 actualVersion,
        TZrUInt64 expectedHash,
        TZrUInt64 actualHash) {
    expect_true(diagnostic->code == code, "diagnostic code differs");
    expect_true(diagnostic->functionToken == functionToken,
                "diagnostic function token differs");
    expect_true(diagnostic->blockId == 0u, "diagnostic retained a block ID");
    expect_true(diagnostic->instructionId == 0u,
                "diagnostic retained an instruction ID");
    expect_true(diagnostic->sourceId == 0u, "diagnostic retained a source ID");
    expect_true(diagnostic->expectedVersion == expectedVersion,
                "diagnostic expected version differs");
    expect_true(diagnostic->actualVersion == actualVersion,
                "diagnostic actual version differs");
    expect_true(diagnostic->expectedHash == expectedHash,
                "diagnostic expected hash differs");
    expect_true(diagnostic->actualHash == actualHash,
                "diagnostic actual hash differs");
}

static void test_logical_mismatch_reports_logical_versions(void) {
    const SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;
    SZrExecIrDiagnostic diagnostic;

    actual.logicalVersion++;
    memset(&diagnostic, 0xff, sizeof(diagnostic));
    expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                "logical mismatch did not request recompilation");
    expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                      expected.targetToken, expected.logicalVersion,
                      actual.logicalVersion, 0u, 0u);
}

static void test_logical_version_diagnostic_preserves_uint32_boundaries(void) {
    const TZrUInt32 versions[] = {0u, UINT32_MAX};
    size_t i;

    for (i = 0u; i < sizeof(versions) / sizeof(versions[0]); ++i) {
        SZrExecutionContract expected = make_contract();
        SZrExecutionContract actual = expected;
        SZrExecIrDiagnostic diagnostic;

        expected.logicalVersion = versions[i];
        actual.logicalVersion = versions[1u - i];
        memset(&diagnostic, 0xff, sizeof(diagnostic));
        expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                        ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                    "boundary logical mismatch did not request recompilation");
        expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                          expected.targetToken, expected.logicalVersion,
                          actual.logicalVersion, 0u, 0u);
    }
}

static void test_schema_and_abi_diagnostics_take_priority_over_logical(void) {
    const SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;
    SZrExecIrDiagnostic diagnostic;

    actual.schemaVersion++;
    actual.abiVersion++;
    actual.logicalVersion++;
    memset(&diagnostic, 0xff, sizeof(diagnostic));
    expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                "combined schema/ABI/logical mismatch was accepted");
    expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                      expected.targetToken, expected.schemaVersion,
                      actual.schemaVersion, expected.abiVersion,
                      actual.abiVersion);

    actual.schemaVersion = expected.schemaVersion;
    memset(&diagnostic, 0xff, sizeof(diagnostic));
    expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                "combined ABI/logical mismatch was accepted");
    expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                      expected.targetToken, expected.schemaVersion,
                      actual.schemaVersion, expected.abiVersion,
                      actual.abiVersion);
}

static void test_logical_diagnostic_precedes_identity_checks(void) {
    const SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;
    SZrExecIrDiagnostic diagnostic;

    actual.logicalVersion++;
    actual.targetToken++;
    actual.generation++;
    actual.signatureHash++;
    actual.layoutHash++;
    actual.moduleHash++;
    actual.requiredCapabilities |= ZR_EXECUTION_CAPABILITY_NATIVE_CALL;
    actual.declaredEffects |= ZR_EXECUTION_EFFECT_WRITE_MEMORY;
    memset(&diagnostic, 0xff, sizeof(diagnostic));
    expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                "logical mismatch lost priority over identity checks");
    expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                      expected.targetToken, expected.logicalVersion,
                      actual.logicalVersion, 0u, 0u);
}

static void test_logical_checks_allow_null_diagnostic(void) {
    const SZrExecutionContract expected = make_contract();
    SZrExecutionContract actual = expected;

    actual.logicalVersion++;
    expect_true(check_preserving_inputs(&expected, &actual, NULL) ==
                    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                "logical mismatch requires a diagnostic output");
    actual = expected;
    expect_true(check_preserving_inputs(&expected, &actual, NULL) ==
                    ZR_EXECUTION_CONTRACT_OK,
                "identical contract requires a diagnostic output");
}

static void test_repeated_logical_failure_then_success_clears_diagnostic(void) {
    const SZrExecutionContract expected = make_contract();
    SZrExecIrDiagnostic diagnostic;
    unsigned int i;

    memset(&diagnostic, 0xff, sizeof(diagnostic));
    for (i = 0u; i < 2u; ++i) {
        SZrExecutionContract actual = expected;

        actual.logicalVersion++;
        expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                        ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                    "repeated logical mismatch was accepted");
        expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                          expected.targetToken, expected.logicalVersion,
                          actual.logicalVersion, 0u, 0u);

        actual = expected;
        expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) ==
                        ZR_EXECUTION_CONTRACT_OK,
                    "success after logical failure was rejected");
        expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_NONE,
                          0u, 0u, 0u, 0u, 0u);
    }
}

typedef enum EContractTestField {
    CONTRACT_TEST_SCHEMA,
    CONTRACT_TEST_ABI,
    CONTRACT_TEST_LOGICAL,
    CONTRACT_TEST_TARGET,
    CONTRACT_TEST_GENERATION,
    CONTRACT_TEST_SIGNATURE,
    CONTRACT_TEST_LAYOUT,
    CONTRACT_TEST_MODULE,
    CONTRACT_TEST_CAPABILITIES,
    CONTRACT_TEST_EFFECTS
} EContractTestField;

typedef struct SContractFailureCase {
    EContractTestField field;
    EZrExecutionContractStatus status;
    EZrExecutionDiagnosticCode code;
} SContractFailureCase;

/* Published first-failure order; schema and ABI share one diagnostic payload. */
static const SContractFailureCase contract_failure_cases[] = {
    {CONTRACT_TEST_SCHEMA, ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
     ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH},
    {CONTRACT_TEST_ABI, ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
     ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH},
    {CONTRACT_TEST_LOGICAL, ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
     ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH},
    {CONTRACT_TEST_TARGET, ZR_EXECUTION_CONTRACT_TARGET_MISMATCH,
     ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH},
    {CONTRACT_TEST_GENERATION, ZR_EXECUTION_CONTRACT_STALE_GENERATION,
     ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION},
    {CONTRACT_TEST_SIGNATURE, ZR_EXECUTION_CONTRACT_SIGNATURE_MISMATCH,
     ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH},
    {CONTRACT_TEST_LAYOUT, ZR_EXECUTION_CONTRACT_LAYOUT_MISMATCH,
     ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH},
    {CONTRACT_TEST_MODULE, ZR_EXECUTION_CONTRACT_MODULE_MISMATCH,
     ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH},
    {CONTRACT_TEST_CAPABILITIES, ZR_EXECUTION_CONTRACT_CAPABILITY_MISMATCH,
     ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH},
    {CONTRACT_TEST_EFFECTS, ZR_EXECUTION_CONTRACT_EFFECT_MISMATCH,
     ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH}
};

static void set_test_field(SZrExecutionContract *contract,
                           EContractTestField field, TZrUInt64 value) {
    switch (field) {
        case CONTRACT_TEST_SCHEMA: contract->schemaVersion = (TZrUInt32)value; break;
        case CONTRACT_TEST_ABI: contract->abiVersion = (TZrUInt32)value; break;
        case CONTRACT_TEST_LOGICAL: contract->logicalVersion = (TZrUInt32)value; break;
        case CONTRACT_TEST_TARGET: contract->targetToken = (TZrMetadataToken)value; break;
        case CONTRACT_TEST_GENERATION: contract->generation = value; break;
        case CONTRACT_TEST_SIGNATURE: contract->signatureHash = value; break;
        case CONTRACT_TEST_LAYOUT: contract->layoutHash = value; break;
        case CONTRACT_TEST_MODULE: contract->moduleHash = value; break;
        case CONTRACT_TEST_CAPABILITIES: contract->requiredCapabilities = (TZrUInt32)value; break;
        case CONTRACT_TEST_EFFECTS: contract->declaredEffects = (TZrUInt32)value; break;
        default: expect_true(0, "unknown contract test field"); break;
    }
}

static void expect_failure_payload(const SContractFailureCase *testCase,
                                   const SZrExecutionContract *expected,
                                   const SZrExecutionContract *actual,
                                   const SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 expectedVersion = 0u;
    TZrUInt32 actualVersion = 0u;
    TZrUInt64 expectedHash = 0u;
    TZrUInt64 actualHash = 0u;

    switch (testCase->field) {
        case CONTRACT_TEST_SCHEMA:
        case CONTRACT_TEST_ABI:
            expectedVersion = expected->schemaVersion;
            actualVersion = actual->schemaVersion;
            expectedHash = expected->abiVersion;
            actualHash = actual->abiVersion;
            break;
        case CONTRACT_TEST_LOGICAL:
            expectedVersion = expected->logicalVersion;
            actualVersion = actual->logicalVersion;
            break;
        case CONTRACT_TEST_TARGET:
            expectedVersion = expected->targetToken;
            actualVersion = actual->targetToken;
            break;
        case CONTRACT_TEST_GENERATION:
            expectedHash = expected->generation;
            actualHash = actual->generation;
            break;
        case CONTRACT_TEST_SIGNATURE:
            expectedHash = expected->signatureHash;
            actualHash = actual->signatureHash;
            break;
        case CONTRACT_TEST_LAYOUT:
            expectedHash = expected->layoutHash;
            actualHash = actual->layoutHash;
            break;
        case CONTRACT_TEST_MODULE:
            expectedHash = expected->moduleHash;
            actualHash = actual->moduleHash;
            break;
        case CONTRACT_TEST_CAPABILITIES:
            expectedVersion = expected->requiredCapabilities;
            actualVersion = actual->requiredCapabilities;
            break;
        case CONTRACT_TEST_EFFECTS:
            expectedVersion = expected->declaredEffects;
            actualVersion = actual->declaredEffects;
            break;
        default: expect_true(0, "unknown diagnostic test field"); break;
    }
    expect_diagnostic(diagnostic, testCase->code, expected->targetToken,
                      expectedVersion, actualVersion, expectedHash, actualHash);
}

static void test_null_arguments_clear_all_diagnostic_fields(void) {
    const SZrExecutionContract contract = make_contract();
    const SZrExecutionContract *expectedInputs[] = {NULL, &contract, NULL};
    const SZrExecutionContract *actualInputs[] = {&contract, NULL, NULL};
    unsigned char before[sizeof(contract)];
    SZrExecIrDiagnostic diagnostic;
    size_t i;

    memcpy(before, &contract, sizeof(before));
    for (i = 0u; i < sizeof(expectedInputs) / sizeof(expectedInputs[0]); ++i) {
        memset(&diagnostic, 0xff, sizeof(diagnostic));
        expect_true(ZrCore_ExecutionContract_Check(expectedInputs[i], actualInputs[i],
                                                 &diagnostic) ==
                        ZR_EXECUTION_CONTRACT_INVALID_ARGUMENT,
                    "NULL contract did not report invalid argument");
        expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          0u, 0u, 0u, 0u, 0u);
        expect_true(memcmp(before, &contract, sizeof(before)) == 0,
                    "NULL argument check modified its non-NULL input");
        expect_true(ZrCore_ExecutionContract_Check(expectedInputs[i], actualInputs[i],
                                                 NULL) ==
                        ZR_EXECUTION_CONTRACT_INVALID_ARGUMENT,
                    "NULL contract requires a diagnostic output");
        expect_true(memcmp(before, &contract, sizeof(before)) == 0,
                    "NULL diagnostic check modified its non-NULL input");
        expect_true(check_preserving_inputs(&contract, &contract, &diagnostic) ==
                        ZR_EXECUTION_CONTRACT_OK,
                    "success after NULL argument failure was rejected");
        expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_NONE,
                          0u, 0u, 0u, 0u, 0u);
    }
}

static void test_scalar_mismatch_payloads_preserve_both_boundary_directions(void) {
    size_t i;
    unsigned int direction;

    for (i = 0u; i < sizeof(contract_failure_cases) / sizeof(contract_failure_cases[0]); ++i) {
        const SContractFailureCase *testCase = &contract_failure_cases[i];
        TZrUInt64 maximum;

        if (testCase->field == CONTRACT_TEST_LOGICAL ||
            testCase->field == CONTRACT_TEST_CAPABILITIES ||
            testCase->field == CONTRACT_TEST_EFFECTS) continue;
        maximum = (testCase->field == CONTRACT_TEST_SCHEMA ||
                   testCase->field == CONTRACT_TEST_ABI ||
                   testCase->field == CONTRACT_TEST_TARGET) ? UINT32_MAX : UINT64_MAX;
        for (direction = 0u; direction < 2u; ++direction) {
            SZrExecutionContract expected = make_contract();
            SZrExecutionContract actual = expected;
            SZrExecIrDiagnostic diagnostic;

            set_test_field(&expected, testCase->field, direction == 0u ? 0u : maximum);
            set_test_field(&actual, testCase->field, direction == 0u ? maximum : 0u);
            memset(&diagnostic, 0xff, sizeof(diagnostic));
            expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == testCase->status,
                        "scalar boundary mismatch returned a different status");
            expect_failure_payload(testCase, &expected, &actual, &diagnostic);
            expect_true(check_preserving_inputs(&expected, &actual, NULL) == testCase->status,
                        "scalar boundary mismatch requires a diagnostic output");
        }
    }
}

static void test_mask_payloads_reject_unknown_bits_on_either_or_both_sides(void) {
    const TZrUInt32 unknownValues[] = {UINT32_C(0x80000000), UINT32_MAX};
    size_t i;
    size_t valueIndex;
    unsigned int side;

    for (i = 0u; i < sizeof(contract_failure_cases) / sizeof(contract_failure_cases[0]); ++i) {
        const SContractFailureCase *testCase = &contract_failure_cases[i];
        TZrUInt32 knownMask;

        if (testCase->field != CONTRACT_TEST_CAPABILITIES &&
            testCase->field != CONTRACT_TEST_EFFECTS) continue;
        knownMask = testCase->field == CONTRACT_TEST_CAPABILITIES
                ? ZR_EXECUTION_CAPABILITY_KNOWN_MASK : ZR_EXECUTION_EFFECT_KNOWN_MASK;
        for (valueIndex = 0u; valueIndex < sizeof(unknownValues) / sizeof(unknownValues[0]); ++valueIndex) {
            expect_true((unknownValues[valueIndex] & ~knownMask) != 0u,
                        "unknown-bit fixture contains only defined mask bits");
            for (side = 0u; side < 3u; ++side) {
                SZrExecutionContract expected = make_contract();
                SZrExecutionContract actual = expected;
                SZrExecIrDiagnostic diagnostic;

                set_test_field(&expected, testCase->field, side == 1u ? 0u : unknownValues[valueIndex]);
                set_test_field(&actual, testCase->field, side == 0u ? 0u : unknownValues[valueIndex]);
                memset(&diagnostic, 0xff, sizeof(diagnostic));
                expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == testCase->status,
                            "unknown mask bits were accepted");
                expect_failure_payload(testCase, &expected, &actual, &diagnostic);
                expect_true(check_preserving_inputs(&expected, &actual, NULL) == testCase->status,
                            "unknown mask bits require a diagnostic output");
            }
        }
        for (side = 0u; side < 2u; ++side) {
            SZrExecutionContract expected = make_contract();
            SZrExecutionContract actual = expected;
            SZrExecIrDiagnostic diagnostic;

            set_test_field(&expected, testCase->field, side == 0u ? 0u : knownMask);
            set_test_field(&actual, testCase->field, side == 0u ? knownMask : 0u);
            memset(&diagnostic, 0xff, sizeof(diagnostic));
            expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == testCase->status,
                        "different known masks were accepted");
            expect_failure_payload(testCase, &expected, &actual, &diagnostic);
        }
    }
}

static void test_equal_known_masks_clear_all_diagnostic_fields(void) {
    const TZrUInt32 capabilities[] = {0u, ZR_EXECUTION_CAPABILITY_KNOWN_MASK};
    const TZrUInt32 effects[] = {0u, ZR_EXECUTION_EFFECT_KNOWN_MASK};
    size_t capabilityIndex;
    size_t effectIndex;

    for (capabilityIndex = 0u; capabilityIndex < sizeof(capabilities) / sizeof(capabilities[0]); ++capabilityIndex) {
        for (effectIndex = 0u; effectIndex < sizeof(effects) / sizeof(effects[0]); ++effectIndex) {
            SZrExecutionContract expected = make_contract();
            SZrExecutionContract actual;
            SZrExecIrDiagnostic diagnostic;

            expected.requiredCapabilities = capabilities[capabilityIndex];
            expected.declaredEffects = effects[effectIndex];
            actual = expected;
            memset(&diagnostic, 0xff, sizeof(diagnostic));
            expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == ZR_EXECUTION_CONTRACT_OK,
                        "equal known masks were rejected");
            expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u, 0u, 0u, 0u, 0u);
            expect_true(check_preserving_inputs(&expected, &actual, NULL) == ZR_EXECUTION_CONTRACT_OK,
                        "equal known masks require a diagnostic output");
        }
    }
}

static void test_each_earlier_failure_precedes_each_later_failure(void) {
    const SZrExecutionContract expected = make_contract();
    size_t earlier;
    size_t later;

    for (earlier = 0u; earlier < sizeof(contract_failure_cases) / sizeof(contract_failure_cases[0]); ++earlier) {
        for (later = earlier + 1u; later < sizeof(contract_failure_cases) / sizeof(contract_failure_cases[0]); ++later) {
            const SContractFailureCase *testCase = &contract_failure_cases[earlier];
            SZrExecutionContract actual = expected;
            SZrExecIrDiagnostic diagnostic;

            set_test_field(&actual, testCase->field, UINT32_MAX);
            set_test_field(&actual, contract_failure_cases[later].field, UINT32_MAX);
            memset(&diagnostic, 0xff, sizeof(diagnostic));
            expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == testCase->status,
                        "earlier failure lost its documented priority");
            expect_failure_payload(testCase, &expected, &actual, &diagnostic);
        }
    }
}

static void test_each_failure_repeats_and_recovers_without_stale_diagnostics(void) {
    const SZrExecutionContract expected = make_contract();
    size_t i;
    unsigned int repeat;

    for (i = 0u; i < sizeof(contract_failure_cases) / sizeof(contract_failure_cases[0]); ++i) {
        const SContractFailureCase *testCase = &contract_failure_cases[i];
        SZrExecIrDiagnostic diagnostic;

        memset(&diagnostic, 0xff, sizeof(diagnostic));
        for (repeat = 0u; repeat < 2u; ++repeat) {
            SZrExecutionContract actual = expected;

            set_test_field(&actual, testCase->field, UINT32_MAX);
            expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == testCase->status,
                        "repeated mismatch returned a different status");
            expect_failure_payload(testCase, &expected, &actual, &diagnostic);
            expect_true(check_preserving_inputs(&expected, &actual, NULL) == testCase->status,
                        "mismatch requires a diagnostic output");
            actual = expected;
            expect_true(check_preserving_inputs(&expected, &actual, &diagnostic) == ZR_EXECUTION_CONTRACT_OK,
                        "success after mismatch was rejected");
            expect_diagnostic(&diagnostic, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u, 0u, 0u, 0u, 0u);
        }
    }
}

int main(void) {
    expect_true(ZR_CALL_BINDING_SCHEMA_VERSION == 1u,
                "legacy call-binding schema changed unexpectedly");
    expect_true(ZR_ARTIFACT_SCHEMA_VERSION == 6u,
                "canonical artifact schema is not v6");
    expect_true(ZR_VM_AOT_ABI_VERSION == 17u,
                "AOT ABI is not v17");
    expect_true(ZR_EXECUTION_CONTRACT_SCHEMA_VERSION == 6u,
                "candidate execution schema is not v6");
    expect_true(ZR_EXECUTION_CONTRACT_ABI_VERSION == 17u,
                "candidate execution ABI is not v17");
    test_identical_contract_is_accepted();
    test_signature_and_layout_mismatch_are_structured();
    test_version_and_generation_fail_before_identity_resolution();
    test_capability_is_not_inferred_from_effects();
    test_logical_mismatch_reports_logical_versions();
    test_logical_version_diagnostic_preserves_uint32_boundaries();
    test_schema_and_abi_diagnostics_take_priority_over_logical();
    test_logical_diagnostic_precedes_identity_checks();
    test_logical_checks_allow_null_diagnostic();
    test_repeated_logical_failure_then_success_clears_diagnostic();
    test_null_arguments_clear_all_diagnostic_fields();
    test_scalar_mismatch_payloads_preserve_both_boundary_directions();
    test_mask_payloads_reject_unknown_bits_on_either_or_both_sides();
    test_equal_known_masks_clear_all_diagnostic_fields();
    test_each_earlier_failure_precedes_each_later_failure();
    test_each_failure_repeats_and_recovers_without_stale_diagnostics();
    puts("ssa contract freeze PASS");
    return EXIT_SUCCESS;
}
