#ifndef ZR_VM_PARSER_TYPE_ENVIRONMENT_CALLABLE_INTERNAL_H
#define ZR_VM_PARSER_TYPE_ENVIRONMENT_CALLABLE_INTERNAL_H

#include "zr_vm_parser/type_system.h"

TZrBool type_environment_callable_context_equal(
        SZrSyntaxContext left,
        SZrSyntaxContext right,
        TZrBool isCallableValueBinding,
        TZrBool *outEqual);
TZrBool type_environment_callable_matches_signature(
        const SZrFunctionTypeInfo *functionInfo,
        const SZrInferredType *returnType,
        const SZrArray *parameterTypes);

#endif
