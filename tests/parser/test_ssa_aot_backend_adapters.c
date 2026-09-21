#include <assert.h>
#include <string.h>

#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.h"
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.h"
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.h"

static void fill_contract(SZrExecutionContract *contract,
                          TZrMetadataToken token,
                          TZrUInt64 moduleHash,
                          TZrUInt64 signatureHash,
                          TZrUInt64 layoutHash) {
    (void)memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = token;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
    contract->moduleHash = moduleHash;
}

static SZrAotIrModule make_module(SZrAotIrFunction *function,
                                  SZrAotIrBlock *block,
                                  SZrAotIrInstruction *instructions,
                                  const TZrUInt32 *operands,
                                  const TZrUInt32 *results) {
    static const TZrUInt32 successors[] = {1u};
    SZrAotIrModule module;
    (void)memset(function, 0, sizeof(*function));
    function->id = 1u;
    function->functionToken = 101u;
    function->signatureHash = 111u;
    function->frameLayout.frameByteSize = 32u;
    function->frameLayout.frameByteAlign = 8u;
    function->frameLayout.layoutHash = 121u;
    function->blocks = block;
    function->blockCount = 1u;
    function->instructions = instructions;
    function->instructionCount = 3u;
    function->operandPool = operands;
    function->operandCount = 1u;
    function->resultPool = results;
    function->resultCount = 1u;
    function->successorPool = successors;
    function->successorCount = 1u;
    fill_contract(&function->contract, function->functionToken, 131u,
                  function->signatureHash, function->frameLayout.layoutHash);

    (void)memset(block, 0, sizeof(*block));
    block->id = 1u;
    block->flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    block->instructions.offset = 0u;
    block->instructions.count = 3u;
    block->terminatorInstructionId = 3u;

    (void)memset(&module, 0, sizeof(module));
    module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    module.target.pointerSize = (TZrUInt32)sizeof(void *);
    module.target.targetTripleHash = 142u;
    module.target.abiHash = 141u;
    module.moduleHash = 131u;
    module.functions = function;
    module.functionCount = 1u;
    fill_contract(&module.contract, 0u, module.moduleHash, 151u, 161u);
    return module;
}

