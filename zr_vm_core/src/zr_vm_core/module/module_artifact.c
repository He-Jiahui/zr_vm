#include "zr_vm_core/module.h"

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
