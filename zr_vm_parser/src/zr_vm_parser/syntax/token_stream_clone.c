#include "syntax/token_stream_internal.h"
#include "syntax/syntax_context_internal.h"

#include <limits.h>
#include <string.h>

typedef struct SZrTokenSourceMap {
    TZrTokenSourceId *ids;
    TZrSize count;
    const SZrTokenStream *source;
} SZrTokenSourceMap;

static TZrBool token_stream_clone_source(
        SZrTokenStream *destination,
        const SZrTokenStream *source,
        TZrTokenSourceId sourceId,
        SZrTokenSourceMap *map,
        TZrTokenSourceId *outSourceId) {
    const SZrTokenSourceRecord *record;
    TZrTokenSourceId mapped;

    if (map == ZR_NULL || outSourceId == ZR_NULL ||
        sourceId == ZR_TOKEN_SOURCE_INVALID || sourceId > source->sourceCount ||
        source != map->source || sourceId > map->count) {
        return ZR_FALSE;
    }
    mapped = map->ids[sourceId];
    if (mapped != ZR_TOKEN_SOURCE_INVALID) {
        *outSourceId = mapped;
        return ZR_TRUE;
    }
    record = &source->sources[sourceId - 1u];
    if (!token_stream_import_source(destination, record, &mapped)) {
        return ZR_FALSE;
    }
    map->ids[sourceId] = mapped;
    *outSourceId = mapped;
    return ZR_TRUE;
}

static TZrBool token_stream_remap_text(
        SZrTokenStream *destination,
        const SZrTokenStream *source,
        SZrTokenText input,
        SZrTokenSourceMap *map,
        SZrTokenText *outText) {
    TZrTokenSourceId sourceId;

    if (input.sourceId == ZR_TOKEN_SOURCE_INVALID ||
        !token_stream_clone_source(destination, source, input.sourceId,
                                    map, &sourceId)) {
        return ZR_FALSE;
    }
    *outText = input;
    outText->sourceId = sourceId;
    return token_stream_valid_source_text(destination, *outText);
}

static TZrBool token_stream_remap_span(
        SZrTokenStream *destination,
        const SZrTokenStream *source,
        SZrTokenSpan input,
        SZrTokenSourceMap *map,
        SZrTokenSpan *outSpan) {
    TZrTokenSourceId sourceId;

    if (!token_stream_clone_source(destination, source, input.sourceId,
                                   map, &sourceId)) {
        return ZR_FALSE;
    }
    *outSpan = input;
    outSpan->sourceId = sourceId;
    return token_stream_valid_span(destination, *outSpan);
}

static TZrBool token_stream_clone_tree(
        SZrTokenStream *destination,
        const SZrTokenTree *sourceTree,
        SZrTokenSourceMap *map,
        const SZrTokenTree **outTree);

