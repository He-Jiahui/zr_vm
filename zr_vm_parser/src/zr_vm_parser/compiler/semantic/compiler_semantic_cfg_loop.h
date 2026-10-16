#ifndef ZR_VM_PARSER_COMPILER_SEMANTIC_CFG_LOOP_H
#define ZR_VM_PARSER_COMPILER_SEMANTIC_CFG_LOOP_H

#include "zr_vm_parser/ast.h"

/* While-only extension; the shared for/foreach body analyzer stays unchanged. */
TZrBool compiler_semantic_cfg_while_body_is_supported(const SZrAstNode *node);

#endif
