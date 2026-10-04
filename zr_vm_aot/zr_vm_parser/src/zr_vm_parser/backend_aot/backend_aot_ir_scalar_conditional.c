#include "backend_aot_ir_scalar_conditional.h"
#include "backend_aot_ir_scalar_text_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct ScalarConditionalPlan {
    TZrUInt32 functionId;
    TZrUInt32 predicate;
    TZrUInt32 conditionId;
    TZrUInt32 blockIds[2];
    TZrUInt32 targets[2];
    TZrUInt64 operandBits[2];
    TZrUInt64 returnBits[2];
} ScalarConditionalPlan;

static EZrAotIrStatus unsupported(SZrAotIrDiagnostic *diagnostic,
                                 TZrUInt32 functionId, TZrUInt32 instructionId) {
    return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED,
                                          functionId, instructionId, 0u, 1u);
}

static TZrBool plain(const SZrAotIrInstruction *instruction) {
    return (TZrBool)(instruction->flags == 0u && instruction->effectIn == 0u &&
            instruction->effectOut == 0u && instruction->phiIncoming.count == 0u &&
            instruction->memoryIn.count == 0u && instruction->memoryOut.count == 0u &&
            instruction->bindingRow == 0u && instruction->deoptId == 0u &&
            instruction->matchTypeToken == 0u);
}

