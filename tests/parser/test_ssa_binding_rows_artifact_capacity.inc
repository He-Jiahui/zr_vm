static int roundtrip_reserved_row_capacity(const SZrExecIrModule *source) {
    SZrExecIrModule borrowedModule = *source;
    SZrExecIrFunction borrowedFunction = source->functions[0];
    TZrUInt32 capacity = borrowedFunction.bindingRowCount + 3u;
    SZrExecIrBindingRow *reserved = (SZrExecIrBindingRow *)calloc(
            capacity, sizeof(*reserved));
    int ok;
    if (!require_true(reserved != ZR_NULL,
                      "reserved binding-row test storage allocation failed")) return 0;
    if (borrowedFunction.bindingRowCount != 0u)
        memcpy(reserved, borrowedFunction.bindingRows,
                borrowedFunction.bindingRowCount * sizeof(*reserved));
    borrowedFunction.bindingRows = reserved;
    borrowedFunction.bindingRowCapacity = capacity;
    borrowedModule.functions = &borrowedFunction;
    ok = require_true(ZrCore_ExecIr_VerifyModule(&borrowedModule, ZR_NULL),
                      "Core rejected legal reserved binding-row capacity");
    if (ok) {
        ok = borrowedFunction.bindingRowCount == 0u
                ? roundtrip_typed_empty(&borrowedModule)
                : roundtrip_typed_call(&borrowedModule);
    }
    free(reserved);
    return ok;
}

static int roundtrip_call_without_binding_facts(const SZrExecIrModule *source) {
    SZrExecIrModule borrowedModule = *source;
    SZrExecIrFunction borrowedFunction = source->functions[0];
    SZrExecIrInstruction *instructions = (SZrExecIrInstruction *)malloc(
            borrowedFunction.instructionCount * sizeof(*instructions));
    int ok;
    if (!require_true(instructions != ZR_NULL,
                      "rowless CALL test instruction allocation failed")) return 0;
    memcpy(instructions, borrowedFunction.instructions,
            borrowedFunction.instructionCount * sizeof(*instructions));
    for (TZrUInt32 index = 0u; index < borrowedFunction.instructionCount; ++index)
        instructions[index].bindingRow = ZR_EXEC_IR_BINDING_ROW_REF_NONE;
    borrowedFunction.instructions = instructions;
    borrowedFunction.bindingRows = ZR_NULL;
    borrowedFunction.bindingRowCount = 0u;
    borrowedFunction.bindingRowCapacity = 0u;
    borrowedModule.functions = &borrowedFunction;
    ok = require_true(ZrCore_ExecIr_VerifyModule(&borrowedModule, ZR_NULL),
                      "Core rejected a typed CALL with no binding facts");
    if (ok) ok = roundtrip_typed_empty(&borrowedModule);
    free(instructions);
    return ok;
}
