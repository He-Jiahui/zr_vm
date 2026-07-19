#ifndef ZR_VM_PARSER_ARTIFACT_PROJECTION_H
#define ZR_VM_PARSER_ARTIFACT_PROJECTION_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_core/artifact_schema.h"

typedef struct SZrParserArtifactPublicContract {
    TZrMetadataToken typeRefToken;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    TZrUInt64 callableContractHash;
    TZrUInt64 moduleHash;
} SZrParserArtifactPublicContract;

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_WriteSignature(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_InternSignature(
        struct SZrSemanticContext *context,
        SZrString *moduleIdentity,
        const TZrByte *signature,
        TZrSize signatureLength,
        TZrTypeId *outTypeId,
        SZrArtifactDiagnostic *diagnostic);

ZR_PARSER_API EZrArtifactStatus ZrParser_ArtifactType_BuildPublicIdentity(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrParserArtifactPublicContract *contract,
        TZrByte *signatureBuffer,
        TZrSize signatureBufferCapacity,
        TZrSize *outSignatureLength,
        SZrArtifactPublicIdentity *outIdentity,
        SZrArtifactDiagnostic *diagnostic);

#endif // ZR_VM_PARSER_ARTIFACT_PROJECTION_H
