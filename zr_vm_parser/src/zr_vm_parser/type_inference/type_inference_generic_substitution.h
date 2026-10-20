#ifndef ZR_VM_PARSER_TYPE_INFERENCE_GENERIC_SUBSTITUTION_H
#define ZR_VM_PARSER_TYPE_INFERENCE_GENERIC_SUBSTITUTION_H

#include "zr_vm_parser/compiler.h"

TZrBool type_inference_append_type_text(TZrChar *buffer, TZrSize bufferSize,
        TZrSize *offset, const TZrChar *fragment);
SZrString *substitute_generic_type_name(SZrState *state, SZrString *sourceTypeName,
        const SZrArray *genericParameters, const SZrArray *argumentTypeNames);
TZrBool substitute_parameter_type(SZrCompilerState *cs, const SZrInferredType *sourceType,
        const SZrArray *genericParameters, const SZrArray *argumentTypeNames,
        const SZrArray *argumentTypes, SZrInferredType *outType);
void finalize_generic_instance_inline_layout(
        SZrCompilerState *cs, SZrTypePrototypeInfo *prototypeInfo);

#endif
