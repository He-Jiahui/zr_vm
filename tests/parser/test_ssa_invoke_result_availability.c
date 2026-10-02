#include "zr_vm_core/exec_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;
static unsigned cases;

static void require(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "fixture error: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void edges(SZrExecIrFunction *function, TZrExecIrBlockId block,
                  const TZrExecIrBlockId *successors, TZrUInt32 count) {
    require(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, count,
                &function->blocks[block - 1u].successorRange), "successors");
}

static void predecessors(SZrExecIrFunction *function, TZrExecIrBlockId block,
                         const TZrExecIrBlockId *incoming, TZrUInt32 count) {
    require(ZrCore_ExecIr_FunctionAppendPredecessors(function, incoming, count,
                &function->blocks[block - 1u].predecessorRange), "predecessors");
}

static void instruction(SZrExecIrFunction *function, TZrExecIrBlockId block,
                        EZrExecIrOpcode opcode, TZrExecIrValueId operand,
                        TZrExecIrValueId result) {
    SZrExecIrInstruction item = {0};
    SZrExecIrBlock *container = &function->blocks[block - 1u];
    item.opcode = (TZrUInt16)opcode;
    item.sourceId = 100u + block;
    item.successorRange = container->successorRange;
    if (opcode == ZR_EXEC_IR_OPCODE_INVOKE) {
        item.flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    }
    if (operand != ZR_EXEC_IR_VALUE_ID_INVALID) {
        require(ZrCore_ExecIr_FunctionAppendOperands(function, &operand, 1u,
                    &item.operands), "operand");
    }
    if (result != ZR_EXEC_IR_VALUE_ID_INVALID) {
        require(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                    &item.results), "result");
    }
    container->instructionRange.start = function->instructionCount;
    container->instructionRange.count = 1u;
    require(ZrCore_ExecIr_FunctionAppendInstruction(function, &item,
                &container->terminatorInstructionId), "instruction");
}

static void phi(SZrExecIrFunction *function, TZrExecIrBlockId block,
                TZrExecIrValueId result, const TZrExecIrValueId *values) {
    SZrExecIrPhi item = {0};
    SZrExecIrPhiIncoming incoming[2] = {{0}};
    SZrExecIrBlock *container = &function->blocks[block - 1u];
    TZrUInt32 index;
    require(container->predecessorRange.count <= 2u, "phi capacity");
    for (index = 0u; index < container->predecessorRange.count; ++index) {
        incoming[index].predecessor = function->predecessors[
                container->predecessorRange.start + index];
        incoming[index].value = values[index];
    }
    item.result = result;
    require(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming,
                container->predecessorRange.count, &item.incomings), "phi inputs");
    require(ZrCore_ExecIr_FunctionAppendPhis(function, &item, 1u,
                &container->phis), "phi");
}

typedef enum FixtureKind {
    DIRECT_EXCEPTION_PHI, NORMAL_PHI, RETRY_OPERAND, RETRY_PHI,
    RETRY_BYPASS, RETRY_BYPASS_PHI_VALID, RETRY_BYPASS_PHI_INVALID,
    EXCEPTION_CLEANUP, EXCEPTION_MERGE
} FixtureKind;

