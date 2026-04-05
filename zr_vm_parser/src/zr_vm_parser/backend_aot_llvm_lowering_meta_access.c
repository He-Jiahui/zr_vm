#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_triple_slot_meta_call(const SZrAotLlvmLoweringContext *context,
                                                            const SZrAotLlvmInstructionContext *instruction,
                                                            const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
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

static const TZrChar *backend_aot_llvm_meta_access_helper_name(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(META_GET):
            return "ZrLibrary_AotRuntime_MetaGet";
        case ZR_INSTRUCTION_ENUM(META_SET):
            return "ZrLibrary_AotRuntime_MetaSet";
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            return "ZrLibrary_AotRuntime_MetaGetCached";
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            return "ZrLibrary_AotRuntime_MetaSetCached";
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
            return "ZrLibrary_AotRuntime_MetaGetStaticCached";
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            return "ZrLibrary_AotRuntime_MetaSetStaticCached";
        default:
            return ZR_NULL;
    }
}

TZrBool backend_aot_llvm_lower_meta_access_value_family(const SZrAotLlvmLoweringContext *context,
                                                        const SZrAotLlvmInstructionContext *instruction) {
    const TZrChar *helperName;

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    helperName = backend_aot_llvm_meta_access_helper_name(instruction->opcode);
    if (helperName == ZR_NULL) {
        return ZR_FALSE;
    }

    return backend_aot_llvm_lower_triple_slot_meta_call(context, instruction, helperName);
}
