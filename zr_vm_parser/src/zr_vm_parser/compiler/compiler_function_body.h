#ifndef ZR_VM_PARSER_COMPILER_FUNCTION_BODY_H
#define ZR_VM_PARSER_COMPILER_FUNCTION_BODY_H

#include "compiler_internal.h"

/* The caller has prepared the declaration's function type and Semantic IR scopes. */
void compiler_compile_function_body(SZrCompilerState *cs, SZrAstNode *node);

#endif
