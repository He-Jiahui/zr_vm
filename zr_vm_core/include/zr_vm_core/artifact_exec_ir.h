#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_H

#include "zr_vm_core/conf.h"

/* A small, pointer-free on-disk contract for ExecIR/ExecBC artifacts.  This
 * contract is intentionally separate from runtime structs: every field is
 * encoded little-endian and all references are IDs or byte offsets. */
#define ZR_ARTIFACT_EXEC_IR_MAGIC ((TZrUInt32)0x31495245u) /* "ERI1" */
#define ZR_ARTIFACT_EXEC_IR_SCHEMA_VERSION ((TZrUInt16)1u)
#define ZR_ARTIFACT_EXEC_IR_HEADER_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_EXEC_IR_SECTION_SIZE ((TZrUInt32)24u)
#define ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE ((TZrUInt32)32u)
#define ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS ((TZrUInt32)32u)
#define ZR_ARTIFACT_EXEC_IR_MAX_BYTES ((TZrUInt32)67108864u)

typedef enum EZrArtifactExecIrSectionKind {
    ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR = 1,
    ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC = 2,
    ZR_ARTIFACT_EXEC_IR_SECTION_BINDINGS = 3,
    ZR_ARTIFACT_EXEC_IR_SECTION_STATE_MAPS = 4,
    ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS = 5
} EZrArtifactExecIrSectionKind;

#define ZR_ARTIFACT_EXEC_IR_SECTION_OPTIONAL ((TZrUInt32)1u)

typedef enum EZrArtifactExecIrStatus {
    ZR_ARTIFACT_EXEC_IR_OK = 0,
    ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,
    ZR_ARTIFACT_EXEC_IR_BAD_MAGIC,
    ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,
    ZR_ARTIFACT_EXEC_IR_TRUNCATED,
    ZR_ARTIFACT_EXEC_IR_LIMIT,
    ZR_ARTIFACT_EXEC_IR_INVALID_DIRECTORY,
    ZR_ARTIFACT_EXEC_IR_OVERLAP,
    ZR_ARTIFACT_EXEC_IR_DUPLICATE_SECTION,
    ZR_ARTIFACT_EXEC_IR_UNKNOWN_SECTION,
    ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
    ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION,
    ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH,
    ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED
} EZrArtifactExecIrStatus;

typedef struct SZrArtifactExecIrDiagnostic {
    EZrArtifactExecIrStatus status;
    TZrUInt32 byteOffset;
    TZrUInt32 sectionKind;
    TZrUInt32 rowIndex;
    TZrUInt32 token;
} SZrArtifactExecIrDiagnostic;

typedef struct SZrArtifactExecIrSectionInput {
    EZrArtifactExecIrSectionKind kind;
    TZrUInt32 flags;
    TZrUInt32 elementCount;
    TZrUInt32 elementSize;
    const TZrByte *data;
    TZrUInt32 byteLength;
} SZrArtifactExecIrSectionInput;

typedef struct SZrArtifactExecIrDocument {
    TZrUInt16 abiVersion;
    TZrUInt16 flags;
    TZrUInt64 moduleHash;
    TZrUInt64 execIrHash;
    TZrUInt64 execBcHash;
    TZrUInt32 sectionCount;
    const SZrArtifactExecIrSectionInput *sections;
} SZrArtifactExecIrDocument;

typedef struct SZrArtifactExecIrSectionView {
    EZrArtifactExecIrSectionKind kind;
    TZrUInt32 flags;
    TZrUInt32 elementCount;
    TZrUInt32 elementSize;
    TZrUInt32 byteOffset;
    TZrUInt32 byteLength;
    const TZrByte *data;
} SZrArtifactExecIrSectionView;

typedef struct SZrArtifactExecIrView {
    TZrUInt16 abiVersion;
    TZrUInt16 flags;
    TZrUInt64 moduleHash;
    TZrUInt64 execIrHash;
    TZrUInt64 execBcHash;
    TZrUInt32 sectionCount;
    const TZrByte *buffer;
    TZrUInt32 bufferLength;
    SZrArtifactExecIrSectionView sections[ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS];
} SZrArtifactExecIrView;

typedef struct SZrArtifactExecIrRelocation {
    TZrUInt32 targetToken;
    TZrUInt32 targetKind;
    TZrUInt32 codeOffset;
    TZrUInt32 targetIndex;
    TZrUInt64 expectedHash;
    TZrUInt64 expectedContractHash;
} SZrArtifactExecIrRelocation;

typedef TZrBool (*FZrArtifactExecIrResolve)(TZrUInt32 targetToken,
                                             TZrUInt32 targetKind,
                                             TZrUInt64 expectedHash,
                                             TZrUInt64 expectedContractHash,
                                             TZrUInt32 *outTargetIndex,
                                             TZrPtr userData);

ZR_CORE_API TZrUInt64 ZrCore_ArtifactExecIr_HashBytes(const TZrByte *bytes,
                                                       TZrUInt32 length);
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_GetEncodedSize(
        const SZrArtifactExecIrDocument *document, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Write(
        const SZrArtifactExecIrDocument *document, TZrByte *buffer,
        TZrUInt32 capacity, TZrUInt32 *outWritten,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Read(
        const TZrByte *buffer, TZrUInt32 length, SZrArtifactExecIrView *outView,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_FindSection(
        const SZrArtifactExecIrView *view, EZrArtifactExecIrSectionKind kind,
        const SZrArtifactExecIrSectionView **outSection,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_CORE_API EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_ValidateRelocations(
        const SZrArtifactExecIrView *view, FZrArtifactExecIrResolve resolver,
        TZrUInt32 *resolvedTargetIndices, TZrUInt32 capacity, TZrPtr userData,
        SZrArtifactExecIrDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_ArtifactExecIr_StatusName(
        EZrArtifactExecIrStatus status);

#endif
