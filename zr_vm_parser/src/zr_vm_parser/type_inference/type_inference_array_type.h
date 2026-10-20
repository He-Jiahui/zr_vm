#ifndef ZR_VM_PARSER_TYPE_INFERENCE_ARRAY_TYPE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_ARRAY_TYPE_H

#include "zr_vm_parser/compiler.h"

typedef struct SZrArrayTypeExtent {
    TZrSize fixedSize;
    TZrSize minSize;
    TZrSize maxSize;
    TZrBool hasFixedSize;
} SZrArrayTypeExtent;

TZrBool type_inference_resolve_array_type_extent(
        SZrCompilerState *cs, const SZrType *astType, SZrArrayTypeExtent *extent);

#endif
