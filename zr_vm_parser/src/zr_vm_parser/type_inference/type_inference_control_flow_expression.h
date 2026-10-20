#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CONTROL_FLOW_EXPRESSION_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CONTROL_FLOW_EXPRESSION_H

#include "zr_vm_parser/type_inference.h"

/* The outer expression dispatcher retains fact publication and cache updates. */
TZrBool type_inference_control_flow_expression(
        SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

#endif
