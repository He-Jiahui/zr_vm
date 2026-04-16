#include "backend_aot_llvm_text_emit.h"

TZrUInt32 backend_aot_llvm_next_temp(TZrUInt32 *tempCounter) {
    TZrUInt32 current = 0;

    if (tempCounter != ZR_NULL) {
        current = *tempCounter;
        (*tempCounter)++;
    }

    return current;
}

void backend_aot_llvm_make_function_label(TZrChar *buffer,
                                          TZrSize bufferSize,
                                          TZrUInt32 functionIndex,
                                          const TZrChar *suffix) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    if (suffix == ZR_NULL || suffix[0] == '\0') {
        snprintf(buffer, bufferSize, "zr_aot_fn_%u", (unsigned)functionIndex);
    } else {
        snprintf(buffer, bufferSize, "zr_aot_fn_%u_%s", (unsigned)functionIndex, suffix);
    }
}

void backend_aot_llvm_make_instruction_label(TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrUInt32 functionIndex,
                                             TZrUInt32 instructionIndex,
                                             const TZrChar *suffix) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    if (suffix == ZR_NULL || suffix[0] == '\0') {
        snprintf(buffer, bufferSize, "zr_aot_fn_%u_ins_%u", (unsigned)functionIndex, (unsigned)instructionIndex);
    } else {
        snprintf(buffer,
                 bufferSize,
                 "zr_aot_fn_%u_ins_%u_%s",
                 (unsigned)functionIndex,
                 (unsigned)instructionIndex,
                 suffix);
    }
}
