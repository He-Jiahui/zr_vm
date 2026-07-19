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
