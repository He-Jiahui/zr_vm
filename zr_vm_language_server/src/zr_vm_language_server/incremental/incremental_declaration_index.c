#include "incremental/incremental_declaration_index.h"

static TZrBool range_contains(
        const SZrFileRange *container,
        const SZrFileRange *contained) {
    return container != ZR_NULL && contained != ZR_NULL &&
           container->start.offset <= contained->start.offset &&
           container->end.offset >= contained->end.offset;
}

TZrBool ZrLanguageServer_IncrementalDeclarationIndex_FindContainingTopLevel(
        SZrAstNode *root,
        const SZrFileRange *changedRange,
        SZrIncrementalDeclarationSelection *outSelection) {
    SZrAstNodeArray *statements;
    TZrSize selectedIndex = 0;
    TZrSize selectedCount = 0;

    if (outSelection != ZR_NULL) {
        *outSelection = (SZrIncrementalDeclarationSelection){0};
    }
    if (root == ZR_NULL || root->type != ZR_AST_SCRIPT ||
        changedRange == ZR_NULL || outSelection == ZR_NULL ||
        changedRange->end.offset <= changedRange->start.offset) {
        return ZR_FALSE;
    }

    statements = root->data.script.statements;
    if (statements == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];

        if (statement != ZR_NULL && range_contains(&statement->location, changedRange)) {
            selectedIndex = index;
            selectedCount++;
        }
    }

    if (selectedCount != 1U) {
        return ZR_FALSE;
    }

    outSelection->statementIndex = selectedIndex;
    outSelection->statement = statements->nodes[selectedIndex];
    outSelection->range = outSelection->statement->location;
    return ZR_TRUE;
}