static void test_fixture(FixtureKind kind, const char *name) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrExecIrFunctionId id;
    TZrExecIrValueId argument, result, merged = 0u;
    TZrExecIrBlockId invokeSuccessors[2] = {2u, 3u};
    TZrExecIrBlockId entry = 1u, normal = 2u, exception = 3u, join = 4u;
    TZrExecIrBlockId twoPredecessors[2] = {2u, 3u};
    TZrExecIrBlockId bypassSuccessors[2] = {1u, 2u};
    TZrExecIrValueId phiValues[2];
    TZrUInt32 target = kind == DIRECT_EXCEPTION_PHI ? 3u :
                       (kind == EXCEPTION_CLEANUP || kind == EXCEPTION_MERGE)
                               ? 4u : 2u;
    TZrBool expectSuccess = (TZrBool)(kind == NORMAL_PHI ||
                                    kind == RETRY_OPERAND || kind == RETRY_PHI ||
                                    kind == RETRY_BYPASS_PHI_VALID);
    TZrBool actual;
    TZrUInt32 block;
    TZrBool hasPhi = (TZrBool)(kind == DIRECT_EXCEPTION_PHI ||
                               kind == NORMAL_PHI || kind == RETRY_PHI ||
                               kind == EXCEPTION_MERGE ||
                               kind == RETRY_BYPASS_PHI_VALID ||
                               kind == RETRY_BYPASS_PHI_INVALID);
    TZrBool bypass = (TZrBool)(kind == RETRY_BYPASS ||
                               kind == RETRY_BYPASS_PHI_VALID ||
                               kind == RETRY_BYPASS_PHI_INVALID);
    ZrCore_ExecIr_ModuleInit(&module);
    require(ZrCore_ExecIr_ModuleAddFunction(&module, 7101u, 1u, &id), "function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    argument = ZrCore_ExecIr_FunctionAddExternalValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    result = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (hasPhi) {
        merged = ZrCore_ExecIr_FunctionAddValue(function, 1u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    }
    require(argument != 0u && result != 0u && (!hasPhi || merged != 0u), "values");
    for (block = 1u; block <= ((target == 4u) ? 4u : 3u); ++block) {
        require(ZrCore_ExecIr_FunctionAddBlock(function,
                    block == 1u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY :
                    block == 3u ? ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION : 0u) == block,
                "block");
    }
    function->entryBlockId = entry;
    edges(function, entry, invokeSuccessors, 2u);
    predecessors(function, normal, &entry, 1u);
    predecessors(function, exception, &entry, 1u);
    if (kind == RETRY_OPERAND || kind == RETRY_PHI || bypass) {
        edges(function, exception, bypassSuccessors,
                bypass ? 2u : 1u);
        predecessors(function, entry, &exception, 1u);
        if (bypass) {
            TZrExecIrBlockId incoming[2] = {1u, 3u};
            predecessors(function, normal, incoming, 2u);
        }
    } else if (kind == EXCEPTION_CLEANUP || kind == EXCEPTION_MERGE) {
        edges(function, exception, &join, 1u);
        if (kind == EXCEPTION_MERGE) {
            edges(function, normal, &join, 1u);
            predecessors(function, join, twoPredecessors, 2u);
        } else {
            predecessors(function, join, &exception, 1u);
        }
    }
    instruction(function, entry, ZR_EXEC_IR_OPCODE_INVOKE, 0u, result);
    instruction(function, normal,
            kind == EXCEPTION_MERGE ? ZR_EXEC_IR_OPCODE_BRANCH : ZR_EXEC_IR_OPCODE_RETURN,
            kind == EXCEPTION_MERGE ? 0u :
                    ((hasPhi && target == 2u) ? merged : result), 0u);
    instruction(function, exception,
            bypass ? ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH :
            (kind == RETRY_OPERAND || kind == RETRY_PHI || target == 4u)
                    ? ZR_EXEC_IR_OPCODE_BRANCH : ZR_EXEC_IR_OPCODE_RETURN,
            bypass ? argument :
            kind == DIRECT_EXCEPTION_PHI ? merged :
            (kind == NORMAL_PHI ? argument : 0u), 0u);
    if (target == 4u) {
        instruction(function, join, ZR_EXEC_IR_OPCODE_RETURN,
                kind == EXCEPTION_MERGE ? merged : result, 0u);
    }
    if (hasPhi) {
        phiValues[0] = kind == DIRECT_EXCEPTION_PHI ? argument : result;
        phiValues[1] = kind == RETRY_BYPASS_PHI_INVALID ? result : argument;
        phi(function, target, merged, phiValues);
        if (kind == DIRECT_EXCEPTION_PHI || kind == EXCEPTION_MERGE) {
            /* Prove the identical CFG and phi are valid with an entry value. */
            require(ZrCore_ExecIr_VerifyFunction(function,
                        (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                               ZR_EXEC_IR_VERIFY_SSA), &diagnostic),
                    "valid exceptional phi baseline");
            function->phiIncoming[function->phiPool[
                    function->blocks[target - 1u].phis.start].incomings.start +
                    (kind == EXCEPTION_MERGE ? 1u : 0u)].value = result;
        }
    }
    actual = ZrCore_ExecIr_VerifyFunction(function,
            (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA),
            &diagnostic);
    ++cases;
    if (actual != expectSuccess ||
            (expectSuccess && diagnostic.code != ZR_EXECUTION_DIAGNOSTIC_NONE) ||
            (!expectSuccess &&
            (diagnostic.code != ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE ||
             diagnostic.functionToken != 7101u || diagnostic.blockId != target ||
             diagnostic.instructionId != target || diagnostic.sourceId != 100u + target ||
             diagnostic.expectedVersion != 1u || diagnostic.actualVersion != result))) {
        ++failures;
        fprintf(stderr, "FAIL %s: accepted=%u diagnostic=%u block=%u instruction=%u source=%u expected=%u actual=%u\n",
                name, (unsigned)actual, (unsigned)diagnostic.code,
                diagnostic.blockId, diagnostic.instructionId, diagnostic.sourceId,
                diagnostic.expectedVersion, diagnostic.actualVersion);
    } else {
        printf("PASS %s\n", name);
    }
    ZrCore_ExecIr_FreeModule(&module);
}

/* The retrying INVOKE's exceptional edge returns directly to its definition.
 * Its normal result remains available on the distinct edge to block 4. */
