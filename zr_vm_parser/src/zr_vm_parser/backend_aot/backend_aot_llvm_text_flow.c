#include "backend_aot_llvm_text_flow.h"

void backend_aot_llvm_write_begin_instruction(FILE *file,
                                              TZrUInt32 *tempCounter,
                                              TZrUInt32 instructionIndex,
                                              TZrUInt32 stepFlags,
                                              const TZrChar *bodyLabel,
                                              const TZrChar *failLabel) {
    TZrUInt32 tempValue;

    if (file == ZR_NULL || bodyLabel == ZR_NULL || failLabel == ZR_NULL) {
        return;
    }

    tempValue = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file,
            "  %%t%u = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %%state, ptr %%frame, i32 %u, i32 %u)\n",
            (unsigned)tempValue,
            (unsigned)instructionIndex,
            (unsigned)stepFlags);
    fprintf(file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)tempValue,
            bodyLabel,
            failLabel);
}

void backend_aot_llvm_write_resume_dispatch(FILE *file,
                                            TZrUInt32 *tempCounter,
                                            TZrUInt32 functionIndex,
                                            TZrUInt32 instructionIndex,
                                            TZrUInt32 instructionCount,
                                            const TZrChar *resumePointerName,
                                            const TZrChar *fallthroughLabel) {
    TZrUInt32 resumeTemp;
    TZrUInt32 fallthroughTemp;
    TZrUInt32 targetIndex;
    TZrChar dispatchLabel[96];
    TZrChar unsupportedLabel[96];
    TZrChar instructionLabel[96];
    TZrChar resumeValueText[32];

    if (file == ZR_NULL || tempCounter == ZR_NULL || resumePointerName == ZR_NULL || fallthroughLabel == ZR_NULL) {
        return;
    }

    backend_aot_llvm_make_instruction_label(dispatchLabel,
                                            sizeof(dispatchLabel),
                                            functionIndex,
                                            instructionIndex,
                                            "resume_dispatch");
    backend_aot_llvm_make_instruction_label(unsupportedLabel,
                                            sizeof(unsupportedLabel),
                                            functionIndex,
                                            instructionIndex,
                                            "resume_unsupported");
    resumeTemp = backend_aot_llvm_next_temp(tempCounter);
    fallthroughTemp = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file, "  %%t%u = load i32, ptr %s, align 4\n", (unsigned)resumeTemp, resumePointerName);
    fprintf(file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)fallthroughTemp,
            (unsigned)resumeTemp,
            (unsigned)ZR_AOT_LLVM_RESUME_FALLTHROUGH);
    fprintf(file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)fallthroughTemp,
            fallthroughLabel,
            dispatchLabel);
    fprintf(file, "%s:\n", dispatchLabel);
    fprintf(file, "  switch i32 %%t%u, label %%%s [\n", (unsigned)resumeTemp, unsupportedLabel);
    for (targetIndex = 0; targetIndex < instructionCount; targetIndex++) {
        backend_aot_llvm_make_instruction_label(instructionLabel,
                                                sizeof(instructionLabel),
                                                functionIndex,
                                                targetIndex,
                                                ZR_NULL);
        fprintf(file, "    i32 %u, label %%%s\n", (unsigned)targetIndex, instructionLabel);
    }
    fprintf(file, "  ]\n");
    fprintf(file, "%s:\n", unsupportedLabel);
    snprintf(resumeValueText, sizeof(resumeValueText), "%%t%u", (unsigned)resumeTemp);
    backend_aot_llvm_write_report_unsupported_value_return(file,
                                                           tempCounter,
                                                           functionIndex,
                                                           resumeValueText,
                                                           0);
}
