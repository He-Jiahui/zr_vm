#ifndef ZR_VM_PARSER_COMPILER_SEMANTIC_SCALAR_EXPRESSION_H
#define ZR_VM_PARSER_COMPILER_SEMANTIC_SCALAR_EXPRESSION_H

#include "compiler_internal.h"

/* Syntax and canonical admission are separate: identifiers alone prove no type. */
TZrBool compiler_semantic_scalar_expression_shape(const SZrAstNode *node);
TZrBool compiler_semantic_scalar_expression_typed(SZrCompilerState *cs,
                                                 const SZrAstNode *node);
TZrBool compiler_semantic_scalar_while_body_supported(SZrCompilerState *cs,
                                                     const SZrAstNode *node);
TZrBool compiler_semantic_scalar_value_matches(const SZrCompilerState *cs,
        const SZrAstNode *node, TZrValueId valueId);

#endif
