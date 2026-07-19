#include "backend_aot_canonical_artifact.h"

ZR_PARSER_API EZrArtifactStatus backend_aot_open_canonical_artifact(
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
