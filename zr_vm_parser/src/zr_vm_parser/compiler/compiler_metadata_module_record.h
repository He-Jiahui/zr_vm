#ifndef ZR_VM_PARSER_COMPILER_METADATA_MODULE_RECORD_H
#define ZR_VM_PARSER_COMPILER_METADATA_MODULE_RECORD_H

#include "compiler_internal.h"
#include "compiler_metadata_type_def.h"

/**
 * @brief Module 记录阶段用于协调分配的临时容量计划。
 *
 * 两字段描述同一个 function 的输出规模；由 plan 填充、紧接着供 token
 * 构建器累计分配，不能跨构建复用或作为持久 metadata 保存。
 */
typedef struct SZrMetadataModuleRecordPlan {
    /** emit 将写入的 Module token 记录数；每个非空 function 为一条。 */
    TZrUInt32 moduleRecordCount;
    /** Module 签名在共享签名堆中占用的字节数，不含其它记录。 */
    TZrSize signatureHeapLength;
} SZrMetadataModuleRecordPlan;

/** @brief 收集 Module 签名引用的名称与版本，供统一 string heap 建表。 */
TZrBool compiler_metadata_module_record_collect_strings(SZrCompilerState *cs,
                                                        const SZrFunction *function,
                                                        TZrMetadataTypeDefStringCollector collector,
                                                        void *userData);

/**
 * @brief 计算 Module token 与签名堆的预留量。
 * @pre outPlan 必须指向可写计划；空 function 会得到零容量计划。
 */
TZrBool compiler_metadata_module_record_plan(SZrCompilerState *cs,
                                             const SZrFunction *function,
                                             SZrMetadataModuleRecordPlan *outPlan);

/**
 * @brief 将 Module token 及其 Signature 配对写入调用方的暂存输出。
 * @pre 记录容量至少容纳一对记录，heap 容量覆盖 plan，且 string heap 已完成收集。
 * @note 返回失败后调用方应丢弃本次暂存输出与游标，不可继续写后续记录。
 */
TZrBool compiler_metadata_module_record_emit(SZrCompilerState *cs,
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
