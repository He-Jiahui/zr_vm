#ifndef ZR_VM_CORE_ARTIFACT_SCHEMA_H
#define ZR_VM_CORE_ARTIFACT_SCHEMA_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"

#define ZR_ARTIFACT_SCHEMA_VERSION ((TZrUInt16)1u)
#define ZR_ARTIFACT_HEADER_ENCODED_SIZE ((TZrUInt32)112u)
#define ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE ((TZrUInt32)24u)
#define ZR_ARTIFACT_HEADER_SECTION_COUNT_OFFSET ((TZrUInt32)16u)
#define ZR_ARTIFACT_MAX_SECTION_COUNT ((TZrUInt32)32u)
#define ZR_ARTIFACT_MAX_ROW_COUNT ((TZrUInt32)1048576u)
#define ZR_ARTIFACT_MAX_BYTE_LENGTH ((TZrUInt32)67108864u)
#define ZR_ARTIFACT_SIGNATURE_MAX_DEPTH ((TZrUInt32)128u)
#define ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT ((TZrUInt32)1048576u)

#define ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE ((TZrUInt32)40u)
#define ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE ((TZrUInt32)40u)
#define ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE ((TZrUInt32)40u)

typedef enum EZrArtifactKind {
    ZR_ARTIFACT_KIND_ZRS = 1,
    ZR_ARTIFACT_KIND_ZRI = 2,
    ZR_ARTIFACT_KIND_ZRO = 3
} EZrArtifactKind;

typedef enum EZrArtifactSectionKind {
    ZR_ARTIFACT_SECTION_INVALID = 0,
    ZR_ARTIFACT_SECTION_STRING_HEAP = 1,
    ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE = 2,
    ZR_ARTIFACT_SECTION_TYPE_REF_TABLE = 3,
    ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE = 4,
    ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE = 5,
    ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE = 6,
    ZR_ARTIFACT_SECTION_SIGNATURE_HEAP = 7,
    ZR_ARTIFACT_SECTION_CONTRACT_TABLE = 8,
    ZR_ARTIFACT_SECTION_LAYOUT_TABLE = 9,
    ZR_ARTIFACT_SECTION_CODE_TABLE = 10,
    ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE = 11,
    ZR_ARTIFACT_SECTION_DEBUG_MAP = 12,
    ZR_ARTIFACT_SECTION_SYNTAX_TREE = 13,
    ZR_ARTIFACT_SECTION_SEMANTIC_IR = 14
} EZrArtifactSectionKind;

#define ZR_ARTIFACT_SECTION_FLAG_MANDATORY ((TZrUInt32)0u)
#define ZR_ARTIFACT_SECTION_FLAG_OPTIONAL ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_SECTION_FLAG_KNOWN_MASK ZR_ARTIFACT_SECTION_FLAG_OPTIONAL

typedef enum EZrArtifactStatus {
    ZR_ARTIFACT_STATUS_OK = 0,
    ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
    ZR_ARTIFACT_STATUS_BAD_MAGIC,
    ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION,
    ZR_ARTIFACT_STATUS_INVALID_KIND,
    ZR_ARTIFACT_STATUS_TRUNCATED,
    ZR_ARTIFACT_STATUS_COUNT_LIMIT,
    ZR_ARTIFACT_STATUS_UNKNOWN_MANDATORY_SECTION,
    ZR_ARTIFACT_STATUS_DUPLICATE_SECTION,
    ZR_ARTIFACT_STATUS_FORBIDDEN_SECTION,
    ZR_ARTIFACT_STATUS_INVALID_SECTION,
    ZR_ARTIFACT_STATUS_SECTION_OVERLAP,
    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
    ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
    ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
    ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
    ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
    ZR_ARTIFACT_STATUS_INVALID_TEXT
} EZrArtifactStatus;

typedef enum EZrArtifactSignatureNode {
    ZR_ARTIFACT_SIGNATURE_NODE_INVALID = 0,
    ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE = 1,
    ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF = 2,
    ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_PARAMETER = 3,
    ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_INSTANCE = 4,
    ZR_ARTIFACT_SIGNATURE_NODE_ARRAY = 5,
    ZR_ARTIFACT_SIGNATURE_NODE_TUPLE = 6,
    ZR_ARTIFACT_SIGNATURE_NODE_UNION = 7,
    ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE = 8,
    ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION = 9,
    ZR_ARTIFACT_SIGNATURE_NODE_REF = 10,
    ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW = 11,
    ZR_ARTIFACT_SIGNATURE_NODE_OWNER = 12,
    ZR_ARTIFACT_SIGNATURE_NODE_NEVER = 13,
    ZR_ARTIFACT_SIGNATURE_NODE_ERROR = 14,
    ZR_ARTIFACT_SIGNATURE_NODE_CONST_INT = 15,
    ZR_ARTIFACT_SIGNATURE_NODE_CONST_PARAMETER = 16
} EZrArtifactSignatureNode;

typedef enum EZrArtifactRefAccess {
    ZR_ARTIFACT_REF_WRITABLE = 0,
    ZR_ARTIFACT_REF_READONLY = 1
} EZrArtifactRefAccess;

