#ifndef ZR_VM_PARSER_ITERATION_CONTRACT_H
#define ZR_VM_PARSER_ITERATION_CONTRACT_H

#include "zr_vm_parser/compiler.h"

/*
 * Resolves an enumerated element only from the source type's canonical
 * Iterator<T> or Iterable<T> protocol projection.
 */
ZR_PARSER_API TZrBool ZrParser_EnumeratorBinding_ResolveElementType(
        SZrCompilerState *compiler,
        const SZrInferredType *source,
        SZrInferredType *outElementType);

#endif /* ZR_VM_PARSER_ITERATION_CONTRACT_H */
