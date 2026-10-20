#ifndef ZR_VM_PARSER_COMPILER_TYPE_DECLARATION_H
#define ZR_VM_PARSER_COMPILER_TYPE_DECLARATION_H

#include "zr_vm_parser/compiler.h"

TZrBool compiler_type_declaration_item_context(const SZrAstNode *declaration,
        SZrSyntaxContext *outContext);
SZrAstNode *find_type_declaration_with_context(SZrCompilerState *cs,
        SZrString *typeName, SZrSyntaxContext syntaxContext);
SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName);

#endif
