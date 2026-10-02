#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_state_maps.h"
#include "zr_vm_common/zr_type_conf.h"
#include "../../zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%u: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

#include "ssa_dce_phi_liveness_cases.inc"

static SZrExecIrOracleValue runtime_pool[5];
static SZrExecIrConstant pass_pool[5];

static void verify(const SZrExecIrFunction *function) {
    SZrExecIrDiagnostic diagnostic;
    CHECK(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
}

static void runners(const SZrExecIrFunction *function, TZrInt64 expected) {
    SZrExecIrOracleInput input = {0}; SZrExecIrOracleExecutionResult oracle = {0};
    SZrExecBcProjection projection = {0}; SZrExecBcExecutionInput bcInput = {0};
    SZrExecBcExecutionResult bcResult; SZrExecIrDiagnostic diagnostic;
    input.function = function; input.constants = runtime_pool; input.constantCount = 5u;
    CHECK(ZrCore_ExecIr_RunOracleEx(&input, &oracle, &diagnostic));
    CHECK(oracle.returned && oracle.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
    CHECK(oracle.returnValue.as.signedInteger == expected);
    ZrCore_ExecIr_OracleResultFree(&oracle);
    CHECK(ZrParser_ExecIr_LowerExecBc(function, &projection, &diagnostic));
    bcInput.constants = runtime_pool; bcInput.constantCount = 5u;
    ZrParser_ExecBcExecutionResult_Init(&bcResult);
    CHECK(ZrParser_ExecBcProjection_Run(&projection, &bcInput, &bcResult, &diagnostic));
    CHECK(bcResult.returned && bcResult.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
    CHECK(bcResult.returnValue.as.signedInteger == expected);
    ZrParser_ExecBcExecutionResult_Free(&bcResult);
    ZrParser_ExecBcProjection_Free(&projection);
}

static void pipeline(SZrExecIrFunction *function, TZrBool dceOnly) {
    SZrExecIrAnalysisCache cache; SZrExecIrPassContext context = {0};
    SZrExecIrDiagnostic diagnostic; TZrUInt32 count;
    const SZrExecIrPassInfo *passes = ZrParser_ExecIr_GetScalarPasses(&count);
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    context.cache = &cache; context.constants = pass_pool; context.constantCount = 5u;
    TZrBool result = ZrParser_ExecIr_RunPassPipeline(function,
            dceOnly ? &passes[1] : passes, dceOnly ? 1u : count, &context, &diagnostic);
    if (!result) fprintf(stderr, "pipeline diagnostic=%u block=%u instruction=%u\n",
            diagnostic.code, diagnostic.blockId, diagnostic.instructionId);
    CHECK(result);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
}

static void successful_fixture(SZrExecIrFunction *function, TZrInt64 expected,
                                TZrBool dceOnly) {
    verify(function); runners(function, expected);
    pipeline(function, dceOnly);
    verify(function); runners(function, expected);
    TZrUInt64 hash = ZrParser_ExecIr_FunctionHash(function);
    pipeline(function, dceOnly);
    CHECK(hash == ZrParser_ExecIr_FunctionHash(function));
    verify(function); runners(function, expected);
}

static void test_diamonds(void) {
    for (unsigned transitive = 0u; transitive < 2u; ++transitive) {
        for (unsigned dceOnly = 0u; dceOnly < 2u; ++dceOnly) {
            SZrExecIrFunction f;
            build_diamond(&f, (TZrBool)transitive, ZR_FALSE);
            successful_fixture(&f, 223, (TZrBool)dceOnly);
            unsigned nops = 0u;
            for (unsigned i = 0u; i < f.instructionCount; ++i)
                if (f.instructions[i].opcode == ZR_EXEC_IR_OPCODE_NOP) ++nops;
            CHECK(nops >= 3u && f.phiCount == 1u);
            ZrCore_ExecIr_FreeFunction(&f);
        }
    }
}

static void test_chained_and_loop_phis(void) {
    for (unsigned dceOnly = 0u; dceOnly < 2u; ++dceOnly) {
        SZrExecIrFunction f;
        build_chained_phis(&f);
        successful_fixture(&f, 223, (TZrBool)dceOnly);
        CHECK(f.phiCount == 2u);
        ZrCore_ExecIr_FreeFunction(&f);
        build_loop(&f);
        successful_fixture(&f, 3, (TZrBool)dceOnly);
        CHECK(f.instructions[6].opcode == ZR_EXEC_IR_OPCODE_ADD);
        ZrCore_ExecIr_FreeFunction(&f);
    }
}

static void test_unused_phi_keeps_valid_incomings(void) {
    SZrExecIrFunction f;
    build_diamond(&f, ZR_TRUE, ZR_TRUE);
    successful_fixture(&f, 2, ZR_TRUE);
    ZrCore_ExecIr_FreeFunction(&f);
}

static void test_metadata_and_observable_roots(void) {
    for (unsigned mode = 0u; mode < 5u; ++mode) {
        SZrExecIrFunction f; SZrExecIrDiagnostic diagnostic;
        build_diamond(&f, ZR_FALSE, ZR_FALSE);
        unsigned copyIndex = f.instructionCount - 3u;
        if (mode == 0u) {
            f.deoptValues = (TZrExecIrValueId *)calloc(1u, sizeof(*f.deoptValues));
            CHECK(f.deoptValues != NULL);
            f.deoptValueCount = f.deoptValueCapacity = 1u; f.deoptValues[0] = 10u;
        } else if (mode == 1u) {
            f.gcRoots = (TZrExecIrValueId *)calloc(1u, sizeof(*f.gcRoots));
            CHECK(f.gcRoots != NULL);
            f.gcRootCount = f.gcRootCapacity = 1u; f.gcRoots[0] = 10u;
        } else if (mode == 2u) {
            f.instructions[copyIndex].flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
            CHECK(ZrParser_ExecIr_SynthesizeCfgEffects(&f, &diagnostic));
            CHECK(ZrParser_ExecIr_BuildStateMaps(&f, &diagnostic));
            CHECK(f.stateMap != NULL);
        } else if (mode == 3u) {
            f.instructions[copyIndex].opcode = ZR_EXEC_IR_OPCODE_MOVE;
        } else {
            f.instructions[copyIndex].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
            CHECK(ZrParser_ExecIr_SynthesizeCfgEffects(&f, &diagnostic));
        }
        successful_fixture(&f, 223, ZR_TRUE);
        CHECK(f.instructions[copyIndex].opcode != ZR_EXEC_IR_OPCODE_NOP);
        CHECK(f.instructions[copyIndex - 1u].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
        ZrCore_ExecIr_FreeFunction(&f);
    }
}

static TZrBool reject_after_mutation(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrBool *changed, SZrExecIrDiagnostic *diagnostic) {
    (void)context; (void)diagnostic;
    function->instructions[0].opcode = ZR_EXEC_IR_OPCODE_COUNT;
    *changed = ZR_TRUE;
    return ZR_TRUE;
}

static void test_transaction_failure(void) {
    SZrExecIrFunction f; SZrExecIrPassContext context = {0}; SZrExecIrDiagnostic diagnostic;
    SZrExecIrPassFailure failure; SZrExecIrPassInfo passes[2] = {{0}};
    build_diamond(&f, ZR_TRUE, ZR_FALSE); verify(&f); runners(&f, 223);
    TZrUInt64 hash = ZrParser_ExecIr_FunctionHash(&f);
    const SZrExecIrPassInfo *scalar = ZrParser_ExecIr_GetScalarPasses(NULL);
    passes[0] = scalar[1]; passes[1].name = "reject-after-dce";
    passes[1].run = reject_after_mutation;
    ZrParser_ExecIr_PassFailureInit(&failure);
    context.failure = &failure;
    CHECK(!ZrParser_ExecIr_RunPassPipeline(&f, passes, 2u, &context, &diagnostic));
    CHECK(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE);
    CHECK(failure.passName != NULL && strcmp(failure.passName, "reject-after-dce") == 0);
    CHECK(hash == ZrParser_ExecIr_FunctionHash(&f));
    verify(&f); runners(&f, 223);
    ZrParser_ExecIr_PassFailureFree(&failure); ZrCore_ExecIr_FreeFunction(&f);
}

static void test_budget_cancellation(void) {
    SZrExecIrFunction f; SZrExecIrDiagnostic diagnostic; TZrBool changed = ZR_FALSE;
    SZrExecIrPassContext complete = {0}; SZrExecIrPassBudget unlimited = {0};
    build_diamond(&f, ZR_TRUE, ZR_FALSE);
    complete.budget = &unlimited;
    CHECK(ZrParser_ExecIr_RunDcePass(&f, &complete, &changed, &diagnostic));
    CHECK(changed && complete.workUsed != 0u);
    TZrUInt64 required = complete.workUsed;
    ZrCore_ExecIr_FreeFunction(&f);
    for (TZrUInt64 limit = 1u; limit < required; ++limit) {
        build_diamond(&f, ZR_TRUE, ZR_FALSE); verify(&f);
        TZrUInt64 hash = ZrParser_ExecIr_FunctionHash(&f);
        SZrExecIrPassContext context = {0}; SZrExecIrPassBudget budget = {0};
        SZrExecIrRemarkSink remarks;
        ZrParser_ExecIr_RemarkSinkInit(&remarks); context.remarks = &remarks;
        SZrExecIrInstruction *before = (SZrExecIrInstruction *)malloc(
                f.instructionCount * sizeof(*before));
        CHECK(before != NULL);
        memcpy(before, f.instructions, f.instructionCount * sizeof(*before));
        budget.maxWork = limit; context.budget = &budget;
        changed = ZR_TRUE;
        CHECK(ZrParser_ExecIr_RunDcePass(&f, &context, &changed, &diagnostic));
        CHECK(context.budgetExhausted && !changed);
        CHECK(context.lastSourceId == 0u && remarks.count == 0u);
        CHECK(memcmp(before, f.instructions, f.instructionCount * sizeof(*before)) == 0);
        CHECK(hash == ZrParser_ExecIr_FunctionHash(&f));
        verify(&f); runners(&f, 223);
        free(before); ZrParser_ExecIr_RemarkSinkFree(&remarks);
        ZrCore_ExecIr_FreeFunction(&f);
    }
    printf("DCE budget cancellation: %llu limits preserved IR and remarks\n",
            (unsigned long long)(required - 1u));
}

int main(void) {
    const TZrInt64 values[5] = {0, 111, 222, 1, 3};
    for (unsigned i = 0u; i < 5u; ++i) {
        runtime_pool[i].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        runtime_pool[i].as.signedInteger = values[i];
        pass_pool[i].typeToken = ZR_VALUE_TYPE_INT64; pass_pool[i].bits = (TZrUInt64)values[i];
    }
    test_diamonds(); test_chained_and_loop_phis();
    test_unused_phi_keeps_valid_incomings(); test_metadata_and_observable_roots();
    test_transaction_failure(); test_budget_cancellation();
    puts("DCE PHI liveness: 16 groups passed (diamonds, chained PHIs, loop, roots, dead chains, transaction and budget)");
    return 0;
}
