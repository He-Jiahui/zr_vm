#include "backend_aot_exec_ir.h"

#include "backend_aot_function_table.h"
#include "backend_aot_internal.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

#include <stdint.h>

#define ZR_AOT_COUNT_NONE 0U
#define ZR_AOT_EXEC_IR_INDEX_NONE ((TZrUInt32)UINT32_MAX)

static TZrUInt32 backend_aot_exec_ir_runtime_contracts_for_opcode(TZrUInt32 opcode);
static TZrUInt32 backend_aot_exec_ir_callsite_kind_for_instruction(const SZrFunction *function,
                                                                   TZrUInt32 execInstructionIndex,
                                                                   TZrUInt16 opcode);
static TZrUInt32 backend_aot_exec_ir_debug_line_for_instruction(const SZrFunction *function,
                                                                TZrUInt32 execInstructionIndex);
static TZrUInt32 backend_aot_exec_ir_terminator_kind_for_instruction(TZrUInt16 opcode);
static TZrBool backend_aot_exec_ir_instruction_ends_block(TZrUInt16 opcode);
static TZrBool backend_aot_exec_ir_branch_target(const SZrFunction *function,
                                                 TZrUInt32 instructionIndex,
                                                 TZrUInt32 *outTargetIndex);
static TZrBool backend_aot_exec_ir_build_frame_layout(const SZrFunction *function,
                                                      SZrAotExecIrFrameLayout *outFrameLayout);
static void backend_aot_exec_ir_find_block_successors(const SZrFunction *function,
                                                      const TZrUInt32 *instructionToBlockIndex,
                                                      SZrAotExecIrBasicBlock *block);
static TZrBool backend_aot_exec_ir_build_basic_blocks(SZrState *state,
                                                      const SZrFunction *function,
                                                      SZrAotExecIrFunction *outFunction,
                                                      TZrUInt32 **outInstructionToBlockIndex);
static TZrUInt32 backend_aot_exec_ir_find_parent_function_index(SZrState *state,
                                                                const SZrAotFunctionTable *functionTable,
                                                                const SZrFunction *function);
static TZrBool backend_aot_exec_ir_build_function(SZrState *state,
                                                  const SZrAotFunctionTable *functionTable,
                                                  const SZrAotFunctionEntry *entry,
                                                  SZrAotExecIrInstruction *moduleInstructions,
                                                  TZrUInt32 *ioInstructionOffset,
                                                  SZrAotExecIrFunction *outFunction);

static const SZrFunction *backend_aot_exec_ir_function_from_constant_value(SZrState *state,
                                                                           const SZrTypeValue *value) {
    return ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
}

const TZrChar *backend_aot_exec_ir_semir_opcode_name(TZrUInt32 opcode) {
    switch ((EZrSemIrOpcode)opcode) {
        case ZR_SEMIR_OPCODE_OWN_UNIQUE:
            return "OWN_UNIQUE";
        case ZR_SEMIR_OPCODE_OWN_USING:
            return "OWN_USING";
        case ZR_SEMIR_OPCODE_OWN_SHARE:
            return "OWN_SHARE";
        case ZR_SEMIR_OPCODE_OWN_WEAK:
            return "OWN_WEAK";
        case ZR_SEMIR_OPCODE_OWN_UPGRADE:
            return "OWN_UPGRADE";
        case ZR_SEMIR_OPCODE_OWN_RELEASE:
            return "OWN_RELEASE";
        case ZR_SEMIR_OPCODE_TYPEOF:
            return "TYPEOF";
        case ZR_SEMIR_OPCODE_DYN_CALL:
            return "DYN_CALL";
        case ZR_SEMIR_OPCODE_DYN_TAIL_CALL:
            return "DYN_TAIL_CALL";
        case ZR_SEMIR_OPCODE_META_CALL:
            return "META_CALL";
        case ZR_SEMIR_OPCODE_META_TAIL_CALL:
            return "META_TAIL_CALL";
        case ZR_SEMIR_OPCODE_META_GET:
            return "META_GET";
        case ZR_SEMIR_OPCODE_META_SET:
            return "META_SET";
        case ZR_SEMIR_OPCODE_DYN_ITER_INIT:
            return "DYN_ITER_INIT";
        case ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT:
            return "DYN_ITER_MOVE_NEXT";
        case ZR_SEMIR_OPCODE_NOP:
        default:
            return "NOP";
    }
}

