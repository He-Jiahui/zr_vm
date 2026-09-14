#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/exec_ir_loops.h"
#include "zr_vm_parser/exec_ir_profile.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange value;
    value.start = start;
    value.count = count;
    return value;
}

static TZrExecIrValueId add_value(SZrExecIrFunction *function) {
    return ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode,
                               SZrExecIrRange operands,
                               SZrExecIrRange results,
                               TZrUInt32 layoutId,
                               TZrUInt16 flags,
                               TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.layoutId = layoutId;
    instruction.flags = flags;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    assert(id == function->instructionCount);
}

static void append_successors(SZrExecIrFunction *function,
                              TZrExecIrBlockId blockId,
                              const TZrExecIrBlockId *successors,
                              TZrUInt32 count) {
    SZrExecIrRange successorRange;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, count,
                                                   &successorRange));
    function->blocks[blockId - 1u].successorRange = successorRange;
}

static void append_predecessors(SZrExecIrFunction *function,
                                TZrExecIrBlockId blockId,
                                const TZrExecIrBlockId *predecessors,
                                TZrUInt32 count) {
    SZrExecIrRange predecessorRange;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, count,
                                                     &predecessorRange));
    function->blocks[blockId - 1u].predecessorRange = predecessorRange;
}

static void build_loop(SZrExecIrFunction *function, TZrBool zeroTrip,
                       TZrBool throwingBody, TZrBool strengthBody) {
    TZrExecIrValueId condition;
    TZrExecIrValueId invariant;
    TZrExecIrValueId divisor = 0u;
    TZrExecIrValueId quotient = 0u;
    TZrExecIrValueId factor = 0u;
    TZrExecIrValueId product = 0u;
    SZrExecIrRange resultRange;
    SZrExecIrRange operandRange;
    TZrExecIrBlockId edge;
    TZrExecIrBlockId edges[2];
    TZrUInt32 blockIndex;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 0x1101u;
    function->signatureHash = UINT64_C(0x10101010);
    condition = add_value(function);
    invariant = add_value(function);
    if (throwingBody) {
        divisor = add_value(function);
        quotient = add_value(function);
    }
    if (strengthBody) {
        factor = add_value(function);
        product = add_value(function);
    }

    for (blockIndex = 0u; blockIndex < 4u; ++blockIndex) {
        TZrExecIrBlockId id = ZrCore_ExecIr_FunctionAddBlock(
                function, blockIndex == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u);
        assert(id == blockIndex + 1u);
    }

    assert(ZrCore_ExecIr_FunctionAppendResults(function, &condition, 1u,
                                                &resultRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       resultRange, zeroTrip ? 0u : 1u, 0u, 11u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, range(0u, 0u),
                       range(0u, 0u), 0u, 0u, 12u);
    function->blocks[0].instructionRange = range(0u, 2u);
    function->blocks[0].terminatorInstructionId = 2u;

    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u,
                                                &operandRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                       operandRange, range(0u, 0u), 0u, 0u, 21u);
    function->blocks[1].instructionRange = range(2u, 1u);
    function->blocks[1].terminatorInstructionId = 3u;

    assert(ZrCore_ExecIr_FunctionAppendResults(function, &invariant, 1u,
                                                &resultRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       resultRange, 9u, 0u, 31u);
    if (strengthBody) {
        TZrExecIrValueId operands[2] = {invariant, factor};
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &factor, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                           resultRange, 1u, 0u, 32u);
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, operands, 2u,
                                                    &operandRange));
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &product, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_MUL, operandRange,
                           resultRange, 0u, 0u, 33u);
    }
    if (throwingBody) {
        TZrExecIrValueId divisors[2] = {invariant, divisor};
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &divisor, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                           resultRange, 0u, 0u, strengthBody ? 34u : 32u);
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, divisors, 2u,
                                                    &operandRange));
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &quotient, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_DIV, operandRange,
                           resultRange, 0u, ZR_EXEC_IR_FLAG_MAY_THROW,
                           strengthBody ? 35u : 33u);
    }
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, range(0u, 0u),
                       range(0u, 0u), 0u, 0u,
                       (TZrExecIrSourceId)(strengthBody ? (throwingBody ? 36u : 34u)
                                                         : (throwingBody ? 34u : 32u)));
    function->blocks[2].instructionRange = range(3u,
                                                  (throwingBody ? 4u : 2u) +
                                                  (strengthBody ? 2u : 0u));
    function->blocks[2].terminatorInstructionId =
            function->blocks[2].instructionRange.start +
            function->blocks[2].instructionRange.count;

    {
        TZrExecIrValueId returnValue = throwingBody ? quotient
                                                     : (strengthBody ? product : invariant);
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, &returnValue, 1u,
                                                    &operandRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, operandRange,
                           range(0u, 0u), 0u, 0u, 41u);
    }
    function->blocks[3].instructionRange = range(function->instructionCount - 1u, 1u);
    function->blocks[3].terminatorInstructionId = function->instructionCount;

    edge = 2u;
    append_successors(function, 1u, &edge, 1u);
    edges[0] = 3u;
    edges[1] = 4u;
    append_successors(function, 2u, edges, 2u);
    edge = 2u;
    append_successors(function, 3u, &edge, 1u);
    append_successors(function, 4u, ZR_NULL, 0u);

    edge = 1u;
    append_predecessors(function, 1u, ZR_NULL, 0u);
    edges[0] = 1u;
    edges[1] = 3u;
    append_predecessors(function, 2u, edges, 2u);
    edge = 2u;
    append_predecessors(function, 3u, &edge, 1u);
    edges[0] = 2u;
    edges[1] = 0u; /* overwritten below; keeps the fixture explicit */
    edge = 2u;
    append_predecessors(function, 4u, &edge, 1u);
    (void)edges;
}

