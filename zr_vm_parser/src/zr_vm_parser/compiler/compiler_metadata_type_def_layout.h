#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_DEF_LAYOUT_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_DEF_LAYOUT_H

#include "compiler_internal.h"

TZrBool compiler_metadata_type_def_compute_union_layout_identity(SZrCompilerState *cs,
                                                                 const SZrAstNode *unionDeclaration,
                                                                 TZrUInt32 *outLayoutVersion,
                                                                 TZrUInt64 *outLayoutHash);

#endif
