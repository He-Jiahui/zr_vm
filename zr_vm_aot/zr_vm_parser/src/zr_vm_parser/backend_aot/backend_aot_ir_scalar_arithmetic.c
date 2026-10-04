#include "backend_aot_ir_scalar_arithmetic.h"
#include "backend_aot_ir_scalar_text_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct ScalarArithmeticPlan {
    TZrUInt32 functionId;
    TZrUInt32 branchTarget;
    TZrUInt32 opcode;
    TZrUInt32 valueIds[3];
    TZrUInt64 bits[2];
} ScalarArithmeticPlan;

static EZrAotIrStatus unsupported(SZrAotIrDiagnostic *diagnostic,
                                 TZrUInt32 functionId, TZrUInt32 instructionId) {
    return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED,
                                          functionId, instructionId, 0u, 1u);
}

static TZrBool plain(const SZrAotIrInstruction *instruction) {
    return (TZrBool)(instruction->flags == 0u &&
            instruction->effectIn == 0u && instruction->effectOut == 0u &&
            instruction->phiIncoming.count == 0u &&
            instruction->memoryIn.count == 0u && instruction->memoryOut.count == 0u &&
            instruction->bindingRow == 0u && instruction->deoptId == 0u &&
            instruction->matchTypeToken == 0u);
}

/* Every conversion and intermediate is representable, including INT64_MIN. */
static int64_t signed_bits(TZrUInt64 bits) {
    if (bits <= (TZrUInt64)INT64_MAX) return (int64_t)bits;
    return -INT64_C(1) - (int64_t)(~bits);
}

static TZrBool overflows(TZrUInt32 opcode, TZrUInt64 left, TZrUInt64 right) {
    const int64_t a = signed_bits(left);
    const int64_t b = signed_bits(right);
    if (opcode == ZR_EXEC_IR_OPCODE_ADD) {
        return (TZrBool)((b > 0 && a > INT64_MAX - b) ||
                         (b < 0 && a < INT64_MIN - b));
    }
    return (TZrBool)((b > 0 && a < INT64_MIN + b) ||
                     (b < 0 && a > INT64_MAX + b));
}

