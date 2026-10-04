#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Expected columns follow the existing selector ABI: ==, <, <=, >, >=, !=. */
static const TZrBool compare_equal_expected[6] = {1, 0, 1, 0, 1, 0};
static const TZrBool compare_less_expected[6] = {0, 1, 1, 0, 0, 1};
static const TZrBool compare_greater_expected[6] = {0, 0, 0, 1, 1, 1};
static const TZrBool compare_unordered_expected[6] = {0, 0, 0, 0, 0, 1};

static void compare_require(
        TZrBool actual, TZrBool expected, const char *executor,
        TZrUInt32 selector, const SZrExecIrOracleValue *left,
        const SZrExecIrOracleValue *right) {
    TZrUInt64 leftBits = 0u, rightBits = 0u;
    if (actual == expected) return;
    memcpy(&leftBits, &left->as, sizeof(leftBits));
    memcpy(&rightBits, &right->as, sizeof(rightBits));
    fprintf(stderr,
            "compare executor=%s selector=%u case=(%u:0x%llx,%u:0x%llx) "
            "expected=%u actual=%u\n",
            executor, (unsigned int)selector, (unsigned int)left->kind,
            (unsigned long long)leftBits, (unsigned int)right->kind,
            (unsigned long long)rightBits, (unsigned int)expected,
            (unsigned int)actual);
    exit(1);
}

static void assert_compare_predicates(
        TZrExecIrTypeToken operandType,
        const SZrExecIrOracleValue *left,
        const SZrExecIrOracleValue *right,
        const TZrBool expected[6]) {
    TZrUInt32 selector;
    for (selector = 0u; selector < 6u; ++selector) {
        SZrExecIrFunction function;
        TZrExecIrValueId operands[2], result;
        SZrExecIrRange operandRange, resultRange, returnRange;
        SZrExecIrOracleValue initial[2];
        SZrExecIrOracleInput input;
        SZrExecIrOracleExecutionResult execution;
        SZrExecBcExecutionInput bcInput;
        SZrExecBcExecutionResult bcExecution;
        SZrExecBcProjection bc = {0};
        SZrExecIrDiagnostic diagnostic;

        ZrCore_ExecIr_FunctionInit(&function);
        operands[0] = ZrCore_ExecIr_FunctionAddExternalValue(
                &function, operandType, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        operands[1] = ZrCore_ExecIr_FunctionAddExternalValue(
                &function, operandType, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        result = ZrCore_ExecIr_FunctionAddValue(
                &function, ZR_VALUE_TYPE_BOOL, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        assert(operands[0] != 0u && operands[1] != 0u && result != 0u);
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                &function, operands, 2u, &operandRange));
        assert(ZrCore_ExecIr_FunctionAppendResults(
                &function, &result, 1u, &resultRange));
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                &function, &result, 1u, &returnRange));
        append_instruction(&function, ZR_EXEC_IR_OPCODE_COMPARE,
                           operandRange, resultRange, range(0u, 0u),
                           0u, 910u);
        function.instructions[0].typeToken = selector;
        function.instructions[0].matchTypeToken = operandType;
        append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN,
                           returnRange, range(0u, 0u), range(0u, 0u),
                           0u, 911u);
        initial[0] = *left;
        initial[1] = *right;

        memset(&input, 0, sizeof(input));
        input.function = &function;
        input.initialValues = initial;
        input.initialValueCount = 2u;
        ZrCore_ExecIr_OracleResultInit(&execution);
        assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
        assert(execution.returned);
        assert(execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL);
        compare_require(execution.returnValue.as.boolean, expected[selector],
                        "Oracle", selector, left, right);
        ZrCore_ExecIr_OracleResultFree(&execution);

        assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
        assert(bc.instructions[0].typeToken == selector);
        memset(&bcInput, 0, sizeof(bcInput));
        bcInput.initialValues = initial;
        bcInput.initialValueCount = 2u;
        ZrParser_ExecBcExecutionResult_Init(&bcExecution);
        assert(ZrParser_ExecBcProjection_Run(
                &bc, &bcInput, &bcExecution, &diagnostic));
        assert(bcExecution.returned);
        assert(bcExecution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL);
        compare_require(bcExecution.returnValue.as.boolean, expected[selector],
                        "ExecBC projection", selector, left, right);
        ZrParser_ExecBcExecutionResult_Free(&bcExecution);
        ZrParser_ExecBcProjection_Free(&bc);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_compare_float_predicates_oracle_and_projection(void) {
    static const struct {
        TZrFloat64 left;
        TZrFloat64 right;
        const TZrBool *expected;
    } cases[] = {
        {NAN, 1.0, compare_unordered_expected},
        {1.0, NAN, compare_unordered_expected},
        {NAN, NAN, compare_unordered_expected},
        {-7.5, -7.5, compare_equal_expected},
        {-7.5, 2.25, compare_less_expected},
        {2.25, -7.5, compare_greater_expected},
        {-0.0, 0.0, compare_equal_expected},
        {0.0, -0.0, compare_equal_expected},
        {-INFINITY, INFINITY, compare_less_expected},
        {INFINITY, -INFINITY, compare_greater_expected},
        {INFINITY, INFINITY, compare_equal_expected},
        {-INFINITY, -INFINITY, compare_equal_expected},
        {1.0, INFINITY, compare_less_expected},
        {-INFINITY, 1.0, compare_less_expected}
    };
    TZrSize index;
    SZrExecIrOracleValue left = {0}, right = {0};
    left.kind = right.kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        left.as.floating = cases[index].left;
        right.as.floating = cases[index].right;
        assert_compare_predicates(ZR_VALUE_TYPE_DOUBLE, &left, &right,
                                  cases[index].expected);
    }
}

