#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_SPEC_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_SPEC_H

#include "compiler_internal.h"

/**
 * @brief TypeSpec 发射前的容量计划，供统一 metadata 构建器预留记录和签名堆。
 * @note 计数按规范化签名去重；调用方须以同一函数导出和 string heap 再执行 emit。
 */
typedef struct SZrMetadataTypeSpecPlan {
    TZrUInt32 typeSpecCount;
    TZrSize signatureHeapLength;
} SZrMetadataTypeSpecPlan;

/**
 * @brief 计算导出类型引用所需的唯一 TypeSpec 行数与签名堆字节数。
 * @pre stringHeapEntries 与后续 emit 使用的字符串堆必须属于同一轮 metadata 计划。
 * @return 无需 TypeSpec 时成功并返回零计划；无效输入、编码或分配失败时返回 false。
 */
TZrBool compiler_metadata_type_spec_plan(SZrCompilerState *cs,
                                         const SZrFunction *function,
                                         const SZrMetadataStringHeapEntry *stringHeapEntries,
                                         TZrUInt32 stringHeapEntryCount,
                                         SZrMetadataTypeSpecPlan *outPlan);

/**
 * @brief 按 plan 阶段的遍历与去重规则写入 TypeSpec/Signature 成对记录及签名字节。
 * @pre 调用方提供与计划相符且尚未越界的记录游标、Signature RID 游标和堆偏移。
 * @note 游标和已写记录不是事务式回滚；失败后由外层 metadata 构建流程整体丢弃产物。
 */
TZrBool compiler_metadata_type_spec_emit(SZrCompilerState *cs,
                                         const SZrFunction *function,
                                         SZrMetadataTokenRecord *records,
                                         TZrUInt32 recordCount,
                                         TZrUInt32 *ioRecordIndex,
                                         TZrByte *heap,
                                         TZrSize heapLength,
                                         TZrSize *ioHeapOffset,
                                         TZrUInt32 *ioSignatureRidCursor,
                                         const SZrMetadataStringHeapEntry *stringHeapEntries,
                                         TZrUInt32 stringHeapEntryCount);

#endif
