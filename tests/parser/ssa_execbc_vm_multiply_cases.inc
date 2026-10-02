/* Scalar multiplication through the shared ExecBC-to-Core-VM consumer. */
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/exception.h"

typedef enum EZrMultiplyVmMutation {
    ZR_TEST_MULTIPLY_VM_VALID = 0,
    ZR_TEST_MULTIPLY_VM_WRONG_TYPE,
    ZR_TEST_MULTIPLY_VM_MISSING_OPERAND,
    ZR_TEST_MULTIPLY_VM_OVERFLOW
} EZrMultiplyVmMutation;

typedef struct SZrMultiplyVmReport {
    TZrBool fixtureVerified;
    TZrBool oracleSucceeded;
    TZrBool oracleReturned;
    TZrBool projectionBuilt;
    TZrBool materialized;
    TZrBool outputEmpty;
    TZrBool projectionUnchanged;
    TZrBool hasSignedMultiply;
    TZrBool vmReturned;
    TZrBool faultPcMatches;
    TZrBool hasRuntimeException;
    TZrBool recoveryReturned;
    TZrBool recoveryExceptionCleared;
    EZrThreadStatus runtimeStatus;
    EZrThreadStatus runtimeExceptionStatus;
    EZrThreadStatus recoveryStatus;
    TZrExecIrInstructionId faultInstructionId;
    TZrExecIrSourceId faultSourceId;
    TZrInt64 oracleInteger;
    TZrInt64 vmInteger;
    TZrInt64 recoveryInteger;
    SZrExecIrDiagnostic oracleDiagnostic;
    SZrExecIrDiagnostic materializerDiagnostic;
} SZrMultiplyVmReport;

static void multiply_vm_execute_call(SZrState *state, TZrPtr arguments) {
    ZrCore_Execute(state, (SZrCallInfo *)arguments);
}

static SZrCallInfo *multiply_vm_prepare_call(SZrState *state, SZrFunction *function) {
    SZrTypeValue callable;
    TZrStackValuePointer base;
    SZrCallInfo *callInfo;
    TZrUInt32 index;
    ZrCore_Value_InitAsRawObject(state, &callable, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callable.type = ZR_VALUE_TYPE_FUNCTION;
    callable.isGarbageCollectable = ZR_TRUE;
    callable.isNative = ZR_FALSE;
    base = ZrCore_Function_CheckStackAndGc(state, function->stackSize + 1u,
            state->stackTop.valuePointer);
    if (base == ZR_NULL) return ZR_NULL;
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), &callable);
    for (index = 0u; index < function->stackSize; ++index)
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base + index + 1u));
    state->stackTop.valuePointer = base + function->stackSize + 1u;
    callInfo = ZrCore_CallInfo_Extend(state);
    if (callInfo == ZR_NULL) return ZR_NULL;
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase,
            state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1u;
    state->callInfoList = callInfo;
    return callInfo;
}

