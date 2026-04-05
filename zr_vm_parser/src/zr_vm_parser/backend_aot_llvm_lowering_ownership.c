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
        case ZR_INSTRUCTION_ENUM(OWN_USING):
            return "ZrLibrary_AotRuntime_OwnUsing";
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            return "ZrLibrary_AotRuntime_OwnShare";
        case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            return "ZrLibrary_AotRuntime_OwnWeak";
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            return "ZrLibrary_AotRuntime_OwnUpgrade";
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            return "ZrLibrary_AotRuntime_OwnRelease";
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
