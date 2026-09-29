#include "zr_vm_parser/semantic_query.h"

#include <string.h>

/*
 * 源身份按可选身份比较：同一字符串指针或相同字符串值均匹配，两个 NULL 也视为相同；
 * 是否存在真实可定位范围由调用方另行决定。
 */
static TZrBool semantic_property_same_source(
        SZrString *left,
        SZrString *right) {
    return (TZrBool)(
            left == right ||
            (left != ZR_NULL &&
             right != ZR_NULL &&
             ZrCore_String_Equal(left, right)));
}

/*
 * 比较同源范围：两边都有非零字节偏移时按偏移判断，否则以起止行列比较。
 * 该 helper 用于节点作用域以及 PropertyAt 的引用/声明定位。
 */
static TZrBool semantic_property_range_contains(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_property_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }
    if ((range->start.offset > 0U || range->end.offset > 0U) &&
        (position->start.offset > 0U || position->end.offset > 0U)) {
        return (TZrBool)(range->start.offset <= position->start.offset &&
                         position->end.offset <= range->end.offset);
    }
    return (TZrBool)(
            (range->start.line < position->start.line ||
             (range->start.line == position->start.line &&
              range->start.column <= position->start.column)) &&
            (position->end.line < range->end.line ||
             (position->end.line == range->end.line &&
              position->end.column <= range->end.column)));
}

/* 模块作用域不裁剪结果；节点作用域要求节点位置包住完整目标范围且源身份一致。 */
static TZrBool semantic_property_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    return (TZrBool)(
            scope == ZR_NULL ||
            scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE ||
            (scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
             scope->root != ZR_NULL &&
             semantic_property_range_contains(&scope->root->location, range)));
}

/* accessor 与其 value 参数都回指同一可见 property；查询只接受这些规范身份，不从名称猜测。 */
static TZrBool semantic_property_matches_symbol(
        const SZrSemanticPropertyContract *contract,
        TZrSymbolId symbolId) {
    return (TZrBool)(
            contract != ZR_NULL && symbolId != ZR_SEMANTIC_ID_INVALID &&
            (contract->propertySymbolId == symbolId ||
             contract->getterSymbolId == symbolId ||
             contract->setterSymbolId == symbolId ||
             contract->initializerSymbolId == symbolId ||
             contract->setterValueSymbolId == symbolId ||
             contract->initializerValueSymbolId == symbolId));
}

/**
 * @brief 由可见 property、accessor 或 value 参数的稳定 SymbolId 取回其规范契约。
 * @pre symbolId 必须来自 context 当前快照；Reset 会清空记录并重置编号分配器，旧 ID 可能与新记录数值重合。
 * @return 有效 outQuery 会先清零；命中时按值复制到该输出。
 * @note 不依赖源位置，适用于已获得规范身份的导入成员；范围内的 source 字符串仍由语义快照借用。
 */
TZrBool ZrParser_SemanticQuery_PropertyBySymbolId(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrParserSemanticPropertyQuery *outQuery) {
    if (outQuery != ZR_NULL) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->propertyContracts.isValid) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        (SZrArray *)&context->propertyContracts,
                        index);
        if (semantic_property_matches_symbol(contract, symbolId)) {
            /*
             * TODO: 此分支仅按 SymbolId 复制已发布行；基础 publisher 未校验 accessor callable TypeId 与 accessor symbol 的关系。
             * 正常 parser 编译在成功返回前另调用 SemanticRelations_PublishPropertyContracts；LSP 则先在
             * PrepareState/BootstrapTypePrototypes 绑定并发布 property，再于 CollectSymbols 注册源符号时经 PropertyAt 查询；
             * 稍后的 PublishCompilerContracts 是独立阶段，不是该 accessor 校验器。需明确这段中间态查询契约并补 LSP 用例；
             * 当前证据未证明 LSP 实际返回失配结果。
             */
            *outQuery = *contract;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 先用解析得到的引用身份解析 property，再以 selection/declaration range 作声明位置回退。
 * @pre position 与 context 属于同一个语义快照；LSP 调用必须携带当前文档 URI 以避免跨源命中。
 * @pre node scope 的 root 必须是同一仍存活 AST 快照中的节点；查询会读取 root->location。
 * @return 有效 outQuery 会先清零；命中时复制完整契约。
 */
TZrBool ZrParser_SemanticQuery_PropertyAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticPropertyQuery *outQuery) {
    const SZrSemanticReferenceFact *reference;

    if (outQuery != ZR_NULL) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        !semantic_property_scope_allows(scope, &position)) {
        return ZR_FALSE;
    }
    /* 精确引用事实优先：它已解析出可见 property/accessor SymbolId，避免只凭位置重建关联。 */
    if (context->referenceFacts.isValid) {
        for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
            const SZrSemanticReferenceFact *candidate =
                    (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->referenceFacts,
                            index);
            if (candidate != ZR_NULL && candidate->isResolved &&
                semantic_property_range_contains(&candidate->range, &position) &&
                semantic_property_scope_allows(scope, &candidate->range) &&
                ZrParser_SemanticQuery_PropertyBySymbolId(
                        context,
                        candidate->symbolId,
                        outQuery)) {
                return ZR_TRUE;
            }
        }
    }
    /* 兼容未进入上述已解析成员事实数组、但仍能由通用引用索引定位的引用。 */
    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(
            context,
            position);
    if (reference != ZR_NULL &&
        semantic_property_scope_allows(scope, &reference->range) &&
        ZrParser_SemanticQuery_PropertyBySymbolId(
                context,
                reference->symbolId,
                outQuery)) {
        return ZR_TRUE;
    }
    if (!context->propertyContracts.isValid) {
        return ZR_FALSE;
    }
    /* 无引用事实时才按声明名或声明主体范围定位，保留 PropertySymbol 作为唯一结果来源。 */
    for (TZrSize index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        (SZrArray *)&context->propertyContracts,
                        index);
        /*
         * TODO: 二进制导入契约使用全零、无 source 的 unavailableRange；同样全零的无 source 查询会被
         * range_contains 当作命中并返回数组首个导入属性。先在 runtime-bootstrap 测试中固定该输入的预期，
         * 再明确空位置应否禁止进入此声明范围回退，避免把无定位信息误作某个属性的位置。
         */
        if (contract != ZR_NULL &&
            semantic_property_scope_allows(scope, &contract->declarationRange) &&
            (semantic_property_range_contains(
                     &contract->selectionRange,
                     &position) ||
             semantic_property_range_contains(
                     &contract->declarationRange,
                     &position))) {
            *outQuery = *contract;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}
