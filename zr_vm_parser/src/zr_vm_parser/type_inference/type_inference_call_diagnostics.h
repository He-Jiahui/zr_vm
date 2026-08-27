#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CALL_DIAGNOSTICS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CALL_DIAGNOSTICS_H

#include "zr_vm_parser/type_inference.h"

const TZrChar *type_inference_call_diagnostic_ownership_message(
        EZrParameterPassingMode passingMode,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType);
TZrBool type_inference_diagnostic_report_ownership_mismatch(
        SZrCompilerState *cs,
        EZrParameterPassingMode passingMode,
        SZrAstNode *node,
        SZrFileRange location,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType);
TZrBool type_inference_call_diagnostic_report_ownership_mismatch(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *funcType,
        const SZrResolvedCallSignature *resolvedSignature,
        const SZrFunctionCall *call,
        TZrSize parameterIndex,
        const SZrInferredType *argumentType);
TZrBool type_inference_call_diagnostic_report_argument_mismatch(
        SZrCompilerState *cs,
        const SZrFunctionTypeInfo *funcType,
        const SZrResolvedCallSignature *resolvedSignature,
        const SZrFunctionCall *call,
        TZrSize parameterIndex,
        const SZrInferredType *argumentType);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_CALL_DIAGNOSTICS_H
