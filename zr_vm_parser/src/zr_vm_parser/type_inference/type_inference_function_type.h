#ifndef ZR_VM_PARSER_TYPE_INFERENCE_FUNCTION_TYPE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_FUNCTION_TYPE_H

#include "zr_vm_parser/compiler.h"

SZrString *type_inference_build_callable_type_name_from_signature(
        SZrState *state, const SZrInferredType *returnType,
        const SZrArray *paramTypes, const SZrArray *parameterPassingModes);
TZrBool type_inference_infer_function_type_signature(
        SZrCompilerState *cs, const SZrFunctionType *funcType,
        SZrArray *paramTypes, SZrArray *parameterPassingModes,
        SZrInferredType *returnType);
const SZrFunctionType *type_inference_function_field_type(const SZrTypeMemberInfo *memberInfo);

#endif
