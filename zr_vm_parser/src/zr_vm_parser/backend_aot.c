//
// Shared AOT lowering helpers used by backend-specific emitters.
//

#include "zr_vm_parser/writer.h"

#include "backend_aot_exec_ir.h"
#include "backend_aot_internal.h"

#include <stdio.h>

#include "zr_vm_core/log.h"

void backend_aot_write_instruction_listing(FILE *file,
                                           const TZrChar *prefix,
                                           const SZrAotExecIrModule *module) {
    TZrUInt32 index;

    if (file == ZR_NULL || module == ZR_NULL) {
        return;
    }

    for (index = 0; index < module->instructionCount; index++) {
        const SZrAotExecIrInstruction *instruction = &module->instructions[index];
        fprintf(file,
                "%s[%u] %s exec=%u type=%u effect=%u dst=%u op0=%u op1=%u deopt=%u\n",
                prefix,
                index,
                backend_aot_exec_ir_semir_opcode_name(instruction->semIrOpcode),
                instruction->execInstructionIndex,
                instruction->typeTableIndex,
                instruction->effectTableIndex,
                instruction->destinationSlot,
                instruction->operand0,
                instruction->operand1,
                instruction->deoptId);
    }
}

const TZrChar *backend_aot_option_text(const SZrAotWriterOptions *options,
                                       const TZrChar *candidate,
                                       const TZrChar *fallback) {
    ZR_UNUSED_PARAMETER(options);
    return (candidate != ZR_NULL && candidate[0] != '\0') ? candidate : fallback;
}

static TZrBool backend_aot_c_instruction_supported(const TZrInstruction *instruction) {
    TZrUInt16 opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(NOP):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(POW):
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
        case ZR_INSTRUCTION_ENUM(OWN_WEAK):
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
        case ZR_INSTRUCTION_ENUM(TRY):
        case ZR_INSTRUCTION_ENUM(END_TRY):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(CATCH):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

TZrBool backend_aot_function_is_executable_subset(const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        if (!backend_aot_c_instruction_supported(&function->instructionsList[instructionIndex])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static const TZrChar *backend_aot_function_display_name(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "__anonymous__";
    }

    return ZrCore_String_GetNativeString(function->functionName);
}

TZrBool backend_aot_report_first_unsupported_instruction(const TZrChar *backendName,
                                                         const TZrChar *moduleName,
                                                         const SZrAotFunctionTable *table) {
    TZrUInt32 functionIndex;

    if (table == ZR_NULL) {
        return ZR_FALSE;
    }

    for (functionIndex = 0; functionIndex < table->count; functionIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[functionIndex];
        TZrUInt32 instructionIndex;

        if (entry->function == ZR_NULL) {
            continue;
        }

        for (instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
            const TZrInstruction *instruction = &entry->function->instructionsList[instructionIndex];
            if (backend_aot_c_instruction_supported(instruction)) {
                continue;
            }

            ZrCore_Log_Diagnosticf(ZR_NULL,
                                   ZR_LOG_LEVEL_ERROR,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "%s lowering unsupported: module='%s' function='%s' functionIndex=%u instructionIndex=%u opcode=%u\n",
                                   backendName != ZR_NULL ? backendName : "aot",
                                   moduleName != ZR_NULL ? moduleName : "__entry__",
                                   backend_aot_function_display_name(entry->function),
                                   (unsigned)entry->flatIndex,
                                   (unsigned)instructionIndex,
                                   (unsigned)instruction->instruction.operationCode);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrUInt32 backend_aot_option_input_kind(const SZrAotWriterOptions *options) {
    return (options != ZR_NULL && options->inputKind != ZR_AOT_INPUT_KIND_NONE)
                   ? options->inputKind
                   : ZR_AOT_INPUT_KIND_SOURCE;
}

const TZrChar *backend_aot_option_input_hash(const SZrAotWriterOptions *options,
                                             const TZrChar *sourceHash,
                                             const TZrChar *zroHash) {
    if (options != ZR_NULL && options->inputHash != ZR_NULL && options->inputHash[0] != '\0') {
        return options->inputHash;
    }

    if (options != ZR_NULL && options->inputKind == ZR_AOT_INPUT_KIND_BINARY) {
        return zroHash;
    }

    return sourceHash;
}

TZrUInt32 backend_aot_c_step_flags_for_instruction(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(POW):
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(TRY):
            return ZR_AOT_EMITTER_STEP_FLAG_MAY_THROW;
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
            return ZR_AOT_EMITTER_STEP_FLAG_CONTROL_FLOW;
        case ZR_INSTRUCTION_ENUM(END_TRY):
        case ZR_INSTRUCTION_ENUM(CATCH):
            return ZR_AOT_EMITTER_STEP_FLAG_NONE;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(META_CALL):
            return ZR_AOT_EMITTER_STEP_FLAG_CALL | ZR_AOT_EMITTER_STEP_FLAG_MAY_THROW;
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            return ZR_AOT_EMITTER_STEP_FLAG_CALL | ZR_AOT_EMITTER_STEP_FLAG_MAY_THROW |
                   ZR_AOT_EMITTER_STEP_FLAG_RETURN;
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            return ZR_AOT_EMITTER_STEP_FLAG_RETURN;
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
            return ZR_AOT_EMITTER_STEP_FLAG_NONE;
        default:
            return ZR_AOT_EMITTER_STEP_FLAG_NONE;
    }
}

TZrUInt32 backend_aot_get_callsite_cache_argument_count(const SZrFunction *function,
                                                        TZrUInt32 cacheIndex,
                                                        EZrFunctionCallSiteCacheKind expectedKind) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;

    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        return 0;
    }

    cacheEntry = &function->callSiteCaches[cacheIndex];
    if ((EZrFunctionCallSiteCacheKind)cacheEntry->kind != expectedKind) {
        return 0;
    }

    return cacheEntry->argumentCount;
}
