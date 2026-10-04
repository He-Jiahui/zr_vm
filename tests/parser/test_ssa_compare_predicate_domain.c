#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { FUNCTION_TOKEN = 43u, COMPARE_SOURCE = 501u, RETURN_SOURCE = 502u,
       LEFT_VALUE = 1u, RIGHT_VALUE = 2u, BOOL_VALUE = 3u };

typedef struct SPredicateCase {
    const char *name;
    TZrExecIrTypeToken selector;
    TZrBool truth[3]; /* left < right, left == right, left > right */
} SPredicateCase;

static unsigned cases, failures, preconditions, precondition_failures, truths;
static const char *case_name;
static TZrBool case_ok;

#define EXPECT(condition) do { if (!(condition)) { case_ok = ZR_FALSE; \
    fprintf(stderr, "SEMANTIC FAIL [%s] line=%u: %s\n", \
            case_name, (unsigned)__LINE__, #condition); \
} } while (0)

static TZrBool precondition(const char *label, TZrBool success,
                            const SZrExecIrDiagnostic *diagnostic) {
    ++preconditions;
    printf("PRECONDITION %s %s %s\n", case_name, label, success ? "PASS" : "FAIL");
    if (!success) {
        ++precondition_failures;
        case_ok = ZR_FALSE;
        fprintf(stderr, "PRECONDITION FAIL [%s] %s code=%u instruction=%u source=%u\n",
                case_name, label, (unsigned)diagnostic->code,
                (unsigned)diagnostic->instructionId, (unsigned)diagnostic->sourceId);
    }
    return success;
}

/* Actual Core-owned storage. Only COMPARE and RETURN can reach the Oracle;
 * opaque value TypeTokens do not choose the runtime tagged numeric kind. */
static TZrBool build(SZrExecIrFunction *function, TZrExecIrTypeToken selector) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    TZrExecIrValueId operands[2] = {LEFT_VALUE, RIGHT_VALUE};
    TZrExecIrValueId result = BOOL_VALUE;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = FUNCTION_TOKEN;
    if (ZrCore_ExecIr_FunctionAddExternalValue(function, 0xCA110u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) != LEFT_VALUE ||
        ZrCore_ExecIr_FunctionAddExternalValue(function, 0xCA110u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) != RIGHT_VALUE ||
        ZrCore_ExecIr_FunctionAddValue(function, 0xB001u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) != BOOL_VALUE ||
        ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 1u)
        return ZR_FALSE;
    instruction.opcode = ZR_EXEC_IR_OPCODE_COMPARE;
    instruction.typeToken = selector;
    instruction.sourceId = COMPARE_SOURCE;
    if (!ZrCore_ExecIr_FunctionAppendOperands(function, operands, 2u, &instruction.operands) ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &instruction.results) ||
        !ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id) || id != 1u)
        return ZR_FALSE;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.sourceId = RETURN_SOURCE;
    if (!ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u, &instruction.operands) ||
        !ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id) || id != 2u)
        return ZR_FALSE;
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 2u;
    function->blocks[0].terminatorInstructionId = 2u;
    return ZR_TRUE;
}

static TZrBool run_oracle(const SZrExecIrFunction *function, TZrBool floating,
                          TZrUInt32 relation, SZrExecIrOracleExecutionResult *result,
                          SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOracleValue initial[3] = {{0}};
    SZrExecIrOracleInput input = {0};
    TZrInt64 left = relation == 2u ? 11 : 7;
    TZrInt64 right = relation == 0u ? 11 : 7;
    if (floating) {
        initial[0].kind = initial[1].kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        initial[0].as.floating = (TZrFloat64)left;
        initial[1].as.floating = (TZrFloat64)right;
    } else {
        initial[0].kind = initial[1].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        initial[0].as.signedInteger = left;
        initial[1].as.signedInteger = right;
    }
    input.function = function;
    input.initialValues = initial;
    input.initialValueCount = 3u;
    input.maxSteps = 2u;
    return ZrCore_ExecIr_RunOracleEx(&input, result, diagnostic);
}

static TZrBool returned_bool(const SZrExecIrOracleExecutionResult *result,
                             TZrBool expected) {
    return (TZrBool)(result->returned && !result->terminatedByThrow &&
            result->eventCount == 0u && result->executedInstructionCount == 2u &&
            result->returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL &&
            result->returnValue.as.boolean == expected);
}

static void finish(SZrExecIrFunction *function) {
    ZrCore_ExecIr_FreeFunction(function);
    if (!case_ok) ++failures;
    printf("CASE %s %s\n", case_name, case_ok ? "PASS" : "FAIL");
}

static void legal_case(const SPredicateCase *test) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic = {0};
    case_name = test->name;
    case_ok = ZR_TRUE;
    ++cases;
    if (!precondition("Build", build(&function, test->selector), &diagnostic)) goto done;
    if (!precondition("CoreVerifyAll", ZrCore_ExecIr_VerifyFunction(
                    &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic), &diagnostic)) goto done;
    for (TZrUInt32 kind = 0u; kind < 2u; ++kind) {
        for (TZrUInt32 relation = 0u; relation < 3u; ++relation) {
            SZrExecIrOracleExecutionResult result;
            char label[32];
            TZrBool success, matches;
            ZrCore_ExecIr_OracleResultInit(&result);
            memset(&diagnostic, 0, sizeof(diagnostic));
            success = run_oracle(&function, kind != 0u, relation, &result, &diagnostic);
            snprintf(label, sizeof(label), "%sRelation%u", kind ? "Float" : "Signed", (unsigned)relation);
            if (!precondition(label, (TZrBool)(success && diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE),
                              &diagnostic)) {
                ZrCore_ExecIr_OracleResultFree(&result);
                goto done;
            }
            matches = returned_bool(&result, test->truth[relation]);
            EXPECT(matches);
            EXPECT(function.instructions[0].typeToken == test->selector);
            ++truths;
            printf("TRUTH %s kind=%s relation=%u expected=%u %s\n", case_name,
                    kind ? "float" : "signed", (unsigned)relation,
                    (unsigned)test->truth[relation], matches ? "PASS" : "FAIL");
            ZrCore_ExecIr_OracleResultFree(&result);
        }
    }
