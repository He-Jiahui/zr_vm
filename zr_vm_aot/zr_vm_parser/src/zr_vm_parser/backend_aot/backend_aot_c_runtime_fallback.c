#include "backend_aot_c_runtime_fallback.h"

#include "backend_aot_internal.h"

#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/string.h"

static const SZrAotExecIrInstruction *backend_aot_c_find_exec_ir_instruction(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 execInstructionIndex) {
    TZrUInt32 index;

    if (module == ZR_NULL || module->instructions == ZR_NULL || functionIr == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < functionIr->instructionCount; index++) {
        const SZrAotExecIrInstruction *instruction =
                &module->instructions[functionIr->firstInstructionOffset + index];
        if (instruction->execInstructionIndex == execInstructionIndex) {
            return instruction;
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_c_semir_instruction_is_dynamic_call(const SZrAotExecIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->semIrOpcode == ZR_SEMIR_OPCODE_DYN_CALL ||
                     instruction->semIrOpcode == ZR_SEMIR_OPCODE_DYN_TAIL_CALL);
}

static TZrBool backend_aot_c_semir_instruction_is_dynamic_value_access(const SZrAotExecIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->semIrOpcode) {
        case ZR_SEMIR_OPCODE_META_GET:
        case ZR_SEMIR_OPCODE_META_SET:
        case ZR_SEMIR_OPCODE_DYN_INDEX_GET:
        case ZR_SEMIR_OPCODE_DYN_INDEX_SET:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_semir_instruction_is_dynamic_iterator(const SZrAotExecIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->semIrOpcode == ZR_SEMIR_OPCODE_DYN_ITER_INIT ||
                     instruction->semIrOpcode == ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT);
}

static TZrBool backend_aot_c_semir_instruction_is_reflection_runtime_contract(
        const SZrAotExecIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->semIrOpcode == ZR_SEMIR_OPCODE_TYPEOF);
}

static TZrBool backend_aot_c_instruction_is_super_dynamic_call(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_instruction_is_call_candidate(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_instruction_is_explicit_dynamic_call(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||
                     instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL));
}

static TZrBool backend_aot_c_instruction_is_explicit_dynamic_iterator(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_instruction_is_reflection_runtime_contract(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(TYPEOF));
}

typedef enum EZrAotRuntimeFallbackReason {
    ZR_AOT_RUNTIME_FALLBACK_REASON_NONE = 0,
    ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_CALL = 1,
    ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_VALUE_ACCESS = 2,
    ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_ITERATOR = 3,
    ZR_AOT_RUNTIME_FALLBACK_REASON_REFLECTION = 4
} EZrAotRuntimeFallbackReason;

static const TZrChar *backend_aot_c_runtime_fallback_reason_text(EZrAotRuntimeFallbackReason reason) {
    switch (reason) {
        case ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_CALL:
            return "dynamic-call";
        case ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_VALUE_ACCESS:
            return "dynamic-value-access";
        case ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_ITERATOR:
            return "dynamic-iterator";
        case ZR_AOT_RUNTIME_FALLBACK_REASON_REFLECTION:
            return "reflection";
        case ZR_AOT_RUNTIME_FALLBACK_REASON_NONE:
        default:
            return "none";
    }
}

static TZrUInt32 backend_aot_c_runtime_fallback_warning_flag_for_reason(EZrAotRuntimeFallbackReason reason) {
    switch (reason) {
        case ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_CALL:
            return ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_CALL;
        case ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_VALUE_ACCESS:
            return ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_VALUE_ACCESS;
        case ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_ITERATOR:
            return ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_ITERATOR;
        case ZR_AOT_RUNTIME_FALLBACK_REASON_REFLECTION:
            return ZR_AOT_RUNTIME_FALLBACK_WARNING_REFLECTION;
        case ZR_AOT_RUNTIME_FALLBACK_REASON_NONE:
        default:
            return ZR_AOT_RUNTIME_FALLBACK_WARNING_NONE;
    }
}

static TZrBool backend_aot_c_runtime_fallback_reason_is_suppressed(EZrAotRuntimeFallbackReason reason,
                                                                   TZrUInt32 suppressedReasonMask) {
    TZrUInt32 reasonFlag = backend_aot_c_runtime_fallback_warning_flag_for_reason(reason);
    return (TZrBool)(reasonFlag != ZR_AOT_RUNTIME_FALLBACK_WARNING_NONE &&
                     (suppressedReasonMask & reasonFlag) != 0u);
}

static TZrUInt32 backend_aot_c_runtime_fallback_source_line(const SZrAotExecIrModule *module,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            TZrUInt32 instructionIndex) {
    const SZrAotExecIrInstruction *execIrInstruction =
            backend_aot_c_find_exec_ir_instruction(module, functionIr, instructionIndex);

    return execIrInstruction != ZR_NULL ? execIrInstruction->debugLine : 0u;
}

static TZrUInt32 backend_aot_c_runtime_fallback_source_line_end(const SZrAotExecIrModule *module,
                                                                const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 instructionIndex,
                                                                TZrUInt32 sourceLine) {
    const SZrAotExecIrInstruction *execIrInstruction =
            backend_aot_c_find_exec_ir_instruction(module, functionIr, instructionIndex);
    TZrUInt32 sourceLineEnd = execIrInstruction != ZR_NULL ? execIrInstruction->debugLineEnd : 0u;

    if (sourceLineEnd == 0u) {
        sourceLineEnd = sourceLine;
    }
    if (sourceLine > 0u && sourceLineEnd < sourceLine) {
        sourceLineEnd = sourceLine;
    }

    return sourceLineEnd;
}

static TZrUInt32 backend_aot_c_runtime_fallback_source_column(const SZrAotExecIrModule *module,
                                                              const SZrAotExecIrFunction *functionIr,
                                                              TZrUInt32 instructionIndex) {
    const SZrAotExecIrInstruction *execIrInstruction =
            backend_aot_c_find_exec_ir_instruction(module, functionIr, instructionIndex);

    return execIrInstruction != ZR_NULL ? execIrInstruction->debugColumn : 0u;
}

static TZrUInt32 backend_aot_c_runtime_fallback_source_column_end(const SZrAotExecIrModule *module,
                                                                  const SZrAotExecIrFunction *functionIr,
                                                                  TZrUInt32 instructionIndex,
                                                                  TZrUInt32 sourceColumn) {
    const SZrAotExecIrInstruction *execIrInstruction =
            backend_aot_c_find_exec_ir_instruction(module, functionIr, instructionIndex);
    TZrUInt32 sourceColumnEnd = execIrInstruction != ZR_NULL ? execIrInstruction->debugColumnEnd : 0u;

    if (sourceColumnEnd == 0u) {
        sourceColumnEnd = sourceColumn;
    }
    if (sourceColumn > 0u && sourceColumnEnd < sourceColumn) {
        sourceColumnEnd = sourceColumn;
    }

    return sourceColumnEnd;
}

static const TZrChar *backend_aot_c_runtime_fallback_source_file(const SZrAotFunctionEntry *entry) {
    const TZrChar *sourceFile;

    if (entry == ZR_NULL || entry->function == ZR_NULL || entry->function->sourceCodeList == ZR_NULL) {
        return "<unknown>";
    }

    sourceFile = ZrCore_String_GetNativeString(entry->function->sourceCodeList);
    return sourceFile != ZR_NULL && sourceFile[0] != '\0' ? sourceFile : "<unknown>";
}

static void backend_aot_c_write_trim_warning_quoted_text(FILE *file, const TZrChar *text) {
    const unsigned char *cursor;

    fputc('"', file);
    if (text != ZR_NULL) {
        for (cursor = (const unsigned char *)text; *cursor != '\0'; cursor++) {
            switch (*cursor) {
                case '\\':
                    fputs("\\\\", file);
                    break;
                case '"':
                    fputs("\\\"", file);
                    break;
                case '\n':
                    fputs("\\n", file);
                    break;
                case '\r':
                    fputs("\\r", file);
                    break;
                case '\t':
                    fputs("\\t", file);
                    break;
                default:
                    if (*cursor < 0x20u || *cursor == 0x7Fu) {
                        fprintf(file, "\\x%02X", (unsigned)*cursor);
                    } else {
                        fputc((int)*cursor, file);
                    }
                    break;
            }
        }
    }
    fputc('"', file);
}

static EZrAotRuntimeFallbackReason backend_aot_c_runtime_fallback_reason_for_instruction(
        SZrState *state,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionEntry *entry,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex) {
    const TZrInstruction *instruction;
    const SZrAotExecIrInstruction *execIrInstruction;
    TZrUInt32 calleeFunctionIndex;

    if (state == ZR_NULL || functionTable == ZR_NULL || entry == ZR_NULL || entry->function == ZR_NULL ||
        instructionIndex >= entry->function->instructionsLength) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_NONE;
    }

    instruction = &entry->function->instructionsList[instructionIndex];
    if (backend_aot_c_instruction_is_super_dynamic_call(instruction)) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_CALL;
    }
    execIrInstruction = backend_aot_c_find_exec_ir_instruction(module, functionIr, instructionIndex);
    if (backend_aot_c_semir_instruction_is_dynamic_value_access(execIrInstruction)) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_VALUE_ACCESS;
    }
    if (backend_aot_c_instruction_is_explicit_dynamic_iterator(instruction) ||
        backend_aot_c_semir_instruction_is_dynamic_iterator(execIrInstruction)) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_ITERATOR;
    }
    if (backend_aot_c_instruction_is_reflection_runtime_contract(instruction) ||
        backend_aot_c_semir_instruction_is_reflection_runtime_contract(execIrInstruction)) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_REFLECTION;
    }
    if (!backend_aot_c_instruction_is_call_candidate(instruction)) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_NONE;
    }

    if (!backend_aot_c_instruction_is_explicit_dynamic_call(instruction) &&
        !backend_aot_c_semir_instruction_is_dynamic_call(execIrInstruction)) {
        return ZR_AOT_RUNTIME_FALLBACK_REASON_NONE;
    }

    calleeFunctionIndex =
            backend_aot_resolve_callable_slot_function_index_before_instruction(functionTable,
                                                                                state,
                                                                                entry->function,
                                                                                instructionIndex,
                                                                                instruction->instruction.operand.operand1[0],
                                                                                0);
    return calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX
                   ? ZR_AOT_RUNTIME_FALLBACK_REASON_DYNAMIC_CALL
                   : ZR_AOT_RUNTIME_FALLBACK_REASON_NONE;
}

