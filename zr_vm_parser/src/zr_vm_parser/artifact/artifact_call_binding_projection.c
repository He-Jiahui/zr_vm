#include "artifact/artifact_projection.h"

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

/**
 * @fn static EZrArtifactStatus projection_fail( SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrUInt32 cacheIndex)
 *
 * ## Summary
 * 把绑定投影的局部失败归入调用绑定表，并保留编码器已给出的其他诊断细节。 由计数/发布入口的参数、指令边界、编码或容量拒绝分支调用。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空诊断；仅覆盖三个归属字段并保留先前编码诊断其他细节。
 * @param[in] status 本拒绝分支需要传播的artifact状态，原样返回。
 * @param[in] cacheIndex 源缓存槽序号；入口/容量错误使用0占位。
 *
 * ## Returns
 * @return 原样返回status；可选诊断的其他字段保持原值。
 *
 * ## Constraints and lifecycle
 * diagnostic可空；仅覆写status/sectionKind/rowIndex。逐缓存失败使用真实cacheIndex，入口与容量错误的0只是占位，不证明第0槽有错；不分配资源。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:8; zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:51; zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:63。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static void projection_copy_row( SZrArtifactCallBindingRow *row, const SZrFunctionCallSiteCacheEntry *cache, TZrUInt32 functionIndex, TZrUInt32 cacheIndex)
 *
 * ## Summary
 * 把一个缓存槽的稳定契约及重定位坐标复制到固定布局的持久化行。 验证轮构造临时行及成功发布轮共用，避免两轮选取不同字段。
 *
 * ## Parameters
 * @param[out] row 调用者提供的完整行存储，必须与cache独立。
 * @param[in] cache 非空缓存条目，稳定合同与重定位字段在复制期间有效。
 * @param[in] functionIndex artifact函数表的稳定行索引，入口要求不为SLOT_NONE。
 * @param[in] cacheIndex 源缓存槽序号，由外层遍历传入并直接写入持久行；0可为合法槽号。
 *
 * ## Returns
 * void；完整覆盖row，不返回所有权或运行时目标指针。
 *
 * ## Constraints and lifecycle
 * row/cache均非空且不可重叠；先清零row再复制。只借用cache至返回，不复制binding.target或generation；functionIndex/cacheIndex由外层给定，不在此验证。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:19; zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:65; zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:80。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn EZrArtifactStatus ZrParser_ArtifactCallBinding_BuildRows( const SZrFunction *function, TZrUInt32 functionIndex, SZrArtifactCallBindingRow *rows, TZrUInt32 rowCapacity, TZrUInt32 *outRowCount, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将函数的非NONE调用缓存投影为按原缓存槽排列的artifact绑定行，并支持只校验/查询数量。 AOT验证整张函数表时以rows=NULL调用；需要固定行数据的消费者提供输出数组。
 *
 * ## Parameters
 * @param[in] function 非空且已建立缓存/指令/常量计数的函数，整个两轮遍历期间稳定。
 * @param[in] functionIndex artifact函数表的稳定行索引，入口要求不为SLOT_NONE。
 * @param[out] rows 可空输出数组；为空仅允许rowCapacity=0，用于只查询数量。
 * @param[in] rowCapacity rows可写的行容量，非空时须容纳全部有效槽。
 * @param[out] outRowCount 必填数量输出；入口先清0，成功才交付完整数量。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时outRowCount为实际行数；非空rows仅在全量行编码校验与容量检查通过后写入。普通失败使非空outRowCount为0且不写rows；诊断可空。
 *
 * ## Constraints and lifecycle
 * function/outRowCount必填，functionIndex不能为SLOT_NONE；非零缓存长度须有缓存数组。rows=NULL仅允许rowCapacity=0。两轮间函数/缓存和输入字符串状态必须稳定，rows与缓存不得重叠；不分配。只核本函数指令和本地常量边界，祖先/外部重定位目标和目标签名由AOT等上层继续解析。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:36; zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c:111; tests/parser/test_call_binding_artifact.c:191。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @par ZrParser_ArtifactCallBinding_BuildRows::validate_before_publish
 *
 * ## Summary
 * 先验证全部有效缓存行，再检查总容量，防止向调用者发布部分绑定行。 第一轮对每个非NONE槽构造栈上行并经Core编码器核ABI。
 *
 * ## Constraints and lifecycle
 * 临时encoded和row不逃逸；指令与本地常量界限在编码前核实，祖先/外部目标留给后续消费者；任何失败都在写rows前发生。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:53; zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:54。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @par ZrParser_ArtifactCallBinding_BuildRows::second_pass
 *
 * ## Summary
 * 在数量和ABI都已确认后才复制稳定行，保留原缓存槽顺序。 rows非空且全部前置校验/容量检查成功后进入。
 *
 * ## Constraints and lifecycle
 * 两轮间输入不得改变；目标rows容量至少为已计算count且不与缓存重叠。本轮不再执行会失败的行校验。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:75; zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c:76。这些位置分别为本单元定义/使用或下层合同证据。
 */
