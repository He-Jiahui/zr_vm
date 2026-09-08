#ifndef ZR_VM_PARSER_COMPILER_TYPED_CALL_BINDING_H
#define ZR_VM_PARSER_COMPILER_TYPED_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_inference.h"

TZrBool compiler_resolve_typed_callable_value(SZrCompilerState *compiler,
        SZrString *name, SZrResolvedCallSignature *signature, SZrFileRange location);
TZrBool compiler_register_typed_callable_parameter(SZrCompilerState *compiler,
        SZrString *name, SZrType *type);
void compiler_typed_call_use_generic_dispatch(SZrFunction *function, TZrUInt32 instructionIndex);

#endif
