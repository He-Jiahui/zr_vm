#include "zr_vm_parser/semantic_query.h"

#include <string.h>

static TZrBool semantic_query_imports_same_string(
        SZrString *left,
        SZrString *right) {
    return left == right ||
           (left != ZR_NULL && right != ZR_NULL &&
            ZrCore_String_Equal(left, right));
}

static TZrBool semantic_query_imports_same_range(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           semantic_query_imports_same_string(left->source, right->source) &&
           left->start.offset == right->start.offset &&
           left->end.offset == right->end.offset;
}

static TZrBool semantic_query_imports_range_contains(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    return range != ZR_NULL && position != ZR_NULL &&
           semantic_query_imports_same_string(range->source, position->source) &&
           range->start.offset <= position->start.offset &&
           range->end.offset >= position->start.offset;
}

static TZrBool semantic_query_imports_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    if (scope == ZR_NULL ||
        scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
           scope->root != ZR_NULL &&
           semantic_query_imports_range_contains(&scope->root->location, range) &&
           scope->root->location.end.offset >= range->end.offset;
}

static const SZrSemanticRelationFact *semantic_query_imports_find_relation(
        const SZrSemanticContext *context,
        const SZrSemanticVisibleSymbolFact *visible,
        const SZrSemanticSymbolRecord *symbol,
        TZrBool *outAmbiguous) {
    const SZrSemanticRelationFact *match = ZR_NULL;
    TZrSize index;

    *outAmbiguous = ZR_FALSE;
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *candidate =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);

        if (candidate == ZR_NULL ||
            candidate->kind != ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN ||
            candidate->sourceSymbolId != visible->symbolId ||
            candidate->sourceTypeId != symbol->typeId ||
            candidate->targetTypeId != symbol->typeId ||
            !candidate->isExternal) {
            continue;
        }
        if (match != ZR_NULL) {
            *outAmbiguous = ZR_TRUE;
            return ZR_NULL;
        }
        match = candidate;
    }
    return match;
}

EZrParserSemanticImportOriginResolution
ZrParser_SemanticQuery_ImportOriginAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticImportOriginQuery *outImport) {
    const SZrSemanticVisibleSymbolFact *firstVisible = ZR_NULL;
    const SZrSemanticRelationFact *firstRelation = ZR_NULL;
    TZrSize index;

    if (outImport != ZR_NULL) {
        memset(outImport, 0, sizeof(*outImport));
    }
    if (context == ZR_NULL || outImport == ZR_NULL ||
        !context->visibleSymbolFacts.isValid ||
        !context->relationFacts.isValid ||
        !semantic_query_imports_scope_allows(scope, &position)) {
        return ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE;
    }

    for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *visible =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->visibleSymbolFacts, index);
        const SZrSemanticSymbolRecord *symbol;
        const SZrSemanticRelationFact *relation;
        TZrBool ambiguous = ZR_FALSE;

        if (visible == ZR_NULL || !visible->isImport ||
            !visible->hasExternalOriginRange ||
            !semantic_query_imports_range_contains(
                    &visible->externalOriginRange, &position) ||
            !semantic_query_imports_scope_allows(
                    scope, &visible->externalOriginRange)) {
            continue;
        }
        symbol = ZrParser_Semantic_FindSymbolById(context, visible->symbolId);
        relation = symbol != ZR_NULL
                ? semantic_query_imports_find_relation(
                          context, visible, symbol, &ambiguous)
                : ZR_NULL;
        if (visible->externalOriginUri == ZR_NULL || symbol == ZR_NULL ||
            symbol->typeId == ZR_SEMANTIC_ID_INVALID || relation == ZR_NULL ||
            ambiguous || relation->externalOriginUri == ZR_NULL ||
            !relation->hasSourceRange ||
            !semantic_query_imports_same_range(
                    &relation->sourceRange, &symbol->location) ||
            !semantic_query_imports_same_string(
                    visible->externalOriginUri, relation->externalOriginUri)) {
            memset(outImport, 0, sizeof(*outImport));
            return ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_INVALID;
        }
        if (firstVisible != ZR_NULL &&
            (!semantic_query_imports_same_range(
                    &firstVisible->externalOriginRange,
                    &visible->externalOriginRange) ||
             !semantic_query_imports_same_string(
                    firstVisible->externalOriginUri,
                    visible->externalOriginUri) ||
             !semantic_query_imports_same_string(
                    firstRelation->virtualDeclarationUri,
                    relation->virtualDeclarationUri))) {
            memset(outImport, 0, sizeof(*outImport));
            return ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_INVALID;
        }
        if (firstVisible == ZR_NULL) {
            firstVisible = visible;
            firstRelation = relation;
        }
    }

    if (firstVisible == ZR_NULL || firstRelation == ZR_NULL) {
        return ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE;
    }
    outImport->referenceRange = firstVisible->externalOriginRange;
    outImport->externalOriginUri = firstVisible->externalOriginUri;
    outImport->virtualDeclarationUri = firstRelation->virtualDeclarationUri;
    return ZR_PARSER_SEMANTIC_IMPORT_ORIGIN_RESOLVED;
}
