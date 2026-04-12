#ifndef ZR_VM_PARSER_WRITER_BINARY_INTERNAL_H
#define ZR_VM_PARSER_WRITER_BINARY_INTERNAL_H

#include <stdio.h>

#include "zr_vm_parser/writer.h"

TZrBool ZrParser_Writer_FunctionTreeHasDebugInfo(const SZrFunction *function);

TZrBool ZrParser_Writer_WriteIoFunction(SZrState *state,
                                        FILE *file,
                                        SZrFunction *function,
                                        const TZrChar *defaultName,
                                        const SZrBinaryWriterOptions *options);

#endif