static void test_self_exception_retry(TZrBool normalPhi, TZrBool exceptionPhi,
                                      const char *name) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrExecIrFunctionId id;
    TZrExecIrValueId argument, entryResult, retryResult, merged = 0u;
    TZrExecIrBlockId entryEdges[2] = {2u, 3u};
    TZrExecIrBlockId retryEdges[2] = {4u, 3u};
    TZrExecIrBlockId retryPredecessors[2] = {1u, 3u};
    TZrExecIrBlockId entry = 1u, retry = 3u;
    TZrExecIrValueId values[2];
    TZrBool actual;
    TZrUInt32 block;
    ZrCore_ExecIr_ModuleInit(&module);
    require(ZrCore_ExecIr_ModuleAddFunction(&module, 7101u, 1u, &id), "function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    argument = ZrCore_ExecIr_FunctionAddExternalValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    entryResult = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    retryResult = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (normalPhi || exceptionPhi) {
        merged = ZrCore_ExecIr_FunctionAddValue(function, 1u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    }
    require(argument != 0u && entryResult != 0u && retryResult != 0u &&
            (!(normalPhi || exceptionPhi) || merged != 0u), "values");
    for (block = 1u; block <= 4u; ++block) {
        require(ZrCore_ExecIr_FunctionAddBlock(function,
                    block == 1u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY :
                    block == 3u ? ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION : 0u) == block,
                "block");
    }
    function->entryBlockId = entry;
    edges(function, entry, entryEdges, 2u);
    edges(function, retry, retryEdges, 2u);
    predecessors(function, 2u, &entry, 1u);
    predecessors(function, retry, retryPredecessors, 2u);
    predecessors(function, 4u, &retry, 1u);
    instruction(function, entry, ZR_EXEC_IR_OPCODE_INVOKE, 0u, entryResult);
    instruction(function, 2u, ZR_EXEC_IR_OPCODE_RETURN, entryResult, 0u);
    instruction(function, retry, ZR_EXEC_IR_OPCODE_INVOKE, 0u, retryResult);
    instruction(function, 4u, ZR_EXEC_IR_OPCODE_RETURN,
            normalPhi ? merged : retryResult, 0u);
    if (normalPhi) {
        values[0] = retryResult;
        phi(function, 4u, merged, values);
    } else if (exceptionPhi) {
        values[0] = argument;
        values[1] = argument;
        phi(function, retry, merged, values);
        require(ZrCore_ExecIr_VerifyFunction(function,
                    (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                           ZR_EXEC_IR_VERIFY_SSA), &diagnostic),
                "valid self exceptional phi baseline");
        function->phiIncoming[function->phiPool[
                function->blocks[retry - 1u].phis.start].incomings.start + 1u].value = retryResult;
    }
    actual = ZrCore_ExecIr_VerifyFunction(function,
            (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA),
            &diagnostic);
    ++cases;
    if (actual != (TZrBool)!exceptionPhi ||
            (!exceptionPhi && diagnostic.code != ZR_EXECUTION_DIAGNOSTIC_NONE) ||
            (exceptionPhi &&
             (diagnostic.code != ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE ||
              diagnostic.functionToken != 7101u || diagnostic.blockId != retry ||
              diagnostic.instructionId != 3u || diagnostic.sourceId != 103u ||
              diagnostic.expectedVersion != 3u || diagnostic.actualVersion != retryResult))) {
        ++failures;
        fprintf(stderr, "FAIL %s: accepted=%u diagnostic=%u block=%u instruction=%u source=%u expected=%u actual=%u\n",
                name, (unsigned)actual, (unsigned)diagnostic.code,
                diagnostic.blockId, diagnostic.instructionId, diagnostic.sourceId,
                diagnostic.expectedVersion, diagnostic.actualVersion);
    } else {
        printf("PASS %s\n", name);
    }
    ZrCore_ExecIr_FreeModule(&module);
}

int main(void) {
    test_fixture(DIRECT_EXCEPTION_PHI, "direct exception phi rejects invoke result");
    test_fixture(NORMAL_PHI, "direct normal phi accepts invoke result");
    test_fixture(RETRY_OPERAND, "handler retry accepts normal operand");
    test_fixture(RETRY_PHI, "handler retry accepts normal phi");
    test_fixture(RETRY_BYPASS, "handler bypass still rejects normal operand");
    test_fixture(RETRY_BYPASS_PHI_VALID, "handler bypass phi preserves normal edge result");
    test_fixture(RETRY_BYPASS_PHI_INVALID, "handler bypass phi rejects exceptional edge result");
    test_fixture(EXCEPTION_CLEANUP, "exception cleanup rejects invoke result");
    test_fixture(EXCEPTION_MERGE, "exception merge phi rejects invoke result");
    test_self_exception_retry(ZR_TRUE, ZR_FALSE, "self exception retry accepts normal phi");
    test_self_exception_retry(ZR_FALSE, ZR_FALSE, "self exception retry accepts normal operand");
    test_self_exception_retry(ZR_FALSE, ZR_TRUE, "self exception retry rejects exceptional phi");
    printf("invoke result availability: %u cases, %u failures\n", cases, failures);
    return failures == 0u ? EXIT_SUCCESS : EXIT_FAILURE;
}
