#ifndef ZR_VM_PARSER_PROPERTY_MIGRATION_H
#define ZR_VM_PARSER_PROPERTY_MIGRATION_H

#include "zr_vm_parser/parser.h"
#include "zr_vm_core/array.h"

/** 迁移诊断暂存的旧式属性节点及其原始成员序号；成功入队后由收集器持有。 */
typedef struct SZrLegacyPropertyMigrationEntry {
    SZrAstNode *declaration;
    TZrUInt32 memberOrdinal;
} SZrLegacyPropertyMigrationEntry;

/** class/interface 成员解析期间的临时所有者。
 * nextMemberOrdinal 对每个正式成员和旧式属性都推进，使配对只允许相邻访问器。 */
typedef struct SZrLegacyPropertyMigrationCollection {
    SZrArray entries;
    TZrUInt32 nextMemberOrdinal;
} SZrLegacyPropertyMigrationCollection;

/** @brief 为一次 class/interface 成员扫描准备旧属性诊断收集器。
 * @pre state 与 collection 属于当前解析过程，后续需调用 publish_and_free。 */
void parser_property_migration_collection_init(
        SZrState *state,
        SZrLegacyPropertyMigrationCollection *collection);

/** @brief 暂存一个旧式 getter/setter AST，供相邻配对与替换建议使用。
 * @return 成功时收集器接管 declaration；失败时调用方仍负责释放。
 * @note BUG: 当前底层数组分配失败不总能被此返回值反映，见实现处的 OOM 标记。 */
TZrBool parser_property_migration_collection_append(
        SZrState *state,
        SZrLegacyPropertyMigrationCollection *collection,
        SZrAstNode *declaration);

/** @brief 记录一个已进入正式 AST 的普通成员，阻断跨成员的旧访问器配对。 */
void parser_property_migration_collection_mark_current_member(
        SZrLegacyPropertyMigrationCollection *collection);

/** @brief 发布旧属性的结构化迁移诊断并释放所有暂存节点。
 * @pre 必须在 class/interface 成员循环结束、解析器源文本仍有效时调用。 */
void parser_property_migration_collection_publish_and_free(
        SZrParserState *ps,
        SZrLegacyPropertyMigrationCollection *collection);

#endif
