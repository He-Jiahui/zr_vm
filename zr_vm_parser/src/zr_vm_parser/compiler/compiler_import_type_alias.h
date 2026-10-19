#ifndef ZR_VM_PARSER_COMPILER_IMPORT_TYPE_ALIAS_H
#define ZR_VM_PARSER_COMPILER_IMPORT_TYPE_ALIAS_H

#include "zr_vm_parser/compiler.h"

TZrBool compiler_register_imported_destructured_type_aliases(
        SZrCompilerState *cs, SZrAstNode *node);

#endif
