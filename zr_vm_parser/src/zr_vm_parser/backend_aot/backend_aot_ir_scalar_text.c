#include "backend_aot/backend_aot_ir_scalar_text_internal.h"

#include <string.h>

EZrAotIrStatus backend_aot_ir_scalar_text_fail(
        SZrAotIrDiagnostic *diagnostic, EZrAotIrStatus status,
        TZrUInt32 functionId, TZrUInt32 instructionId,
        TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->functionId = functionId;
        diagnostic->instructionId = instructionId;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

EZrAotIrStatus backend_aot_ir_scalar_text_check_output(
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic) {
    if (outLength != ZR_NULL) *outLength = 0u;
    if (output != ZR_NULL && capacity != 0u) output[0] = '\0';
    if (output == ZR_NULL || capacity == 0u || outLength == ZR_NULL) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, 0u, 0u, 1u, 0u);
    }
    return ZR_AOT_IR_OK;
}

/* 文本模板没有效果链、内存链或去优化承载能力，不能默默丢弃这些语义。 */
static TZrBool scalar_text_instruction_plain(
        const SZrAotIrInstruction *instruction) {
    return (TZrBool)(instruction->flags == 0u &&
                    instruction->effectIn == 0u &&
                    instruction->effectOut == 0u &&
                    instruction->phiIncoming.count == 0u &&
                    instruction->memoryIn.count == 0u &&
                    instruction->memoryOut.count == 0u &&
                    instruction->bindingRow == 0u &&
                    instruction->deoptId == 0u);
}

/* 此源码配置仅承载一个宿主 i64 值，字节宽度和对齐是配置的显式约束。 */
enum { SCALAR_TEXT_I64_BYTES = 8u, SCALAR_TEXT_SOURCE_INSTRUCTIONS = 3u };

static TZrBool scalar_text_nop_plain(
        const SZrAotIrInstruction *instruction, TZrExecIrTypeToken returnTypeToken) {
    /* Core 已验证范围边界；空范围可保留前一指令之后的合法游标。 */
    return (TZrBool)(scalar_text_instruction_plain(instruction) &&
            instruction->results.count == 0u && instruction->operands.count == 0u &&
            instruction->successors.count == 0u &&
            instruction->layoutId == 0u && instruction->typeToken == returnTypeToken &&
            instruction->matchTypeToken == 0u);
}

static EZrAotIrStatus scalar_text_source_layout(
        const SZrAotIrModule *module, const SZrAotIrFunction *function,
        const SZrAotIrCallableAbi *abi, TZrUInt32 instructionId,
        TZrUInt32 valueId, SZrAotIrDiagnostic *diagnostic) {
    const SZrExecIrLayout *layout;
    const SZrAotIrFrameSlot *slot;
    const SZrAotIrFrameLayout *frame = &function->frameLayout;
    const SZrExecIrStateMap *map = function->logicalStateMap;
    if (module->layoutCount != 1u || function->frameSlotCount != 1u ||
        function->valueSlotCount != 1u || map == ZR_NULL ||
        map->entryCount != 0u || map->valueCount != 0u ||
        map->rootCount != 0u || map->ownerStateCount != 0u ||
        module->contract.generation != 1u || function->contract.generation != 1u ||
        module->contract.layoutHash != 0u) {
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED,
                function->id, instructionId, 1u, module->layoutCount);
    }
    layout = &module->layoutPool[0];
    slot = &function->frameSlots[0];
    if (frame->layoutHash == 0u) {
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT,
                function->id, instructionId, 1u, frame->layoutHash);
    }
#define SOURCE_LAYOUT_REQUIRE(actualValue, expectedValue) \
    do { \
        if ((actualValue) != (expectedValue)) \
            return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, \
                    function->id, instructionId, (expectedValue), (actualValue)); \
    } while (0)
    SOURCE_LAYOUT_REQUIRE(layout->typeToken, abi->returnTypeToken);
    SOURCE_LAYOUT_REQUIRE(layout->byteSize, SCALAR_TEXT_I64_BYTES);
    SOURCE_LAYOUT_REQUIRE(layout->byteAlign, SCALAR_TEXT_I64_BYTES);
    SOURCE_LAYOUT_REQUIRE(slot->slotId, valueId);
    SOURCE_LAYOUT_REQUIRE(slot->typeToken, abi->returnTypeToken);
    SOURCE_LAYOUT_REQUIRE(slot->kind, ZR_EXEC_IR_FRAME_SLOT_VALUE);
    SOURCE_LAYOUT_REQUIRE(slot->byteOffset, 0u);
    SOURCE_LAYOUT_REQUIRE(slot->byteSize, layout->byteSize);
    SOURCE_LAYOUT_REQUIRE(slot->byteAlign, layout->byteAlign);
    SOURCE_LAYOUT_REQUIRE(function->valueSlotPool[0], 0u);
    SOURCE_LAYOUT_REQUIRE(frame->logicalSlotCount, 1u);
    SOURCE_LAYOUT_REQUIRE(frame->storageSlotCount, 1u);
    SOURCE_LAYOUT_REQUIRE(frame->parameterPrefixBytes, 0u);
    SOURCE_LAYOUT_REQUIRE(frame->frameByteSize, SCALAR_TEXT_I64_BYTES);
    SOURCE_LAYOUT_REQUIRE(frame->frameByteAlign, SCALAR_TEXT_I64_BYTES);
    SOURCE_LAYOUT_REQUIRE(frame->returnAreaOffset, SCALAR_TEXT_I64_BYTES);
    SOURCE_LAYOUT_REQUIRE(frame->layoutHash, function->contract.layoutHash);
