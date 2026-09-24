#ifndef ZR_TEST_SSA_ESCAPE_AGGREGATE_CASES_H
#define ZR_TEST_SSA_ESCAPE_AGGREGATE_CASES_H

#include <stdlib.h>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

static void check_aggregate_recovery_lifetime(TZrUInt32 checkpointIndex,
                                              TZrBool crossesSuspend) {
    SZrExecIrFunction function;
    SZrExecIrEscapeSummary summary;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId allocation, tested, suspended;
    init_function(&function);
    allocation = add_gc_value(&function, 9u);
    tested = add_value(&function, 1u);
    suspended = add_value(&function, 1u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_ALLOC, NULL, 0u,
                       &allocation, 1u, 4u, 0u, 0u, 101u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_TYPE_TEST, &allocation, 1u,
                       &tested, 1u, 0u, 0u, 0u, 102u);
    function.instructions[1].matchTypeToken = 9u;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_SUSPEND, NULL, 0u,
                       &suspended, 1u, 0u, ZR_EXEC_IR_FLAG_MAY_SUSPEND, 0u, 103u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_NOP, NULL, 0u,
                       NULL, 0u, 0u, 0u, 0u, 104u);
    function.deoptStates = calloc(1u, sizeof(*function.deoptStates));
    function.deoptAggregates = calloc(1u, sizeof(*function.deoptAggregates));
    function.deoptAggregateFields = calloc(1u, sizeof(*function.deoptAggregateFields));
    assert(function.deoptStates && function.deoptAggregates && function.deoptAggregateFields);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptAggregateCount = function.deoptAggregateCapacity = 1u;
    function.deoptAggregateFieldCount = function.deoptAggregateFieldCapacity = 1u;
    function.deoptStates[0].id = 1u;
    function.deoptStates[0].sourceId = function.instructions[checkpointIndex].sourceId;
    function.deoptStates[0].resumeId = 1u;
    function.deoptStates[0].aggregates.count = 1u;
    function.deoptAggregates[0].identityId = 1u;
    function.deoptAggregates[0].typeToken = 11u;
    function.deoptAggregates[0].layoutId = 12u;
    function.deoptAggregates[0].fields.count = 1u;
    function.deoptAggregateFields[0].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[0].valueId = allocation;
    function.instructions[checkpointIndex].deoptId = 1u;
    ZrParser_ExecIr_EscapeSummaryInit(&summary);
    assert(ZrParser_ExecIr_AnalyzeEscape(&function, &summary, &diagnostic));
    assert(fact(&summary, allocation)->lastUseInstructionId == checkpointIndex + 1u);
    assert(fact(&summary, allocation)->crossesSuspend == crossesSuspend);
    assert(fact(&summary, allocation)->decision ==
           (crossesSuspend ? ZR_EXEC_IR_ALLOC_HEAP : ZR_EXEC_IR_ALLOC_STACK));
    ZrParser_ExecIr_EscapeSummaryFree(&summary);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_aggregate_recovery_lifetime_across_suspend(void) {
#if defined(_MSC_VER)
    /* Negative regression runs must report to CTest without a desktop dialog. */
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    check_aggregate_recovery_lifetime(3u, ZR_TRUE);
    check_aggregate_recovery_lifetime(2u, ZR_TRUE);
    check_aggregate_recovery_lifetime(1u, ZR_FALSE);
}

#endif
