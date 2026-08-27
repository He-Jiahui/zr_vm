#ifndef ZR_VM_PARSER_COMPILER_EXTERN_DECORATOR_DIAGNOSTICS_H
#define ZR_VM_PARSER_COMPILER_EXTERN_DECORATOR_DIAGNOSTICS_H

#include "zr_vm_parser/compiler.h"

TZrBool compiler_extern_report_invalid_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion);

#endif
