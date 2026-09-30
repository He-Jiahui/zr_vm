static void test_module_constant_pool_accepts_valid_reference(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_TRUE);
    expect_true(fixture.module.constantCount == 1u &&
                    fixture.function->instructions[0].layoutId == 0u &&
                    ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "valid module-owned CONSTANT pool index was rejected");
    free_constant_module(&fixture);
}

static void expect_module_constant_index_failure(TZrUInt32 index,
                                                 const char *message) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_TRUE);
    fixture.function->instructions[0].layoutId = index;
    expect_true(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                message);
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
                    diagnostic.functionToken ==
                            fixture.function->functionToken &&
                    diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
                    diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 701u &&
                    diagnostic.expectedVersion == 1u &&
                    diagnostic.actualVersion == index,
                "module constant index diagnostic lost structured location/range");
    free_constant_module(&fixture);
}

static void test_module_constant_pool_rejects_count_boundary_index(void) {
    expect_module_constant_index_failure(
            1u, "module constant index equal to constantCount was accepted");
}

static void test_module_constant_pool_rejects_uint32_max_index(void) {
    expect_module_constant_index_failure(
            UINT32_MAX, "UINT32_MAX module constant index was accepted");
}

static void test_module_constant_pool_rejects_constant_type_mismatch(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_TRUE);
    fixture.module.constants[0].typeToken =
            (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL;
    expect_true(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "module CONSTANT pool entry with a mismatched type was accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                    diagnostic.functionToken ==
                            fixture.function->functionToken &&
                    diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
                    diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 701u &&
                    diagnostic.expectedVersion ==
                            (TZrUInt32)ZR_VALUE_TYPE_INT64 &&
                    diagnostic.actualVersion ==
                            (TZrUInt32)ZR_VALUE_TYPE_BOOL,
                "module constant type diagnostic lost structured location/types");
    free_constant_module(&fixture);
}

static void test_module_without_owned_pool_accepts_external_oracle_pool(void) {
    SModuleConstantFixture fixture;
    SZrExecIrOracleValue externalConstant;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_FALSE);
    memset(&externalConstant, 0, sizeof(externalConstant));
    externalConstant.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    externalConstant.as.signedInteger = 73;
    memset(&input, 0, sizeof(input));
    input.function = fixture.function;
    input.constants = &externalConstant;
    input.constantCount = 1u;
    ZrCore_ExecIr_OracleResultInit(&execution);

    expect_true(fixture.module.constantCount == 0u &&
                    ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "module without an owned pool rejected a structurally valid CONSTANT");
    expect_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic) &&
                    execution.returned &&
                    execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                    execution.returnValue.as.signedInteger == 73,
                "separately configured Oracle constant pool was not consumed");

    ZrCore_ExecIr_OracleResultFree(&execution);
    free_constant_module(&fixture);
}

static void test_module_constant_pool_rejects_instruction_type_mismatch(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;
    build_constant_module(&fixture, ZR_TRUE);
    fixture.function->instructions[0].typeToken = ZR_VALUE_TYPE_BOOL;
    expect_true(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                    diagnostic.functionToken == fixture.function->functionToken &&
                    diagnostic.blockId == 1u && diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 701u &&
                    diagnostic.expectedVersion == ZR_VALUE_TYPE_INT64 &&
                    diagnostic.actualVersion == ZR_VALUE_TYPE_BOOL,
                "module CONSTANT accepted a mismatched explicit instruction type");
    free_constant_module(&fixture);
}

static void test_function_only_verifier_does_not_require_an_owned_pool(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;
    build_constant_module(&fixture, ZR_FALSE);
    fixture.function->instructions[0].layoutId = UINT32_MAX;
    expect_true(ZrCore_ExecIr_VerifyFunction(
                        fixture.function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic) &&
                    ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "no-pool verification imposed a module pool bound");
    free_constant_module(&fixture);
}
