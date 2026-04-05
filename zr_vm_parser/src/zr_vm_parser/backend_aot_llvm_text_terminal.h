#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_TERMINAL_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_TERMINAL_H

#include "backend_aot_llvm_text_emit.h"

void backend_aot_llvm_write_report_unsupported_return(FILE *file,
                                                      TZrUInt32 *tempCounter,
                                                      TZrUInt32 functionIndex,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 opcode);
void backend_aot_llvm_write_report_unsupported_value_return(FILE *file,
                                                            TZrUInt32 *tempCounter,
                                                            TZrUInt32 functionIndex,
                                                            const TZrChar *instructionValueText,
                                                            TZrUInt32 opcode);
void backend_aot_llvm_write_return_call(FILE *file,
                                        TZrUInt32 *tempCounter,
                                        TZrUInt32 sourceSlot,
                                        TZrBool publishExports);

#endif
