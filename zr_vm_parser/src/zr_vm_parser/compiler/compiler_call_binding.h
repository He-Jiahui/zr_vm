#ifndef ZR_VM_PARSER_COMPILER_CALL_BINDING_H
#define ZR_VM_PARSER_COMPILER_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_inference.h"

void compiler_get_member_call_binding_fact(SZrCompilerState *compiler,
        const SZrTypeMemberInfo *member, SZrCallBindingContract *fact);

TZrUInt64 compiler_typed_call_signature_hash(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature);

TZrBool compiler_record_typed_call_binding(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature,
        TZrUInt32 argumentCount,
        SZrFileRange location);

TZrBool compiler_finalize_call_bindings(SZrCompilerState *compiler, SZrFunction *function);
TZrBool compiler_publish_module_call_bindings(SZrCompilerState *compiler, SZrFunction *function);

#endif
