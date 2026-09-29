#ifndef ZR_VM_PARSER_SEMANTIC_RELATIONS_IDENTITY_H
#define ZR_VM_PARSER_SEMANTIC_RELATIONS_IDENTITY_H

#include "zr_vm_parser/semantic.h"

/**
 * @brief 判断两个源范围是否表示同一完整位置。
 * @param left 第一个借用范围，可以为 NULL。
 * @param right 第二个借用范围，可以为 NULL。
 * @return 任一范围为空时返回 false；否则要求 offset、行、列全部相同；缺失 source 仅与缺失 source 相等，
 *         非空 source 按内容比较。
 */
TZrBool ZrParser_SemanticRelations_RangesEqual(
        const SZrFileRange *left,
        const SZrFileRange *right);

/**
 * @brief 判断两个关系事实是否具有相同的边身份。
 * @param left 第一个借用事实，可以为 NULL。
 * @param right 第二个借用事实，可以为 NULL。
 * @return 仅当关系种类、两端符号与类型 id、provider generation、范围标记及已提供的范围、
 *         external 状态和可选 URI 值均相同时返回 true；缺失 URI 只与另一个缺失 URI 相等。
 */
TZrBool ZrParser_SemanticRelations_FactsEqual(
        const SZrSemanticRelationFact *left,
        const SZrSemanticRelationFact *right);

#endif // ZR_VM_PARSER_SEMANTIC_RELATIONS_IDENTITY_H
