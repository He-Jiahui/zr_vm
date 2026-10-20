#ifndef ZR_VM_PARSER_TYPE_INFERENCE_GENERIC_PROTOTYPE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_GENERIC_PROTOTYPE_H

#include "zr_vm_parser/compiler.h"

SZrTypePrototypeInfo *find_compiler_type_prototype_inference_exact(
        SZrCompilerState *cs, SZrString *typeName);
SZrTypePrototypeInfo *type_inference_find_generic_prototype(
        SZrCompilerState *cs, const SZrInferredType *type);
TZrBool type_inference_generic_prototype_is_root_visible(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype);

#endif
