#include "semantic/lsp_semantic_token_entries.h"

#include "interface/lsp_position_codec.h"

#include <stdlib.h>

/* qsort 比较结果仅表达相对次序，不泄漏为协议值。 */
#define ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS (-1)
#define ZR_LSP_SEMANTIC_TOKEN_COMPARE_EQUAL 0
#define ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER 1

/* 当声明事实与文本扫描落在同一跨度时，优先保留更具体的类别。 */
static TZrUInt32 semantic_token_type_priority(TZrUInt32 typeIndex) {
    switch (typeIndex) {
        case ZR_LSP_SEMANTIC_TOKEN_VARIABLE:
            return 0;
        case ZR_LSP_SEMANTIC_TOKEN_PARAMETER:
            return 1;
        case ZR_LSP_SEMANTIC_TOKEN_PROPERTY:
        case ZR_LSP_SEMANTIC_TOKEN_METHOD:
        case ZR_LSP_SEMANTIC_TOKEN_FUNCTION:
            return 2;
        case ZR_LSP_SEMANTIC_TOKEN_NAMESPACE:
        case ZR_LSP_SEMANTIC_TOKEN_CLASS:
        case ZR_LSP_SEMANTIC_TOKEN_STRUCT:
        case ZR_LSP_SEMANTIC_TOKEN_INTERFACE:
        case ZR_LSP_SEMANTIC_TOKEN_ENUM:
        case ZR_LSP_SEMANTIC_TOKEN_KEYWORD:
        case ZR_LSP_SEMANTIC_TOKEN_DECORATOR:
        case ZR_LSP_SEMANTIC_TOKEN_META_METHOD:
            return 3;
        default:
            return 0;
    }
}

void ZrLanguageServer_LspSemanticTokenEntries_Add(
        SZrState *state,
        SZrArray *entries,
        TZrUInt32 line,
        TZrUInt32 character,
        TZrUInt32 length,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers) {
    SZrLspSemanticTokenEntry entry;

    if (state == ZR_NULL || entries == ZR_NULL || length == 0) {
        return;
    }

    /* 先合并相同跨度，避免声明与扫描为客户端生成重复 token。 */
    for (TZrSize index = 0; index < entries->length; index++) {
        SZrLspSemanticTokenEntry *current =
            (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(entries, index);
        if (current != ZR_NULL &&
            current->line == line &&
            current->character == character &&
            current->length == length) {
            if (current->typeIndex == typeIndex) {
                current->modifiers |= modifiers;
                return;
            }
            if (semantic_token_type_priority(typeIndex) >
                semantic_token_type_priority(current->typeIndex)) {
                current->typeIndex = typeIndex;
                current->modifiers = modifiers;
            }
            return;
        }
    }

    entry.line = line;
    entry.character = character;
    entry.length = length;
    entry.typeIndex = typeIndex;
    entry.modifiers = modifiers;
    ZrCore_Array_Push(state, entries, &entry);
}

TZrBool ZrLanguageServer_LspSemanticTokenEntries_AddUtf16Span(
        SZrState *state,
        SZrArray *entries,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize startOffset,
        TZrSize endOffset,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers) {
    SZrLspPosition startPosition;
    SZrLspPosition endPosition;
    TZrInt32 tokenLength;

    if (state == ZR_NULL || entries == ZR_NULL || content == ZR_NULL ||
        startOffset >= endOffset || endOffset > contentLength) {
        return ZR_FALSE;
    }

    /* LSP 的列和长度以 UTF-16 code unit 计数，原始字节差不可直接发送。 */
    startPosition = ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(
            content, contentLength, startOffset);
    endPosition = ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(
            content, contentLength, endOffset);
    if (startPosition.line != endPosition.line) {
        return ZR_FALSE;
    }

    tokenLength = endPosition.character - startPosition.character;
    if (tokenLength <= 0) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspSemanticTokenEntries_Add(
            state,
            entries,
            (TZrUInt32)startPosition.line,
            (TZrUInt32)startPosition.character,
            (TZrUInt32)tokenLength,
            typeIndex,
            modifiers);
    return ZR_TRUE;
}

void ZrLanguageServer_LspSemanticTokenEntries_AddFileRange(
        SZrState *state,
        SZrLspContext *context,
        SZrArray *entries,
        SZrString *uri,
        SZrFileRange range,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers) {
    SZrLspRange lspRange;
    TZrInt32 tokenLength;

    if (state == ZR_NULL || entries == ZR_NULL || range.source == ZR_NULL) {
        return;
    }

    /* parser 的范围属于源文件坐标域，由文档适配器统一转到客户端坐标域。 */
    lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, range);
    if (lspRange.start.line != lspRange.end.line) {
        return;
    }

    tokenLength = lspRange.end.character - lspRange.start.character;
    if (tokenLength <= 0) {
        return;
    }

    ZrLanguageServer_LspSemanticTokenEntries_Add(
            state,
            entries,
            (TZrUInt32)lspRange.start.line,
            (TZrUInt32)lspRange.start.character,
            (TZrUInt32)tokenLength,
            typeIndex,
            modifiers);
}

