#ifndef ZR_VM_PARSER_FFI_CONTRACT_H
#define ZR_VM_PARSER_FFI_CONTRACT_H

#include "zr_vm_common/zr_ffi_contract.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/conf.h"

typedef enum EZrFfiContractStatus {
    ZR_FFI_CONTRACT_STATUS_OK = 0,
    ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT,
    ZR_FFI_CONTRACT_STATUS_SCHEMA_MISMATCH,
    ZR_FFI_CONTRACT_STATUS_LIBRARY_LOCATOR_LIMIT,
    ZR_FFI_CONTRACT_STATUS_ENTRY_POINT_LIMIT,
    ZR_FFI_CONTRACT_STATUS_SOURCE_DOCUMENT_LIMIT,
    ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT,
    ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE,
    ZR_FFI_CONTRACT_STATUS_INVALID_ABI,
    ZR_FFI_CONTRACT_STATUS_INVALID_DIRECTION,
    ZR_FFI_CONTRACT_STATUS_INVALID_MARSHALLING,
    ZR_FFI_CONTRACT_STATUS_FORBIDDEN_MANAGED_TYPE,
    ZR_FFI_CONTRACT_STATUS_CALLBACK_POLICY_REQUIRED,
    ZR_FFI_CONTRACT_STATUS_INVALID_POLICY,
    ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT,
    ZR_FFI_CONTRACT_STATUS_INVALID_TARGET_ABI,
    ZR_FFI_CONTRACT_STATUS_HASH_MISMATCH
} EZrFfiContractStatus;

typedef struct SZrFfiContractDiagnostic {
    EZrFfiContractStatus status;
    TZrUInt32 parameterIndex;
    SZrFileRange sourceRange;
} SZrFfiContractDiagnostic;

struct SZrSemanticContext;

ZR_PARSER_API EZrFfiContractStatus ZrParser_FfiContract_Build(
        struct SZrSemanticContext *semanticContext,
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrNativeImportContract *outContract,
        SZrFfiContractDiagnostic *diagnostic);

ZR_PARSER_API EZrFfiContractStatus ZrParser_FfiContract_BuildDelegateSignature(
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrFfiSignatureContract *outSignature,
        SZrFfiContractDiagnostic *diagnostic);

ZR_PARSER_API EZrFfiContractStatus ZrParser_FfiContract_Validate(
        const SZrNativeImportContract *contract,
        SZrFfiContractDiagnostic *diagnostic);

#endif // ZR_VM_PARSER_FFI_CONTRACT_H