static void test_loop_forest_and_trip_count(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrDiagnostic diagnostic;
    build_loop(&function, ZR_FALSE, ZR_FALSE, ZR_FALSE);
    ZrParser_ExecIr_LoopInfoInit(&loops);
    assert(ZrParser_ExecIr_AnalyzeLoops(&function, &loops, &diagnostic));
    assert(loops.loopCount == 1u);
    assert(loops.loops[0].headerBlockId == 2u);
    assert(loops.loops[0].preheaderBlockId == 1u);
    assert(loops.loops[0].reducible == ZR_TRUE);
    assert(loops.loops[0].tripCountKnown == ZR_TRUE);
    assert(loops.loops[0].tripCountMin == 1u);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_licm_hoists_nontrapping_constant(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrDiagnostic diagnostic;
    build_loop(&function, ZR_FALSE, ZR_FALSE, ZR_FALSE);
    ZrParser_ExecIr_LoopInfoInit(&loops);
    assert(ZrParser_ExecIr_OptimizeLoops(&function, &loops, &diagnostic));
    assert(loops.hoistedInstructionCount == 1u);
    assert(function.blocks[0].instructionRange.count == 3u);
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
    assert(ZrCore_ExecIr_VerifyFunction(&function,
                                        (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                                ZR_EXEC_IR_VERIFY_SSA),
                                        &diagnostic));
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_zero_trip_and_throwing_instruction_are_not_hoisted(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrDiagnostic diagnostic;
    build_loop(&function, ZR_TRUE, ZR_TRUE, ZR_FALSE);
    ZrParser_ExecIr_LoopInfoInit(&loops);
    assert(ZrParser_ExecIr_OptimizeLoops(&function, &loops, &diagnostic));
    assert(loops.hoistedInstructionCount == 0u);
    assert(loops.blockedInstructionCount != 0u);
    assert(function.instructions[5].opcode == ZR_EXEC_IR_OPCODE_DIV);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_strength_reduces_checked_identity_multiply(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrDiagnostic diagnostic;
    build_loop(&function, ZR_FALSE, ZR_FALSE, ZR_TRUE);
    ZrParser_ExecIr_LoopInfoInit(&loops);
    assert(ZrParser_ExecIr_StrengthReduce(&function, &loops, &diagnostic));
    assert(loops.strengthReducedCount == 1u);
    assert(function.instructions[5].opcode == ZR_EXEC_IR_OPCODE_COPY);
    assert(function.instructions[5].operands.count == 1u);
    assert(ZrCore_ExecIr_VerifyFunction(&function,
                                        (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                                ZR_EXEC_IR_VERIFY_SSA),
                                        &diagnostic));
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_profile_key_import_and_mismatch(void) {
    SZrExecIrModule module;
    SZrExecIrProfile profile;
    SZrExecIrProfileImportResult result;
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_ModuleInit(&module);
    module.id = 4u;
    module.moduleToken = 44u;
    module.moduleHash = ZrParser_ExecIr_ProfileModuleHash(&module);
    module.contract.abiVersion = 17u;
    module.contract.signatureHash = UINT64_C(0x4455);
    module.contract.layoutHash = 99u;
    ZrParser_ExecIr_ProfileInit(&profile);
    assert(ZrParser_ExecIr_ProfileBuildKey(&module, &profile.key));
    assert(ZrParser_ExecIr_ProfileImport(&module, &profile, &result,
                                         &diagnostic));
    assert(result.accepted == ZR_TRUE);
    profile.key.irHash ^= UINT64_C(1);
    assert(ZrParser_ExecIr_ProfileImport(&module, &profile, &result,
                                         &diagnostic));
    assert(result.accepted == ZR_FALSE);
    assert(result.status == ZR_EXEC_IR_PROFILE_IMPORT_STALE);
    assert(result.reason == ZR_EXEC_IR_PROFILE_REASON_IR_HASH);
    ZrParser_ExecIr_ProfileFree(&profile);
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_specialization_budget_and_cooldown(void) {
    SZrExecIrSpecializationPolicy policy;
    SZrExecIrSpecializationState state;
    SZrExecIrSpecializationTotals totals;
    SZrExecIrSpecializationDecision decision;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_SpecializationPolicyInit(&policy);
    policy.maxVersionsPerSite = 2u;
    policy.maxCodeBytesPerSite = 32u;
    policy.minSamples = 1u;
    policy.minHitRatePermille = 800u;
    policy.maxMissRatePermille = 200u;
    policy.maxConsecutiveDeopts = 2u;
    policy.cooldownSamples = 3u;
    ZrParser_ExecIr_SpecializationStateInit(&state, 0xabcdu);
    memset(&totals, 0, sizeof(totals));
    assert(ZrParser_ExecIr_SpecializationObserve(&state, &policy,
                                                 ZR_TRUE, ZR_FALSE, 12u,
                                                 &diagnostic));
    assert(ZrParser_ExecIr_SpecializationDecide(&state, &policy, 12u, 1u,
                                                &decision, &diagnostic));
    assert(decision.useSpecialized == ZR_TRUE);
    assert(decision.retainBaseline == ZR_TRUE);
    assert(ZrParser_ExecIr_SpecializationObserve(&state, &policy,
                                                 ZR_FALSE, ZR_TRUE, 12u,
                                                 &diagnostic));
    assert(ZrParser_ExecIr_SpecializationObserve(&state, &policy,
                                                 ZR_FALSE, ZR_TRUE, 12u,
                                                 &diagnostic));
    assert(state.cooldownRemaining == policy.cooldownSamples);
    assert(ZrParser_ExecIr_SpecializationDecide(&state, &policy, 12u, 1u,
                                                &decision, &diagnostic));
    assert(decision.useSpecialized == ZR_FALSE);
    assert(decision.retainBaseline == ZR_TRUE);
    assert(decision.reason == ZR_EXEC_IR_SPECIALIZATION_REASON_COOLDOWN);
    ZrParser_ExecIr_SpecializationStateFree(&state);

    ZrParser_ExecIr_SpecializationStateInit(&state, 0xabcdu);
    policy.maxVersionsPerFunction = 1u;
    policy.maxVersionsPerModule = 1u;
    policy.maxCodeBytesPerFunction = 16u;
    policy.maxCodeBytesPerModule = 16u;
    assert(ZrParser_ExecIr_SpecializationCommitVersionEx(
            &state, &policy, &totals, 8u, 1u, ZR_FALSE, &diagnostic));
    assert(totals.functionVersions == 1u && totals.moduleVersions == 1u);
    assert(!ZrParser_ExecIr_SpecializationCommitVersionEx(
            &state, &policy, &totals, 1u, 1u, ZR_FALSE, &diagnostic));
    ZrParser_ExecIr_SpecializationStateFree(&state);
}

int main(void) {
    test_loop_forest_and_trip_count();
    test_licm_hoists_nontrapping_constant();
    test_zero_trip_and_throwing_instruction_are_not_hoisted();
    test_strength_reduces_checked_identity_multiply();
    test_profile_key_import_and_mismatch();
    test_specialization_budget_and_cooldown();
    return 0;
}
