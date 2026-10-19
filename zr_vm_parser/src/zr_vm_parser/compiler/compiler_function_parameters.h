#ifndef ZR_VM_PARSER_COMPILER_FUNCTION_PARAMETERS_H
#define ZR_VM_PARSER_COMPILER_FUNCTION_PARAMETERS_H

#include "zr_vm_parser/compiler.h"

typedef enum EZrCompilerParameterIdentityResult {
    ZR_COMPILER_PARAMETER_IDENTITY_ERROR = 0,
    ZR_COMPILER_PARAMETER_IDENTITY_UNAVAILABLE,
    ZR_COMPILER_PARAMETER_IDENTITY_REGISTERED
} EZrCompilerParameterIdentityResult;

/* UNAVAILABLE preserves a real declaration whose canonical type is not modeled. */
EZrCompilerParameterIdentityResult compiler_register_parameter_semantic_identity(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange);

#endif
