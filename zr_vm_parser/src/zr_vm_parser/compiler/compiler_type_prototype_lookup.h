#ifndef ZR_VM_PARSER_COMPILER_TYPE_PROTOTYPE_LOOKUP_H
#define ZR_VM_PARSER_COMPILER_TYPE_PROTOTYPE_LOOKUP_H

#include "zr_vm_parser/compiler.h"

SZrTypePrototypeInfo *find_compiler_type_prototype(
        SZrCompilerState *cs, SZrString *typeName);
/* Success may return no prototype for a deferred Root base. Any returned pointer is borrowed. */
TZrBool compiler_resolve_type_annotation_prototype(
        SZrCompilerState *cs, SZrType *annotation, SZrString *legacyTypeName,
        SZrTypePrototypeInfo **outPrototype);
SZrString *compile_expression_build_canonical_module_member_type_name(
        SZrCompilerState *cs, SZrString *canonicalMemberTypeName,
        SZrString *requestedMemberTypeName);

#endif
