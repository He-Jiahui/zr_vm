#ifndef ZR_VM_PARSER_COMPILER_TYPE_VALUE_ALIAS_H
#define ZR_VM_PARSER_COMPILER_TYPE_VALUE_ALIAS_H

#include "zr_vm_parser/compiler.h"

SZrTypeBinding *compiler_find_type_value_alias(SZrCompilerState *cs,
        SZrString *name, SZrSyntaxContext syntaxContext);
SZrTypeBinding *compiler_find_current_type_value_alias(SZrCompilerState *cs,
        SZrString *name, SZrSyntaxContext syntaxContext);
void compiler_truncate_type_value_aliases(SZrCompilerState *cs, TZrSize count);
void compiler_register_type_value_alias(SZrCompilerState *cs,
        SZrString *name, const SZrInferredType *type, SZrSyntaxContext syntaxContext);
void compiler_register_type_value_alias_literal(SZrCompilerState *cs,
        SZrString *name, SZrAstNode *value, SZrSyntaxContext syntaxContext);

#endif
