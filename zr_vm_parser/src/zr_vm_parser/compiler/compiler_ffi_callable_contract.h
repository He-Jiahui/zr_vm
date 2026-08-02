#ifndef ZR_VM_PARSER_COMPILER_FFI_CALLABLE_CONTRACT_H
#define ZR_VM_PARSER_COMPILER_FFI_CALLABLE_CONTRACT_H

#include "zr_vm_parser/ffi_contract.h"
#include "zr_vm_parser/semantic.h"

EZrFfiContractStatus compiler_ffi_callable_contract_build(
        SZrSemanticContext *context,
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrFfiSignatureContract *signature,
        SZrFfiCallableContract *outContract);

#endif // ZR_VM_PARSER_COMPILER_FFI_CALLABLE_CONTRACT_H
