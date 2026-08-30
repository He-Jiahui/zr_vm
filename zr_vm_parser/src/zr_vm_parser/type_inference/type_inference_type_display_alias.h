#ifndef ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H

#include "zr_vm_parser/type_inference.h"

void type_inference_publish_explicit_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        SZrString *alias,
        const SZrAstNode *typeUseNode);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H