static TZrBool backend_aot_c_full_aot_instruction_requires_runtime_fallback(
        SZrState *state,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionEntry *entry,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex) {
    return (TZrBool)(backend_aot_c_runtime_fallback_reason_for_instruction(state,
                                                                           functionTable,
                                                                           module,
                                                                           entry,
                                                                           functionIr,
                                                                           instructionIndex) !=
                     ZR_AOT_RUNTIME_FALLBACK_REASON_NONE);
}

TZrBool backend_aot_c_validate_full_aot_runtime_closure(SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        const SZrAotExecIrModule *module) {
    TZrUInt32 functionIndex;

    if (state == ZR_NULL || functionTable == ZR_NULL || module == ZR_NULL) {
        return ZR_FALSE;
    }

    for (functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[functionIndex];
        const SZrAotExecIrFunction *functionIr;
        TZrUInt32 instructionIndex;

        if (entry->function == ZR_NULL) {
            return ZR_FALSE;
        }
        functionIr = backend_aot_exec_ir_find_function(module, entry->flatIndex);
        for (instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
            if (backend_aot_c_full_aot_instruction_requires_runtime_fallback(state,
                                                                             functionTable,
                                                                             module,
                                                                             entry,
                                                                             functionIr,
                                                                             instructionIndex)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrUInt32 backend_aot_c_count_runtime_fallback_warnings(SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        const SZrAotExecIrModule *module,
                                                        TZrUInt32 suppressedReasonMask) {
    TZrUInt32 warningCount = 0u;

    if (state == ZR_NULL || functionTable == ZR_NULL || module == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[functionIndex];
        const SZrAotExecIrFunction *functionIr;

        if (entry->function == ZR_NULL) {
            continue;
        }

        functionIr = backend_aot_exec_ir_find_function(module, entry->flatIndex);
        for (TZrUInt32 instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
            EZrAotRuntimeFallbackReason reason =
                    backend_aot_c_runtime_fallback_reason_for_instruction(state,
                                                                          functionTable,
                                                                          module,
                                                                          entry,
                                                                          functionIr,
                                                                          instructionIndex);
            if (reason != ZR_AOT_RUNTIME_FALLBACK_REASON_NONE &&
                !backend_aot_c_runtime_fallback_reason_is_suppressed(reason, suppressedReasonMask)) {
                warningCount++;
            }
        }
    }

    return warningCount;
}

TZrUInt32 backend_aot_c_count_suppressed_runtime_fallback_warnings(SZrState *state,
                                                                   const SZrAotFunctionTable *functionTable,
                                                                   const SZrAotExecIrModule *module,
                                                                   TZrUInt32 suppressedReasonMask) {
    TZrUInt32 warningCount = 0u;

    if (state == ZR_NULL || functionTable == ZR_NULL || module == ZR_NULL ||
        suppressedReasonMask == ZR_AOT_RUNTIME_FALLBACK_WARNING_NONE) {
        return 0u;
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[functionIndex];
        const SZrAotExecIrFunction *functionIr;

        if (entry->function == ZR_NULL) {
            continue;
        }

        functionIr = backend_aot_exec_ir_find_function(module, entry->flatIndex);
        for (TZrUInt32 instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
            EZrAotRuntimeFallbackReason reason =
                    backend_aot_c_runtime_fallback_reason_for_instruction(state,
                                                                          functionTable,
                                                                          module,
                                                                          entry,
                                                                          functionIr,
                                                                          instructionIndex);
            if (backend_aot_c_runtime_fallback_reason_is_suppressed(reason, suppressedReasonMask)) {
                warningCount++;
            }
        }
    }

    return warningCount;
}

void backend_aot_write_c_trim_warnings(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotExecIrModule *module,
                                       TZrUInt32 suppressedReasonMask) {
    TZrUInt32 warningIndex = 0u;

    if (file == ZR_NULL || state == ZR_NULL || functionTable == ZR_NULL || module == ZR_NULL) {
        return;
    }

    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable->count; functionIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[functionIndex];
        const SZrAotExecIrFunction *functionIr;

        if (entry->function == ZR_NULL) {
            continue;
        }

        functionIr = backend_aot_exec_ir_find_function(module, entry->flatIndex);
        for (TZrUInt32 instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
            EZrAotRuntimeFallbackReason reason =
                    backend_aot_c_runtime_fallback_reason_for_instruction(state,
                                                                          functionTable,
                                                                          module,
                                                                          entry,
                                                                          functionIr,
                                                                          instructionIndex);
            if (reason != ZR_AOT_RUNTIME_FALLBACK_REASON_NONE) {
                TZrUInt32 sourceLine =
                        backend_aot_c_runtime_fallback_source_line(module, functionIr, instructionIndex);
                TZrUInt32 sourceLineEnd =
                        backend_aot_c_runtime_fallback_source_line_end(module,
                                                                       functionIr,
                                                                       instructionIndex,
                                                                       sourceLine);
                TZrUInt32 sourceColumn =
                        backend_aot_c_runtime_fallback_source_column(module, functionIr, instructionIndex);
                TZrUInt32 sourceColumnEnd =
                        backend_aot_c_runtime_fallback_source_column_end(module,
                                                                         functionIr,
                                                                         instructionIndex,
                                                                         sourceColumn);
                TZrUInt32 reasonFlag = backend_aot_c_runtime_fallback_warning_flag_for_reason(reason);
                const TZrChar *sourceFile = backend_aot_c_runtime_fallback_source_file(entry);
                if (backend_aot_c_runtime_fallback_reason_is_suppressed(reason, suppressedReasonMask)) {
                    continue;
                }
                fprintf(file,
                        "/* trim_warning.runtimeFallback[%u] function=%u instruction=%u sourceFile=",
                        (unsigned)warningIndex,
                        (unsigned)entry->flatIndex,
                        (unsigned)instructionIndex);
                backend_aot_c_write_trim_warning_quoted_text(file, sourceFile);
                fprintf(file,
                        " sourceLine=%u sourceLineEnd=%u sourceColumn=%u sourceColumnEnd=%u reasonFlag=%u reason=%s */\n",
                        (unsigned)sourceLine,
                        (unsigned)sourceLineEnd,
                        (unsigned)sourceColumn,
                        (unsigned)sourceColumnEnd,
                        (unsigned)reasonFlag,
                        backend_aot_c_runtime_fallback_reason_text(reason));
                warningIndex++;
            }
        }
    }
}
