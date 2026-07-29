#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_triple_slot_member_call(const SZrAotLlvmLoweringContext *context,
                                                              const SZrAotLlvmInstructionContext *instruction,
                                                              const TZrChar *helperName,
                                                              TZrBool clearCallableProvenance) {
    TZrChar argsBuffer[256];

    if (clearCallableProvenance) {
        backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                     context->entry->function,
                                                     instruction->destinationSlot,
                                                     ZR_AOT_INVALID_FUNCTION_INDEX);
    }

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_pair_slot_property_reference_call(
        const SZrAotLlvmLoweringContext *context,
        const SZrAotLlvmInstructionContext *instruction,
        const TZrChar *helperName,
        TZrUInt32 firstSlot,
        TZrUInt32 secondSlot,
        TZrBool clearCallableProvenance) {
    TZrChar argsBuffer[256];

    if (clearCallableProvenance) {
        backend_aot_set_callable_slot_function_index(
                context->callableSlotFunctionIndices,
                context->entry->function,
                firstSlot,
                ZR_AOT_INVALID_FUNCTION_INDEX);
    }
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)firstSlot,
             (unsigned)secondSlot);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_member_value_family(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_MEMBER):
            return backend_aot_llvm_lower_triple_slot_member_call(
                    context,
                    instruction,
                    "ZrLibrary_AotRuntime_PropertyReferenceCreateMember",
                    ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_INDEX):
            return backend_aot_llvm_lower_triple_slot_member_call(
                    context,
                    instruction,
                    "ZrLibrary_AotRuntime_PropertyReferenceCreateIndex",
                    ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_LOCAL):
            return backend_aot_llvm_lower_pair_slot_property_reference_call(
                    context,
                    instruction,
                    "ZrLibrary_AotRuntime_PropertyReferenceCreateLocal",
                    instruction->destinationSlot,
                    (TZrUInt32)instruction->operandA2,
                    ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_LOAD):
            return backend_aot_llvm_lower_pair_slot_property_reference_call(
                    context,
                    instruction,
                    "ZrLibrary_AotRuntime_PropertyReferenceLoad",
                    instruction->destinationSlot,
                    (TZrUInt32)instruction->operandA2,
                    ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_STORE):
            return backend_aot_llvm_lower_pair_slot_property_reference_call(
                    context,
                    instruction,
                    "ZrLibrary_AotRuntime_PropertyReferenceStore",
                    instruction->destinationSlot,
                    instruction->operandA1,
                    ZR_FALSE);
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            return backend_aot_llvm_lower_triple_slot_member_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_GetMember",
                                                                  ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            return backend_aot_llvm_lower_triple_slot_member_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_GetMemberSlot",
                                                                  ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            return backend_aot_llvm_lower_triple_slot_member_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_SetMember",
                                                                  ZR_FALSE);
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
            return backend_aot_llvm_lower_triple_slot_member_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_SetMemberSlot",
                                                                  ZR_FALSE);
        default:
            return ZR_FALSE;
    }
}
