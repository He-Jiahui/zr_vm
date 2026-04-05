#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_CALL_RESULT_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_TEXT_CALL_RESULT_H

#include "backend_aot_llvm_text_emit.h"

void backend_aot_llvm_write_guarded_call_text(FILE *file,
                                              TZrUInt32 *tempCounter,
                                              const TZrChar *calleeName,
                                              const TZrChar *argsText,
                                              const TZrChar *successLabel,
                                              const TZrChar *failLabel);
void backend_aot_llvm_write_nonzero_call_text(FILE *file,
                                              TZrUInt32 *tempCounter,
                                              const TZrChar *calleeName,
                                              const TZrChar *argsText,
                                              const TZrChar *successLabel,
                                              const TZrChar *failLabel);

#endif