static void adapter_and_backend_facts_are_shared(void) {
    const TZrUInt32 operands[] = {1u};
    const TZrUInt32 results[] = {2u};
    SZrAotIrInstruction instructions[] = {
        {1u, ZR_EXEC_IR_OPCODE_ADD, 0u, {0u, 1u}, {0u, 1u},
         {0u, 0u}, {0u, 0u}, 0u, 0u, 7u, 0u, 0u, 0u},
        {2u, ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW, {0u, 0u}, {0u, 0u},
         {0u, 0u}, {0u, 0u}, 1u, 2u, 8u, 0u, 0u, 0u},
        {3u, ZR_EXEC_IR_OPCODE_SUSPEND, ZR_EXEC_IR_FLAG_MAY_SUSPEND, {0u, 0u}, {0u, 0u},
         {0u, 0u}, {0u, 0u}, 2u, 3u, 9u, 0u, 0u, 0u}
    };
    SZrAotIrFunction function;
    SZrAotIrBlock block;
    SZrAotIrModule module = make_module(&function, &block, instructions,
                                        operands, results);
    SZrAotIrDiagnostic irDiagnostic;
    SZrBackendAotIrDiagnostic diagnostic;
    SZrBackendAotIrFacts cFacts;
    SZrBackendAotIrFacts llvmFacts;
    SZrAotIrLoweringRecord loweringRecords[3];
    SZrAotIrLoweringResult lowering;
    SZrBackendAotIrCoverage coverage;
    SZrBackendAotIrCoverageDiagnostic coverageDiagnostic;
    SZrAotIrEmitOptions options;

    assert(backend_aot_ir_adapter_validate(&module, &diagnostic) ==
           ZR_BACKEND_AOT_IR_OK);
    (void)memset(&options, 0, sizeof(options));
    options.target = ZR_AOT_IR_EMITTER_C;
    options.strictFloatingPoint = ZR_TRUE;
    options.allowRuntimeBridge = ZR_TRUE;
    options.allowInterpreterFallback = ZR_TRUE;
    assert(backend_aot_ir_c_emit(&module, &options, &cFacts, &diagnostic));
    assert(cFacts.descriptorOnly == ZR_TRUE);
    assert(cFacts.artifactAvailable == ZR_FALSE);
    assert(cFacts.instructionCount == 3u);
    assert(cFacts.nativeLoweredCount == 2u);
    assert(cFacts.runtimeBridgeCount == 1u);
    assert(cFacts.interpreterFallbackCount == 0u);
    assert(cFacts.moduleHash == module.moduleHash);
    (void)memset(&lowering, 0, sizeof(lowering));
    assert(backend_aot_ir_adapter_collect(&module, loweringRecords, 3u,
                                          &lowering, &diagnostic));
    assert(lowering.count == 3u);
    assert(backend_aot_ir_adapter_facts_from_lowering(
        &module, ZR_AOT_IR_EMITTER_C, &options, &lowering, &cFacts,
        &diagnostic));
    assert(cFacts.runtimeBridgeCount == 1u);
    assert(backend_aot_ir_coverage_from_facts(&cFacts, &coverage,
                                              &coverageDiagnostic));
    assert(coverage.semanticSiteCount == 3u);
    assert(coverage.nativeSiteCount == 2u);
    assert(coverage.runtimeHelperSiteCount == 1u);
    options.target = ZR_AOT_IR_EMITTER_LLVM;
    assert(backend_aot_ir_llvm_emit(&module, &options, &llvmFacts, &diagnostic));
    assert(llvmFacts.nativeLoweredCount == cFacts.nativeLoweredCount);
    assert(llvmFacts.runtimeBridgeCount == cFacts.runtimeBridgeCount);
    assert(llvmFacts.sourceHash == cFacts.sourceHash);
    assert(llvmFacts.contractHash != cFacts.contractHash);
    {
        SZrAotIrEmitOptions policyOptions = options;
        SZrBackendAotIrFacts policyFacts;
        policyOptions.target = ZR_AOT_IR_EMITTER_C;
        policyOptions.allowInterpreterFallback = ZR_FALSE;
        assert(backend_aot_ir_c_emit(&module, &policyOptions, &policyFacts,
                                     &diagnostic));
        assert(policyFacts.contractHash != cFacts.contractHash);
    }
    assert(ZrCore_AotIr_ValidateModule(&module, &irDiagnostic) == ZR_AOT_IR_OK);

    /* A caller cannot smuggle a dangling lowering array into the facts
     * adapter; the result is cleared and the failure remains locatable. */
    {
        SZrAotIrLoweringResult invalidLowering = lowering;
        invalidLowering.records = ZR_NULL;
        invalidLowering.count = 1u;
        invalidLowering.capacity = 1u;
        assert(!backend_aot_ir_adapter_facts_from_lowering(
            &module, ZR_AOT_IR_EMITTER_LLVM, &options, &invalidLowering,
            &llvmFacts, &diagnostic));
        assert(diagnostic.status == ZR_BACKEND_AOT_IR_INVALID_AOTIR);
        assert(llvmFacts.descriptorOnly == ZR_TRUE);
        assert(llvmFacts.instructionCount == 0u);
    }

    /* Invalid wrapper options must not leave stale facts from a prior emit. */
    options.target = ZR_AOT_IR_EMITTER_C;
    cFacts.nativeLoweredCount = 99u;
    assert(!backend_aot_ir_c_emit_ex(&module, &options, (TZrBool)2u,
                                     &cFacts, &diagnostic));
    assert(diagnostic.status == ZR_BACKEND_AOT_IR_INVALID_ARGUMENT);
    assert(cFacts.descriptorOnly == ZR_TRUE);
    assert(cFacts.nativeLoweredCount == 0u);

    options.target = ZR_AOT_IR_EMITTER_C;
    assert(!backend_aot_ir_c_emit_ex(&module, &options, ZR_TRUE,
                                     &cFacts, &diagnostic));
    assert(diagnostic.status == ZR_BACKEND_AOT_IR_ARTIFACT_UNAVAILABLE);

    options.target = ZR_AOT_IR_EMITTER_C;
    options.allowRuntimeBridge = ZR_FALSE;
    assert(!backend_aot_ir_c_emit(&module, &options, &cFacts, &diagnostic));
    assert(diagnostic.status == ZR_BACKEND_AOT_IR_UNSUPPORTED);
}

