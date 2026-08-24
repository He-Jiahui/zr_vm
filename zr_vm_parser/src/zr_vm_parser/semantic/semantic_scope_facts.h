#ifndef ZR_VM_PARSER_SEMANTIC_SCOPE_FACTS_H
#define ZR_VM_PARSER_SEMANTIC_SCOPE_FACTS_H

#include "zr_vm_parser/semantic.h"

/* Builds source lexical scope facts from AST boundaries and resolved declaration facts. */
TZrBool ZrParser_Semantic_BuildSourceScopeFacts(
        SZrSemanticContext *context,
        SZrAstNode *root);

#endif /* ZR_VM_PARSER_SEMANTIC_SCOPE_FACTS_H */
