#ifndef ZR_VM_PARSER_SEMANTIC_RELATIONS_ORDER_H
#define ZR_VM_PARSER_SEMANTIC_RELATIONS_ORDER_H

#include "zr_vm_parser/semantic_query.h"

/**
 * @brief 按两端身份和来源信息稳定排序关系查询行。
 * @param relations 要重排的已初始化查询行数组，可以为 NULL。
 * @pre 非空数组的元素类型必须为 SZrParserSemanticRelationQuery。
 * @note 原地重排行数据，指针成员随行移动；本函数不复制、保留或释放其所指对象。
 */
void ZrParser_SemanticRelations_SortQueries(SZrArray *relations);

#endif // ZR_VM_PARSER_SEMANTIC_RELATIONS_ORDER_H
