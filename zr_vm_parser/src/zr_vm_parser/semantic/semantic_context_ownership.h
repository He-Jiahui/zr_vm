#ifndef ZR_VM_PARSER_SEMANTIC_CONTEXT_OWNERSHIP_H
#define ZR_VM_PARSER_SEMANTIC_CONTEXT_OWNERSHIP_H

#include "zr_vm_parser/semantic.h"

TZrBool semantic_context_retain_syntax_table(
        SZrSemanticContext *context, SZrSyntaxContext syntaxContext);

#endif
