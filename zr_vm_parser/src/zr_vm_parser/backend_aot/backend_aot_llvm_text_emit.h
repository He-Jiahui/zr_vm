#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_EMIT_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_EMIT_H

#include "backend_aot_internal.h"

TZrUInt32 backend_aot_llvm_next_temp(TZrUInt32 *tempCounter);
void backend_aot_llvm_make_function_label(TZrChar *buffer,
                                          TZrSize bufferSize,
                                          TZrUInt32 functionIndex,
                                          const TZrChar *suffix);
void backend_aot_llvm_make_instruction_label(TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrUInt32 functionIndex,
                                             TZrUInt32 instructionIndex,
                                             const TZrChar *suffix);

#endif
