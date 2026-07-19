#ifndef ZR_VM_PARSER_COMPILER_UNION_CANONICAL_H
#define ZR_VM_PARSER_COMPILER_UNION_CANONICAL_H

#include "zr_vm_parser/compiler.h"

TZrBool compiler_union_register_canonical_type(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const SZrTypePrototypeInfo *prototype);

#endif // ZR_VM_PARSER_COMPILER_UNION_CANONICAL_H
