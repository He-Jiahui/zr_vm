#include "zr_vm_core/module.h"

/* 模块消费者沿统一的 canonical 投影入口打开产物，身份校验由共享解码器执行。 */
EZrArtifactStatus ZrCore_Module_OpenCanonicalArtifact(
        const TZrByte *buffer,
        TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrCanonicalConsumerProjection *outProjection,
        SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_CanonicalConsumer_Open(buffer,
                                         bufferLength,
                                         expectedIdentity,
                                         outProjection,
                                         diagnostic);
}
