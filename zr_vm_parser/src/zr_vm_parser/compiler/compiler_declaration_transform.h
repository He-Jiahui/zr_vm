#ifndef ZR_VM_PARSER_COMPILER_DECLARATION_TRANSFORM_H
#define ZR_VM_PARSER_COMPILER_DECLARATION_TRANSFORM_H

#include "compiler_internal.h"

TZrBool ZrParser_DeclarationTransform_ValidateSignature(
        SZrCompilerState *cs,
        SZrAstNode *functionNode);

#endif // ZR_VM_PARSER_COMPILER_DECLARATION_TRANSFORM_H
