#ifndef ZR_VM_PARSER_BACKEND_AOT_CANONICAL_ARTIFACT_H
#define ZR_VM_PARSER_BACKEND_AOT_CANONICAL_ARTIFACT_H

#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_parser/conf.h"

ZR_PARSER_API EZrArtifactStatus backend_aot_open_canonical_artifact(
        const TZrByte *buffer,
        TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrCanonicalConsumerProjection *outProjection,
        SZrArtifactDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_CANONICAL_ARTIFACT_H */
