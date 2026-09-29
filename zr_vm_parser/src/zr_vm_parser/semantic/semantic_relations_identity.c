#include "semantic_relations_identity.h"

#include "zr_vm_core/string.h"

/**
 * @brief 按字符串内容比较可选 URI 字段，不取得其所有权。
 * @note 两端都未提供时相等；缺失值不等于任何非空字符串。
 */
static TZrBool semantic_relations_optional_strings_equal(
        const SZrString *left,
        const SZrString *right) {
    return (TZrBool)(left == right ||
                      (left != ZR_NULL && right != ZR_NULL &&
                       ZrCore_String_Equal((SZrString *)left,
                                           (SZrString *)right)));
}

/**
 * @brief 比较两个范围的源身份及全部坐标。
 * @pre 非空范围及其 source 字符串在调用期间有效。
 * @return 任一范围指针为空时返回 false；否则仅当 source、offset、行列坐标全部相同时返回 true。
 */
TZrBool ZrParser_SemanticRelations_RangesEqual(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->start.offset != right->start.offset ||
        left->end.offset != right->end.offset ||
        left->start.line != right->start.line ||
        left->start.column != right->start.column ||
        left->end.line != right->end.line ||
        left->end.column != right->end.column) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->source == right->source ||
                      (left->source != ZR_NULL && right->source != ZR_NULL &&
                       ZrCore_String_Equal(left->source, right->source)));
}

/**
 * @brief 比较两个关系事实的完整结构化边身份。
 * @note 仅在对应 has 标记为 true 时比较范围内容；可选 URI 按字符串内容比较。
 * TODO: 核实生产关系发布路径是否应填写 provider generation。现有聚焦用例直接设置这些字段；
 *       下一步检查 compiler_semantic_relations.c，并在
 *       tests/language_server/test_lsp_multi_project_provider_generation_cases.h 中断言真实重载关系边。
 */
TZrBool ZrParser_SemanticRelations_FactsEqual(
        const SZrSemanticRelationFact *left,
        const SZrSemanticRelationFact *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->kind != right->kind ||
        left->sourceSymbolId != right->sourceSymbolId ||
        left->targetSymbolId != right->targetSymbolId ||
        left->sourceTypeId != right->sourceTypeId ||
        left->targetTypeId != right->targetTypeId ||
        left->sourceProviderGeneration != right->sourceProviderGeneration ||
        left->targetProviderGeneration != right->targetProviderGeneration ||
        left->hasSourceRange != right->hasSourceRange ||
        left->hasTargetRange != right->hasTargetRange ||
        left->isExternal != right->isExternal ||
        (left->hasSourceRange &&
         !ZrParser_SemanticRelations_RangesEqual(
                 &left->sourceRange, &right->sourceRange)) ||
        (left->hasTargetRange &&
         !ZrParser_SemanticRelations_RangesEqual(
                 &left->targetRange, &right->targetRange))) {
        return ZR_FALSE;
    }
    return (TZrBool)(semantic_relations_optional_strings_equal(
                              left->externalOriginUri,
                              right->externalOriginUri) &&
                      semantic_relations_optional_strings_equal(
                              left->virtualDeclarationUri,
                              right->virtualDeclarationUri));
}
