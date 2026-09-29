#ifndef ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H
#define ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H

#include "compiler_internal.h"

/**
 * @brief 计算当前模块导出与相关类型布局组成的稳定 ABI 摘要，供跨模块绑定识别合同。
 * @pre cs、function 有效；有导出时导出数组、签名堆及其中的签名区间必须一致。
 * @note 输入数组与签名堆仅在本次调用中借用；摘要计算分配临时排序索引和序列化缓冲区，不改变输入顺序。
 * @return 有导出时返回带 v2 域前缀的摘要；无导出按约定返回零，分配或序列化失败也返回零，调用方需结合导出数区分。
 */
TZrUInt64 metadata_token_compute_module_signature_hash(
        SZrCompilerState *cs,
        const SZrFunction *function,
        const SZrMetadataStringHeapEntry *stringHeapEntries,
        TZrUInt32 stringHeapEntryCount);

#endif // ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H
