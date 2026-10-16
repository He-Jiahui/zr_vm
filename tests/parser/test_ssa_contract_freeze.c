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
    puts("ssa contract freeze PASS");
    return EXIT_SUCCESS;
}
