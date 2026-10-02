#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/exec_ir_owner_state.h"
#include "../../zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h"
#include "zr_vm_common/zr_type_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static TZrUInt32 cases;

static void append(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                   TZrExecIrValueId operand, TZrExecIrValueId result) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    instruction.opcode = (TZrUInt16)opcode;
    instruction.sourceId = 101u + function->instructionCount;
    if (opcode == ZR_EXEC_IR_OPCODE_CONSTANT) instruction.layoutId = 7u;
    if (operand != 0u)
        CHECK(ZrCore_ExecIr_FunctionAppendOperands(function, &operand, 1u,
                                                  &instruction.operandRange));
    if (result != 0u)
        CHECK(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                                 &instruction.resultRange));
    if (opcode == ZR_EXEC_IR_OPCODE_DROP ||
        opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) {
        TZrExecIrMemoryTokenId input = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                ZR_EXEC_IR_MEMORY_OWNERSHIP, 1u);
        TZrExecIrMemoryTokenId output = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                ZR_EXEC_IR_MEMORY_OWNERSHIP, 2u);
        CHECK(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &input, 1u,
                                                       &instruction.memoryIn));
        CHECK(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &output, 1u,
                                                       &instruction.memoryOut));
        instruction.effectIn = 1u;
        instruction.effectOut = 2u;
    }
    CHECK(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
}

