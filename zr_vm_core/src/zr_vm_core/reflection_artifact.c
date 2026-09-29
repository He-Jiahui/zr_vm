#include "zr_vm_core/reflection.h"
/* 反射层沿用 canonical consumer 的类型投影，使模块、调试和反射读取同一 artifact 契约。 */
#include "zr_vm_core/canonical_consumer.h"
/* 调用方需保持 projection 的源 buffer 有效；失败状态及 diagnostic 原样转交公共查询器。 */
EZrArtifactStatus ZrCore_Reflection_ResolveArtifactType(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_CanonicalConsumer_ResolveTypeToken(
            projection, typeToken, outType, diagnostic);
}