static EZrAotIrStatus prepare(const SZrAotIrModule *module, TZrUInt32 functionId,
                              ScalarConditionalPlan *plan,
                              SZrAotIrDiagnostic *diagnostic) {
    static const unsigned constantIndices[4] = {0u, 1u, 4u, 6u};
    SZrAotIrCallableAbi abi;
    const SZrAotIrFunction *f;
    const SZrAotIrInstruction *cmp, *branch;
    TZrUInt64 constantBits[4];
    TZrUInt32 ids[5], usedConstants = 0u;
    unsigned i, j;
    EZrAotIrStatus status;
    memset(plan, 0, sizeof(*plan));
    status = ZrCore_AotIr_RequireExecutableAbi(module, functionId, &abi, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    if (module->functionCount != 1u) return unsupported(diagnostic, functionId, 0u);
    f = &module->functions[0];
    if (f->id != functionId || abi.kind != ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64 ||
        f->blockCount != 3u || f->instructionCount != 8u || f->resultCount != 5u ||
        f->operandCount != 5u || f->successorCount != 4u || module->constantCount != 4u ||
        module->layoutCount != 0u || module->flags != 0u ||
        module->target.requiredCapabilities != 0u ||
        module->contract.requiredCapabilities != 0u || module->contract.declaredEffects != 0u ||
        f->contract.requiredCapabilities != 0u || f->contract.declaredEffects != 0u ||
        f->frameSlotCount != 0u || f->valueSlotCount != 0u || f->phiIncomingCount != 0u ||
        f->memoryTokenCount != 0u || f->gcMap != ZR_NULL || f->gcRootCount != 0u ||
        f->deoptStateCount != 0u || f->deoptValueCount != 0u || f->deoptAggregateCount != 0u ||
        f->deoptAggregateFieldCount != 0u || f->logicalStateMap != ZR_NULL ||
        f->gcMapHash != 0u || f->exceptionMapHash != 0u || f->debugMapHash != 0u ||
        f->frameLayout.parameterPrefixBytes != 0u || f->frameLayout.logicalSlotCount != 0u ||
        f->frameLayout.storageSlotCount != 0u || f->frameLayout.returnAreaOffset != 0u) {
        return unsupported(diagnostic, functionId, 0u);
    }
    cmp = &f->instructions[2];
    branch = &f->instructions[3];
    for (i = 0u; i < 8u; ++i) {
        if (!plain(&f->instructions[i])) {
            return unsupported(diagnostic, functionId, f->instructions[i].id);
        }
    }
    for (i = 0u; i < 5u; ++i) {
        ids[i] = f->resultPool[i];
        if (ids[i] == 0u) return unsupported(diagnostic, functionId, cmp->id);
        for (j = 0u; j < i; ++j) {
            if (ids[i] == ids[j]) return unsupported(diagnostic, functionId, cmp->id);
        }
    }
    if (f->blocks[0].flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        f->blocks[0].instructions.offset != 0u || f->blocks[0].instructions.count != 4u ||
        f->blocks[0].terminatorInstructionId != branch->id ||
        f->blocks[0].predecessors.count != 0u || f->blocks[0].successors.offset != 0u ||
        f->blocks[0].successors.count != 2u ||
        branch->opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
        branch->results.count != 0u || branch->operands.offset != 2u ||
        branch->operands.count != 1u || f->operandPool[2] != ids[2] ||
        branch->successors.offset != 0u || branch->successors.count != 2u ||
        branch->typeToken != 0u || branch->layoutId != 0u) {
        return unsupported(diagnostic, functionId, branch->id);
    }
    for (i = 0u; i < 2u; ++i) {
        const SZrAotIrBlock *block = &f->blocks[i + 1u];
        const SZrAotIrInstruction *ret = &f->instructions[5u + 2u * i];
        if (block->flags != 0u || block->instructions.offset != 4u + 2u * i ||
            block->instructions.count != 2u || block->terminatorInstructionId != ret->id ||
            block->predecessors.offset != 2u + i || block->predecessors.count != 1u ||
            f->successorPool[2u + i] != f->blocks[0].id || block->successors.count != 0u ||
            ret->opcode != ZR_EXEC_IR_OPCODE_RETURN || ret->results.count != 0u ||
            ret->operands.offset != 3u + i || ret->operands.count != 1u ||
            f->operandPool[3u + i] != ids[3u + i] || ret->successors.count != 0u ||
            ret->layoutId != 0u || ret->typeToken != abi.returnTypeToken) {
            return unsupported(diagnostic, functionId, ret->id);
        }
        plan->blockIds[i] = block->id;
    }
    if (f->successorPool[0] == f->successorPool[1] ||
        (f->successorPool[0] != plan->blockIds[0] && f->successorPool[0] != plan->blockIds[1]) ||
        (f->successorPool[1] != plan->blockIds[0] && f->successorPool[1] != plan->blockIds[1])) {
        return unsupported(diagnostic, functionId, branch->id);
    }
    if (cmp->opcode != ZR_EXEC_IR_OPCODE_COMPARE ||
        (cmp->typeToken != 1u && cmp->typeToken != 3u) ||
        cmp->results.offset != 2u || cmp->results.count != 1u ||
        cmp->operands.offset != 0u || cmp->operands.count != 2u ||
        cmp->successors.count != 0u || cmp->layoutId != 0u) {
        return unsupported(diagnostic, functionId, cmp->id);
    }
    for (i = 0u; i < 4u; ++i) {
        const SZrAotIrInstruction *constant = &f->instructions[constantIndices[i]];
        const SZrExecIrConstant *value;
        const TZrUInt32 resultOffset = i < 2u ? i : i + 1u;
        if (constant->opcode != ZR_EXEC_IR_OPCODE_CONSTANT ||
            constant->results.offset != resultOffset || constant->results.count != 1u ||
            constant->operands.count != 0u || constant->successors.count != 0u ||
            constant->layoutId >= 4u || constant->typeToken != abi.returnTypeToken ||
            (usedConstants & (1u << constant->layoutId)) != 0u) {
            return unsupported(diagnostic, functionId, constant->id);
        }
        usedConstants |= 1u << constant->layoutId;
        value = &module->constantPool[constant->layoutId];
        if (value->flags != 0u || value->typeToken != abi.returnTypeToken) {
            return unsupported(diagnostic, functionId, constant->id);
        }
        constantBits[i] = value->bits;
    }
    for (i = 0u; i < 2u; ++i) {
        if (f->operandPool[i] == ids[0]) plan->operandBits[i] = constantBits[0];
        else if (f->operandPool[i] == ids[1]) plan->operandBits[i] = constantBits[1];
        else return unsupported(diagnostic, functionId, cmp->id);
        plan->returnBits[i] = constantBits[2u + i];
        plan->targets[i] = f->successorPool[i];
    }
    plan->functionId = functionId;
    plan->predicate = cmp->typeToken;
    plan->conditionId = ids[2];
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

EZrAotIrStatus backend_aot_ir_scalar_conditional_emit(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic, TZrBool llvm) {
    ScalarConditionalPlan plan;
    char values[4][64];
    int count;
    unsigned i;
    EZrAotIrStatus status = prepare(module, functionId, &plan, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    for (i = 0u; i < 4u; ++i) {
        count = literal(values[i], sizeof(values[i]),
                i < 2u ? plan.operandBits[i] : plan.returnBits[i - 2u], llvm);
        if (count < 0 || (size_t)count >= sizeof(values[i])) {
            return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE,
                    functionId, 0u, sizeof(values[i]), count < 0 ? 0u : (TZrUInt64)count + 1u);
        }
    }
    if (llvm) {
        count = snprintf(output, capacity,
                "define i64 @zr_aot_scalar_fn_%u() {\nentry:\n"
                "  %%condition_%u = icmp %s i64 %s, %s\n"
                "  br i1 %%condition_%u, label %%block_%u, label %%block_%u\n"
                "block_%u:\n  ret i64 %s\nblock_%u:\n  ret i64 %s\n}\n",
                plan.functionId, plan.conditionId, plan.predicate == 1u ? "slt" : "sgt",
                values[0], values[1], plan.conditionId, plan.targets[0], plan.targets[1],
                plan.blockIds[0], values[2], plan.blockIds[1], values[3]);
    } else {
        count = snprintf(output, capacity,
                "#include <stdint.h>\nint64_t zr_aot_scalar_fn_%u(void) {\n"
                "    int64_t left = %s;\n    int64_t right = %s;\n"
                "    _Bool condition = left %s right;\n"
                "    if (condition) goto zr_aot_block_%u;\n"
                "    goto zr_aot_block_%u;\nzr_aot_block_%u:;\n"
                "    return %s;\nzr_aot_block_%u:;\n    return %s;\n}\n",
                plan.functionId, values[0], values[1], plan.predicate == 1u ? "<" : ">",
                plan.targets[0], plan.targets[1], plan.blockIds[0], values[2],
                plan.blockIds[1], values[3]);
    }
    if (count < 0 || (size_t)count >= capacity) {
        output[0] = '\0';
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE,
                functionId, 0u, count < 0 ? 0u : (TZrUInt64)count + 1u, capacity);
    }
    *outLength = (size_t)count;
    return ZR_AOT_IR_OK;
}
