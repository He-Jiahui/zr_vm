#include "backend_aot_llvm_text_terminal.h"

void backend_aot_llvm_write_report_unsupported_return(FILE *file,
                                                      TZrUInt32 *tempCounter,
                                                      TZrUInt32 functionIndex,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 opcode) {
    TZrUInt32 tempValue;

    if (file == ZR_NULL) {
        return;
    }

    tempValue = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file,
            "  %%t%u = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %%state, i32 %u, i32 %u, i32 %u)\n",
            (unsigned)tempValue,
            (unsigned)functionIndex,
            (unsigned)instructionIndex,
            (unsigned)opcode);
    fprintf(file, "  ret i64 %%t%u\n", (unsigned)tempValue);
}

void backend_aot_llvm_write_report_unsupported_value_return(FILE *file,
                                                            TZrUInt32 *tempCounter,
                                                            TZrUInt32 functionIndex,
                                                            const TZrChar *instructionValueText,
                                                            TZrUInt32 opcode) {
    TZrUInt32 tempValue;

    if (file == ZR_NULL || instructionValueText == ZR_NULL) {
        return;
    }

    tempValue = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file,
            "  %%t%u = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %%state, i32 %u, i32 %s, i32 %u)\n",
            (unsigned)tempValue,
            (unsigned)functionIndex,
            instructionValueText,
            (unsigned)opcode);
    fprintf(file, "  ret i64 %%t%u\n", (unsigned)tempValue);
}

void backend_aot_llvm_write_return_call(FILE *file,
                                        TZrUInt32 *tempCounter,
                                        TZrUInt32 sourceSlot,
                                        TZrBool publishExports) {
    TZrUInt32 tempValue;

    if (file == ZR_NULL) {
        return;
    }

    tempValue = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file,
            "  %%t%u = call i64 @ZrLibrary_AotRuntime_Return(ptr %%state, ptr %%frame, i32 %u, i1 %s)\n",
            (unsigned)tempValue,
            (unsigned)sourceSlot,
            publishExports ? "true" : "false");
    fprintf(file, "  ret i64 %%t%u\n", (unsigned)tempValue);
}
