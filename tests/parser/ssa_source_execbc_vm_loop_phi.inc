/* Finite source-loop REDs use the same parser -> SemIR -> verified ExecIR ->
 * Oracle -> ExecBC -> Core VM harness as the source branch cases. */
static TZrExecIrValueId source_execbc_skip_copies(
        const SZrExecIrFunction *function, TZrExecIrValueId value) {
    TZrUInt32 step;
    for (step = 0u; step < function->valueCount; ++step) {
        TZrExecIrInstructionId definition;
        const SZrExecIrInstruction *instruction;
        if (value == 0u || value > function->valueCount) return value;
        definition = function->values[value - 1u].definition;
        if (definition == 0u || definition > function->instructionCount) return value;
        instruction = &function->instructions[definition - 1u];
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_COPY ||
            instruction->operands.count != 1u) return value;
        value = function->operandPool[instruction->operands.start];
    }
    return 0u;
}

/* Called only after VerifyAll: its SSA check validates incoming edge order
 * and dominance. Also require an arithmetic update using the header phi and
 * feeding it from the body predecessor, rather than counting any merge phi. */
static void source_execbc_capture_loop_phis(
        const SZrExecIrFunction *function, SZrSourceExecBcReport *report) {
    TZrUInt32 blockIndex;
    report->phiPredecessorOrderValid = ZR_TRUE;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        for (phiIndex = 0u; phiIndex < block->phis.count; ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[block->phis.start + phiIndex];
            TZrUInt32 incomingIndex;
            TZrBool carried = ZR_FALSE;
            TZrBool initializedAtEntry = ZR_FALSE;
            TZrBool usedByCondition = ZR_FALSE;
            const SZrExecIrInstruction *terminator = &function->instructions[
                    block->terminatorInstructionId - 1u];
            if (terminator->opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                terminator->operands.count == 1u) {
                TZrExecIrValueId condition = source_execbc_skip_copies(function,
                        function->operandPool[terminator->operands.start]);
                if (condition != 0u && condition <= function->valueCount) {
                    TZrExecIrInstructionId definition = function->values[condition - 1u].definition;
                    if (definition != 0u && definition <= function->instructionCount) {
                        const SZrExecIrInstruction *compare = &function->instructions[definition - 1u];
                        TZrUInt32 operandIndex;
                        if (compare->opcode == ZR_EXEC_IR_OPCODE_COMPARE) {
                            for (operandIndex = 0u; operandIndex < compare->operands.count;
                                 ++operandIndex) {
                                if (source_execbc_skip_copies(function, function->operandPool[
                                            compare->operands.start + operandIndex]) == phi->result) {
                                    usedByCondition = ZR_TRUE;
                                }
                            }
                        }
                    }
                }
            }
            if (phi->incomings.count != block->predecessorRange.count) {
                report->phiPredecessorOrderValid = ZR_FALSE;
                continue;
            }
            for (incomingIndex = 0u; incomingIndex < phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[
                        phi->incomings.start + incomingIndex];
                TZrExecIrValueId value = source_execbc_skip_copies(function, incoming->value);
                TZrExecIrInstructionId definition;
                const SZrExecIrInstruction *update;
                const SZrExecIrBlock *predecessor;
                TZrUInt32 operandIndex;
                TZrUInt32 successorIndex;
                TZrBool bodySuccessor = ZR_FALSE;
                if (incoming->predecessor != function->predecessors[
                            block->predecessorRange.start + incomingIndex]) {
                    report->phiPredecessorOrderValid = ZR_FALSE;
                }
                if (phi->incomings.count != 2u || value == 0u ||
                    value > function->valueCount) continue;
                definition = function->values[value - 1u].definition;
                if (definition == 0u || definition > function->instructionCount) continue;
                update = &function->instructions[definition - 1u];
                if (incoming->predecessor == function->entryBlockId &&
                    update->opcode == ZR_EXEC_IR_OPCODE_CONSTANT) {
                    initializedAtEntry = ZR_TRUE;
                }
                if (update->opcode != ZR_EXEC_IR_OPCODE_ADD &&
                    update->opcode != ZR_EXEC_IR_OPCODE_SUB) continue;
                predecessor = &function->blocks[incoming->predecessor - 1u];
                if (definition - 1u < predecessor->instructions.start ||
                    definition - 1u - predecessor->instructions.start >=
                            predecessor->instructions.count) continue;
                for (successorIndex = 0u; successorIndex < block->successorRange.count;
                     ++successorIndex) {
                    if (function->successors[block->successorRange.start + successorIndex] ==
                            incoming->predecessor) bodySuccessor = ZR_TRUE;
                }
                if (bodySuccessor == ZR_FALSE) continue;
                for (operandIndex = 0u; operandIndex < update->operands.count; ++operandIndex) {
                    if (source_execbc_skip_copies(function, function->operandPool[
                                update->operands.start + operandIndex]) == phi->result) {
                        carried = ZR_TRUE;
                    }
                }
            }
            if (carried != ZR_FALSE && initializedAtEntry != ZR_FALSE &&
                usedByCondition != ZR_FALSE) ++report->loopCarriedPhiCount;
        }
    }
}

