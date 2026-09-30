#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_SYMBOLS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_SYMBOLS_H

#include "zr_vm_parser/semantic.h"
/** 标记尚未绑定到 owner 符号槽的无效索引。 */
#define ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID ((TZrSize)-1)
/** @brief 一个带 ownership qualifier 的符号流槽。 @note ownerIndex 初始为 INVALID，region 绑定后可指向本表中的 owner 槽。 */
typedef struct SZrSemanticOwnershipSymbolEntry {
    TZrSymbolId symbolId;
    EZrOwnershipQualifier qualifier;
    TZrLifetimeRegionId regionId;
    TZrSize ownerIndex;
} SZrSemanticOwnershipSymbolEntry;
/** @brief 一次控制流所有权分析期间按 symbolId 去重的临时槽表。 @note entries 缓冲归 map 持有，条目地址在释放或扩容后失效。 */
typedef struct SZrSemanticOwnershipSymbolMap {
    SZrArray entries;
} SZrSemanticOwnershipSymbolMap;

void ZrParser_DataflowOwnership_SymbolMapConstruct(SZrSemanticOwnershipSymbolMap *map); /**< @brief 构造供单次所有权分析使用的空表；@pre map 尚未持有活动 entries 缓冲。 */
TZrBool ZrParser_DataflowOwnership_SymbolMapBuild(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map); /**< @brief 从已解析引用事实建立本轮槽表；@pre map 已 Construct，context、state 与 referenceFacts 有效；@return 参数无效或声明 fact 写入失败返回 false，失败后的部分表仍须 Free；@note 初始 Array_Init OOM 与 compiler 忽略后续 resolver OOM 的影响见 C 注释；LSP 诊断完整性策略仍待确认。 */
void ZrParser_DataflowOwnership_SymbolMapFree(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map); /**< @brief 释放 Build 建立的 entries；@pre 使用创建数组时所属的 semanticContext，Build 失败后也清理部分结果。 */
TZrBool ZrParser_DataflowOwnership_SymbolFind(
        const SZrSemanticOwnershipSymbolMap *map,
        TZrSymbolId symbolId,
        TZrSize *outIndex); /**< @brief 按语义 symbolId 查找本轮流状态槽；@return 找到返回 true 并写出可选索引；失败时 outIndex（若提供）置零。 */
SZrSemanticOwnershipSymbolEntry *ZrParser_DataflowOwnership_SymbolEntry(
        SZrSemanticOwnershipSymbolMap *map,
        TZrSize index); /**< @brief 读取或就地更新槽元数据；@pre index 来自同一已构建 map；@return 借用表内条目，释放或扩容后失效，越界返回 NULL。 */

#endif
