#include "writer_binary_internal.h"

#include "zr_vm_core/call_binding.h"

/* call-binding 小节的索引、位置与版本字段固定为小端 u32，不能依赖宿主字节序。 */
static TZrBool write_u32(FILE *file, TZrUInt32 value) {
    TZrByte bytes[4] = {(TZrByte)value, (TZrByte)(value >> 8u),
                       (TZrByte)(value >> 16u), (TZrByte)(value >> 24u)};
    return fwrite(bytes, 1u, sizeof(bytes), file) == sizeof(bytes);
}

/* 与 ZrCore_Io_ReadCallBindings 的头和 84 字节记录成对；只发布有绑定的缓存项，
 * 其余项由读取端初始化为未绑定，具体目标在导入/链接阶段重新解析。 */
TZrBool ZrParser_Writer_WriteCallBindings(FILE *file, const SZrFunction *function) {
    TZrUInt32 count = 0u;
    if (file == ZR_NULL || function == ZR_NULL ||
        (function->callSiteCacheLength != 0u && function->callSiteCaches == ZR_NULL)) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        if (function->callSiteCaches[index].binding.contract.bindingKind != ZR_CALL_BINDING_NONE) ++count;
    }
    if (!write_u32(file, ZR_CALL_BINDING_SECTION_MAGIC) ||
        !write_u32(file, ZR_CALL_BINDING_SCHEMA_VERSION) ||
        !write_u32(file, ZR_CALL_BINDING_SECTION_ROW_SIZE) || !write_u32(file, count)) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        TZrByte contract[ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE];
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (!ZrCore_CallBinding_EncodeContract(&entry->binding.contract, contract, sizeof(contract)) ||
            !write_u32(file, index) ||
            fwrite(contract, 1u, sizeof(contract), file) != sizeof(contract) ||
            !write_u32(file, entry->bindingLocation.kind) ||
            !write_u32(file, entry->bindingLocation.targetIndex) ||
            !write_u32(file, entry->bindingLocation.ownerDepth) ||
            !write_u32(file, entry->bindingLocation.flags)) return ZR_FALSE;
    }
    return ZR_TRUE;
}
