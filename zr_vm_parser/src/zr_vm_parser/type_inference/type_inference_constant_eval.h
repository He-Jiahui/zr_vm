#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CONSTANT_EVAL_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CONSTANT_EVAL_H

#include "zr_vm_parser/ast.h"

TZrBool type_inference_node_integer_value(SZrAstNode *node, TZrInt64 *outValue);
TZrBool type_inference_node_double_value(SZrAstNode *node, TZrDouble *outValue);
TZrBool type_inference_node_bool_value(SZrAstNode *node, TZrBool *outValue);

#endif