static EZrAotIrStatus prepare(const SZrAotIrModule *module, TZrUInt32 functionId,
                              ScalarArithmeticPlan *plan,
                              SZrAotIrDiagnostic *diagnostic) {
    SZrAotIrCallableAbi abi;
    const SZrAotIrFunction *f;
    const SZrAotIrBlock *block;
    const SZrAotIrInstruction *instructions;
    const SZrAotIrInstruction *arithmetic;
    const SZrAotIrInstruction *ret;
    unsigned first, i;
    TZrUInt64 constantBits[2];
    EZrAotIrStatus status;
    memset(plan, 0, sizeof(*plan));
    status = ZrCore_AotIr_RequireExecutableAbi(module, functionId, &abi, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    if (module->functionCount != 1u) return unsupported(diagnostic, functionId, 0u);
    f = &module->functions[0];
    if (f->id != functionId || abi.kind != ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64 ||
        (f->blockCount != 1u && f->blockCount != 2u) ||
        f->operandCount != 3u || f->resultCount != 3u ||
        module->constantCount != 2u || module->layoutCount != 0u ||
        module->flags != 0u || module->target.requiredCapabilities != 0u ||
        module->contract.requiredCapabilities != 0u || module->contract.declaredEffects != 0u ||
        f->contract.requiredCapabilities != 0u || f->contract.declaredEffects != 0u ||
        f->frameSlotCount != 0u || f->valueSlotCount != 0u ||
        f->phiIncomingCount != 0u || f->memoryTokenCount != 0u ||
        f->gcMap != ZR_NULL || f->gcRootCount != 0u || f->deoptStateCount != 0u ||
        f->deoptValueCount != 0u || f->deoptAggregateCount != 0u ||
        f->deoptAggregateFieldCount != 0u || f->logicalStateMap != ZR_NULL ||
        f->gcMapHash != 0u || f->exceptionMapHash != 0u || f->debugMapHash != 0u) {
        return unsupported(diagnostic, functionId, 0u);
    }
    first = f->blockCount == 2u ? 1u : 0u;
    if (f->instructionCount != first + 4u ||
        f->successorCount != (first ? 2u : 0u)) {
        return unsupported(diagnostic, functionId, 0u);
    }
    instructions = &f->instructions[first];
    arithmetic = &instructions[2];
    ret = &instructions[3];
    block = &f->blocks[first];
    if (block->flags != (first ? 0u : ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ||
        block->instructions.offset != first || block->instructions.count != 4u ||
        block->terminatorInstructionId != ret->id ||
        block->successors.count != 0u || block->predecessors.count != first) {
        return unsupported(diagnostic, functionId, 0u);
    }
    if (first) {
        const SZrAotIrBlock *entry = &f->blocks[0];
        const SZrAotIrInstruction *branch = &f->instructions[0];
        if (entry->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
            entry->instructions.offset != 0u || entry->instructions.count != 1u ||
            entry->terminatorInstructionId != branch->id ||
            entry->predecessors.count != 0u || entry->successors.offset != 0u ||
            entry->successors.count != 1u || block->predecessors.offset != 1u ||
            f->successorPool[0] != block->id || f->successorPool[1] != entry->id ||
            branch->opcode != ZR_EXEC_IR_OPCODE_BRANCH ||
            branch->results.count != 0u || branch->operands.count != 0u ||
            branch->successors.offset != 0u || branch->successors.count != 1u ||
            branch->layoutId != 0u || branch->typeToken != 0u || !plain(branch)) {
            return unsupported(diagnostic, functionId, branch->id);
        }
        plan->branchTarget = block->id;
    }
    for (i = 0u; i < 4u; ++i) {
        if (!plain(&instructions[i]) || instructions[i].successors.count != 0u ||
            instructions[i].typeToken != abi.returnTypeToken) {
            return unsupported(diagnostic, functionId, instructions[i].id);
        }
    }
    for (i = 0u; i < 2u; ++i) {
        const SZrAotIrInstruction *constant = &instructions[i];
        const SZrExecIrConstant *value;
        if (constant->opcode != ZR_EXEC_IR_OPCODE_CONSTANT ||
            constant->results.count != 1u || constant->results.offset != i ||
            constant->operands.count != 0u || constant->layoutId >= 2u) {
            return unsupported(diagnostic, functionId, constant->id);
        }
        value = &module->constantPool[constant->layoutId];
        if (value->flags != 0u || value->typeToken != abi.returnTypeToken) {
            return unsupported(diagnostic, functionId, constant->id);
        }
        plan->valueIds[i] = f->resultPool[constant->results.offset];
        constantBits[i] = value->bits;
    }
    if (instructions[0].layoutId == instructions[1].layoutId ||
        (arithmetic->opcode != ZR_EXEC_IR_OPCODE_ADD && arithmetic->opcode != ZR_EXEC_IR_OPCODE_SUB) ||
        arithmetic->results.offset != 2u || arithmetic->results.count != 1u ||
        arithmetic->operands.offset != 0u || arithmetic->operands.count != 2u ||
        arithmetic->layoutId != 0u || ret->opcode != ZR_EXEC_IR_OPCODE_RETURN ||
        ret->results.count != 0u || ret->operands.offset != 2u ||
        ret->operands.count != 1u || ret->layoutId != 0u) {
        return unsupported(diagnostic, functionId, arithmetic->id);
    }
    plan->valueIds[2] = f->resultPool[2];
    if (plan->valueIds[0] == 0u || plan->valueIds[1] == 0u || plan->valueIds[2] == 0u ||
        plan->valueIds[0] == plan->valueIds[1] || plan->valueIds[0] == plan->valueIds[2] ||
        plan->valueIds[1] == plan->valueIds[2] || f->operandPool[2] != plan->valueIds[2]) {
        return unsupported(diagnostic, functionId, arithmetic->id);
    }
    for (i = 0u; i < 2u; ++i) {
        const TZrUInt32 operand = f->operandPool[i];
        if (operand == plan->valueIds[0]) plan->bits[i] = constantBits[0];
        else if (operand == plan->valueIds[1]) plan->bits[i] = constantBits[1];
        else return unsupported(diagnostic, functionId, arithmetic->id);
    }
    if (overflows(arithmetic->opcode, plan->bits[0], plan->bits[1])) {
        return unsupported(diagnostic, functionId, arithmetic->id);
    }
    plan->opcode = arithmetic->opcode;
    plan->functionId = functionId;
    return ZR_AOT_IR_OK;
}

static int literal(char *output, size_t capacity, TZrUInt64 bits, TZrBool llvm) {
    if (llvm) {
        if (bits <= (TZrUInt64)INT64_MAX) {
            return snprintf(output, capacity, "%llu", (unsigned long long)bits);
        }
        return snprintf(output, capacity, "-%llu", (unsigned long long)(~bits + 1u));
    }
    if (bits == UINT64_C(0x8000000000000000)) return snprintf(output, capacity, "INT64_MIN");
    if (bits <= (TZrUInt64)INT64_MAX) {
        return snprintf(output, capacity, "INT64_C(%llu)", (unsigned long long)bits);
    }
    return snprintf(output, capacity, "-INT64_C(%llu)", (unsigned long long)(~bits + 1u));
}

EZrAotIrStatus backend_aot_ir_scalar_arithmetic_emit(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic, TZrBool llvm) {
    ScalarArithmeticPlan plan;
    char inputs[2][64], branch[128];
    const char *operation;
    int count;
    unsigned i;
    EZrAotIrStatus status = prepare(module, functionId, &plan, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    for (i = 0u; i < 2u; ++i) {
        count = literal(inputs[i], sizeof(inputs[i]), plan.bits[i], llvm);
        if (count < 0 || (size_t)count >= sizeof(inputs[i])) {
            return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE,
                    functionId, 0u, sizeof(inputs[i]), count < 0 ? 0u : (TZrUInt64)count + 1u);
        }
    }
    branch[0] = '\0';
    if (plan.branchTarget) {
        count = llvm ? snprintf(branch, sizeof(branch),
                "  br label %%block_%u\nblock_%u:\n", plan.branchTarget, plan.branchTarget) :
                snprintf(branch, sizeof(branch),
                "    goto zr_aot_block_%u;\nzr_aot_block_%u:;\n", plan.branchTarget, plan.branchTarget);
        if (count < 0 || (size_t)count >= sizeof(branch)) {
            return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE,
                    functionId, 0u, sizeof(branch), count < 0 ? 0u : (TZrUInt64)count + 1u);
        }
    }
    operation = plan.opcode == ZR_EXEC_IR_OPCODE_ADD ? (llvm ? "add" : "+") : (llvm ? "sub" : "-");
    if (llvm) {
        count = snprintf(output, capacity,
                "define i64 @zr_aot_scalar_fn_%u() {\nentry:\n%s"
                "  %%value_%u = %s i64 %s, %s\n  ret i64 %%value_%u\n}\n",
                plan.functionId, branch, plan.valueIds[2], operation,
                inputs[0], inputs[1], plan.valueIds[2]);
    } else {
        count = snprintf(output, capacity,
                "#include <stdint.h>\nint64_t zr_aot_scalar_fn_%u(void) {\n%s"
                "    int64_t left = %s;\n    int64_t right = %s;\n"
                "    int64_t result = left %s right;\n    return result;\n}\n",
                plan.functionId, branch, inputs[0], inputs[1], operation);
    }
    if (count < 0 || (size_t)count >= capacity) {
        output[0] = '\0';
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE,
                functionId, 0u, count < 0 ? 0u : (TZrUInt64)count + 1u, capacity);
    }
    *outLength = (size_t)count;
    return ZR_AOT_IR_OK;
}
