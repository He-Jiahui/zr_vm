#include <stdlib.h>

static void test_typed_empty_binding_schema_is_rejected_by_projection(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    memset(&projection, 0, sizeof(projection));
    assert(ZrCore_ExecIr_FunctionSetBindingRows(&function, ZR_NULL, 0u,
                                                 &diagnostic));
    assert(function.bindingRowsSchemaVersion ==
           ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED);
    assert(function.bindingRowCount == 0u);

    assert(!ZrParser_ExecIr_BuildProjection(&function, &projection,
                                            &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert(function.bindingRowsSchemaVersion ==
           ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_projection_rejects_orphan_legacy_rows_but_keeps_compact_hints(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    function.bindingRows = (SZrExecIrBindingRow *)calloc(
            1u, sizeof(*function.bindingRows));
    assert(function.bindingRows != ZR_NULL);
    function.bindingRowCount = 1u;
    function.bindingRowCapacity = 1u;
    memset(&projection, 0, sizeof(projection));
    assert(!ZrParser_ExecIr_BuildProjection(&function, &projection,
                                             &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);

    /* Legacy compact references remain scalar hints owned by the old path;
     * the row validator must not treat them as a typed table index. */
    build_call_function(&function);
    function.instructions[1].bindingRow = 17u;
    memset(&projection, 0, sizeof(projection));
    assert(ZrParser_ExecIr_BuildProjection(&function, &projection,
                                            &diagnostic));
    assert(projection.instructionCount == 3u);
    assert(projection.instructions[1].bindingRow == 17u);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}
