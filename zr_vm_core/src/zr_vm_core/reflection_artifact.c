#include "zr_vm_core/reflection.h"
/* 反射层沿用 canonical consumer 的类型投影，使模块、调试和反射读取同一 artifact 契约。 */
#include "zr_vm_core/canonical_consumer.h"
/** @brief 让反射按 metadata token 读取与模块、调试消费者一致的 canonical 类型投影。
 * @pre projection 来自成功的 Open，源 buffer 在查询及借用结果的使用期间保持存活且不改写。
 * @note 不创建反射对象或持有 GC 根；TypeDef/TypeRef/TypeSpec 的失败状态与 diagnostic 原样委托查询器。
 */
EZrArtifactStatus ZrCore_Reflection_ResolveArtifactType(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_CanonicalConsumer_ResolveTypeToken(
            projection, typeToken, outType, diagnostic);
}