static TZrBool source_execbc_has_repeated_block(const SZrSourceExecBcTrace *trace) {
    TZrUInt32 index;
    TZrUInt32 previous;
    /* The harness collapses adjacent PCs from the same block first. */
    for (index = 1u; index < trace->pathCount; ++index) {
        for (previous = 0u; previous < index; ++previous) {
            if (trace->path[index] == trace->path[previous]) return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void assert_source_finite_loop(
        const TZrChar *source, TZrInt64 expectedReturn,
        TZrUInt32 arithmetic, TZrUInt32 comparison, TZrBool takesBackedge) {
    SZrSourceExecBcReport report = run_source_branch(source);
    TEST_ASSERT_TRUE_MESSAGE(report.parsedAndCompiled,
            "finite loop source must parse and compile before source CFG admission");
    TEST_ASSERT_TRUE_MESSAGE(report.preSemanticIrValidated,
            "finite loop preSemanticIr validation failed");
    TEST_ASSERT_TRUE_MESSAGE(report.sourceCfgValidated,
            "finite source loop did not establish an executable source CFG");
    assert_source_branch_report(report, expectedReturn, arithmetic);
    TEST_ASSERT_TRUE_MESSAGE(report.coreVerifyAllPassed,
            "Core VerifyAll must validate source loop SSA and CFG before execution");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, report.execIrPhiCount,
            "mutable loop local did not produce SSA phi nodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, report.loopCarriedPhiCount,
            "loop header phi must receive an arithmetic update using its prior value");
    TEST_ASSERT_TRUE_MESSAGE(report.phiPredecessorOrderValid,
            "phi incoming order differs from verified predecessor edge order");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, report.execIrComparisonCount,
            "source loop condition must reach ExecIR as a comparison");
    TEST_ASSERT_EQUAL_UINT32(comparison, report.execIrComparisonKind);
    TEST_ASSERT_EQUAL_UINT32(report.execIrComparisonCount, report.projectionComparisonCount);
    TEST_ASSERT_EQUAL_UINT32(comparison, report.projectionComparisonKind);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, report.oracleEventCount,
            "promoted scalar loop must not emit semantic effect events");
    TEST_ASSERT_EQUAL_MESSAGE(takesBackedge, report.vmRepeatedMappedBlock,
            "mapped VM blocks must repeat exactly when the finite loop takes a backedge");
}

static void test_source_while_less_loop_carried_phi(void) {
    assert_source_finite_loop(
            "var i:int=0; while(i<3){i=i+1;} return i;\n", 3,
            ZR_EXEC_IR_OPCODE_ADD, ZR_EXEC_IR_COMPARE_KIND_LESS, ZR_TRUE);
}

static void test_source_while_less_zero_iterations(void) {
    assert_source_finite_loop(
            "var i:int=3; while(i<3){i=i+1;} return i;\n", 3,
            ZR_EXEC_IR_OPCODE_ADD, ZR_EXEC_IR_COMPARE_KIND_LESS, ZR_FALSE);
}

static void test_source_while_greater_loop_carried_phi(void) {
    assert_source_finite_loop(
            "var i:int=3; while(i>0){i=i-1;} return i;\n", 0,
            ZR_EXEC_IR_OPCODE_SUB, ZR_EXEC_IR_COMPARE_KIND_GREATER, ZR_TRUE);
}
