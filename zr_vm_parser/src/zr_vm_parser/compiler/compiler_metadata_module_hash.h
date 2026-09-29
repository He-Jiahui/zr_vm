#ifndef ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H
#define ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H

#include "compiler_internal.h"

/**
 * @brief 计算公开模块合同的稳定 ABI 哈希，供编译收尾写入并由加载器验证。
 * @pre cs 与 function 有效；仅有 typed exports 时要求导出表及其签名堆有效。
 * @return 有导出时返回版本化摘要，分配或编码失败返回零；无导出时按约定返回零。
 */
TZrUInt64 metadata_token_compute_module_signature_hash(
        SZrCompilerState *cs,
        const SZrFunction *function,
        const SZrMetadataStringHeapEntry *stringHeapEntries,
        TZrUInt32 stringHeapEntryCount);

#endif // ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H