#undef SOURCE_LAYOUT_REQUIRE
    /* Core 已验证 primitive/hash 非零及状态表的完整身份。 */
    return ZR_AOT_IR_OK;
}

static EZrAotIrStatus scalar_text_source_prepare(
        const SZrAotIrModule *module, const SZrAotIrFunction *function,
        const SZrAotIrCallableAbi *abi, SZrBackendAotIrScalarTextPlan *plan,
        SZrAotIrDiagnostic *diagnostic) {
    const SZrAotIrBlock *block = &function->blocks[0];
    const SZrAotIrInstruction *literal = ZR_NULL, *returned = ZR_NULL;
    const SZrExecIrConstant *constant;
    TZrUInt32 valueId;
    EZrAotIrStatus status;
    if (block->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        block->instructions.count != SCALAR_TEXT_SOURCE_INSTRUCTIONS ||
        function->successorCount != 0u || block->predecessors.count != 0u ||
        block->successors.count != 0u) {
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED,
                function->id, 0u, SCALAR_TEXT_SOURCE_INSTRUCTIONS, block->instructions.count);
    }
    /* 扫描 Core 已验证的真实块范围；位置只规定顺序，不规定稳定 ID。 */
    for (TZrUInt32 i = 0u; i < block->instructions.count; ++i) {
        const SZrAotIrInstruction *instruction =
                &function->instructions[block->instructions.offset + i];
        TZrBool accepted = ZR_FALSE;
        if (i == 0u) {
            literal = instruction;
            accepted = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT &&
                    scalar_text_instruction_plain(instruction) &&
                    instruction->results.count == 1u && instruction->operands.count == 0u &&
                    instruction->successors.count == 0u && instruction->matchTypeToken == 0u);
        } else if (i == 1u) {
            accepted = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_NOP &&
                    scalar_text_nop_plain(instruction, abi->returnTypeToken));
        } else {
            returned = instruction;
            accepted = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_RETURN &&
                    scalar_text_instruction_plain(instruction) &&
                    instruction->results.count == 0u && instruction->operands.count == 1u &&
                    instruction->successors.count == 0u && instruction->layoutId == 0u &&
                    instruction->matchTypeToken == 0u &&
                    block->terminatorInstructionId == instruction->id);
        }
        if (!accepted) return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, function->id, instruction->id, 0u, 1u);
    }
    valueId = function->operandPool[returned->operands.offset];
    if (function->resultPool[literal->results.offset] != valueId) {
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED,
                function->id, literal->id, valueId,
                function->resultPool[literal->results.offset]);
    }
    constant = &module->constantPool[literal->layoutId];
    if (constant->flags != 0u || constant->typeToken != abi->returnTypeToken) {
        return backend_aot_ir_scalar_text_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED,
                function->id, literal->id, abi->returnTypeToken, constant->typeToken);
    }
    status = scalar_text_source_layout(module, function, abi, literal->id, valueId, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    plan->functionId = function->id;
    plan->bits = constant->bits;
    return ZR_AOT_IR_OK;
}