typedef enum EZrArtifactOwnerKind {
    ZR_ARTIFACT_OWNER_UNIQUE = 0,
    ZR_ARTIFACT_OWNER_SHARED = 1,
    ZR_ARTIFACT_OWNER_WEAK = 2,
    ZR_ARTIFACT_OWNER_ATOMIC_SHARED = 3
} EZrArtifactOwnerKind;

typedef enum EZrArtifactPassingForm {
    ZR_ARTIFACT_PASSING_VALUE = 0,
    ZR_ARTIFACT_PASSING_IN = 1,
    ZR_ARTIFACT_PASSING_REF = 2,
    ZR_ARTIFACT_PASSING_REF_READONLY = 3,
    ZR_ARTIFACT_PASSING_OUT = 4
} EZrArtifactPassingForm;

typedef enum EZrArtifactEscapeUpperBound {
    ZR_ARTIFACT_ESCAPE_BLOCK = 0,
    ZR_ARTIFACT_ESCAPE_FUNCTION = 1,
    ZR_ARTIFACT_ESCAPE_CALLER = 2,
    ZR_ARTIFACT_ESCAPE_HEAP_STATIC = 3,
    ZR_ARTIFACT_ESCAPE_UNKNOWN = 4
} EZrArtifactEscapeUpperBound;

typedef enum EZrArtifactEntryInitialization {
    ZR_ARTIFACT_ENTRY_INITIALIZED = 0,
    ZR_ARTIFACT_ENTRY_UNINITIALIZED = 1
} EZrArtifactEntryInitialization;

typedef enum EZrArtifactExitInitialization {
    ZR_ARTIFACT_EXIT_UNCHANGED = 0,
    ZR_ARTIFACT_EXIT_DEFINITELY_INITIALIZED = 1
} EZrArtifactExitInitialization;

typedef enum EZrArtifactCallSiteMarker {
    ZR_ARTIFACT_CALL_SITE_NONE = 0,
    ZR_ARTIFACT_CALL_SITE_REF = 1,
    ZR_ARTIFACT_CALL_SITE_OUT = 2
} EZrArtifactCallSiteMarker;

typedef enum EZrArtifactReceiverEffect {
    ZR_ARTIFACT_RECEIVER_NONE = 0,
    ZR_ARTIFACT_RECEIVER_READONLY = 1,
    ZR_ARTIFACT_RECEIVER_MUTABLE = 2
} EZrArtifactReceiverEffect;

typedef enum EZrArtifactRefExportEffect {
    ZR_ARTIFACT_REF_EXPORT_NONE = 0,
    ZR_ARTIFACT_REF_EXPORT_READONLY = 1,
    ZR_ARTIFACT_REF_EXPORT_WRITABLE = 2
} EZrArtifactRefExportEffect;

typedef enum EZrArtifactGcScanKind {
    ZR_ARTIFACT_GC_SCAN_FREE = 0,
    ZR_ARTIFACT_GC_SCAN_MAPPED = 1,
    ZR_ARTIFACT_GC_SCAN_BARRIERED = 2
} EZrArtifactGcScanKind;

#define ZR_ARTIFACT_TYPE_FLAG_VALUE ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_TYPE_FLAG_GC ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_TYPE_FLAG_RESOURCE ((TZrUInt32)1u << 2u)
#define ZR_ARTIFACT_TYPE_FLAG_READONLY ((TZrUInt32)1u << 3u)
#define ZR_ARTIFACT_TYPE_FLAG_REF_LIKE ((TZrUInt32)1u << 4u)
#define ZR_ARTIFACT_TYPE_FLAG_DROP ((TZrUInt32)1u << 5u)
#define ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE ((TZrUInt32)1u << 6u)
#define ZR_ARTIFACT_TYPE_FLAG_KNOWN_MASK ((TZrUInt32)0x7fu)

#define ZR_ARTIFACT_CONTRACT_FLAG_SCOPED ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_CONTRACT_FLAG_THROWS ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_CONTRACT_FLAG_ASYNC ((TZrUInt32)1u << 2u)
#define ZR_ARTIFACT_CONTRACT_FLAG_GENERATOR ((TZrUInt32)1u << 3u)
#define ZR_ARTIFACT_CONTRACT_FLAG_KNOWN_MASK ((TZrUInt32)0x0fu)

#define ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_LAYOUT_CAPABILITY_KNOWN_MASK ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE

