#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

static TZrBool corrupt_value_count(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrBool *changed,
        SZrExecIrDiagnostic *diagnostic) {
    (void)context;
    (void)diagnostic;
    function->valueCount = function->valueCapacity + 1u;
    *changed = ZR_FALSE;
    return ZR_TRUE;
}

static void test_pipeline_verifies_storage_before_hashing(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPassContext context = {0};
    SZrExecIrPassInfo pass = {0};
    SZrExecIrPassFailure failure;
    TZrUInt64 before;
    build_constant_copy_function(&function);
    before = ZrParser_ExecIr_FunctionHash(&function);
    ZrParser_ExecIr_PassFailureInit(&failure);
    context.failure = &failure;
    pass.name = "corrupt-value-count";
    pass.run = corrupt_value_count;
    assert(!ZrParser_ExecIr_RunPassPipeline(&function, &pass, 1u, &context, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(ZrParser_ExecIr_FunctionHash(&function) == before);
    assert(function.valueCount <= function.valueCapacity);
    assert(context.passesRun == 0u);
    assert(failure.function.valueCount == failure.function.valueCapacity + 1u);
    assert(failure.diagnostic.code == diagnostic.code);
    ZrParser_ExecIr_PassFailureFree(&failure);
    ZrCore_ExecIr_FreeFunction(&function);
}

static TZrBool introduce_borrowed_suspend(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrBool *changed,
        SZrExecIrDiagnostic *diagnostic) {
    (void)diagnostic;
    function->instructions[0].flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    ++context->workUsed;
    *changed = ZR_TRUE;
    return ZR_TRUE;
}

static void test_pipeline_rolls_back_failed_map_rebuild(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPassContext context = {0};
    SZrExecIrPassInfo pass = {0};
    SZrExecIrRemarkSink remarks;
    SZrExecIrRange returnOperands;
    TZrExecIrValueId borrowed;
    TZrUInt64 before;
    init_function(&function);
    borrowed = ZrCore_ExecIr_FunctionAddExternalValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_BORROWED, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    assert(borrowed != 0u);
    assert(ZrCore_ExecIr_FunctionAddBlock(&function,
            ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u);
    assert(ZrCore_ExecIr_FunctionAppendOperands(&function, &borrowed, 1u, &returnOperands));
    append_instruction(&function, ZR_EXEC_IR_OPCODE_NOP, range(0u, 0u),
            range(0u, 0u), 0u, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u, 501u);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
            range(0u, 0u), 0u, 0u, 0u, 0u, 502u);
    function.blocks[0].instructionRange = range(0u, 2u);
    function.blocks[0].terminatorInstructionId = 2u;
    assert(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    before = ZrParser_ExecIr_FunctionHash(&function);
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    context.remarks = &remarks;
    context.workUsed = 4u;
    pass.name = "borrowed-suspend";
    pass.run = introduce_borrowed_suspend;
    assert(!ZrParser_ExecIr_RunPassPipeline(&function, &pass, 1u, &context, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND);
    assert(diagnostic.instructionId == 1u && diagnostic.sourceId == 501u);
    assert(ZrParser_ExecIr_FunctionHash(&function) == before);
    assert(function.instructions[0].flags == ZR_EXEC_IR_FLAG_DEBUG_POLL);
    assert(function.stateMap != ZR_NULL && function.stateMap->entryCount == 2u);
    assert(context.workUsed == 4u && context.passesRun == 0u && remarks.count == 0u);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_scalar_pipeline_rebuilds_checkpoint_liveness(void) {
    /* GC retains its independent root identity; UNKNOWN exercises rebuilding
     * checkpoint liveness after the eligible ordinary COPY alias rewrite. */
    for (TZrUInt32 mode = 0u; mode < 2u; ++mode) {
        SZrExecIrFunction function;
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrMaterializedState target;
        SZrExecIrResumeRequest request = {0};
        SZrExecIrRange copyOperands, copyResults, returnOperands;
        TZrExecIrValueId source, copied;
        TZrExecIrValueId expected;
        EZrExecIrOwnership ownership = mode == 0u ? ZR_EXEC_IR_OWNERSHIP_GC
                                                  : ZR_EXEC_IR_OWNERSHIP_UNKNOWN;
        TZrUInt32 rootCount = mode == 0u ? 1u : 0u;
        const SZrExecIrStateMapEntry *entry;
        init_function(&function);
        function.contract.generation = 7u;
        source = ZrCore_ExecIr_FunctionAddExternalValue(&function, 1u,
                ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE);
        copied = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
                ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE);
        assert(source != 0u && copied != 0u);
        expected = mode == 0u ? copied : source;
        assert(ZrCore_ExecIr_FunctionAddBlock(&function,
                ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u);
        assert(ZrCore_ExecIr_FunctionAppendOperands(&function, &source, 1u, &copyOperands));
        assert(ZrCore_ExecIr_FunctionAppendResults(&function, &copied, 1u, &copyResults));
        assert(ZrCore_ExecIr_FunctionAppendOperands(&function, &copied, 1u, &returnOperands));
        append_instruction(&function, ZR_EXEC_IR_OPCODE_COPY, copyOperands,
                copyResults, 0u, 0u, 0u, 0u, 101u);
        append_instruction(&function, ZR_EXEC_IR_OPCODE_NOP, range(0u, 0u),
                range(0u, 0u), 0u, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u, 102u);
        append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
                range(0u, 0u), 0u, 0u, 0u, 0u, 103u);
        function.blocks[0].instructionRange = range(0u, 3u);
        function.blocks[0].terminatorInstructionId = 3u;
        assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
        assert(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
        assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
        assert(function.operands[function.instructions[2].operands.start] == copied);
        entry = ZrCore_ExecIr_StateMapFind(function.stateMap, 102u, 1u,
                ZR_EXEC_IR_STATE_BEFORE_EFFECT);
        assert(entry != ZR_NULL && entry->liveValues.count == 1u);
        assert(function.stateMap->valuePool[entry->liveValues.start] == copied);
        assert(entry->ownerStates.count == entry->liveValues.count);
        assert(entry->rootValues.count == rootCount);
        if (rootCount != 0u)
            assert(function.stateMap->rootPool[entry->rootValues.start] == copied);
        assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, ZR_NULL, &diagnostic));
        assert(function.operands[function.instructions[2].operands.start] == expected);
        if (mode == 0u) {
            assert(function.values[source - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_GC);
            assert(function.values[copied - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_GC);
            assert(function.instructions[0].opcode == ZR_EXEC_IR_OPCODE_COPY);
            assert(function.values[copied - 1u].definition == 1u);
            assert(function.results[function.instructions[0].results.start] == copied);
        }
        entry = ZrCore_ExecIr_StateMapFind(function.stateMap, 102u, 1u,
                ZR_EXEC_IR_STATE_BEFORE_EFFECT);
        assert(entry != ZR_NULL && entry->liveValues.count == 1u);
        assert(function.stateMap->valuePool[entry->liveValues.start] == expected);
        assert(entry->ownerStates.count == entry->liveValues.count);
        assert(entry->rootValues.count == rootCount);
        if (rootCount != 0u)
            assert(function.stateMap->rootPool[entry->rootValues.start] == copied);
        ZrCore_ExecIr_MaterializedStateInit(&target);
        request.function = &function;
        request.map = function.stateMap;
        request.sourceId = entry->sourceId;
        request.resumeId = entry->resumeId;
        request.phase = entry->phase;
        request.target = &target;
        assert(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
        assert(target.rootCount == rootCount);
        if (rootCount != 0u) assert(target.roots[0] == copied);
        assert(target.valueCount == 1u && target.values[0] == expected);
        assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
        ZrCore_ExecIr_MaterializedStateFree(&target);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}
