#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_immediate_constant_instruction(const SZrAotLlvmLoweringContext *context,
                                                                     const SZrAotLlvmInstructionContext *instruction,
                                                                     const SZrTypeValue *constantValue) {
    TZrUInt32 destinationValueTemp;
    TZrChar bitsBuffer[64];

    if (context == ZR_NULL || instruction == ZR_NULL || constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    switch (constantValue->type) {
        case ZR_VALUE_TYPE_NULL:
            backend_aot_llvm_emit_fast_set_null(context->file, context->tempCounter, destinationValueTemp);
            break;
        case ZR_VALUE_TYPE_BOOL:
            snprintf(bitsBuffer,
                     sizeof(bitsBuffer),
                     "%u",
                     constantValue->value.nativeObject.nativeBool != 0 ? 1u : 0u);
            backend_aot_llvm_emit_fast_set_bits(context->file,
                                                context->tempCounter,
                                                destinationValueTemp,
                                                bitsBuffer,
                                                ZR_VALUE_TYPE_BOOL);
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            snprintf(bitsBuffer,
                     sizeof(bitsBuffer),
                     "%lld",
                     (long long)constantValue->value.nativeObject.nativeInt64);
            backend_aot_llvm_emit_fast_set_bits(context->file,
                                                context->tempCounter,
                                                destinationValueTemp,
                                                bitsBuffer,
                                                constantValue->type);
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            snprintf(bitsBuffer,
                     sizeof(bitsBuffer),
                     "%llu",
                     (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
            backend_aot_llvm_emit_fast_set_bits(context->file,
                                                context->tempCounter,
                                                destinationValueTemp,
                                                bitsBuffer,
                                                constantValue->type);
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            snprintf(bitsBuffer,
                     sizeof(bitsBuffer),
                     "%llu",
                     (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
            backend_aot_llvm_emit_fast_set_bits(context->file,
                                                context->tempCounter,
                                                destinationValueTemp,
                                                bitsBuffer,
                                                constantValue->type);
            break;
        default:
            return ZR_FALSE;
    }

    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_constant_instruction(const SZrAotLlvmLoweringContext *context,
                                                           const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 callableFlatIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    const SZrTypeValue *constantValue = ZR_NULL;
    TZrChar argsBuffer[256];

    backend_aot_resolve_callable_constant_function_index(context->functionTable,
                                                         context->state,
                                                         context->entry->function,
                                                         instruction->operandA2,
                                                         &callableFlatIndex);
    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 callableFlatIndex);
    if (instruction->operandA2 >= 0 &&
        (TZrUInt32)instruction->operandA2 < context->entry->function->constantValueLength) {
        constantValue = &context->entry->function->constantValueList[instruction->operandA2];
    }
    if (backend_aot_llvm_lower_immediate_constant_instruction(context, instruction, constantValue)) {
        return ZR_TRUE;
    }

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA2);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_CopyConstant",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_set_constant_instruction(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA2);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_SetConstant",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_constant_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return backend_aot_llvm_lower_constant_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            return backend_aot_llvm_lower_set_constant_instruction(context, instruction);
        default:
            return ZR_FALSE;
    }
}