static void test_compare_integer_and_bool_predicates_oracle_and_projection(void) {
    SZrExecIrOracleValue left = {0}, right = {0};
    left.kind = right.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    left.as.signedInteger = INT64_MIN;
    right.as.signedInteger = INT64_MAX;
    assert_compare_predicates(ZR_VALUE_TYPE_INT64, &left, &right,
                              compare_less_expected);
    left.as.signedInteger = INT64_MAX;
    right.as.signedInteger = INT64_MAX - 1;
    assert_compare_predicates(ZR_VALUE_TYPE_INT64, &left, &right,
                              compare_greater_expected);
    right.as.signedInteger = INT64_MAX;
    assert_compare_predicates(ZR_VALUE_TYPE_INT64, &left, &right,
                              compare_equal_expected);

    left.kind = right.kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    left.as.unsignedInteger = UINT64_MAX - 1u;
    right.as.unsignedInteger = UINT64_MAX;
    assert_compare_predicates(ZR_VALUE_TYPE_UINT64, &left, &right,
                              compare_less_expected);
    left.as.unsignedInteger = UINT64_MAX;
    right.as.unsignedInteger = 0u;
    assert_compare_predicates(ZR_VALUE_TYPE_UINT64, &left, &right,
                              compare_greater_expected);
    right.as.unsignedInteger = UINT64_MAX;
    assert_compare_predicates(ZR_VALUE_TYPE_UINT64, &left, &right,
                              compare_equal_expected);

    left.kind = right.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    left.as.boolean = ZR_FALSE;
    right.as.boolean = ZR_TRUE;
    assert_compare_predicates(ZR_VALUE_TYPE_BOOL, &left, &right,
                              compare_less_expected);
    left.as.boolean = ZR_TRUE;
    right.as.boolean = ZR_FALSE;
    assert_compare_predicates(ZR_VALUE_TYPE_BOOL, &left, &right,
                              compare_greater_expected);
    right.as.boolean = ZR_TRUE;
    assert_compare_predicates(ZR_VALUE_TYPE_BOOL, &left, &right,
                              compare_equal_expected);
}