static TZrUInt32 backend_aot_exec_ir_runtime_contracts_for_opcode(TZrUInt32 opcode) {
    switch ((EZrSemIrOpcode)opcode) {
        case ZR_SEMIR_OPCODE_TYPEOF:
            return ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF;
        case ZR_SEMIR_OPCODE_DYN_CALL:
        case ZR_SEMIR_OPCODE_DYN_TAIL_CALL:
        case ZR_SEMIR_OPCODE_META_CALL:
        case ZR_SEMIR_OPCODE_META_TAIL_CALL:
        case ZR_SEMIR_OPCODE_META_GET:
        case ZR_SEMIR_OPCODE_META_SET:
            return ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL;
        case ZR_SEMIR_OPCODE_DYN_ITER_INIT:
            return ZR_AOT_RUNTIME_CONTRACT_ITER_INIT;
        case ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT:
            return ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT;
        case ZR_SEMIR_OPCODE_OWN_SHARE:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE;
        case ZR_SEMIR_OPCODE_OWN_WEAK:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK;
        case ZR_SEMIR_OPCODE_OWN_UPGRADE:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE;
        case ZR_SEMIR_OPCODE_OWN_RELEASE:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE;
        case ZR_SEMIR_OPCODE_OWN_UNIQUE:
        case ZR_SEMIR_OPCODE_OWN_USING:
        case ZR_SEMIR_OPCODE_NOP:
        default:
            return ZR_AOT_RUNTIME_CONTRACT_NONE;
    }
}

const TZrChar *backend_aot_exec_ir_runtime_contract_name(TZrUInt32 contractBit) {
    switch (contractBit) {
        case ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF:
            return "reflection.typeof";
        case ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL:
            return "dispatch.precall";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE:
            return "ownership.share";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK:
            return "ownership.weak";
        case ZR_AOT_RUNTIME_CONTRACT_ITER_INIT:
            return "iter.init";
        case ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT:
            return "iter.move_next";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE:
            return "ownership.upgrade";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE:
            return "ownership.release";
        case ZR_AOT_RUNTIME_CONTRACT_NONE:
        default:
            return "none";
    }
}

TZrUInt32 backend_aot_exec_ir_runtime_contract_count(TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;
    TZrUInt32 count = ZR_AOT_COUNT_NONE;

    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) != 0) {
            count++;
        }
    }

    return count;
}

const TZrChar *backend_aot_exec_ir_callsite_kind_name(TZrUInt32 callsiteKind) {
    switch ((EZrAotExecIrCallsiteKind)callsiteKind) {
        case ZR_AOT_EXEC_IR_CALLSITE_KIND_STATIC_DIRECT:
            return "static_direct";
        case ZR_AOT_EXEC_IR_CALLSITE_KIND_DIRECT_PROBE:
            return "direct_probe";
        case ZR_AOT_EXEC_IR_CALLSITE_KIND_META:
            return "meta";
        case ZR_AOT_EXEC_IR_CALLSITE_KIND_GENERIC:
            return "generic";
        case ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE:
        default:
            return "none";
    }
}

const TZrChar *backend_aot_exec_ir_terminator_kind_name(TZrUInt32 terminatorKind) {
    switch ((EZrAotExecIrTerminatorKind)terminatorKind) {
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH:
            return "fallthrough";
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH:
            return "branch";
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH:
            return "conditional_branch";
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN:
            return "return";
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN:
            return "tail_return";
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME:
            return "eh_resume";
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_NONE:
        default:
            return "none";
    }
}

