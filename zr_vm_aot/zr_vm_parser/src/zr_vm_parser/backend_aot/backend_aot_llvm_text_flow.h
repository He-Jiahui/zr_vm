#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_FLOW_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_FLOW_H

#include "backend_aot_llvm_text_call_result.h"
#include "backend_aot_llvm_text_terminal.h"

void backend_aot_llvm_write_begin_instruction(FILE *file,
                                              TZrUInt32 *tempCounter,
                                              TZrUInt32 instructionIndex,
                                              TZrUInt32 stepFlags,
                                              const TZrChar *bodyLabel,
                                              const TZrChar *failLabel);
void backend_aot_llvm_write_resume_dispatch(FILE *file,
                                            TZrUInt32 *tempCounter,
                                            TZrUInt32 functionIndex,
                                            TZrUInt32 instructionIndex,
                                            TZrUInt32 instructionCount,
                                            const TZrChar *resumePointerName,
                                            const TZrChar *fallthroughLabel);

#endif
