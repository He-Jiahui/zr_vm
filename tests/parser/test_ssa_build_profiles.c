#include "zr_vm_parser/compile_ir_cache.h"
#include "zr_vm_parser/compile_optimization_profile.h"

#include <assert.h>
#include <string.h>

static void test_default_profile_and_dry_run(void) {
    SZrCompileOptimizationProfile request;
    SZrCompileEffectivePolicy effective;
    SZrCompileOptimizationDiagnostic diagnostic;
    TZrChar report[256];

    ZrParser_CompileOptimizationProfile_Init(&request);
    assert(ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &diagnostic));
    assert(effective.buildMode == ZR_COMPILE_BUILD_MODE_DEV);
    assert(effective.numericPermission == ZR_COMPILE_NUMERIC_STRICT);
    assert(effective.backend == ZR_COMPILE_BACKEND_INTERPRETER);
    assert(effective.passMask != 0U);
    assert(ZrParser_CompileOptimizationProfile_Describe(
            &effective, report, sizeof(report)));
    assert(strstr(report, "build=dev") != NULL);
}

static void test_profile_conflicts_are_diagnostic(void) {
    SZrCompileOptimizationProfile request;
    SZrCompileEffectivePolicy effective;
    SZrCompileOptimizationDiagnostic diagnostic;

    ZrParser_CompileOptimizationProfile_Init(&request);
    request.target = ZR_COMPILE_TARGET_MOBILE;
    request.backend = ZR_COMPILE_BACKEND_HOST_JIT;
    assert(!ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &diagnostic));
    assert(diagnostic.code == ZR_COMPILE_PROFILE_DIAGNOSTIC_TARGET_BACKEND);

    ZrParser_CompileOptimizationProfile_Init(&request);
    request.requestFastMath = ZR_TRUE;
    request.numericPermission = ZR_COMPILE_NUMERIC_STRICT;
    assert(!ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &diagnostic));
    assert(diagnostic.code == ZR_COMPILE_PROFILE_DIAGNOSTIC_NUMERIC_PERMISSION);
}

static void test_presets_are_explicit_and_distinct(void) {
    SZrCompileOptimizationProfile request;
    SZrCompileOptimizationProfile devRequest;
    SZrCompileEffectivePolicy effective;
    SZrCompileEffectivePolicy devEffective;
    SZrCompileOptimizationDiagnostic diagnostic;

    ZrParser_CompileOptimizationProfile_Init(&request);
    assert(ZrParser_CompileOptimizationProfile_ApplyPreset(
            &request, ZR_COMPILE_PRESET_RELEASE_FAST_MATH, &diagnostic));
    assert(ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &diagnostic));
    assert(effective.fastMathEnabled == ZR_TRUE);
    assert(effective.numericPermission == ZR_COMPILE_NUMERIC_FAST_MATH);

    assert(ZrParser_CompileOptimizationProfile_ApplyPreset(
            &request, ZR_COMPILE_PRESET_WASM, &diagnostic));
    assert(ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &diagnostic));
    assert(effective.target == ZR_COMPILE_TARGET_WASM);
    assert(effective.backend == ZR_COMPILE_BACKEND_AOT_C);
    assert(effective.fastMathEnabled == ZR_FALSE);

    ZrParser_CompileOptimizationProfile_Init(&devRequest);
    assert(ZrParser_CompileOptimizationProfile_ApplyPreset(
            &devRequest, ZR_COMPILE_PRESET_DEV, &diagnostic));
    assert(ZrParser_CompileOptimizationProfile_Normalize(
            &devRequest, &devEffective, &diagnostic));
    ZrParser_CompileOptimizationProfile_Init(&request);
    assert(ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &diagnostic));
    assert(effective.profileHash == devEffective.profileHash);
}

