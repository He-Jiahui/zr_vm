#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_pair_slot_ownership_call(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction,
                                                               const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static const TZrChar *backend_aot_llvm_ownership_helper_name(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
            return "ZrLibrary_AotRuntime_OwnUnique";
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
        case ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED):
            return "ZrLibrary_AotRuntime_OwnBorrow";
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
        case ZR_INSTRUCTION_ENUM(OWN_VIEW_MUT):
            return "ZrLibrary_AotRuntime_OwnLoan";
        case ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN):
            return "ZrLibrary_AotRuntime_OwnReturnLoan";
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            return "ZrLibrary_AotRuntime_OwnShare";
        case ZR_INSTRUCTION_ENUM(OWN_DEGRADE):
            return "ZrLibrary_AotRuntime_OwnDegrade";
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
            return "ZrLibrary_AotRuntime_OwnDetach";
        case ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX):
            return "ZrLibrary_AotRuntime_OwnIntoGcBox";
        case ZR_INSTRUCTION_ENUM(OWN_RETURN_TO_GC):
            return "ZrLibrary_AotRuntime_OwnReturnToGc";
        case ZR_INSTRUCTION_ENUM(OWN_WAKE):
            return "ZrLibrary_AotRuntime_OwnWake";
        case ZR_INSTRUCTION_ENUM(OWN_DROP):
            return "ZrLibrary_AotRuntime_OwnDrop";
        default:
            return ZR_NULL;
    }
}

TZrBool backend_aot_llvm_lower_ownership_value_family(const SZrAotLlvmLoweringContext *context,
                                                      const SZrAotLlvmInstructionContext *instruction) {
    const TZrChar *helperName;

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    helperName = backend_aot_llvm_ownership_helper_name(instruction->opcode);
    if (helperName == ZR_NULL) {
        return ZR_FALSE;
    }

    return backend_aot_llvm_lower_pair_slot_ownership_call(context, instruction, helperName);
}
