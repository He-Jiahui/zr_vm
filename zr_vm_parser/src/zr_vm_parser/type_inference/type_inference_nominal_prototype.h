#ifndef ZR_VM_PARSER_TYPE_INFERENCE_NOMINAL_PROTOTYPE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_NOMINAL_PROTOTYPE_H

#include "zr_vm_parser/compiler.h"

TZrBool type_inference_prototype_is_root_visible(const SZrTypePrototypeInfo *prototype);
SZrTypePrototypeInfo *type_inference_find_nominal_prototype(
        SZrCompilerState *cs, const SZrInferredType *type);

#endif
