#include "incremental/incremental_syntax_reparse.h"

#include "incremental/incremental_declaration_index.h"
#include "zr_vm_parser/parser.h"

/** @brief 替换节点前确认新旧顶层语句占据完全相同的源偏移。 */
static TZrBool range_offsets_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset == right->start.offset &&
           left->end.offset == right->end.offset;
}

/**
 * @brief 拒绝会移动后续 AST 位置的编辑，仅允许等长且换行布局一致的局部修改。
 * @pre 调用方已核实两份完整文本等长，changeInfo 的范围来自同一对文本。
 */
static TZrBool changed_layout_is_stable(
        const TZrChar *oldContent,
        const TZrChar *newContent,
        const SZrFileChangeInfo *changeInfo) {
    TZrSize oldLength;
    TZrSize newLength;

    if (oldContent == ZR_NULL || newContent == ZR_NULL || changeInfo == ZR_NULL ||
        changeInfo->oldRange.end.offset < changeInfo->oldRange.start.offset ||
        changeInfo->newRange.end.offset < changeInfo->newRange.start.offset ||
        changeInfo->oldRange.start.offset != changeInfo->newRange.start.offset) {
        return ZR_FALSE;
    }

    oldLength = changeInfo->oldRange.end.offset - changeInfo->oldRange.start.offset;
    newLength = changeInfo->newRange.end.offset - changeInfo->newRange.start.offset;
    if (oldLength == 0U || oldLength != newLength) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < oldLength; index++) {
        TZrChar oldCharacter = oldContent[changeInfo->oldRange.start.offset + index];
        TZrChar newCharacter = newContent[changeInfo->newRange.start.offset + index];

        if ((oldCharacter == '\r' || oldCharacter == '\n') !=
            (newCharacter == '\r' || newCharacter == '\n')) {
            return ZR_FALSE;
        }
        if ((oldCharacter == '\r' || oldCharacter == '\n') && oldCharacter != newCharacter) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/**
 * @brief 尝试仅重建一个顶层语句，供 Parse 的增量路径复用其他 AST 节点。
 * @return 失败时保留旧 AST，调用方必须退回完整重解析；成功时旧候选被释放。
 * BUG: State_Init 会预读首 token，若它是未终止字符串，lexer 直接写 stdout；
 *      此处随后设置的 suppressErrorOutput 无法保护 stdio JSON-RPC 输出。
 */
TZrBool ZrLanguageServer_IncrementalSyntaxReparse_TryDeclaration(
        SZrState *state,
        SZrString *uri,
        const TZrChar *oldContent,
        TZrSize oldContentLength,
        const TZrChar *newContent,
        TZrSize newContentLength,
        SZrAstNode *root,
        SZrFileChangeInfo *changeInfo) {
    SZrIncrementalDeclarationSelection selection;
    SZrParserState parserState;
    SZrAstNode *replacement;

    if (state == ZR_NULL || uri == ZR_NULL || oldContent == ZR_NULL || newContent == ZR_NULL ||
        root == ZR_NULL || changeInfo == ZR_NULL || oldContentLength != newContentLength ||
        !changed_layout_is_stable(oldContent, newContent, changeInfo) ||
        !ZrLanguageServer_IncrementalDeclarationIndex_FindContainingTopLevel(
                root, &changeInfo->oldRange, &selection)) {
        return ZR_FALSE;
    }

    ZrParser_State_Init(&parserState, state, newContent, newContentLength, uri);
    parserState.suppressErrorOutput = ZR_TRUE;
    if (parserState.hasError ||
        !ZrParser_State_SeekToTokenStart(&parserState, selection.range.start.offset)) {
        ZrParser_State_Free(&parserState);
        return ZR_FALSE;
    }

    replacement = ZrParser_ParseTopLevelStatementWithState(&parserState);
    if (replacement == ZR_NULL || parserState.hasError || parserState.hasFatalError ||
        replacement->type != selection.statement->type ||
        !range_offsets_equal(&replacement->location, &selection.range)) {
        if (replacement != ZR_NULL) {
            ZrParser_Ast_Free(state, replacement);
        }
        ZrParser_State_Free(&parserState);
        return ZR_FALSE;
    }

    /* 仅在类型与源范围均吻合后转移所有权，保持未触及语句的 AST 节点与源位置可用。 */
    root->data.script.statements->nodes[selection.statementIndex] = replacement;
    ZrParser_Ast_Free(state, selection.statement);
    ZrParser_State_Free(&parserState);

    changeInfo->hasDeclaration = ZR_TRUE;
    changeInfo->declarationType = replacement->type;
    changeInfo->declarationRange = replacement->location;
    return ZR_TRUE;
}
