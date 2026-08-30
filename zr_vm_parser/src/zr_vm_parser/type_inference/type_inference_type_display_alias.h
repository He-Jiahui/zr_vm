#ifndef ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H

#include "zr_vm_parser/type_inference.h"

void type_inference_publish_explicit_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        SZrString *alias,
        const SZrType *typeUse);

void type_inference_publish_generic_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse);

void type_inference_publish_primitive_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H
