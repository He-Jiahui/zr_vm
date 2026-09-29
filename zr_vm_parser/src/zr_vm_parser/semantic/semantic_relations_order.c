#include "semantic_relations_order.h"

#include <string.h>

/**
 * @brief 按 C 字符串顺序比较可选身份与来源字段。
 * @note 缺失值排在非空值之前；非空字符串仅借用。
 * TODO: 核实所有关系身份与 URI 入口是否拒绝内嵌 NUL。SZrString 的 native view 可含 NUL，
 *       而 strcmp 会截断后续字节；下一步检查 canonical nominal 与 Append/import-origin 校验，
 *       若允许此值，再在 relation source identity 用例中验证完整字节排序。
 */
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

/**
 * @brief 先比较 source，再按可用的范围坐标排序。
 * @note 任一范围含非零 offset 时比较两端 offset；两范围的四个 offset 全为零时才比较行列。
 * TODO: 核实允许的生产路径是否会提供互不一致的 offset 与行列数据。相同 offset、不同坐标在此处并列，
 *       但范围身份会将它们区分；下一步审查 Append 的调用方，并在
 *       tests/parser/test_semantic_query_relation_source_identity_cases.h 增加混合坐标用例。
 */
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

/**
 * @brief 按结构化边身份与来源信息比较关系行。
 * @details 依次比较关系种类、两端符号与类型 id、模块身份、provider generation、范围标记与坐标、
 *          external 状态、来源 URI 和虚拟声明 URI。
 * @return 左侧键小于或等于右侧键时返回 true；插入排序因此保留并列行的既有顺序。
 */
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
    if (left->sourceProviderGeneration != right->sourceProviderGeneration) {
        return left->sourceProviderGeneration < right->sourceProviderGeneration;
    }
    if (left->targetProviderGeneration != right->targetProviderGeneration) {
        return left->targetProviderGeneration < right->targetProviderGeneration;
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

/**
 * @brief 按结构化边身份稳定排序关系查询行。
 * @pre relations 非空时必须是已初始化的查询行数组。
 * @note 使用插入排序原地重排，不分配空间、不复制或释放指针成员；空指针输入会被忽略。
 */
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
