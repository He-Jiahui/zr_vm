#include "zr_vm_parser/artifact_projection.h"

#include <string.h>

#include "zr_vm_core/function.h"

/* 逐缓存校验失败时记录真实槽号；入口参数与容量错误以零槽作诊断占位。 */
static EZrArtifactStatus projection_fail(
        SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrUInt32 cacheIndex) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE;
        diagnostic->rowIndex = cacheIndex;
    }
    return status;
}

/* 从编译器缓存提取稳定契约与重定位坐标；运行时目标指针和 generation 不属于 artifact ABI。 */
static void projection_copy_row(
        SZrArtifactCallBindingRow *row, const SZrFunctionCallSiteCacheEntry *cache,
        TZrUInt32 functionIndex, TZrUInt32 cacheIndex) {
    memset(row, 0, sizeof(*row));
    row->schemaVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    row->functionIndex = functionIndex;
    row->cacheIndex = cacheIndex;
    row->instructionIndex = cache->instructionIndex;
    row->contract = cache->binding.contract;
    row->location = cache->bindingLocation;
}

/* 为已填写稳定绑定契约的函数生成按缓存槽有序的持久化行。
 * AOT 以 rows=NULL 查询并验证数量；测试以调用方缓冲区取行，其他消费者也可按此协议取行。
 * functionIndex 由调用方的 artifact 函数表确定；这里不解析祖先常量或外部模块重定位目标。
 * 借用 function 及其缓存到返回；两轮扫描期间内容须不变，输出数组不得与缓存重叠且不接管所有权。
 * 失败时 outRowCount 为零，rows 仅在全部校验及容量检查通过后才写入。 */
EZrArtifactStatus ZrParser_ArtifactCallBinding_BuildRows(
        const SZrFunction *function,
        TZrUInt32 functionIndex,
        SZrArtifactCallBindingRow *rows,
        TZrUInt32 rowCapacity,
        TZrUInt32 *outRowCount,
        SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 count = 0u;
    TZrByte encoded[ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE];
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (outRowCount != ZR_NULL) *outRowCount = 0u;
    if (function == ZR_NULL || outRowCount == ZR_NULL ||
        functionIndex == ZR_CALL_BINDING_SLOT_NONE ||
        (rows == ZR_NULL && rowCapacity != 0u) ||
        (function->callSiteCacheLength != 0u && function->callSiteCaches == ZR_NULL)) {
        return projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u);
    }
    /* 先借用 core 编码器校验 ABI 行，再确认总容量，避免把部分有效行发布给调用方。 */
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *cache = &function->callSiteCaches[index];
        SZrArtifactCallBindingRow row;
        EZrArtifactStatus status;
        if (cache->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (cache->instructionIndex >= function->instructionsLength ||
            (cache->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT &&
             cache->bindingLocation.ownerDepth == 0u &&
             cache->bindingLocation.targetIndex >= function->constantValueLength)) {
            return projection_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, index);
        }
        projection_copy_row(&row, cache, functionIndex, index);
        status = ZrCore_Artifact_WriteCallBindingRow(&row, encoded, sizeof(encoded), diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return projection_fail(diagnostic, status, index);
        ++count;
    }
    if (rows != ZR_NULL && count > rowCapacity) {
        return projection_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL, 0u);
    }
    *outRowCount = count;
    if (rows == ZR_NULL) return ZR_ARTIFACT_STATUS_OK;
    /* 第二轮只复制已验证的稳定字段；它依赖调用方没有在两轮之间修改缓存。 */
    count = 0u;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        const SZrFunctionCallSiteCacheEntry *cache = &function->callSiteCaches[index];
        if (cache->binding.contract.bindingKind != ZR_CALL_BINDING_NONE) {
            projection_copy_row(&rows[count++], cache, functionIndex, index);
        }
    }
    return ZR_ARTIFACT_STATUS_OK;
}
