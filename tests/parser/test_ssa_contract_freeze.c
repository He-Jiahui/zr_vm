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

int main(void) {
    expect_true(ZR_CALL_BINDING_SCHEMA_VERSION == 1u,
                "legacy call-binding schema changed unexpectedly");
    expect_true(ZR_ARTIFACT_SCHEMA_VERSION == 5u,
                "legacy artifact schema changed unexpectedly");
    expect_true(ZR_VM_AOT_ABI_VERSION == 16u,
                "legacy AOT ABI changed unexpectedly");
    expect_true(ZR_EXECUTION_CONTRACT_SCHEMA_VERSION == 6u,
                "candidate execution schema is not v6");
    expect_true(ZR_EXECUTION_CONTRACT_ABI_VERSION == 17u,
                "candidate execution ABI is not v17");
    test_identical_contract_is_accepted();
    test_signature_and_layout_mismatch_are_structured();
    test_version_and_generation_fail_before_identity_resolution();
    test_capability_is_not_inferred_from_effects();
    puts("ssa contract freeze PASS");
    return EXIT_SUCCESS;
}