typedef struct SZrArtifactDiagnostic {
    EZrArtifactStatus status;
    TZrUInt32 sectionKind;
    TZrUInt32 rowIndex;
    TZrUInt32 byteOffset;
    TZrMetadataToken expectedToken;
    TZrMetadataToken actualToken;
    TZrUInt32 expectedVersion;
    TZrUInt32 actualVersion;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrArtifactDiagnostic;

typedef struct SZrArtifactPublicIdentity {
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken typeRefToken;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 typeRefHash;
    TZrUInt64 typeSpecHash;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
    TZrUInt64 callableContractHash;
    TZrUInt64 moduleHash;
} SZrArtifactPublicIdentity;

typedef struct SZrArtifactTypeDefRow {
    TZrMetadataToken token;
    TZrUInt32 flags;
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken constructorToken;
    TZrMetadataToken constructorSignatureToken;
    TZrUInt32 reserved0;
    TZrUInt64 typeSignatureHash;
    TZrUInt64 constructorContractHash;
    TZrUInt64 reserved1;
} SZrArtifactTypeDefRow;

typedef struct SZrArtifactTypeIdentityRow {
    TZrMetadataToken token;
    TZrMetadataToken signatureToken;
    TZrUInt32 canonicalTypeId;
    TZrUInt32 flags;
    TZrUInt32 signatureOffset;
    TZrUInt32 signatureLength;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
} SZrArtifactTypeIdentityRow;

typedef struct SZrArtifactMemberDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 flags;
    TZrUInt32 nameStringOffset;
    TZrUInt32 reserved0;
    TZrUInt64 signatureHash;
    TZrUInt64 contractHash;
} SZrArtifactMemberDefRow;

typedef struct SZrArtifactPropertyDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrMetadataToken getterToken;
    TZrMetadataToken setterToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 flags;
    TZrUInt64 signatureHash;
    TZrUInt64 contractHash;
    TZrUInt64 reserved0;
} SZrArtifactPropertyDefRow;

typedef struct SZrArtifactContractRow {
    TZrMetadataToken memberToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 parameterCount;
    TZrUInt32 flags;
    TZrUInt32 receiverEffect;
    TZrUInt32 refExportEffect;
    TZrUInt32 escapeFlags;
    TZrUInt32 reserved0;
    TZrUInt64 contractHash;
} SZrArtifactContractRow;

typedef struct SZrArtifactLayoutRow {
    TZrMetadataToken typeToken;
    TZrUInt32 version;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlignment;
    TZrUInt32 gcScanKind;
    TZrUInt32 capabilityFlags;
    TZrUInt32 ownershipMapOffset;
    TZrUInt32 ownershipMapLength;
    TZrUInt64 layoutHash;
    TZrUInt64 stableSlotContractHash;
} SZrArtifactLayoutRow;

typedef struct SZrArtifactRelocationRow {
    TZrUInt32 codeOffset;
    TZrUInt32 kind;
    TZrMetadataToken targetToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 expectedSignatureHash;
    TZrUInt64 expectedContractHash;
    TZrUInt64 expectedModuleHash;
} SZrArtifactRelocationRow;

typedef struct SZrArtifactSectionInput {
    EZrArtifactSectionKind kind;
    TZrUInt32 flags;
    TZrUInt32 elementCount;
    const void *data;
} SZrArtifactSectionInput;

typedef struct SZrArtifactDocument {
    EZrArtifactKind kind;
    TZrUInt32 flags;
    SZrArtifactPublicIdentity identity;
    TZrUInt32 sectionCount;
    const SZrArtifactSectionInput *sections;
} SZrArtifactDocument;

typedef struct SZrArtifactSectionView {
    TZrUInt32 kind;
    TZrUInt32 flags;
    TZrUInt32 byteOffset;
    TZrUInt32 byteLength;
    TZrUInt32 elementCount;
    TZrUInt32 elementSize;
    const TZrByte *data;
} SZrArtifactSectionView;

typedef struct SZrArtifactView {
    EZrArtifactKind kind;
    TZrUInt32 flags;
    SZrArtifactPublicIdentity identity;
    TZrUInt32 sectionCount;
    const TZrByte *buffer;
    TZrSize bufferLength;
} SZrArtifactView;

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_GetEncodedSize(
        const SZrArtifactDocument *document,
        TZrSize *outSize,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_Write(
        const SZrArtifactDocument *document,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_Read(
        const TZrByte *buffer,
        TZrSize bufferLength,
        SZrArtifactView *outView,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_FindSection(
        const SZrArtifactView *view,
        EZrArtifactSectionKind kind,
        SZrArtifactSectionView *outSection,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadTypeDefRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactTypeDefRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadTypeIdentityRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactTypeIdentityRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadMemberDefRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactMemberDefRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadPropertyDefRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactPropertyDefRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadContractRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactContractRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadLayoutRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactLayoutRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadRelocationRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactRelocationRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ValidatePublicIdentity(
        const SZrArtifactView *view,
        const SZrArtifactPublicIdentity *expected,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ValidateSignature(
        const TZrByte *signature,
        TZrSize signatureLength,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API TZrUInt64 ZrCore_Artifact_HashBytes(const TZrByte *bytes, TZrSize byteLength);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_WriteText(
        const SZrArtifactView *view,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadText(
        const TZrChar *text,
        TZrSize textLength,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

ZR_CORE_API const TZrChar *ZrCore_Artifact_StatusName(EZrArtifactStatus status);
ZR_CORE_API const TZrChar *ZrCore_Artifact_SectionName(TZrUInt32 sectionKind);

#endif // ZR_VM_CORE_ARTIFACT_SCHEMA_H
