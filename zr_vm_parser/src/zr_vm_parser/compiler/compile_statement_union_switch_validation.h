#ifndef ZR_VM_PARSER_COMPILE_STATEMENT_UNION_SWITCH_VALIDATION_H
#define ZR_VM_PARSER_COMPILE_STATEMENT_UNION_SWITCH_VALIDATION_H

#include "zr_vm_parser/compiler.h"

void compile_switch_validate_union_duplicate_cases(SZrCompilerState *cs,
                                                   SZrSwitchExpression *switchExpression,
                                                   SZrAstNode *unionDeclaration,
                                                   SZrString *switchUnionTypeName);

TZrBool compile_switch_validate_union_exhaustiveness(SZrCompilerState *cs,
                                                     SZrSwitchExpression *switchExpression,
                                                     SZrAstNode *unionDeclaration,
                                                     SZrString *switchUnionTypeName,
                                                     SZrFileRange location);

#endif
