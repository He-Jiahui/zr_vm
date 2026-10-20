#ifndef ZR_VM_PARSER_SEMANTIC_DA_AST_H
#define ZR_VM_PARSER_SEMANTIC_DA_AST_H

#include "zr_vm_parser/ast.h"

typedef struct SZrSemanticDaAstEntry SZrSemanticDaAstEntry;

/* Borrows the AST; owns only the preorder and address-lookup buffers. */
typedef struct SZrSemanticDaAstIndex {
    SZrState *state;
    SZrSemanticDaAstEntry *entries;
    SZrSemanticDaAstEntry **byAddress;
    TZrSize count;
} SZrSemanticDaAstIndex;

typedef enum EZrSemanticDaAstMatch {
    ZR_SEMANTIC_DA_AST_LEGACY,
    ZR_SEMANTIC_DA_AST_INCLUDED,
    ZR_SEMANTIC_DA_AST_EXCLUDED
} EZrSemanticDaAstMatch;

typedef TZrBool (*TZrSemanticDaAstRootVisitor)(SZrAstNode *root, void *userData);

void ZrParser_SemanticDaAst_Init(SZrSemanticDaAstIndex *index);
TZrBool ZrParser_SemanticDaAst_Build(SZrState *state,
                                   SZrAstNode *root,
                                   SZrSemanticDaAstIndex *index);
void ZrParser_SemanticDaAst_Free(SZrSemanticDaAstIndex *index);

/* A real node excluded by identity must never be rescued by a source range. */
EZrSemanticDaAstMatch ZrParser_SemanticDaAst_Match(
        const SZrSemanticDaAstIndex *index,
        const SZrAstNode *subtree,
        const SZrAstNode *factNode);

/* Visits script/function roots, a standalone block root, and actual lambda bodies. */
TZrBool ZrParser_SemanticDaAst_VisitCfgRoots(
        const SZrSemanticDaAstIndex *index,
        TZrSemanticDaAstRootVisitor visitor,
        void *userData);

#endif