static void coverage_keeps_denominator_and_reports_unavailable(void) {
    SZrBackendAotIrCoverage coverage;
    SZrBackendAotIrCoverageDiagnostic diagnostic;
    TZrUInt32 nativePermille;
    TZrUInt32 helperPermille;
    TZrUInt32 fallbackPermille;

    backend_aot_ir_coverage_init(&coverage);
    assert(!backend_aot_ir_coverage_ratio_per_mille(
        &coverage, &nativePermille, &helperPermille, &fallbackPermille,
        &diagnostic));
    assert(diagnostic.status == ZR_BACKEND_AOT_IR_COVERAGE_UNAVAILABLE);
    backend_aot_ir_coverage_set_semantic_sites(&coverage, 10u, &diagnostic);
    assert(backend_aot_ir_coverage_add(&coverage,
                                       ZR_BACKEND_AOT_IR_COVERAGE_NATIVE,
                                       5u, &diagnostic));
    assert(backend_aot_ir_coverage_add(&coverage,
                                       ZR_BACKEND_AOT_IR_COVERAGE_RUNTIME_HELPER,
                                       2u, &diagnostic));
    assert(backend_aot_ir_coverage_add(&coverage,
                                       ZR_BACKEND_AOT_IR_COVERAGE_INTERPRETER_FALLBACK,
                                       3u, &diagnostic));
    assert(backend_aot_ir_coverage_validate(&coverage, &diagnostic));
    assert(backend_aot_ir_coverage_ratio_per_mille(
        &coverage, &nativePermille, &helperPermille, &fallbackPermille,
        &diagnostic));
    assert(nativePermille == 500u && helperPermille == 200u &&
           fallbackPermille == 300u);
}

static void link_profile_is_conservative(void) {
    SZrAotReleaseProfile profile;
    SZrAotReleaseToolchain toolchain;
    SZrBackendAotLinkProfileResult result;
    SZrAotReleasePolicyDiagnostic diagnostic;
    const SZrAotReleaseRoot roots[] = {
        {1u, ZR_AOT_RELEASE_ROOT_ENTRY},
        {3u, ZR_AOT_RELEASE_ROOT_PATCH_FUTURE_USE}
    };
    TZrBool retained[4];

    (void)memset(&profile, 0, sizeof(profile));
    (void)memset(&toolchain, 0, sizeof(toolchain));
    profile.kind = ZR_AOT_RELEASE_PROFILE_RELEASE_PGO;
    profile.targetFingerprint = 1u;
    profile.irFingerprint = 2u;
    profile.compilerFingerprint = 3u;
    profile.profileTargetFingerprint = 1u;
    profile.profileIrFingerprint = 2u;
    profile.profileCompilerFingerprint = 99u;
    toolchain.supportsPgo = ZR_TRUE;
    assert(backend_aot_link_profile_resolve(&profile, &toolchain, ZR_FALSE,
                                            &result, &diagnostic) ==
           ZR_BACKEND_AOT_LINK_PROFILE_MISMATCH);
    assert(!result.pgoEnabled);
    assert(backend_aot_link_profile_resolve(&profile, &toolchain, ZR_TRUE,
                                            &result, &diagnostic) ==
           ZR_BACKEND_AOT_LINK_PROFILE_FALLBACK);
    assert(result.fallbackToDev && !result.pgoEnabled &&
           result.effectiveKind == ZR_AOT_RELEASE_PROFILE_DEV);
    assert(backend_aot_link_profile_mark_roots(roots, 2u, 4u, retained,
                                               &diagnostic));
    assert(retained[1u] && retained[3u] && !retained[0u]);
    {
        const SZrAotReleaseRoot invalidRoot[] = {
            {2u, (EZrAotReleaseRootKind)99u}
        };
        retained[0u] = ZR_TRUE;
        assert(!backend_aot_link_profile_mark_roots(
            invalidRoot, 1u, 4u, retained, &diagnostic));
        assert(diagnostic.code == ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT);
        assert(!retained[0u] && !retained[1u] && !retained[3u]);
    }
}

int main(void) {
    adapter_and_backend_facts_are_shared();
    coverage_keeps_denominator_and_reports_unavailable();
    link_profile_is_conservative();
    return 0;
}