done:
    finish(&function);
}

static void rejection(TZrBool success, const SZrExecIrDiagnostic *diagnostic,
                       TZrExecIrTypeToken selector, const char *backend) {
    TZrBool matches = (TZrBool)(!success &&
            diagnostic->code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
            diagnostic->functionToken == FUNCTION_TOKEN && diagnostic->blockId == 0u &&
            diagnostic->instructionId == 1u && diagnostic->sourceId == COMPARE_SOURCE &&
            diagnostic->expectedVersion == ZR_EXEC_IR_COMPARE_KIND_NOT_EQUAL &&
            diagnostic->actualVersion == selector);
    EXPECT(matches);
    printf("EXPECTED_DIAGNOSTIC %s %s %s code=%u function=%u block=%u instruction=%u source=%u expected=%u actual=%u\n",
            case_name, backend, matches ? "PASS" : "FAIL", (unsigned)diagnostic->code,
            (unsigned)diagnostic->functionToken, (unsigned)diagnostic->blockId,
            (unsigned)diagnostic->instructionId, (unsigned)diagnostic->sourceId,
            (unsigned)diagnostic->expectedVersion, (unsigned)diagnostic->actualVersion);
}

static void unknown_case(const char *name, TZrExecIrTypeToken selector) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrBool success;
    case_name = name;
    case_ok = ZR_TRUE;
    ++cases;
    if (!precondition("Build", build(&function, ZR_EXEC_IR_COMPARE_KIND_EQUAL), &diagnostic)) goto done;
    if (!precondition("BaselineCoreVerifyAll", ZrCore_ExecIr_VerifyFunction(
                    &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic), &diagnostic)) goto done;
    for (TZrUInt32 kind = 0u; kind < 2u; ++kind) {
        SZrExecIrOracleExecutionResult result;
        ZrCore_ExecIr_OracleResultInit(&result);
        memset(&diagnostic, 0, sizeof(diagnostic));
        success = run_oracle(&function, kind != 0u, 1u, &result, &diagnostic);
        success = (TZrBool)(success && diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE &&
                            returned_bool(&result, ZR_TRUE));
        ZrCore_ExecIr_OracleResultFree(&result);
        if (!precondition(kind ? "BaselineFloatOracle" : "BaselineSignedOracle", success, &diagnostic)) goto done;
    }
    /* The only mutation is the actual selector. Each consumer must reject it
     * independently, rather than silently converting it to equality. */
    function.instructions[0].typeToken = selector;
    memset(&diagnostic, 0, sizeof(diagnostic));
    success = ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic);
    rejection(success, &diagnostic, selector, "CoreVerifyAll");
    for (TZrUInt32 kind = 0u; kind < 2u; ++kind) {
        SZrExecIrOracleExecutionResult result;
        ZrCore_ExecIr_OracleResultInit(&result);
        memset(&diagnostic, 0, sizeof(diagnostic));
        success = run_oracle(&function, kind != 0u, 1u, &result, &diagnostic);
        rejection(success, &diagnostic, selector, kind ? "FloatOracle" : "SignedOracle");
        EXPECT(!result.returned && result.valueCount == 0u && result.eventCount == 0u &&
               result.executedInstructionCount == 0u);
        EXPECT(function.instructions[0].typeToken == selector);
        EXPECT(function.instructions[0].sourceId == COMPARE_SOURCE &&
               function.instructions[1].sourceId == RETURN_SOURCE);
        ZrCore_ExecIr_OracleResultFree(&result);
    }
done:
    finish(&function);
}

int main(void) {
    static const SPredicateCase legal[] = {
        {"equal-zero", ZR_EXEC_IR_COMPARE_KIND_EQUAL, {ZR_FALSE, ZR_TRUE, ZR_FALSE}},
        {"less-one", ZR_EXEC_IR_COMPARE_KIND_LESS, {ZR_TRUE, ZR_FALSE, ZR_FALSE}},
        {"less-equal-two", ZR_EXEC_IR_COMPARE_KIND_LESS_EQUAL, {ZR_TRUE, ZR_TRUE, ZR_FALSE}},
        {"greater-three", ZR_EXEC_IR_COMPARE_KIND_GREATER, {ZR_FALSE, ZR_FALSE, ZR_TRUE}},
        {"greater-equal-four", ZR_EXEC_IR_COMPARE_KIND_GREATER_EQUAL, {ZR_FALSE, ZR_TRUE, ZR_TRUE}},
        {"not-equal-five", ZR_EXEC_IR_COMPARE_KIND_NOT_EQUAL, {ZR_TRUE, ZR_FALSE, ZR_TRUE}}
    };
    (void)setvbuf(stdout, ZR_NULL, _IONBF, 0u);
    for (TZrUInt32 index = 0u; index < sizeof(legal) / sizeof(legal[0]); ++index)
        legal_case(&legal[index]);
    unknown_case("unknown-seven", 7u);
    unknown_case("unknown-nine", 9u);
    unknown_case("unknown-u32-max", UINT32_MAX);
    printf("COMPARE predicate domain: %u cases, %u failures, %u precondition failures, %u preconditions, %u truths\n",
            cases, failures, precondition_failures, preconditions, truths);
    return failures || precondition_failures ? 1 : 0;
}
