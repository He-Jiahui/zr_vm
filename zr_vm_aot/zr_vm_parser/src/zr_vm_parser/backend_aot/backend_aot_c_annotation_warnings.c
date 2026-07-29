#include "backend_aot_c_annotation_warnings.h"

#include "backend_aot_callable_provenance.h"
#include "backend_aot_exec_ir_source_location.h"
#include "backend_aot_internal.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static const SZrTypeValue *backend_aot_c_annotation_function_metadata_field(SZrState *state,
                                                                            const SZrFunction *function,
                                                                            const TZrChar *fieldName) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || function == ZR_NULL || fieldName == ZR_NULL ||
        !function->hasDecoratorMetadata ||
        function->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
        function->decoratorMetadataValue.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    metadataObject = ZR_CAST_OBJECT(state, function->decoratorMetadataValue.value.object);
    if (metadataObject == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    return ZrCore_Object_GetValue(state, metadataObject, &key);
}

static TZrBool backend_aot_c_annotation_function_metadata_bool_field_is_true(SZrState *state,
                                                                             const SZrFunction *function,
                                                                             const TZrChar *fieldName) {
    const SZrTypeValue *value =
            backend_aot_c_annotation_function_metadata_field(state, function, fieldName);

    return (TZrBool)(value != ZR_NULL &&
                     value->type == ZR_VALUE_TYPE_BOOL &&
                     value->value.nativeObject.nativeBool);
}

static const TZrChar *backend_aot_c_annotation_function_metadata_string_field(SZrState *state,
                                                                              const SZrFunction *function,
                                                                              const TZrChar *fieldName) {
    const SZrTypeValue *value =
            backend_aot_c_annotation_function_metadata_field(state, function, fieldName);
    SZrString *stringValue;

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    stringValue = ZR_CAST_STRING(state, value->value.object);
    return stringValue != ZR_NULL ? ZrCore_String_GetNativeString(stringValue) : ZR_NULL;
}

static TZrBool backend_aot_c_annotation_function_requires_unreferenced_code(SZrState *state,
                                                                            const SZrFunction *function) {
    return backend_aot_c_annotation_function_metadata_bool_field_is_true(state,
                                                                        function,
                                                                        "requiresUnreferencedCode");
}

static const TZrChar *backend_aot_c_annotation_function_requires_unreferenced_code_reason(
        SZrState *state,
        const SZrFunction *function) {
    return backend_aot_c_annotation_function_metadata_string_field(state,
                                                                  function,
                                                                  "requiresUnreferencedCodeReason");
}

static TZrBool backend_aot_c_annotation_function_suppresses_requires_unreferenced_code_warning(
        SZrState *state,
        const SZrFunction *function) {
    return backend_aot_c_annotation_function_metadata_bool_field_is_true(
            state,
            function,
            "suppressRequiresUnreferencedCodeWarning");
}

static const TZrChar *backend_aot_c_annotation_source_file(const SZrAotFunctionEntry *entry) {
    const TZrChar *sourceFile;

    if (entry == ZR_NULL || entry->function == ZR_NULL || entry->function->sourceCodeList == ZR_NULL) {
        return "<unknown>";
    }

    sourceFile = ZrCore_String_GetNativeString(entry->function->sourceCodeList);
    return sourceFile != ZR_NULL && sourceFile[0] != '\0' ? sourceFile : "<unknown>";
}