static TZrUInt32 backend_aot_exec_ir_callsite_kind_from_cache_kind(TZrUInt32 cacheKind) {
    switch ((EZrFunctionCallSiteCacheKind)cacheKind) {
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET:
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET:
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC:
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC:
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL:
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL:
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_META;
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL:
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL:
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_DIRECT_PROBE;
        case ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE:
        default:
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE;
    }
}

static TZrUInt32 backend_aot_exec_ir_callsite_kind_for_instruction(const SZrFunction *function,
                                                                   TZrUInt32 execInstructionIndex,
                                                                   TZrUInt16 opcode) {
    if (function != ZR_NULL && function->callSiteCaches != ZR_NULL) {
        for (TZrUInt32 cacheIndex = 0; cacheIndex < function->callSiteCacheLength; cacheIndex++) {
            const SZrFunctionCallSiteCacheEntry *cacheEntry = &function->callSiteCaches[cacheIndex];
            if (cacheEntry->instructionIndex == execInstructionIndex) {
                TZrUInt32 cachedKind = backend_aot_exec_ir_callsite_kind_from_cache_kind(cacheEntry->kind);
                if (cachedKind != ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE) {
                    return cachedKind;
                }
            }
        }
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_STATIC_DIRECT;
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_DIRECT_PROBE;
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_META;
        default:
            return ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE;
    }
}

static TZrUInt32 backend_aot_exec_ir_debug_line_for_instruction(const SZrFunction *function,
                                                                TZrUInt32 execInstructionIndex) {
    TZrUInt32 bestLine = 0;

    if (function == ZR_NULL) {
        return 0;
    }

    if (function->lineInSourceList != ZR_NULL && execInstructionIndex < function->instructionsLength) {
        bestLine = function->lineInSourceList[execInstructionIndex];
        if (bestLine > 0) {
            return bestLine;
        }
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
            const SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
            if ((TZrUInt32)info->currentInstructionOffset > execInstructionIndex) {
                break;
            }
            bestLine = info->lineInSource;
        }
    }

    if (bestLine == 0) {
        bestLine = function->lineInSourceStart;
    }

    return bestLine;
}

static TZrUInt32 backend_aot_exec_ir_terminator_kind_for_instruction(TZrUInt16 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
            return ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            return ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH;
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            return ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN;
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN;
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
            return ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME;
        default:
            return ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH;
    }
}

