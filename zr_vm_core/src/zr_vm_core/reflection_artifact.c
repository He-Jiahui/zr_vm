#include "zr_vm_core/reflection.h"

#include "zr_vm_core/canonical_consumer.h"

EZrArtifactStatus ZrCore_Reflection_ResolveArtifactType(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_CanonicalConsumer_ResolveTypeToken(
            projection, typeToken, outType, diagnostic);
}