static void backend_aot_c_write_annotation_warning_quoted_text(FILE *file, const TZrChar *text) {
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

static const SZrAotFunctionEntry *backend_aot_c_annotation_find_entry_by_flat_index(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 flatIndex) {
    if (functionTable == ZR_NULL || functionTable->entries == ZR_NULL ||
        flatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_NULL;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < functionTable->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[entryIndex];
        if (entry->flatIndex == flatIndex) {
            return entry;
        }
    }
    return ZR_NULL;
}

static TZrBool backend_aot_c_annotation_instruction_is_call_candidate(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
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

static TZrBool backend_aot_c_annotation_instruction_requires_warning(
        SZrState *state,
        const SZrAotFunctionTable *functionTable,
        const SZrAotFunctionEntry *entry,
        TZrUInt32 instructionIndex,
        TZrUInt32 *outTargetFunctionIndex,
        const TZrChar **outMessage) {
    const TZrInstruction *instruction;
    TZrUInt32 targetFunctionIndex;
    const SZrAotFunctionEntry *targetEntry;

    if (outTargetFunctionIndex != ZR_NULL) {
        *outTargetFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    }
    if (outMessage != ZR_NULL) {
        *outMessage = ZR_NULL;
    }
    if (state == ZR_NULL || functionTable == ZR_NULL || entry == ZR_NULL ||
        entry->function == ZR_NULL || entry->function->instructionsList == ZR_NULL ||
        instructionIndex >= entry->function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &entry->function->instructionsList[instructionIndex];
    if (!backend_aot_c_annotation_instruction_is_call_candidate(instruction)) {
        return ZR_FALSE;
    }

    targetFunctionIndex =
            backend_aot_resolve_callable_slot_function_index_before_instruction(functionTable,
                                                                                state,
                                                                                entry->function,
                                                                                instructionIndex,
                                                                                instruction->instruction.operand.operand1[0],
                                                                                0u);
    targetEntry = backend_aot_c_annotation_find_entry_by_flat_index(functionTable, targetFunctionIndex);
    if (targetEntry == ZR_NULL ||
        !backend_aot_c_annotation_function_requires_unreferenced_code(state, targetEntry->function)) {
        return ZR_FALSE;
    }

    if (outTargetFunctionIndex != ZR_NULL) {
        *outTargetFunctionIndex = targetFunctionIndex;
    }
    if (outMessage != ZR_NULL) {
        *outMessage = backend_aot_c_annotation_function_requires_unreferenced_code_reason(state,
                                                                                          targetEntry->function);
    }
    return ZR_TRUE;
}

static TZrUInt32 backend_aot_c_scan_annotation_warnings(FILE *file,
                                                        SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        TZrBool writeWarnings,
                                                        TZrBool scanSuppressedWarnings) {
    TZrUInt32 warningCount = 0u;

    if (state == ZR_NULL || functionTable == ZR_NULL || functionTable->entries == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < functionTable->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[entryIndex];
        if (entry->function == ZR_NULL || entry->function->instructionsList == ZR_NULL) {
            continue;
        }
        for (TZrUInt32 instructionIndex = 0u;
             instructionIndex < entry->function->instructionsLength;
             instructionIndex++) {
            TZrUInt32 targetFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
            const TZrChar *message = ZR_NULL;
            TZrBool isSuppressed;
            if (!backend_aot_c_annotation_instruction_requires_warning(state,
                                                                       functionTable,
                                                                       entry,
                                                                       instructionIndex,
                                                                       &targetFunctionIndex,
                                                                       &message)) {
                continue;
            }
            isSuppressed = backend_aot_c_annotation_function_suppresses_requires_unreferenced_code_warning(
                    state,
                    entry->function);
            if (isSuppressed != scanSuppressedWarnings) {
                continue;
            }
            if (writeWarnings && file != ZR_NULL) {
                TZrUInt32 sourceLine =
                        backend_aot_exec_ir_debug_line_for_instruction(entry->function, instructionIndex);
                TZrUInt32 sourceLineEnd =
                        backend_aot_exec_ir_debug_line_end_for_instruction(entry->function,
                                                                           instructionIndex,
                                                                           sourceLine);
                TZrUInt32 sourceColumn =
                        backend_aot_exec_ir_debug_column_for_instruction(entry->function, instructionIndex);
                TZrUInt32 sourceColumnEnd =
                        backend_aot_exec_ir_debug_column_end_for_instruction(entry->function,
                                                                             instructionIndex,
                                                                             sourceColumn);
                const TZrChar *sourceFile = backend_aot_c_annotation_source_file(entry);
                fprintf(file,
                        "/* trim_warning.annotation[%u] function=%u instruction=%u targetFunction=%u sourceFile=",
                        (unsigned)warningCount,
                        (unsigned)entry->flatIndex,
                        (unsigned)instructionIndex,
                        (unsigned)targetFunctionIndex);
                backend_aot_c_write_annotation_warning_quoted_text(file, sourceFile);
                fprintf(file,
                        " sourceLine=%u sourceLineEnd=%u sourceColumn=%u sourceColumnEnd=%u reason=requires-unreferenced-code",
                        (unsigned)sourceLine,
                        (unsigned)sourceLineEnd,
                        (unsigned)sourceColumn,
                        (unsigned)sourceColumnEnd);
                if (message != ZR_NULL && message[0] != '\0') {
                    fputs(" message=", file);
                    backend_aot_c_write_annotation_warning_quoted_text(file, message);
                }
                fputs(" */\n", file);
            }
            warningCount++;
        }
    }

    return warningCount;
}

TZrUInt32 backend_aot_c_count_annotation_warnings(SZrState *state,
                                                  const SZrAotFunctionTable *functionTable) {
    return backend_aot_c_scan_annotation_warnings(ZR_NULL, state, functionTable, ZR_FALSE, ZR_FALSE);
}

TZrUInt32 backend_aot_c_count_suppressed_annotation_warnings(SZrState *state,
                                                             const SZrAotFunctionTable *functionTable) {
    return backend_aot_c_scan_annotation_warnings(ZR_NULL, state, functionTable, ZR_FALSE, ZR_TRUE);
}

void backend_aot_write_c_annotation_warnings(FILE *file,
                                             SZrState *state,
                                             const SZrAotFunctionTable *functionTable) {
    if (file == ZR_NULL) {
        return;
    }
    (void)backend_aot_c_scan_annotation_warnings(file, state, functionTable, ZR_TRUE, ZR_FALSE);
}
