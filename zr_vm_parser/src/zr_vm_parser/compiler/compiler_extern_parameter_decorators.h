#ifndef ZR_VM_PARSER_COMPILER_EXTERN_PARAMETER_DECORATORS_H
#define ZR_VM_PARSER_COMPILER_EXTERN_PARAMETER_DECORATORS_H

#include "zr_vm_parser/compiler.h"

TZrBool compiler_extern_validate_parameter_decorator_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators);

#endif