static TZrBool token_stream_clone_trivia(
        SZrTokenStream *destination,
        const SZrTokenStream *source,
        const SZrTokenTrivia *input,
        TZrSize count,
        SZrTokenSourceMap *map,
        SZrTokenTrivia **outTrivia) {
    TZrSize bytes;

    *outTrivia = ZR_NULL;
    if (count == 0u) {
        return ZR_TRUE;
    }
    if (input == ZR_NULL || !token_stream_size_mul(count, sizeof(*input), &bytes)) {
        return ZR_FALSE;
    }
    *outTrivia = token_stream_alloc(
            destination, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (*outTrivia == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < count; index++) {
        (*outTrivia)[index].kind = input[index].kind;
        if (!token_stream_remap_text(destination, source, input[index].text,
                                     map, &(*outTrivia)[index].text)) {
            token_stream_free(destination, *outTrivia, bytes,
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            *outTrivia = ZR_NULL;
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool token_stream_make_group(
        SZrTokenStream *stream,
        EZrTokenDelimiter delimiter,
        SZrTokenSpan openSpan,
        SZrTokenSpan closeSpan,
        SZrTokenTree *const *children,
        TZrSize childCount,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    SZrParserComptimeBudgetTransaction transaction;
    SZrTokenTree *tree;
    TZrSize childBytes;
    TZrSize leadingBytes;
    TZrSize trailingBytes;
    TZrSize heapBytes;

    if (stream == ZR_NULL || stream->sealed || outIndex == ZR_NULL ||
        delimiter < ZR_TOKEN_DELIMITER_NONE || delimiter > ZR_TOKEN_DELIMITER_BRACKET ||
        (childCount != 0u && children == ZR_NULL) ||
        !token_stream_valid_span(stream, span) ||
        (delimiter != ZR_TOKEN_DELIMITER_NONE &&
         (!token_stream_valid_span(stream, openSpan) ||
          !token_stream_valid_span(stream, closeSpan))) ||
        !token_stream_context_valid(stream, context) ||
        !token_stream_size_mul(leadingCount, sizeof(*leading), &leadingBytes) ||
        !token_stream_size_mul(trailingCount, sizeof(*trailing), &trailingBytes) ||
        !token_stream_size_mul(childCount, sizeof(*children), &childBytes) ||
        !token_stream_size_add(sizeof(*tree), childBytes, &heapBytes) ||
        !token_stream_size_add(heapBytes, leadingBytes, &heapBytes) ||
        !token_stream_size_add(heapBytes, trailingBytes, &heapBytes)) {
        return ZR_FALSE;
    }
    if (!token_stream_reserve(stream,
                              ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 1u,
                              ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT,
                              delimiter == ZR_TOKEN_DELIMITER_NONE ? 0u : 2u,
                              &transaction)) {
        return ZR_FALSE;
    }
    tree = token_stream_alloc(stream, sizeof(*tree), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (tree == ZR_NULL) {
        token_stream_abort(&transaction);
        return ZR_FALSE;
    }
    memset(tree, 0, sizeof(*tree));
    tree->owner = stream;
    tree->kind = ZR_TOKEN_TREE_GROUP;
    tree->span = span;
    tree->rawText.sourceId = span.sourceId;
    tree->rawText.offset = span.startOffset;
    tree->rawText.length = span.endOffset - span.startOffset;
    tree->context = context.table == ZR_NULL
                            ? ZrParser_SyntaxContext_TableRoot(stream->contextTable)
                            : context;
    tree->delimiter = delimiter;
    tree->openSpan = openSpan;
    tree->closeSpan = closeSpan;
    if (childCount != 0u) {
        tree->children = token_stream_alloc(
                stream, childBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (tree->children == ZR_NULL) {
            token_tree_destroy(stream, tree);
            token_stream_abort(&transaction);
            return ZR_FALSE;
        }
        memcpy(tree->children, children, childBytes);
    }
    tree->childCount = childCount;
    tree->leadingCount = leadingCount;
    tree->trailingCount = trailingCount;
    if (!token_stream_copy_trivia(stream, leading, leadingCount,
                                  &tree->leading) ||
        !token_stream_copy_trivia(stream, trailing, trailingCount,
                                  &tree->trailing)) {
        token_tree_destroy(stream, tree);
        token_stream_abort(&transaction);
        return ZR_FALSE;
    }
    if (!token_stream_ensure_trees(stream, stream->treeCount + 1u) ||
        !token_stream_ensure_roots(stream, stream->rootCount + 1u)) {
        token_tree_destroy(stream, tree);
        token_stream_abort(&transaction);
        return ZR_FALSE;
    }
    if (!token_stream_commit(&transaction)) {
        token_tree_destroy(stream, tree);
        return ZR_FALSE;
    }
    stream->trees[stream->treeCount++] = tree;
    stream->roots[stream->rootCount] = tree;
    *outIndex = stream->rootCount++;
    return ZR_TRUE;
}

static TZrBool token_stream_clone_node(
        SZrTokenStream *destination,
        const SZrTokenTree *input,
        SZrTokenSourceMap *map,
        SZrTokenTree *const *children,
        const SZrTokenTree **outTree) {
    SZrTokenTrivia *leading = ZR_NULL;
    SZrTokenTrivia *trailing = ZR_NULL;
    SZrTokenTrivia *innerTrivia = ZR_NULL;
    SZrTokenNativeSemInfo semInfo = input->semInfo;
    SZrTokenText rawText;
    SZrTokenSpan span;
    SZrTokenSpan openSpan = ZrParser_TokenSpan_None();
    SZrTokenSpan closeSpan = ZrParser_TokenSpan_None();
    SZrSyntaxContext context = input->context;
    TZrSize outIndex;
    TZrBool success = ZR_FALSE;
    const SZrTokenStream *source = input->owner;

    if (!token_stream_remap_span(destination, source, input->span, map, &span) ||
        !ZrParser_SyntaxContext_Import(destination->contextTable, context, &context) ||
        !token_stream_clone_trivia(destination, source, input->leading,
                                   input->leadingCount, map, &leading) ||
        !token_stream_clone_trivia(destination, source, input->trailing,
                                   input->trailingCount, map, &trailing) ||
        !token_stream_clone_trivia(destination, source, input->innerTrivia,
                                   input->innerTriviaCount, map, &innerTrivia)) {
        goto cleanup;
    }
    if (input->kind == ZR_TOKEN_TREE_GROUP) {
        if (input->delimiter != ZR_TOKEN_DELIMITER_NONE &&
            (!token_stream_remap_span(destination, source, input->openSpan,
                                      map, &openSpan) ||
             !token_stream_remap_span(destination, source, input->closeSpan,
                                      map, &closeSpan))) {
            goto cleanup;
        }
        success = token_stream_make_group(
                destination, input->delimiter, openSpan, closeSpan,
                children, input->childCount, span, context,
                leading, input->leadingCount, trailing, input->trailingCount, &outIndex);
    } else {
        if (!token_stream_remap_text(destination, source, input->rawText, map, &rawText) ||
            (semInfo.text.sourceId != ZR_TOKEN_SOURCE_INVALID &&
             !token_stream_remap_text(destination, source, semInfo.text, map, &semInfo.text)) ||
            (semInfo.suffix.sourceId != ZR_TOKEN_SOURCE_INVALID &&
             !token_stream_remap_text(destination, source, semInfo.suffix, map, &semInfo.suffix))) {
            goto cleanup;
        }
        switch (input->kind) {
            case ZR_TOKEN_TREE_IDENT:
                success = ZrParser_TokenStream_AppendIdent(
                        destination, semInfo, rawText, span, context, leading,
                        input->leadingCount, trailing, input->trailingCount, &outIndex);
                break;
            case ZR_TOKEN_TREE_LITERAL:
                success = ZrParser_TokenStream_AppendLiteral(
                        destination, semInfo, rawText, span, context, leading,
                        input->leadingCount, trailing, input->trailingCount, &outIndex);
                break;
            case ZR_TOKEN_TREE_PUNCT:
                success = ZrParser_TokenStream_AppendPunct(
                        destination, semInfo, rawText, input->punctCodepoint,
                        input->punctSpacing, span, context, leading, input->leadingCount,
                        trailing, input->trailingCount, &outIndex);
                break;
            default:
                break;
        }
    }
    if (success) {
        destination->roots[outIndex]->innerTrivia = innerTrivia;
        destination->roots[outIndex]->innerTriviaCount = input->innerTriviaCount;
        innerTrivia = ZR_NULL;
        *outTree = destination->roots[outIndex];
    }
cleanup:
    token_stream_free(destination, leading, input->leadingCount * sizeof(*leading),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    token_stream_free(destination, trailing, input->trailingCount * sizeof(*trailing),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    token_stream_free(destination, innerTrivia,
                      input->innerTriviaCount * sizeof(*innerTrivia),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return success;
}

typedef struct SZrTokenCloneFrame {
    const SZrTokenTree *input;
    SZrTokenTree **children;
    TZrSize childIndex;
    TZrSize rootBase;
} SZrTokenCloneFrame;

static TZrBool token_stream_clone_tree(
        SZrTokenStream *destination,
        const SZrTokenTree *sourceTree,
        SZrTokenSourceMap *map,
        const SZrTokenTree **outTree) {
    SZrTokenCloneFrame *frames = ZR_NULL;
    TZrSize count = 0u;
    TZrSize capacity = 0u;
    const SZrTokenTree *nextInput = sourceTree;
    const SZrTokenTree *result = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (destination == ZR_NULL || sourceTree == ZR_NULL || map == ZR_NULL ||
        outTree == ZR_NULL) {
        return ZR_FALSE;
    }
    for (;;) {
        if (nextInput != ZR_NULL) {
            TZrSize childBytes;
            SZrTokenCloneFrame *frame;
            if (nextInput->owner != map->source ||
                !token_stream_size_mul(nextInput->childCount,
                                       sizeof(*frame->children), &childBytes)) {
                goto cleanup;
            }
            if (count == capacity) {
                TZrSize nextCapacity = capacity == 0u ? 16u : capacity * 2u;
                TZrSize bytes;
                SZrTokenCloneFrame *next;
                if (nextCapacity < capacity ||
                    !token_stream_size_mul(nextCapacity, sizeof(*next), &bytes) ||
                    (next = token_stream_alloc(destination, bytes,
                                               ZR_MEMORY_NATIVE_TYPE_ARRAY)) == ZR_NULL) {
                    goto cleanup;
                }
                if (count != 0u) {
                    memcpy(next, frames, count * sizeof(*next));
                }
                token_stream_free(destination, frames, capacity * sizeof(*frames),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                frames = next;
                capacity = nextCapacity;
            }
            frame = &frames[count++];
            memset(frame, 0, sizeof(*frame));
            frame->input = nextInput;
            frame->rootBase = destination->rootCount;
            if (childBytes != 0u &&
                (frame->children = token_stream_alloc(destination, childBytes,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY)) == ZR_NULL) {
                goto cleanup;
            }
            nextInput = ZR_NULL;
        }
        {
            SZrTokenCloneFrame *frame = &frames[count - 1u];
            if (frame->childIndex < frame->input->childCount) {
                nextInput = frame->input->children[frame->childIndex];
                if (nextInput == ZR_NULL) {
                    goto cleanup;
                }
                continue;
            }
            destination->rootCount = frame->rootBase;
            if (!token_stream_clone_node(destination, frame->input, map,
                                          frame->children, &result)) {
                goto cleanup;
            }
            token_stream_free(destination, frame->children,
                              frame->input->childCount * sizeof(*frame->children),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            count--;
            if (count == 0u) {
                *outTree = result;
                success = ZR_TRUE;
                goto cleanup;
            }
            frame = &frames[count - 1u];
            frame->children[frame->childIndex++] = (SZrTokenTree *)result;
        }
    }
cleanup:
    for (TZrSize index = 0u; index < count; index++) {
        token_stream_free(destination, frames[index].children,
                          frames[index].input->childCount * sizeof(*frames[index].children),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    token_stream_free(destination, frames, capacity * sizeof(*frames),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return success;
}

static TZrBool token_stream_append_group_impl(
        SZrTokenStream *stream,
        EZrTokenDelimiter delimiter,
        SZrTokenSpan openSpan,
        SZrTokenSpan closeSpan,
        const SZrTokenTree *const *children,
        TZrSize childCount,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    const SZrTokenStream *source = ZR_NULL;
    SZrTokenSourceMap map = {0};
    TZrSize initialTreeCount;
    TZrSize initialRootCount;
    TZrSize mapBytes = 0u;
    TZrSize triviaBytes;
    TZrSize index;
    SZrTokenTree **ownedChildren = ZR_NULL;
    TZrSize childBytes;
    TZrBool success = ZR_FALSE;

    if (stream == ZR_NULL || stream->sealed || outIndex == ZR_NULL ||
        (childCount != 0u && children == ZR_NULL) ||
        !token_stream_valid_span(stream, span) ||
        !token_stream_context_valid(stream, context) ||
        (leadingCount != 0u && leading == ZR_NULL) ||
        (trailingCount != 0u && trailing == ZR_NULL) ||
        !token_stream_size_mul(leadingCount, sizeof(*leading), &triviaBytes) ||
        !token_stream_size_mul(trailingCount, sizeof(*trailing), &triviaBytes) ||
        (delimiter != ZR_TOKEN_DELIMITER_NONE &&
         (!token_stream_valid_span(stream, openSpan) ||
          !token_stream_valid_span(stream, closeSpan)))) {
        return ZR_FALSE;
    }
    if (childCount == 0u) {
        return token_stream_make_group(
                stream, delimiter, openSpan, closeSpan, ZR_NULL, 0u,
                span, context, leading, leadingCount, trailing, trailingCount,
                outIndex);
    }
    if (!token_stream_size_mul(childCount, sizeof(*ownedChildren), &childBytes)) {
        return ZR_FALSE;
    }
    ownedChildren = token_stream_alloc(
            stream, childBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (ownedChildren == ZR_NULL) {
        return ZR_FALSE;
    }
    initialTreeCount = stream->treeCount;
    initialRootCount = stream->rootCount;
    for (index = 0u; index < childCount; index++) {
        source = children[index] == ZR_NULL ? ZR_NULL : children[index]->owner;
        if (source == ZR_NULL || !source->sealed || source->global != stream->global) {
            goto group_rollback;
        }
        if (map.source != source) {
            token_stream_free(stream, map.ids, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
            map.ids = ZR_NULL;
            if (!token_stream_size_mul(
                        source->sourceCount + 1u, sizeof(*map.ids), &mapBytes) ||
                (map.ids = token_stream_alloc(
                         stream, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY)) == ZR_NULL) {
                goto group_rollback;
            }
            memset(map.ids, 0, mapBytes);
            map.count = source->sourceCount;
            map.source = source;
        }
        if (!token_stream_clone_tree(
                    stream, children[index], &map,
                    (const SZrTokenTree **)&ownedChildren[index])) {
            goto group_rollback;
        }
    }
    stream->rootCount = initialRootCount;
    if (!token_stream_make_group(
                stream, delimiter, openSpan, closeSpan, ownedChildren,
                childCount, span,
                context, leading, leadingCount, trailing, trailingCount,
                outIndex)) {
        goto group_rollback;
    }
    success = ZR_TRUE;
group_rollback:
    if (!success) {
        stream->rootCount = initialRootCount;
        while (stream->treeCount > initialTreeCount) {
            stream->treeCount--;
            token_tree_destroy(stream, stream->trees[stream->treeCount]);
            stream->trees[stream->treeCount] = ZR_NULL;
        }
    }
    token_stream_free(stream, map.ids, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    token_stream_free(stream, ownedChildren, childBytes,
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return success;
}

TZrBool ZrParser_TokenStream_AppendGroup(
        SZrTokenStream *stream,
        EZrTokenDelimiter delimiter,
        SZrTokenSpan openSpan,
        SZrTokenSpan closeSpan,
        const SZrTokenTree *const *children,
        TZrSize childCount,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    SZrTokenStreamBatch batch;
    TZrBool ownBatch;
    TZrBool success;
    TZrSize result;

    if (stream == ZR_NULL || stream->sealed || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    ownBatch = stream->batch == ZR_NULL;
    if (ownBatch) {
        token_stream_batch_begin(stream, &batch);
    }
    success = token_stream_append_group_impl(
            stream, delimiter, openSpan, closeSpan, children, childCount, span, context, leading, leadingCount, trailing, trailingCount, &result);
    if (ownBatch) {
        success = token_stream_batch_finish(&batch, success);
    }
    if (success) {
        *outIndex = result;
    }
    return success;
}

static TZrBool token_stream_merge_trivia(
        SZrTokenStream *stream, SZrTokenTrivia **target, TZrSize *targetCount,
        const SZrTokenTrivia *input, TZrSize inputCount, TZrBool prepend) {
    SZrTokenTrivia *merged;
    TZrSize count;
    TZrSize bytes;
    if (inputCount == 0u) {
        return ZR_TRUE;
    }
    if (!token_stream_size_add(*targetCount, inputCount, &count) ||
        !token_stream_size_mul(count, sizeof(*merged), &bytes) ||
        (merged = token_stream_alloc(stream, bytes,
                                     ZR_MEMORY_NATIVE_TYPE_ARRAY)) == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(merged + (prepend ? 0u : *targetCount), input,
           inputCount * sizeof(*input));
    if (*targetCount != 0u) {
        memcpy(merged + (prepend ? inputCount : 0u), *target,
               *targetCount * sizeof(**target));
    }
    token_stream_free(stream, *target, *targetCount * sizeof(**target),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    *target = merged;
    *targetCount = count;
    return ZR_TRUE;
}

static TZrBool token_stream_clone_input(
        SZrTokenStream *destination, const SZrTokenStream *input) {
    SZrTokenSourceMap map;
    SZrTokenTrivia *emptyTrivia = ZR_NULL;
    TZrSize firstRoot = destination->rootCount;
    TZrSize mapBytes;
    TZrBool success = ZR_FALSE;

    if (!token_stream_size_mul(input->sourceCount + 1u, sizeof(*map.ids), &mapBytes)) {
        return ZR_FALSE;
    }
    map.ids = token_stream_alloc(destination, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (map.ids == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(map.ids, 0, mapBytes);
    map.count = input->sourceCount;
    map.source = input;
    for (TZrSize index = 0u; index < input->sourceCount; index++) {
        TZrTokenSourceId id;
        if (!token_stream_clone_source(destination, input,
                                        (TZrTokenSourceId)(index + 1u), &map, &id)) {
            goto cleanup;
        }
    }
    for (TZrSize index = 0u; index < input->rootCount; index++) {
        const SZrTokenTree *tree;
        if (!token_stream_clone_tree(destination, input->roots[index], &map, &tree)) {
            goto cleanup;
        }
    }
    if (!token_stream_clone_trivia(destination, input, input->emptyTrivia,
                                   input->emptyTriviaCount, &map, &emptyTrivia)) {
        goto cleanup;
    }
    if (input->rootCount == 0u && destination->rootCount != 0u) {
        SZrTokenTree *last = destination->roots[destination->rootCount - 1u];
        if (!token_stream_merge_trivia(destination, &last->trailing,
                                       &last->trailingCount, emptyTrivia,
                                       input->emptyTriviaCount, ZR_FALSE)) {
            goto cleanup;
        }
    } else if (input->rootCount == 0u) {
        if (!token_stream_merge_trivia(destination, &destination->emptyTrivia,
                                       &destination->emptyTriviaCount, emptyTrivia,
                                       input->emptyTriviaCount, ZR_FALSE)) {
            goto cleanup;
        }
    } else if (destination->emptyTriviaCount != 0u) {
        SZrTokenTree *first = destination->roots[firstRoot];
        if (!token_stream_merge_trivia(destination, &first->leading,
                                       &first->leadingCount, destination->emptyTrivia,
                                       destination->emptyTriviaCount, ZR_TRUE)) {
            goto cleanup;
        }
        token_stream_free(destination, destination->emptyTrivia,
                          destination->emptyTriviaCount * sizeof(*destination->emptyTrivia),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        destination->emptyTrivia = ZR_NULL;
        destination->emptyTriviaCount = 0u;
    }
    if (ZrParser_TokenSpan_IsValid(input->endSpan) &&
        !token_stream_remap_span(destination, input, input->endSpan,
                                 &map, &destination->endSpan)) {
        goto cleanup;
    }
    success = ZR_TRUE;
cleanup:
    token_stream_free(destination, emptyTrivia,
                      input->emptyTriviaCount * sizeof(*emptyTrivia),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    token_stream_free(destination, map.ids, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return success;
}

TZrBool ZrParser_TokenStream_Concat(
        SZrTokenStream *left,
        SZrTokenStream *right,
        SZrParserComptimeBudget *budget,
        SZrTokenStream **outStream) {
    SZrParserComptimeBudget shadow;
    SZrParserComptimeBudgetUsage initialUsage;
    SZrSyntaxContextTable *table;
    SZrTokenStream *result;
    TZrBool success;

    if (outStream == ZR_NULL || left == ZR_NULL || right == ZR_NULL ||
        !left->sealed || !right->sealed || left->global != right->global) {
        return ZR_FALSE;
    }
    *outStream = ZR_NULL;
    if (budget != ZR_NULL) {
        shadow = *budget;
    } else {
        token_stream_default_budget(&shadow);
    }
    initialUsage = shadow.usage;
    table = ZrParser_SyntaxContextTable_Create(left->global, &shadow);
    result = table == ZR_NULL
                     ? ZR_NULL
                     : ZrParser_TokenStream_Create(left->global, table, &shadow);
    ZrParser_SyntaxContextTable_Release(table);
    success = result != ZR_NULL && token_stream_clone_input(result, left) &&
              token_stream_clone_input(result, right);
    if (success && budget != ZR_NULL) {
        success = token_stream_commit_shadow(budget, &initialUsage, &shadow);
    }
    if (!success) {
        if (budget != ZR_NULL && shadow.exceededResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
            budget->exceededResource = shadow.exceededResource;
            budget->exceededLimit = shadow.exceededLimit;
            budget->requestedUsage = shadow.requestedUsage;
        }
        ZrParser_TokenStream_Release(result);
        return ZR_FALSE;
    }
    result->budget = budget;
    result->contextTable->fallbackBudget = budget != ZR_NULL ? *budget : shadow;
    result->contextTable->budget = ZR_NULL;
    if (budget == ZR_NULL) {
        result->fallbackBudget = shadow;
    }
    ZrParser_TokenStream_Seal(result);
    *outStream = result;
    return ZR_TRUE;
}
