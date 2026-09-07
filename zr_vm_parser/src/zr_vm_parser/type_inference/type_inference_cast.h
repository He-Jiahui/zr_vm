#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CAST_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CAST_H

#include "zr_vm_parser/type_inference.h"

TZrBool type_inference_cast_expression(
        SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

#endif
