#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_DECLARATION_INDEX_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_DECLARATION_INDEX_H

#include "zr_vm_parser/ast.h"

/** 局部重解析候选的借用视图：下标用于替换，原范围用于证明其余语句位置不变。 */
typedef struct SZrIncrementalDeclarationSelection {
    TZrSize statementIndex;
    SZrAstNode *statement;
    SZrFileRange range;
} SZrIncrementalDeclarationSelection;

/** @brief 仅在一次非空修改被唯一顶层语句包住时选择可替换的 AST 节点。 */
TZrBool ZrLanguageServer_IncrementalDeclarationIndex_FindContainingTopLevel(
        SZrAstNode *root,
        const SZrFileRange *changedRange,
        SZrIncrementalDeclarationSelection *outSelection);

#endif // ZR_VM_LANGUAGE_SERVER_INCREMENTAL_DECLARATION_INDEX_H