static void test_cache_key_changes_with_contract_inputs(void) {
    static const TZrByte source[] = "fn main() { return 1; }";
    static const TZrByte dependency[] = "dep:v1";
    SZrCompileOptimizationProfile request;
    SZrCompileEffectivePolicy effective;
    SZrCompileOptimizationDiagnostic profileDiagnostic;
    SZrCompileIrCacheInputs inputs;
    SZrCompileIrCacheKey baseline;
    SZrCompileIrCacheKey changed;
    SZrCompileIrCacheDiagnostic cacheDiagnostic;

    ZrParser_CompileOptimizationProfile_Init(&request);
    assert(ZrParser_CompileOptimizationProfile_Normalize(
            &request, &effective, &profileDiagnostic));
    memset(&inputs, 0, sizeof(inputs));
    inputs.sourceBytes = source;
    inputs.sourceSize = sizeof(source) - 1U;
    inputs.dependencyBytes = dependency;
    inputs.dependencySize = sizeof(dependency) - 1U;
    inputs.contractHash = 11U;
    inputs.compilerAbi = 12U;
    inputs.passPipelineHash = 13U;
    inputs.targetHash = 14U;
    inputs.numericPolicyHash = 15U;
    inputs.layoutHash = 16U;
    inputs.importedProfileHash = 17U;
    assert(ZrParser_CompileIrCache_BuildKey(
            &effective, &inputs, &baseline, &cacheDiagnostic));

    inputs.contractHash++;
    assert(ZrParser_CompileIrCache_BuildKey(
            &effective, &inputs, &changed, &cacheDiagnostic));
    assert(!ZrParser_CompileIrCache_KeyEquals(&baseline, &changed));
}

static void test_cache_publish_is_transactional(void) {
    static const TZrByte payload[] = "verified-ir";
    SZrCompileIrCache cache;
    SZrCompileIrCacheTransaction transaction;
    SZrCompileIrCacheKey key;
    SZrCompileIrCacheHit hit;
    SZrCompileIrCacheDiagnostic diagnostic;
    memset(&key, 0, sizeof(key));
    ZrParser_CompileIrCache_TransactionInit(&transaction);
    key.schemaVersion = ZR_COMPILE_IR_CACHE_SCHEMA_VERSION;
    key.valid = ZR_TRUE;
    key.digest[0] = 1U;
    ZrParser_CompileIrCache_Init(&cache);
    assert(ZrParser_CompileIrCache_BeginWrite(&cache, &key, &transaction,
                                               &diagnostic));
    assert(ZrParser_CompileIrCache_Publish(
            &cache, &transaction, payload, sizeof(payload) - 1U,
            ZR_TRUE, &diagnostic));
    assert(ZrParser_CompileIrCache_Lookup(&cache, &key, &hit, &diagnostic));
    assert(hit.payloadSize == sizeof(payload) - 1U);

    assert(ZrParser_CompileIrCache_BeginWrite(&cache, &key, &transaction,
                                               &diagnostic));
    assert(!ZrParser_CompileIrCache_Publish(
            &cache, &transaction, payload, sizeof(payload) - 1U,
            ZR_FALSE, &diagnostic));
    assert(diagnostic.code ==
           ZR_COMPILE_IR_CACHE_DIAGNOSTIC_INCOMPLETE_PAYLOAD);
    assert(ZrParser_CompileIrCache_Lookup(&cache, &key, &hit, &diagnostic));
    assert(hit.payloadSize == sizeof(payload) - 1U);
    assert(ZrParser_CompileIrCache_CancelWrite(&cache, &transaction,
                                                &diagnostic));
    assert(ZrParser_CompileIrCache_Lookup(&cache, &key, &hit, &diagnostic));
    assert(ZrParser_CompileIrCache_MarkCorrupt(&cache, &key, &diagnostic));
    assert(!ZrParser_CompileIrCache_Lookup(&cache, &key, &hit, &diagnostic));
    ZrParser_CompileIrCache_Free(&cache);
}

int main(void) {
    test_default_profile_and_dry_run();
    test_profile_conflicts_are_diagnostic();
    test_presets_are_explicit_and_distinct();
    test_cache_key_changes_with_contract_inputs();
    test_cache_publish_is_transactional();
    return 0;
}
