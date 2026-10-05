#include "zr_vm_core/call_binding.h"

#include <string.h>

#include "artifact_schema_internal.h"

/* writer 和 artifact 行共用这条逐字段编码路径，避免 C 结构体布局进入磁盘格式。 */
/** @brief 为 writer 和 artifact 行编码固定宽度契约，使磁盘格式独立于宿主结构体布局。
 * @note 输出必须恰为 64 字节；校验失败前不写输出，成功按小端字段编码并写零保留位，不保存运行时目标。
 */
TZrBool ZrCore_CallBinding_EncodeContract(const SZrCallBindingContract *contract,
                                         TZrByte *bytes, TZrSize length) {
    if (bytes == ZR_NULL || length != ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE ||
        ZrCore_CallBinding_CheckContract(contract, ZR_NULL) != ZR_CALL_BINDING_OK) {
        return ZR_FALSE;
    }
    zr_artifact_write_u32(bytes, contract->bindingKind);
    zr_artifact_write_u32(bytes + 4u, contract->targetMetadataToken);
    zr_artifact_write_u32(bytes + 8u, contract->signatureToken);
    zr_artifact_write_u32(bytes + 12u, contract->ownerTypeToken);
    zr_artifact_write_u64(bytes + 16u, contract->signatureHash);
    zr_artifact_write_u64(bytes + 24u, contract->moduleSignatureHash);
    zr_artifact_write_u32(bytes + 32u, contract->layoutVersion);
    zr_artifact_write_u32(bytes + 36u, contract->dispatchSlot);
    zr_artifact_write_u64(bytes + 40u, contract->layoutHash);
    zr_artifact_write_u32(bytes + 48u, contract->operation);
    zr_artifact_write_u32(bytes + 52u, 0u);
    zr_artifact_write_u64(bytes + 56u, 0u);
    return ZR_TRUE;
}

/* 保留完整检查状态供 artifact reader 分类，同时维持失败时输出清零。 */
/* 先清输出、在局部契约中完成检查再发布；保留具体状态供 artifact reader 分类，不在这里重建运行时目标。
 * 输入与输出若重叠，清零会改变输入；当前 reader 使用分开的存储。 */
EZrCallBindingStatus zr_call_binding_decode_contract_status(
        const TZrByte *bytes,
        TZrSize length,
        SZrCallBindingContract *contract) {
    SZrCallBindingContract decoded = {0};
    EZrCallBindingStatus status;
    if (contract == ZR_NULL) return ZR_CALL_BINDING_INVALID_ARGUMENT;
    memset(contract, 0, sizeof(*contract));
    if (bytes == ZR_NULL || length != ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE)
        return ZR_CALL_BINDING_INVALID_ARGUMENT;
    decoded.bindingKind = zr_artifact_read_u32(bytes);
    decoded.targetMetadataToken = zr_artifact_read_u32(bytes + 4u);
    decoded.signatureToken = zr_artifact_read_u32(bytes + 8u);
    decoded.ownerTypeToken = zr_artifact_read_u32(bytes + 12u);
    decoded.signatureHash = zr_artifact_read_u64(bytes + 16u);
    decoded.moduleSignatureHash = zr_artifact_read_u64(bytes + 24u);
    decoded.layoutVersion = zr_artifact_read_u32(bytes + 32u);
    decoded.dispatchSlot = zr_artifact_read_u32(bytes + 36u);
    decoded.layoutHash = zr_artifact_read_u64(bytes + 40u);
    decoded.operation = zr_artifact_read_u32(bytes + 48u);
    decoded.reserved0 = zr_artifact_read_u32(bytes + 52u);
    decoded.reserved1 = zr_artifact_read_u64(bytes + 56u);
    status = ZrCore_CallBinding_CheckContract(&decoded, ZR_NULL);
    if (status != ZR_CALL_BINDING_OK) return status;
    *contract = decoded;
    return ZR_CALL_BINDING_OK;
}

/* 公开 bool API 维持原有语义；详细分类仅供 core 内部 artifact 路径使用。 */
/** @brief 为普通二进制 IO 提供契约解码的布尔结果，详细分类由内部 artifact 入口保留。
 * @note 非空输出在失败时清零；成功只恢复静态字段，运行时目标由后续链接重建。
 */
TZrBool ZrCore_CallBinding_DecodeContract(const TZrByte *bytes, TZrSize length,
                                         SZrCallBindingContract *contract) {
    return (TZrBool)(zr_call_binding_decode_contract_status(bytes, length, contract) ==
            ZR_CALL_BINDING_OK);
}
