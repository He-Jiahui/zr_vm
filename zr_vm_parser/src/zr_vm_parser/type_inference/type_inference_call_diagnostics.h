#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CALL_DIAGNOSTICS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CALL_DIAGNOSTICS_H

#include "zr_vm_parser/type_inference.h"

typedef struct SZrTypeMemberInfo SZrTypeMemberInfo;

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
SZrFileRange type_inference_call_diagnostic_argument_location(
        const SZrAstNode *argumentNode);
TZrBool type_inference_member_call_diagnostic_report_argument_mismatch(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrFunctionCall *call,
        SZrAstNode *argumentNode,
        TZrSize parameterIndex,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType);
TZrBool type_inference_member_call_diagnostic_report_ownership_mismatch(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrFunctionCall *call,
        SZrAstNode *argumentNode,
        TZrSize parameterIndex,
        const SZrInferredType *parameterType,
        const SZrInferredType *argumentType);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_CALL_DIAGNOSTICS_H