/* LSP delta 编码要求按位置递增；其他字段保证同起点候选顺序确定。 */
static int semantic_token_entry_compare(const void *leftPtr,
                                        const void *rightPtr) {
    const SZrLspSemanticTokenEntry *left =
            (const SZrLspSemanticTokenEntry *)leftPtr;
    const SZrLspSemanticTokenEntry *right =
            (const SZrLspSemanticTokenEntry *)rightPtr;

    if (left->line != right->line) {
        return left->line < right->line ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                        : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->character != right->character) {
        return left->character < right->character ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                                  : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->length != right->length) {
        return left->length < right->length ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                            : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->typeIndex != right->typeIndex) {
        return left->typeIndex < right->typeIndex ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                                  : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    if (left->modifiers != right->modifiers) {
        return left->modifiers < right->modifiers ? ZR_LSP_SEMANTIC_TOKEN_COMPARE_LESS
                                                   : ZR_LSP_SEMANTIC_TOKEN_COMPARE_GREATER;
    }
    return ZR_LSP_SEMANTIC_TOKEN_COMPARE_EQUAL;
}

/* 编码前只允许同一行的 token 相互排斥，避免客户端收到重叠跨度。 */
static TZrBool semantic_token_entries_overlap(
        const SZrLspSemanticTokenEntry *left,
        const SZrLspSemanticTokenEntry *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->line == right->line &&
           right->character < left->character + left->length;
}

/* 重叠时按类别优先级，再按较短的精确跨度选取候选。 */
static TZrBool semantic_token_entry_is_preferred(
        const SZrLspSemanticTokenEntry *candidate,
        const SZrLspSemanticTokenEntry *current) {
    TZrUInt32 candidatePriority;
    TZrUInt32 currentPriority;

    if (candidate == ZR_NULL || current == ZR_NULL) {
        return ZR_FALSE;
    }
    candidatePriority = semantic_token_type_priority(candidate->typeIndex);
    currentPriority = semantic_token_type_priority(current->typeIndex);
    if (candidatePriority != currentPriority) {
        return candidatePriority > currentPriority;
    }
    return candidate->length < current->length;
}

void ZrLanguageServer_LspSemanticTokenEntries_AppendEncoded(
        SZrState *state,
        SZrArray *entries,
        SZrArray *result) {
    TZrUInt32 previousLine = 0;
    TZrUInt32 previousCharacter = 0;
    TZrBool isFirst = ZR_TRUE;
    TZrSize retainedLength = 0;

    if (state == ZR_NULL || entries == ZR_NULL || result == ZR_NULL) {
        return;
    }
    if (entries->length > 1) {
        qsort(entries->head,
              entries->length,
              sizeof(SZrLspSemanticTokenEntry),
              semantic_token_entry_compare);
    }

    /* TODO: 这里只与上一条保留项比较；需用可达的多重重叠输入核查替换较宽 token 后，
     * 早先因宽 token 被抑制、却不与新 token 相交的候选是否会永久丢失。 */
    for (TZrSize index = 0; index < entries->length; index++) {
        SZrLspSemanticTokenEntry *entry =
            (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(entries, index);

        if (entry == ZR_NULL) {
            continue;
        }
        if (retainedLength > 0) {
            SZrLspSemanticTokenEntry *previous =
                (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(
                        entries, retainedLength - 1);
            if (semantic_token_entries_overlap(previous, entry)) {
                if (semantic_token_entry_is_preferred(entry, previous)) {
                    *previous = *entry;
                }
                continue;
            }
        }
        if (retainedLength != index) {
            SZrLspSemanticTokenEntry *retained =
                (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(
                        entries, retainedLength);
            if (retained == ZR_NULL) {
                continue;
            }
            *retained = *entry;
        }
        retainedLength++;
    }

    entries->length = retainedLength;
    for (TZrSize index = 0; index < entries->length; index++) {
        SZrLspSemanticTokenEntry *entry =
            (SZrLspSemanticTokenEntry *)ZrCore_Array_Get(entries, index);
        TZrUInt32 deltaLine;
        TZrUInt32 deltaCharacter;

        if (entry == ZR_NULL) {
            continue;
        }
        if (isFirst) {
            deltaLine = entry->line;
            deltaCharacter = entry->character;
            isFirst = ZR_FALSE;
        } else {
            deltaLine = entry->line - previousLine;
            deltaCharacter = deltaLine == 0
                                     ? entry->character - previousCharacter
                                     : entry->character;
        }
        ZrCore_Array_Push(state, result, &deltaLine);
        ZrCore_Array_Push(state, result, &deltaCharacter);
        ZrCore_Array_Push(state, result, &entry->length);
        ZrCore_Array_Push(state, result, &entry->typeIndex);
        ZrCore_Array_Push(state, result, &entry->modifiers);

        previousLine = entry->line;
        previousCharacter = entry->character;
    }
}
