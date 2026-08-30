#include "semantic_relations_order.h"

#include <string.h>

static TZrInt32 semantic_relations_order_strings(
        const SZrString *left,
        const SZrString *right) {
    if (left == right) {
        return 0;
    }
    if (left == ZR_NULL) {
        return -1;
    }
    if (right == ZR_NULL) {
        return 1;
    }
    return strcmp(ZrCore_String_GetNativeString(left),
                  ZrCore_String_GetNativeString(right));
}

static TZrInt32 semantic_relations_order_ranges(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    TZrInt32 sourceOrder = semantic_relations_order_strings(
            left->source, right->source);

    if (sourceOrder != 0) {
        return sourceOrder;
    }
    if (left->start.offset > 0U || left->end.offset > 0U ||
        right->start.offset > 0U || right->end.offset > 0U) {
        if (left->start.offset != right->start.offset) {
            return left->start.offset < right->start.offset ? -1 : 1;
        }
        if (left->end.offset != right->end.offset) {
            return left->end.offset < right->end.offset ? -1 : 1;
        }
        return 0;
    }
    if (left->start.line != right->start.line) {
        return left->start.line < right->start.line ? -1 : 1;
    }
    if (left->start.column != right->start.column) {
        return left->start.column < right->start.column ? -1 : 1;
    }
    if (left->end.line != right->end.line) {
        return left->end.line < right->end.line ? -1 : 1;
    }
    if (left->end.column != right->end.column) {
        return left->end.column < right->end.column ? -1 : 1;
    }
    return 0;
}

static TZrBool semantic_relations_query_precedes(
        const SZrParserSemanticRelationQuery *left,
        const SZrParserSemanticRelationQuery *right) {
    TZrInt32 order;

    if (left->kind != right->kind) {
        return left->kind < right->kind;
    }
    if (left->sourceSymbolId != right->sourceSymbolId) {
        return left->sourceSymbolId < right->sourceSymbolId;
    }
    if (left->targetSymbolId != right->targetSymbolId) {
        return left->targetSymbolId < right->targetSymbolId;
    }
    if (left->sourceTypeId != right->sourceTypeId) {
        return left->sourceTypeId < right->sourceTypeId;
    }
    if (left->targetTypeId != right->targetTypeId) {
        return left->targetTypeId < right->targetTypeId;
    }
    order = semantic_relations_order_strings(
            left->sourceModuleIdentity, right->sourceModuleIdentity);
    if (order != 0) {
        return order < 0;
    }
    order = semantic_relations_order_strings(
            left->targetModuleIdentity, right->targetModuleIdentity);
    if (order != 0) {
        return order < 0;
    }
    if (left->hasSourceRange != right->hasSourceRange) {
        return left->hasSourceRange < right->hasSourceRange;
    }
    if (left->hasSourceRange) {
        order = semantic_relations_order_ranges(
                &left->sourceRange, &right->sourceRange);
        if (order != 0) {
            return order < 0;
        }
    }
    if (left->hasTargetRange != right->hasTargetRange) {
        return left->hasTargetRange < right->hasTargetRange;
    }
    if (left->hasTargetRange) {
        order = semantic_relations_order_ranges(
                &left->targetRange, &right->targetRange);
        if (order != 0) {
            return order < 0;
        }
    }
    if (left->isExternal != right->isExternal) {
        return left->isExternal < right->isExternal;
    }
    order = semantic_relations_order_strings(
            left->externalOriginUri, right->externalOriginUri);
    if (order != 0) {
        return order < 0;
    }
    order = semantic_relations_order_strings(
            left->virtualDeclarationUri, right->virtualDeclarationUri);
    return order <= 0;
}

void ZrParser_SemanticRelations_SortQueries(SZrArray *relations) {
    TZrSize index;

    if (relations == ZR_NULL) {
        return;
    }
    for (index = 1U; index < relations->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticRelationQuery *before =
                    (SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            relations, current - 1U);
            SZrParserSemanticRelationQuery *after =
                    (SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            relations, current);
            SZrParserSemanticRelationQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_relations_query_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}
