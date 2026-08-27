#ifndef ZR_VM_PARSER_COMPILER_TOP_LEVEL_DUPLICATE_H
#define ZR_VM_PARSER_COMPILER_TOP_LEVEL_DUPLICATE_H

#include "zr_vm_parser/compiler.h"

TZrBool compiler_report_duplicate_top_level_type(
        SZrCompilerState *cs,
        SZrAstNode *declaration);

#endif // ZR_VM_PARSER_COMPILER_TOP_LEVEL_DUPLICATE_H
