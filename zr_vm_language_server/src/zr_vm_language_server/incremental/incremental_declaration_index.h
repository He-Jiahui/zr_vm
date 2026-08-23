#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_DECLARATION_INDEX_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_DECLARATION_INDEX_H

#include "zr_vm_parser/ast.h"

typedef struct SZrIncrementalDeclarationSelection {
    TZrSize statementIndex;
    SZrAstNode *statement;
    SZrFileRange range;
} SZrIncrementalDeclarationSelection;

TZrBool ZrLanguageServer_IncrementalDeclarationIndex_FindContainingTopLevel(
        SZrAstNode *root,
        const SZrFileRange *changedRange,
        SZrIncrementalDeclarationSelection *outSelection);

#endif // ZR_VM_LANGUAGE_SERVER_INCREMENTAL_DECLARATION_INDEX_H