static SZrMultiplyVmReport run_multiply_vm_case(
        TZrInt64 leftInteger, TZrInt64 rightInteger,
        EZrMultiplyVmMutation mutation) {
    SZrMultiplyVmReport report = {0};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecBcVmEmission emission = {0};
    SZrExecIrOracleInput oracleInput = {0};
    SZrExecIrOracleExecutionResult oracleResult;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrConstant constants[2] = {{0}};
    SZrExecIrOracleValue oracleConstants[2] = {{0}};
    SZrExecBcInstruction beforeInstructions[4];
    SZrExecIrValue beforeValues[3];
    SZrExecIrConstant beforeConstants[2];
    TZrExecIrBlockId entry;
    TZrExecIrValueId left, right, product;
    TZrExecIrValueId operands[2];
    TZrBool functionInitialized = ZR_FALSE;
    TZrBool rooted = ZR_FALSE;
    TZrUInt32 multiplyPc = UINT32_MAX;
    TZrUInt32 index;

    memset(&function, 0, sizeof(function));
    ZrCore_ExecIr_OracleResultInit(&oracleResult);
    if (state == ZR_NULL || !initialize_function(&function)) goto cleanup;
    functionInitialized = ZR_TRUE;
    entry = ZrCore_ExecIr_FunctionAddBlock(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    function.entryBlockId = entry;
    left = add_value(&function, ZR_VALUE_TYPE_INT64);
    right = add_value(&function, ZR_VALUE_TYPE_INT64);
    product = add_value(&function, ZR_VALUE_TYPE_INT64);
    operands[0] = left;
    operands[1] = right;
    if (entry != 1u || left == 0u || right == 0u || product == 0u ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                ZR_VALUE_TYPE_INT64, 0u, 901u, ZR_NULL, 0u,
                &left, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                ZR_VALUE_TYPE_INT64, 1u, 902u, ZR_NULL, 0u,
                &right, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_MUL,
                ZR_VALUE_TYPE_INT64, 0u, 903u, operands, 2u,
                &product, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !append_instruction(&function, entry, ZR_EXEC_IR_OPCODE_RETURN,
                ZR_VALUE_TYPE_INT64, 0u, 904u, &product, 1u,
                ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL) ||
        !add_instruction_source_maps(&function)) goto cleanup;
    report.fixtureVerified = ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA,
            &diagnostic);
    if (!report.fixtureVerified) goto cleanup;

    constants[0].typeToken = constants[1].typeToken = ZR_VALUE_TYPE_INT64;
    memcpy(&constants[0].bits, &leftInteger, sizeof(leftInteger));
    memcpy(&constants[1].bits, &rightInteger, sizeof(rightInteger));
    oracleConstants[0].kind = oracleConstants[1].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    oracleConstants[0].as.signedInteger = leftInteger;
    oracleConstants[1].as.signedInteger = rightInteger;
    oracleInput.function = &function;
    oracleInput.constants = oracleConstants;
    oracleInput.constantCount = 2u;
    report.oracleSucceeded = ZrCore_ExecIr_RunOracleEx(
            &oracleInput, &oracleResult, &diagnostic);
    report.oracleDiagnostic = diagnostic;
    report.oracleReturned = (TZrBool)(report.oracleSucceeded &&
            oracleResult.returned &&
            oracleResult.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
    if (!report.oracleReturned && mutation != ZR_TEST_MULTIPLY_VM_OVERFLOW)
        goto cleanup;
    if (report.oracleReturned)
        report.oracleInteger = oracleResult.returnValue.as.signedInteger;
    report.projectionBuilt = ZrParser_ExecIr_BuildProjectionWithConstants(
            &function, constants, 2u, &projection, &diagnostic);
    if (!report.projectionBuilt || projection.instructionCount != 4u ||
        projection.physicalSlotCount != 3u || projection.constantCount != 2u)
        goto cleanup;
    if (mutation == ZR_TEST_MULTIPLY_VM_WRONG_TYPE)
        projection.instructions[2].typeToken = ZR_VALUE_TYPE_BOOL;
    if (mutation == ZR_TEST_MULTIPLY_VM_MISSING_OPERAND)
        projection.instructions[2].operands.count = 1u;
    memcpy(beforeInstructions, projection.instructions, sizeof(beforeInstructions));
    memcpy(beforeValues, projection.slotValues, sizeof(beforeValues));
    memcpy(beforeConstants, projection.constants, sizeof(beforeConstants));
    report.materialized = ZrParser_ExecBcProjection_MaterializeVmFunction(
            state, &projection, &emission, &diagnostic);
    report.materializerDiagnostic = diagnostic;
    report.outputEmpty = (TZrBool)(emission.function == ZR_NULL &&
            emission.pcMap == ZR_NULL && emission.pcMapCount == 0u);
    report.projectionUnchanged = (TZrBool)(
            memcmp(beforeInstructions, projection.instructions,
                   sizeof(beforeInstructions)) == 0 &&
            memcmp(beforeValues, projection.slotValues, sizeof(beforeValues)) == 0 &&
            memcmp(beforeConstants, projection.constants, sizeof(beforeConstants)) == 0);
    if (!report.materialized) goto cleanup;
    for (index = 0u; index < emission.function->instructionsLength; ++index) {
        if (emission.function->instructionsList[index].instruction.operationCode ==
                ZR_INSTRUCTION_ENUM(MUL_SIGNED)) {
            report.hasSignedMultiply = ZR_TRUE;
            multiplyPc = index;
        }
    }
    rooted = ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
    if (rooted && mutation == ZR_TEST_MULTIPLY_VM_OVERFLOW) {
        SZrCallInfo *callInfo = multiply_vm_prepare_call(state, emission.function);
        SZrTypeValue recoveredValue;
        if (callInfo == ZR_NULL || multiplyPc == UINT32_MAX) goto cleanup;
        report.runtimeStatus = ZrCore_Exception_TryRun(
                state, multiply_vm_execute_call, callInfo);
        report.faultPcMatches = (TZrBool)(
                callInfo->context.context.programCounter ==
                        emission.function->instructionsList + multiplyPc);
        report.hasRuntimeException = state->hasCurrentException;
        report.runtimeExceptionStatus = state->currentExceptionStatus;
        for (index = 0u; index < emission.pcMapCount; ++index) {
            if (emission.pcMap[index].pc == multiplyPc) {
                report.faultInstructionId = emission.pcMap[index].instructionId;
                report.faultSourceId = emission.pcMap[index].sourceId;
            }
        }
        ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        emission.function->constantValueList[0].value.nativeObject.nativeInt64 = -13;
        emission.function->constantValueList[1].value.nativeObject.nativeInt64 = 7;
        report.recoveryReturned = ZrTests_Runtime_Function_ExecuteCaptureFailure(
                state, emission.function, &recoveredValue);
        if (report.recoveryReturned && ZR_VALUE_IS_TYPE_SIGNED_INT(recoveredValue.type))
            report.recoveryInteger = recoveredValue.value.nativeObject.nativeInt64;
        report.recoveryExceptionCleared = (TZrBool)!state->hasCurrentException;
        report.recoveryStatus = state->threadStatus;
    } else if (rooted)
        report.vmReturned = ZrTests_Runtime_Function_ExecuteExpectInt64(
                state, emission.function, &report.vmInteger);

cleanup:
    if (state != ZR_NULL && mutation == ZR_TEST_MULTIPLY_VM_OVERFLOW)
        ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    if (rooted)
        (void)ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
    ZrParser_ExecBcVmEmission_Free(state, &emission);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_OracleResultFree(&oracleResult);
    if (functionInitialized) ZrCore_ExecIr_FreeFunction(&function);
    if (state != ZR_NULL) ZrTests_Runtime_State_Destroy(state);
    return report;
}

static void test_i64_multiply_materializes_and_executes_signed_products(void) {
    const TZrInt64 operands[][3] = {
        {9, 2, 18}, {-7, 3, -21}, {0, INT64_MAX, 0}, {INT64_MAX, 1, INT64_MAX}
    };
    TZrSize index;
    for (index = 0u; index < sizeof(operands) / sizeof(operands[0]); ++index) {
        SZrMultiplyVmReport report = run_multiply_vm_case(
                operands[index][0], operands[index][1], ZR_TEST_MULTIPLY_VM_VALID);
        TEST_ASSERT_TRUE(report.fixtureVerified);
        TEST_ASSERT_TRUE(report.oracleReturned);
        TEST_ASSERT_EQUAL_INT64(operands[index][2], report.oracleInteger);
        TEST_ASSERT_TRUE(report.projectionBuilt);
        TEST_ASSERT_TRUE(report.materialized);
        TEST_ASSERT_TRUE(report.projectionUnchanged);
        TEST_ASSERT_TRUE(report.hasSignedMultiply);
        TEST_ASSERT_TRUE(report.vmReturned);
        TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    }
}

static void assert_multiply_vm_rejected(
        EZrMultiplyVmMutation mutation, EZrExecutionDiagnosticCode expected) {
    SZrMultiplyVmReport report = run_multiply_vm_case(9, 2, mutation);
    TEST_ASSERT_TRUE(report.fixtureVerified);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_FALSE(report.vmReturned);
    TEST_ASSERT_TRUE(report.outputEmpty);
    TEST_ASSERT_TRUE(report.projectionUnchanged);
    TEST_ASSERT_EQUAL_INT(expected, report.materializerDiagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(0x5105u, report.materializerDiagnostic.functionToken);
    TEST_ASSERT_EQUAL_UINT32(1u, report.materializerDiagnostic.blockId);
    TEST_ASSERT_EQUAL_UINT32(3u, report.materializerDiagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(903u, report.materializerDiagnostic.sourceId);
}

static void test_i64_multiply_rejects_wrong_type_before_publication(void) {
    assert_multiply_vm_rejected(ZR_TEST_MULTIPLY_VM_WRONG_TYPE,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_i64_multiply_rejects_missing_operand_before_publication(void) {
    assert_multiply_vm_rejected(ZR_TEST_MULTIPLY_VM_MISSING_OPERAND,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
}

static void test_i64_multiply_oracle_rejects_overflow(void) {
    const TZrInt64 operands[][2] = {{INT64_MAX, 2}, {INT64_MIN, -1}};
    TZrSize index;
    for (index = 0u; index < sizeof(operands) / sizeof(operands[0]); ++index) {
        SZrMultiplyVmReport report = run_multiply_vm_case(
                operands[index][0], operands[index][1], ZR_TEST_MULTIPLY_VM_VALID);
        TEST_ASSERT_TRUE(report.fixtureVerified);
        TEST_ASSERT_FALSE(report.oracleSucceeded);
        TEST_ASSERT_FALSE(report.oracleReturned);
        TEST_ASSERT_FALSE(report.projectionBuilt);
        TEST_ASSERT_FALSE(report.materialized);
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR,
                             report.oracleDiagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(3u, report.oracleDiagnostic.instructionId);
        TEST_ASSERT_EQUAL_UINT32(903u, report.oracleDiagnostic.sourceId);
    }
}

static void test_materialized_i64_multiply_faults_and_recovers_same_state(void) {
    const TZrInt64 operands[][2] = {{INT64_MAX, 2}, {INT64_MIN, -1}};
    TZrSize index;
    for (index = 0u; index < sizeof(operands) / sizeof(operands[0]); ++index) {
        SZrMultiplyVmReport report = run_multiply_vm_case(
                operands[index][0], operands[index][1], ZR_TEST_MULTIPLY_VM_OVERFLOW);
        TEST_ASSERT_TRUE(report.fixtureVerified);
        TEST_ASSERT_FALSE(report.oracleSucceeded);
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR,
                             report.oracleDiagnostic.code);
        TEST_ASSERT_TRUE(report.projectionBuilt);
        TEST_ASSERT_TRUE(report.materialized);
        TEST_ASSERT_TRUE(report.projectionUnchanged);
        TEST_ASSERT_TRUE(report.hasSignedMultiply);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, report.runtimeStatus);
        TEST_ASSERT_TRUE(report.hasRuntimeException);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR,
                             report.runtimeExceptionStatus);
        TEST_ASSERT_TRUE(report.faultPcMatches);
        TEST_ASSERT_EQUAL_UINT32(3u, report.faultInstructionId);
        TEST_ASSERT_EQUAL_UINT32(903u, report.faultSourceId);
        TEST_ASSERT_TRUE(report.recoveryReturned);
        TEST_ASSERT_EQUAL_INT64(-91, report.recoveryInteger);
        TEST_ASSERT_TRUE(report.recoveryExceptionCleared);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, report.recoveryStatus);
    }
}