static void build(SZrExecIrFunction *function, EZrExecIrOwnership ownership,
                  EZrExecIrOpcode consumer, TZrBool consumeCopy) {
    TZrExecIrValueId source, copy, moved;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 91u;
    function->signatureHash = 1u;
    function->entryBlockId = ZrCore_ExecIr_FunctionAddBlock(function,
            ZR_EXEC_IR_BLOCK_FLAG_ENTRY |
            (consumer == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED
                 ? ZR_EXEC_IR_BLOCK_FLAG_CLEANUP : 0u));
    source = ZrCore_ExecIr_FunctionAddValue(function, ZR_VALUE_TYPE_INT64,
            ownership, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    copy = ZrCore_ExecIr_FunctionAddValue(function, ZR_VALUE_TYPE_INT64,
            ownership, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    moved = consumer == ZR_EXEC_IR_OPCODE_MOVE
            ? ZrCore_ExecIr_FunctionAddValue(function, ZR_VALUE_TYPE_INT64,
                  ownership, ZR_EXEC_IR_NULLABILITY_UNKNOWN) : 0u;
    CHECK(source == 1u && copy == 2u);
    CHECK(consumer != ZR_EXEC_IR_OPCODE_MOVE || moved == 3u);
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, source);
    append(function, ZR_EXEC_IR_OPCODE_COPY, source, copy);
    if (consumer != ZR_EXEC_IR_OPCODE_NOP)
        append(function, consumer, consumeCopy ? copy : source,
               consumer == ZR_EXEC_IR_OPCODE_MOVE ? moved : 0u);
    append(function, ZR_EXEC_IR_OPCODE_RETURN,
           consumeCopy ? source : copy, 0u);
    function->blocks[0].instructionRange.count = function->instructionCount;
}

static TZrBool oracle(const SZrExecIrFunction *function,
                      SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult result = {0};
    TZrBool success;
    input.function = function;
    success = ZrCore_ExecIr_RunOracleEx(&input, &result, diagnostic);
    if (success) {
        CHECK(result.returned);
        CHECK(result.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
        CHECK(result.returnValue.as.signedInteger == 7);
        if (function->instructionCount == 4u &&
            (function->instructions[2].opcode == ZR_EXEC_IR_OPCODE_DROP ||
             function->instructions[2].opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED)) {
            CHECK(result.eventCount == 1u);
            CHECK(result.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_DROP);
            CHECK(result.events[0].instructionId == 3u);
            CHECK(result.events[0].sourceId == 103u);
            CHECK(result.events[0].operandCount == 1u);
            CHECK(result.events[0].operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
            CHECK(result.events[0].operands[0].as.signedInteger == 7);
        } else {
            CHECK(result.eventCount == 0u);
        }
    }
    ZrCore_ExecIr_OracleResultFree(&result);
    return success;
}

static void run_case(EZrExecIrOwnership ownership, EZrExecIrOpcode consumer,
                     TZrBool consumeCopy) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOwnerAnalysis before = {0}, after = {0};
    TZrUInt32 repetition;
    build(&function, ownership, consumer, consumeCopy);
    CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                      &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
    CHECK(oracle(&function, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
    if (cases == 0u) printf("Original COPY/MOVE Oracle signed 7 PASS\n");
    CHECK(ZrCore_ExecIr_OwnerAnalysisBuild(&function, &before, &diagnostic));
    for (repetition = 0u; repetition < 3u; ++repetition) {
        TZrBool changed = ZR_FALSE;
        TZrUInt32 instruction, value;
        CHECK(ZrParser_ExecIr_ComputeSccp(&function, ZR_NULL, ZR_TRUE,
                                          &changed, &diagnostic));
        if (!oracle(&function, &diagnostic)) {
            fprintf(stderr, "SCCP availability: ownership=%u consumer=%u copy=%u code=%u instruction=%u source=%u\n",
                    (unsigned)ownership, (unsigned)consumer, (unsigned)consumeCopy,
                    (unsigned)diagnostic.code,
                    (unsigned)diagnostic.instructionId, (unsigned)diagnostic.sourceId);
            CHECK(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
            CHECK(diagnostic.instructionId == 4u);
            CHECK(diagnostic.sourceId == 104u);
            exit(EXIT_FAILURE);
        }
        CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                          &diagnostic));
        CHECK(ZrCore_ExecIr_OwnerAnalysisBuild(&function, &after, &diagnostic));
        for (instruction = 1u; instruction <= function.instructionCount; ++instruction)
            for (value = 1u; value <= function.valueCount; ++value) {
                CHECK(ZrCore_ExecIr_OwnerStateAt(&before, instruction, value,
                        ZR_EXEC_IR_STATE_BEFORE_EFFECT) ==
                      ZrCore_ExecIr_OwnerStateAt(&after, instruction, value,
                        ZR_EXEC_IR_STATE_BEFORE_EFFECT));
                CHECK(ZrCore_ExecIr_OwnerStateAt(&before, instruction, value,
                        ZR_EXEC_IR_STATE_AFTER_EFFECT) ==
                      ZrCore_ExecIr_OwnerStateAt(&after, instruction, value,
                        ZR_EXEC_IR_STATE_AFTER_EFFECT));
            }
        ZrCore_ExecIr_OwnerAnalysisFree(&after);
        if (consumer == ZR_EXEC_IR_OPCODE_NOP &&
            ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN) {
            CHECK(function.operandPool[function.instructions[2].operandRange.start] == 1u);
            CHECK(changed == (repetition == 0u ? ZR_TRUE : ZR_FALSE));
        } else {
            CHECK(!changed);
        }
    }
    ZrCore_ExecIr_OwnerAnalysisFree(&before);
    ZrCore_ExecIr_FreeFunction(&function);
    ++cases;
}

static void test_consumption_scan_budget(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPassBudget budget = {1000000u, 0u};
    SZrExecIrPassContext context = {0};
    TZrBool changed = ZR_FALSE;
    TZrUInt64 analysisWork;
    TZrUInt32 prefix;
    build(&function, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_OPCODE_NOP, ZR_FALSE);
    context.budget = &budget;
    CHECK(ZrParser_ExecIr_ComputeSccp(&function, &context, ZR_FALSE,
                                      &changed, &diagnostic));
    CHECK(!changed && !context.budgetExhausted);
    analysisWork = context.workUsed;
    CHECK(analysisWork != 0u);
    ZrCore_ExecIr_FreeFunction(&function);
    for (prefix = 0u; prefix < 3u; ++prefix) {
        build(&function, ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_OPCODE_NOP, ZR_FALSE);
        memset(&context, 0, sizeof(context));
        budget.maxWork = analysisWork + prefix;
        context.budget = &budget;
        CHECK(ZrParser_ExecIr_ComputeSccp(&function, &context, ZR_TRUE,
                                          &changed, &diagnostic));
        CHECK(context.budgetExhausted && !changed);
        CHECK(function.operandPool[function.instructions[2].operandRange.start] == 2u);
        CHECK(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
        CHECK(oracle(&function, &diagnostic));
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

int main(void) {
    const EZrExecIrOwnership modes[] = {ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_EXEC_IR_OWNERSHIP_SHARED};
    const EZrExecIrOpcode consumers[] = {ZR_EXEC_IR_OPCODE_MOVE,
            ZR_EXEC_IR_OPCODE_DROP, ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED};
    TZrUInt32 mode, consumer, copy;
    for (mode = 0u; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
        for (consumer = 0u; consumer < sizeof(consumers) / sizeof(consumers[0]); ++consumer) {
            if (consumers[consumer] == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED &&
                modes[mode] == ZR_EXEC_IR_OWNERSHIP_UNKNOWN) continue;
            for (copy = 0u; copy < 2u; ++copy)
                run_case(modes[mode], consumers[consumer], (TZrBool)copy);
        }
        run_case(modes[mode], ZR_EXEC_IR_OPCODE_NOP, ZR_FALSE);
    }
    test_consumption_scan_budget();
    printf("SCCP COPY availability PASS %u cases, three iterations each\n", (unsigned)cases);
    return EXIT_SUCCESS;
}