static TZrBool backend_aot_exec_ir_instruction_ends_block(TZrUInt16 opcode) {
    switch (backend_aot_exec_ir_terminator_kind_for_instruction(opcode)) {
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME:
            return ZR_TRUE;
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_NONE:
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_exec_ir_branch_target(const SZrFunction *function,
                                                 TZrUInt32 instructionIndex,
                                                 TZrUInt32 *outTargetIndex) {
    const TZrInstruction *instruction;
    TZrInt32 targetIndex = -1;

    if (outTargetIndex != ZR_NULL) {
        *outTargetIndex = ZR_AOT_EXEC_IR_INDEX_NONE;
    }

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            targetIndex = (TZrInt32)instructionIndex + 1 + instruction->instruction.operand.operand2[0];
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            targetIndex = (TZrInt32)instructionIndex + 1 + (TZrInt16)instruction->instruction.operand.operand1[1];
            break;
        default:
            return ZR_FALSE;
    }

    if (targetIndex < 0 || (TZrUInt32)targetIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    if (outTargetIndex != ZR_NULL) {
        *outTargetIndex = (TZrUInt32)targetIndex;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_exec_ir_build_frame_layout(const SZrFunction *function,
                                                      SZrAotExecIrFrameLayout *outFrameLayout) {
    if (function == ZR_NULL || outFrameLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outFrameLayout, 0, sizeof(*outFrameLayout));
    outFrameLayout->parameterCount = function->parameterCount;
    outFrameLayout->stackSlotCount = function->stackSize;
    outFrameLayout->generatedFrameSlotCount = ZrCore_Function_GetGeneratedFrameSlotCount(function);
    outFrameLayout->closureValueCount = function->closureValueLength;
    outFrameLayout->localVariableCount = function->localVariableLength;
    outFrameLayout->exportedValueCount = function->exportedVariableLength;
    return ZR_TRUE;
}

static void backend_aot_exec_ir_find_block_successors(const SZrFunction *function,
                                                      const TZrUInt32 *instructionToBlockIndex,
                                                      SZrAotExecIrBasicBlock *block) {
    TZrUInt32 fallthroughIndex;
    TZrUInt32 targetIndex = ZR_AOT_EXEC_IR_INDEX_NONE;

    if (function == ZR_NULL || instructionToBlockIndex == ZR_NULL || block == ZR_NULL || block->instructionCount == 0) {
        return;
    }

    fallthroughIndex = block->firstExecInstructionIndex + block->instructionCount;
    switch ((EZrAotExecIrTerminatorKind)block->terminatorKind) {
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH:
            if (backend_aot_exec_ir_branch_target(function, block->terminatorInstructionIndex, &targetIndex)) {
                block->successorBlockIndices[block->successorCount++] = instructionToBlockIndex[targetIndex];
            }
            break;
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH:
            if (backend_aot_exec_ir_branch_target(function, block->terminatorInstructionIndex, &targetIndex)) {
                block->successorBlockIndices[block->successorCount++] = instructionToBlockIndex[targetIndex];
            }
            if (fallthroughIndex < function->instructionsLength) {
                TZrUInt32 fallthroughBlock = instructionToBlockIndex[fallthroughIndex];
                if (block->successorCount == 0 || block->successorBlockIndices[0] != fallthroughBlock) {
                    block->successorBlockIndices[block->successorCount++] = fallthroughBlock;
                }
            }
            break;
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH:
            if (fallthroughIndex < function->instructionsLength) {
                block->successorBlockIndices[block->successorCount++] = instructionToBlockIndex[fallthroughIndex];
            }
            break;
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME:
        case ZR_AOT_EXEC_IR_TERMINATOR_KIND_NONE:
        default:
            break;
    }
}

static TZrBool backend_aot_exec_ir_build_basic_blocks(SZrState *state,
                                                      const SZrFunction *function,
                                                      SZrAotExecIrFunction *outFunction,
                                                      TZrUInt32 **outInstructionToBlockIndex) {
    TZrBool *blockStarts;
    TZrUInt32 *instructionToBlockIndex;
    TZrUInt32 blockCount = 0;

    if (outInstructionToBlockIndex != ZR_NULL) {
        *outInstructionToBlockIndex = ZR_NULL;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outFunction == ZR_NULL ||
        outInstructionToBlockIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    outFunction->basicBlocks = ZR_NULL;
    outFunction->basicBlockCount = 0;

    if (function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                             sizeof(*blockStarts) * function->instructionsLength,
                                                             ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    instructionToBlockIndex = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                           sizeof(*instructionToBlockIndex) *
                                                                                   function->instructionsLength,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (blockStarts == ZR_NULL || instructionToBlockIndex == ZR_NULL) {
        if (blockStarts != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          blockStarts,
                                          sizeof(*blockStarts) * function->instructionsLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (instructionToBlockIndex != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          instructionToBlockIndex,
                                          sizeof(*instructionToBlockIndex) * function->instructionsLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(blockStarts, 0, sizeof(*blockStarts) * function->instructionsLength);
    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        instructionToBlockIndex[index] = ZR_AOT_EXEC_IR_INDEX_NONE;
    }
    blockStarts[0] = ZR_TRUE;

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        TZrUInt32 targetIndex = ZR_AOT_EXEC_IR_INDEX_NONE;
        TZrUInt16 opcode = function->instructionsList[instructionIndex].instruction.operationCode;

        if (backend_aot_exec_ir_branch_target(function, instructionIndex, &targetIndex)) {
            blockStarts[targetIndex] = ZR_TRUE;
        }
        if (backend_aot_exec_ir_instruction_ends_block(opcode) && instructionIndex + 1 < function->instructionsLength) {
            blockStarts[instructionIndex + 1] = ZR_TRUE;
        }
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        if (blockStarts[instructionIndex]) {
            blockCount++;
        }
    }

    if (blockCount == 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      blockStarts,
                                      sizeof(*blockStarts) * function->instructionsLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      instructionToBlockIndex,
                                      sizeof(*instructionToBlockIndex) * function->instructionsLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    outFunction->basicBlocks = (SZrAotExecIrBasicBlock *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*outFunction->basicBlocks) * blockCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outFunction->basicBlocks == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      blockStarts,
                                      sizeof(*blockStarts) * function->instructionsLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      instructionToBlockIndex,
                                      sizeof(*instructionToBlockIndex) * function->instructionsLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outFunction->basicBlocks, 0, sizeof(*outFunction->basicBlocks) * blockCount);
    outFunction->basicBlockCount = blockCount;

    {
        TZrUInt32 blockIndex = 0;
        TZrUInt32 instructionIndex = 0;

        while (instructionIndex < function->instructionsLength && blockIndex < blockCount) {
            TZrUInt32 nextStart = instructionIndex + 1;
            SZrAotExecIrBasicBlock *block = &outFunction->basicBlocks[blockIndex];

            if (!blockStarts[instructionIndex]) {
                instructionIndex++;
                continue;
            }

            while (nextStart < function->instructionsLength && !blockStarts[nextStart]) {
                nextStart++;
            }

            block->blockId = blockIndex;
            block->firstExecInstructionIndex = instructionIndex;
            block->instructionCount = nextStart - instructionIndex;
            block->firstInstructionOffset = ZR_AOT_EXEC_IR_INDEX_NONE;
            block->semIrInstructionCount = 0;
            block->terminatorInstructionIndex = nextStart - 1;
            block->terminatorKind =
                    backend_aot_exec_ir_terminator_kind_for_instruction(
                            function->instructionsList[block->terminatorInstructionIndex].instruction.operationCode);

            for (TZrUInt32 mappedIndex = instructionIndex; mappedIndex < nextStart; mappedIndex++) {
                instructionToBlockIndex[mappedIndex] = blockIndex;
            }

            instructionIndex = nextStart;
            blockIndex++;
        }
    }

    for (TZrUInt32 blockIndex = 0; blockIndex < outFunction->basicBlockCount; blockIndex++) {
        backend_aot_exec_ir_find_block_successors(function, instructionToBlockIndex, &outFunction->basicBlocks[blockIndex]);
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  blockStarts,
                                  sizeof(*blockStarts) * function->instructionsLength,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    *outInstructionToBlockIndex = instructionToBlockIndex;
    return ZR_TRUE;
}

static TZrUInt32 backend_aot_exec_ir_find_parent_function_index(SZrState *state,
                                                                const SZrAotFunctionTable *functionTable,
                                                                const SZrFunction *function) {
    if (state == ZR_NULL || functionTable == ZR_NULL || functionTable->entries == ZR_NULL || function == ZR_NULL) {
        return ZR_AOT_INVALID_FUNCTION_INDEX;
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        const SZrFunction *candidate = functionTable->entries[functionIndex].function;
        if (candidate == ZR_NULL || candidate == function) {
            continue;
        }

        for (TZrUInt32 childIndex = 0; childIndex < candidate->childFunctionLength; childIndex++) {
            if (&candidate->childFunctionList[childIndex] == function) {
                return functionTable->entries[functionIndex].flatIndex;
            }
        }

        for (TZrUInt32 constantIndex = 0; constantIndex < candidate->constantValueLength; constantIndex++) {
            const SZrFunction *constantFunction =
                    backend_aot_exec_ir_function_from_constant_value(state, &candidate->constantValueList[constantIndex]);
            if (constantFunction == function) {
                return functionTable->entries[functionIndex].flatIndex;
            }
        }
    }

    return ZR_AOT_INVALID_FUNCTION_INDEX;
}

static TZrBool backend_aot_exec_ir_build_function(SZrState *state,
                                                  const SZrAotFunctionTable *functionTable,
                                                  const SZrAotFunctionEntry *entry,
                                                  SZrAotExecIrInstruction *moduleInstructions,
                                                  TZrUInt32 *ioInstructionOffset,
                                                  SZrAotExecIrFunction *outFunction) {
    TZrUInt32 *instructionToBlockIndex = ZR_NULL;

    if (state == ZR_NULL || functionTable == ZR_NULL || entry == ZR_NULL || ioInstructionOffset == ZR_NULL ||
        outFunction == ZR_NULL || entry->function == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outFunction, 0, sizeof(*outFunction));
    outFunction->function = entry->function;
    outFunction->flatIndex = entry->flatIndex;
    outFunction->parentFunctionIndex =
            backend_aot_exec_ir_find_parent_function_index(state, functionTable, entry->function);
    outFunction->firstInstructionOffset = *ioInstructionOffset;
    outFunction->instructionCount = entry->function->semIrInstructionLength;
    outFunction->execInstructionCount = entry->function->instructionsLength;

    if (!backend_aot_exec_ir_build_frame_layout(entry->function, &outFunction->frameLayout) ||
        !backend_aot_exec_ir_build_basic_blocks(state, entry->function, outFunction, &instructionToBlockIndex)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 localInstructionIndex = 0; localInstructionIndex < entry->function->semIrInstructionLength;
         localInstructionIndex++) {
        const SZrSemIrInstruction *sourceInstruction = &entry->function->semIrInstructions[localInstructionIndex];
        SZrAotExecIrInstruction *destinationInstruction = &moduleInstructions[*ioInstructionOffset + localInstructionIndex];
        TZrUInt32 execInstructionIndex = sourceInstruction->execInstructionIndex;
        TZrUInt32 blockIndex = ZR_AOT_EXEC_IR_INDEX_NONE;
        TZrUInt16 execOpcode = 0;

        if (entry->function->instructionsList != ZR_NULL && execInstructionIndex < entry->function->instructionsLength) {
            execOpcode = entry->function->instructionsList[execInstructionIndex].instruction.operationCode;
        }
        if (instructionToBlockIndex != ZR_NULL && execInstructionIndex < entry->function->instructionsLength) {
            blockIndex = instructionToBlockIndex[execInstructionIndex];
        }

        destinationInstruction->functionIndex = entry->flatIndex;
        destinationInstruction->blockIndex = blockIndex;
        destinationInstruction->semIrOpcode = sourceInstruction->opcode;
        destinationInstruction->execInstructionIndex = sourceInstruction->execInstructionIndex;
        destinationInstruction->typeTableIndex = sourceInstruction->typeTableIndex;
        destinationInstruction->effectTableIndex = sourceInstruction->effectTableIndex;
        destinationInstruction->destinationSlot = sourceInstruction->destinationSlot;
        destinationInstruction->operand0 = sourceInstruction->operand0;
        destinationInstruction->operand1 = sourceInstruction->operand1;
        destinationInstruction->deoptId = sourceInstruction->deoptId;
        destinationInstruction->debugLine =
                backend_aot_exec_ir_debug_line_for_instruction(entry->function, execInstructionIndex);
        destinationInstruction->callsiteKind =
                backend_aot_exec_ir_callsite_kind_for_instruction(entry->function, execInstructionIndex, execOpcode);

        outFunction->runtimeContracts |= backend_aot_exec_ir_runtime_contracts_for_opcode(sourceInstruction->opcode);

        if (blockIndex != ZR_AOT_EXEC_IR_INDEX_NONE && blockIndex < outFunction->basicBlockCount) {
            SZrAotExecIrBasicBlock *block = &outFunction->basicBlocks[blockIndex];
            if (block->firstInstructionOffset == ZR_AOT_EXEC_IR_INDEX_NONE) {
                block->firstInstructionOffset = *ioInstructionOffset + localInstructionIndex;
            }
            block->semIrInstructionCount++;
        }
    }

    for (TZrUInt32 blockIndex = 0; blockIndex < outFunction->basicBlockCount; blockIndex++) {
        SZrAotExecIrBasicBlock *block = &outFunction->basicBlocks[blockIndex];
        if (block->firstInstructionOffset == ZR_AOT_EXEC_IR_INDEX_NONE) {
            block->firstInstructionOffset = outFunction->firstInstructionOffset;
        }
    }

    *ioInstructionOffset += entry->function->semIrInstructionLength;

    if (instructionToBlockIndex != ZR_NULL && entry->function->instructionsLength > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      instructionToBlockIndex,
                                      sizeof(*instructionToBlockIndex) * entry->function->instructionsLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_TRUE;
}

TZrBool backend_aot_exec_ir_build_module(SZrState *state, SZrFunction *function, SZrAotExecIrModule *outModule) {
    SZrAotFunctionTable functionTable = {0};
    TZrUInt32 totalInstructionCount = 0;
    TZrUInt32 instructionOffset = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outModule == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outModule, 0, sizeof(*outModule));

    if (!backend_aot_build_function_table(state, function, &functionTable)) {
        return ZR_FALSE;
    }

    outModule->functionCount = functionTable.count;
    if (functionTable.count > 0) {
        outModule->functions = (SZrAotExecIrFunction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(*outModule->functions) * functionTable.count,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (outModule->functions == ZR_NULL) {
            backend_aot_release_function_table(state, &functionTable);
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(outModule->functions, 0, sizeof(*outModule->functions) * functionTable.count);
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        if (functionTable.entries[functionIndex].function != ZR_NULL) {
            totalInstructionCount += functionTable.entries[functionIndex].function->semIrInstructionLength;
        }
    }

    outModule->instructionCount = totalInstructionCount;
    if (totalInstructionCount > 0) {
        outModule->instructions = (SZrAotExecIrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(*outModule->instructions) * totalInstructionCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (outModule->instructions == ZR_NULL) {
            backend_aot_exec_ir_release_module(state, outModule);
            backend_aot_release_function_table(state, &functionTable);
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(outModule->instructions, 0, sizeof(*outModule->instructions) * totalInstructionCount);
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        if (!backend_aot_exec_ir_build_function(state,
                                                &functionTable,
                                                &functionTable.entries[functionIndex],
                                                outModule->instructions,
                                                &instructionOffset,
                                                &outModule->functions[functionIndex])) {
            backend_aot_exec_ir_release_module(state, outModule);
            backend_aot_release_function_table(state, &functionTable);
            return ZR_FALSE;
        }
        outModule->runtimeContracts |= outModule->functions[functionIndex].runtimeContracts;
    }

    backend_aot_release_function_table(state, &functionTable);
    return ZR_TRUE;
}

void backend_aot_exec_ir_release_module(SZrState *state, SZrAotExecIrModule *module) {
    if (state == ZR_NULL || state->global == ZR_NULL || module == ZR_NULL) {
        return;
    }

    if (module->functions != ZR_NULL) {
        for (TZrUInt32 functionIndex = 0; functionIndex < module->functionCount; functionIndex++) {
            if (module->functions[functionIndex].basicBlocks != ZR_NULL &&
                module->functions[functionIndex].basicBlockCount > 0) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              module->functions[functionIndex].basicBlocks,
                                              sizeof(*module->functions[functionIndex].basicBlocks) *
                                                      module->functions[functionIndex].basicBlockCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }

        ZrCore_Memory_RawFreeWithType(state->global,
                                      module->functions,
                                      sizeof(*module->functions) * module->functionCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (module->instructions != ZR_NULL && module->instructionCount > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      module->instructions,
                                      sizeof(*module->instructions) * module->instructionCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    ZrCore_Memory_RawSet(module, 0, sizeof(*module));
}

const SZrAotExecIrFunction *backend_aot_exec_ir_find_function(const SZrAotExecIrModule *module, TZrUInt32 functionIndex) {
    if (module == ZR_NULL || module->functions == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < module->functionCount; index++) {
        if (module->functions[index].flatIndex == functionIndex) {
            return &module->functions[index];
        }
    }

    return ZR_NULL;
}
