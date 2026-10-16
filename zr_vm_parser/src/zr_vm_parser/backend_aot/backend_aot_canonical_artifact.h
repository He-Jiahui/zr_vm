#ifndef ZR_VM_PARSER_BACKEND_AOT_CANONICAL_ARTIFACT_H
#define ZR_VM_PARSER_BACKEND_AOT_CANONICAL_ARTIFACT_H

#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_parser/conf.h"

/** @brief 通过 core canonical consumer 打开供 AOT 使用的规范产物投影。
 *  @note 投影的所有权与诊断语义由 ZrCore_CanonicalConsumer_Open 定义。 */
ZR_PARSER_API EZrArtifactStatus backend_aot_open_canonical_artifact(
        const TZrByte *buffer,
        TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrCanonicalConsumerProjection *outProjection,
        SZrArtifactDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_CANONICAL_ARTIFACT_H */
