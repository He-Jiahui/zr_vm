#include "zr_vm_parser/semantic_query.h"

#include <string.h>

/* URI/源码字符串可能分别分配，先允许指针快路径，再比较内容。 */
static TZrBool semantic_query_imports_same_string(
        SZrString *left,
        SZrString *right) {
    return left == right ||
           (left != ZR_NULL && right != ZR_NULL &&
            ZrCore_String_Equal(left, right));
}

/* 来源范围身份按 source 与字节偏移判断；行列坐标不是关系匹配键。 */
static TZrBool semantic_query_imports_same_range(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           semantic_query_imports_same_string(left->source, right->source) &&
           left->start.offset == right->start.offset &&
           left->end.offset == right->end.offset;
}

/* 查询范围按光标位置解释，只使用 position.start；来源末端按当前实现包含在命中范围内。 */
static TZrBool semantic_query_imports_range_contains(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    return range != ZR_NULL && position != ZR_NULL &&
           semantic_query_imports_same_string(range->source, position->source) &&
           range->start.offset <= position->start.offset &&
           range->end.offset >= position->start.offset;
}

/* module scope 不收窄查询；node scope 要求光标及完整导入来源范围都落在根节点内。 */
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

/* 只接受可见导入符号与其唯一外部来源关系在符号身份和 TypeId 上一致的配对。 */
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

/** @brief 按源码光标位置查找 import 字面量的外部来源关系。
 * @return context/out 缺失、事实数组未初始化、未命中或 scope 不匹配返回 NOT_APPLICABLE；命中后事实缺项、冲突或歧义返回 INVALID；唯一一致关系返回 RESOLVED。
 * @note 输出在查询前清零；成功时 range.source 与 URI 借用 context fact，virtualDeclarationUri 可为空，context reset/free 后失效。
 */
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

    /* 连接可见导入来源、符号和关系事实；任一不一致都清空输出，避免下游消费半条目标。 */
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