EZrAotIrStatus backend_aot_ir_scalar_text_prepare(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        SZrBackendAotIrScalarTextPlan *plan,
        SZrAotIrDiagnostic *diagnostic) {
    SZrAotIrCallableAbi abi;
    const SZrAotIrFunction *function;
    const SZrAotIrBlock *block;
    const SZrAotIrInstruction *constantInstruction;
    const SZrAotIrInstruction *returnInstruction;
    const SZrExecIrConstant *constant;
    EZrAotIrStatus status;

    if (plan == ZR_NULL) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, functionId, 0u,
                1u, 0u);
    }
    memset(plan, 0, sizeof(*plan));
    /* 先验证数组范围、CFG 和显式 ABI；下面的定址读取依赖这些检查。 */
    status = ZrCore_AotIr_RequireExecutableAbi(module, functionId, &abi,
                                               diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    if (module->functionCount != 1u) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u,
                1u, module->functionCount);
    }
    function = &module->functions[0];
    /* 模板只承载一个纯常量函数；拒绝需要运行时存储、状态恢复或能力支持的输入。 */
    if (function->id != functionId ||
        abi.kind != ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64 ||
        (function->blockCount != 1u && function->blockCount != 2u) ||
        function->operandCount != 1u || function->resultCount != 1u ||
        module->constantCount != 1u ||
        module->flags != 0u || module->target.requiredCapabilities != 0u ||
        module->contract.requiredCapabilities != 0u ||
        module->contract.declaredEffects != 0u ||
        function->contract.requiredCapabilities != 0u ||
        function->contract.declaredEffects != 0u ||
        function->phiIncomingCount != 0u ||
        function->memoryTokenCount != 0u || function->gcMap != ZR_NULL ||
        function->gcRootCount != 0u || function->deoptStateCount != 0u ||
        function->deoptValueCount != 0u ||
        function->deoptAggregateCount != 0u ||
        function->deoptAggregateFieldCount != 0u ||
        function->gcMapHash != 0u || function->exceptionMapHash != 0u ||
        function->relocationCount != 0u || module->relocationCount != 0u) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
    }
    if (function->blockCount == 1u &&
        function->instructionCount == SCALAR_TEXT_SOURCE_INSTRUCTIONS) {
        return scalar_text_source_prepare(module, function, &abi, plan, diagnostic);
    }
    /* 既有两指令/入口分支配置仍不承载源码 primitive、frame 或状态表。 */
    if (module->layoutCount != 0u || function->frameSlotCount != 0u ||
        function->valueSlotCount != 0u || function->logicalStateMap != ZR_NULL) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
    }
    if (function->blockCount == 1u) {
        if (function->instructionCount != 2u || function->successorCount != 0u) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
        }
        block = &function->blocks[0];
        constantInstruction = &function->instructions[0];
        returnInstruction = &function->instructions[1];
        if (block->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
            block->terminatorInstructionId != returnInstruction->id ||
            block->instructions.offset != 0u ||
            block->instructions.count != 2u ||
            block->predecessors.count != 0u ||
            block->successors.count != 0u ||
            !scalar_text_instruction_plain(constantInstruction) ||
            !scalar_text_instruction_plain(returnInstruction)) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                    constantInstruction->id, 0u, 1u);
        }
    } else {
        const SZrAotIrBlock *entry = &function->blocks[0];
        const SZrAotIrBlock *exit = &function->blocks[1];
        const SZrAotIrInstruction *branch = &function->instructions[0];
        if (function->instructionCount != 3u || function->successorCount != 2u) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
        }
        constantInstruction = &function->instructions[1];
        returnInstruction = &function->instructions[2];
        /* 前驱和后继共用 successorPool；两项互证同一入口到出口边，避免模板跳转改义。 */
        if (entry->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY || exit->flags != 0u ||
            entry->instructions.offset != 0u ||
            entry->instructions.count != 1u ||
            exit->instructions.offset != 1u ||
            exit->instructions.count != 2u ||
            entry->terminatorInstructionId != branch->id ||
            exit->terminatorInstructionId != returnInstruction->id ||
            entry->predecessors.count != 0u ||
            entry->successors.offset != 0u ||
            entry->successors.count != 1u ||
            exit->predecessors.offset != 1u ||
            exit->predecessors.count != 1u ||
            exit->successors.count != 0u ||
            function->successorPool[0] != exit->id ||
            function->successorPool[1] != entry->id ||
            branch->opcode != ZR_EXEC_IR_OPCODE_BRANCH ||
            branch->results.count != 0u || branch->operands.count != 0u ||
            branch->successors.offset != 0u ||
            branch->successors.count != 1u ||
            branch->layoutId != 0u || branch->typeToken != 0u ||
            !scalar_text_instruction_plain(branch) ||
            !scalar_text_instruction_plain(constantInstruction) ||
            !scalar_text_instruction_plain(returnInstruction)) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                    branch->id, 0u, 1u);
        }
        plan->branchTargetBlockId = exit->id;
    }
    /* 返回值必须正是唯一常量的 SSA 结果，不能仅因指令数量相符就替换函数含义。 */
    if (constantInstruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT ||
        returnInstruction->opcode != ZR_EXEC_IR_OPCODE_RETURN ||
        constantInstruction->results.count != 1u ||
        constantInstruction->operands.count != 0u ||
        returnInstruction->results.count != 0u ||
        returnInstruction->operands.count != 1u ||
        constantInstruction->successors.count != 0u ||
        returnInstruction->successors.count != 0u ||
        constantInstruction->layoutId != 0u ||
        returnInstruction->layoutId != 0u ||
        function->resultPool[constantInstruction->results.offset] !=
                function->operandPool[returnInstruction->operands.offset]) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                constantInstruction->id, 0u, 1u);
    }
    constant = &module->constantPool[0];
    if (constant->flags != 0u ||
        constant->typeToken != abi.returnTypeToken) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                constantInstruction->id, abi.returnTypeToken,
                constant->typeToken);
    }
    plan->functionId = functionId;
    plan->bits = constant->bits;
    return ZR_AOT_IR_OK;
}
