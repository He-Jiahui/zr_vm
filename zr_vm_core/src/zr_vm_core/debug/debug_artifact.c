#include "zr_vm_core/debug.h"

#include "zr_vm_core/canonical_consumer.h"

EZrArtifactStatus ZrCore_Debug_ResolveArtifactType(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 canonicalTypeId,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_CanonicalConsumer_ResolveTypeId(
            projection, canonicalTypeId, outType, diagnostic);
}

/**
 * @fn EZrArtifactStatus ZrCore_Debug_ResolveArtifactType(const SZrCanonicalConsumerProjection *projection, TZrUInt32 canonicalTypeId, SZrCanonicalTypeProjection *outType, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 调试侧在工件已成功打开后，按 canonical TypeId 取得类型身份及可选布局、契约和域传递信息。
 * 此入口直接委托 ZrCore_CanonicalConsumer_ResolveTypeId，使调试侧沿用规范消费者的解析顺序和状态。
 *
 * ## Parameters
 * @param[in] projection 借用成功 ZrCore_CanonicalConsumer_Open 得到的只读投影；查询期间投影及其原始工件 buffer 必须存活且未改写。空指针返回 INVALID_ARGUMENT。
 * @param[in] canonicalTypeId 非零 canonical 类型 ID，不是 metadata token；零返回 INVALID_ARGUMENT。
 * @param[out] outType 调用方提供的可写类型投影；空指针返回 INVALID_ARGUMENT。仅 OK 时可消费其字段；signatureData 若非空仍借用原工件 buffer，has* 决定对应行值是否有效。
 * @param[out] diagnostic 可为空；非空时由下层在入口清零，并在失败时填写状态与相应位置或哈希信息；其存储由调用方持有。
 *
 * ## Returns
 * @return 原样返回规范消费者的 EZrArtifactStatus。OK 表示 outType 有效；无效参数返回 INVALID_ARGUMENT，未找到非零 ID 返回 INVALID_SECTION，行投影失败沿用下层状态。
 *
 * ## Constraints and lifecycle
 * @pre projection 来自成功 Open；不能把 Open 的失败或部分初始化输出交给本查询。
 * 根 ID 直接复制 Open 已验证的 rootType；其余依次查 TypeSpec 和 TypeDef，不按名称查找，也不另行扫描 TypeRef。
 * 此包装层不分配、释放或持有资源，不延长投影及签名字节的寿命；值行复制不转移工件所有权。
 * 失败不保证 outType 清空或回滚：早拒绝与查无结果可保留旧值，行投影也可能先改写字段再失败；调用方应先检查返回状态。
 * 输出存储由调用方提供；本层不检测输入、输出之间的重叠，调用方须保证写入不破坏查询仍在使用的视图。
 * 包装层不提供锁；共享工件的改写、释放以及共享输出的并发访问由调用方协调，独立输出的查询仍须保持输入只读。
 *
 * ## References
 * - ZrCore_CanonicalConsumer_ResolveTypeId：根、TypeSpec、TypeDef 顺序及失败状态，zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c:350。
 * - SZrCanonicalConsumerProjection / SZrCanonicalTypeProjection：借用视图和 has* 字段，zr_vm_core/include/zr_vm_core/canonical_consumer.h:9、zr_vm_core/include/zr_vm_core/canonical_consumer.h:28。
 * - ZrCore_Debug_ResolveArtifactType 声明：zr_vm_core/include/zr_vm_core/debug.h:440。
 * - test_reflection_debug_and_layout_resolve_only_canonical_ids_and_tokens：成功与缺失 ID 调用，tests/parser/test_canonical_consumers.c:461、tests/parser/test_canonical_consumers.c:482。
 */
