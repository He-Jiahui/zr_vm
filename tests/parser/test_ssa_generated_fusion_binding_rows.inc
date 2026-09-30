static void test_typed_binding_rows_invalidate_hash_and_fail_closed(void) {
    SZrExecIrFunction function;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    SZrExecBcFusionPlan plan;
    TZrUInt64 beforeHash;

    make_six_pattern_function(&function);
    memset(&row, 0, sizeof(row));
    row.rowIndex = 0u;
    row.instructionId = 8u;
    row.segmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
    row.contract = make_direct_binding_contract(
            1601u, function.contract.moduleHash);
    row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    row.sourceId = function.instructions[row.instructionId - 1u].sourceId;
    assert(ZrCore_ExecIr_FunctionSetBindingRows(&function, &row, 1u,
                                                 &diagnostic));
    beforeHash = ZrParser_ExecBcFusion_InputHash(&function);
    assert(beforeHash != 0u);
    function.bindingRows[0].contract.signatureHash += 1u;
    assert(ZrCore_ExecIr_FunctionValidateBindingRows(&function, &diagnostic));
    assert(ZrParser_ExecBcFusion_InputHash(&function) != beforeHash);

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(!ZrParser_ExecIr_BuildExecBcFusion(
            &function, &options, &plan, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert(function.bindingRowsSchemaVersion ==
           ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED);
    ZrParser_ExecBcFusionPlan_Free(&plan);

    function.bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY;
    assert(ZrParser_ExecBcFusion_InputHash(&function) == 0u);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(!ZrParser_ExecIr_BuildExecBcFusion(
            &function, &options, &plan, &diagnostic));
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);

    /* An empty typed table is still a schema boundary for direct callers. */
    ZrCore_ExecIr_FunctionInit(&function);
    assert(ZrCore_ExecIr_FunctionSetBindingRows(&function, ZR_NULL, 0u,
                                                 &diagnostic));
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(!ZrParser_ExecIr_BuildExecBcFusion(
            &function, &options, &plan, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert(function.bindingRowsSchemaVersion ==
           ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}
