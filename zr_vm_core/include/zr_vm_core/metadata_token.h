#ifndef ZR_VM_CORE_METADATA_TOKEN_H
#define ZR_VM_CORE_METADATA_TOKEN_H

#include "zr_vm_core/conf.h"

struct SZrString;

typedef TZrUInt32 TZrMetadataToken;

typedef enum EZrMetadataTableTag {
    ZR_METADATA_TABLE_MODULE = 1,
    ZR_METADATA_TABLE_TYPE_DEF = 2,
    ZR_METADATA_TABLE_MEMBER_DEF = 3,
    ZR_METADATA_TABLE_ASSEMBLY_REF = 4,
    ZR_METADATA_TABLE_TYPE_REF = 5,
    ZR_METADATA_TABLE_MEMBER_REF = 6,
    ZR_METADATA_TABLE_TYPE_SPEC = 7,
    ZR_METADATA_TABLE_SIGNATURE = 8
} EZrMetadataTableTag;

#define ZR_METADATA_TOKEN_TABLE_SHIFT 24U
#define ZR_METADATA_TOKEN_RID_MASK ((TZrUInt32)0x00FFFFFFu)
#define ZR_METADATA_TOKEN_TABLE_MASK ((TZrUInt32)0xFFu)
#define ZR_METADATA_TOKEN_MAKE(TABLE, RID)                                                                            \
    ((((TZrUInt32)(TABLE) & ZR_METADATA_TOKEN_TABLE_MASK) << ZR_METADATA_TOKEN_TABLE_SHIFT) |                         \
     ((TZrUInt32)(RID) & ZR_METADATA_TOKEN_RID_MASK))
#define ZR_METADATA_TOKEN_TABLE(TOKEN)                                                                                \
    (((TZrUInt32)(TOKEN) >> ZR_METADATA_TOKEN_TABLE_SHIFT) & ZR_METADATA_TOKEN_TABLE_MASK)
#define ZR_METADATA_TOKEN_RID(TOKEN) ((TZrUInt32)(TOKEN) & ZR_METADATA_TOKEN_RID_MASK)

typedef enum EZrMetadataSignatureNode {
    ZR_METADATA_SIGNATURE_NODE_INVALID = 0,
    ZR_METADATA_SIGNATURE_NODE_PRIMITIVE = 1,
    ZR_METADATA_SIGNATURE_NODE_TYPE_REF = 2,
    ZR_METADATA_SIGNATURE_NODE_TYPE_DEF = 3,
    ZR_METADATA_SIGNATURE_NODE_ARRAY = 4,
    ZR_METADATA_SIGNATURE_NODE_TUPLE = 5,
    ZR_METADATA_SIGNATURE_NODE_FUNC = 6,
    ZR_METADATA_SIGNATURE_NODE_GENERIC_INST = 7,
    ZR_METADATA_SIGNATURE_NODE_OWNERSHIP = 8,
    ZR_METADATA_SIGNATURE_NODE_UNION = 9,
    ZR_METADATA_SIGNATURE_NODE_NULLABLE = 10,
    ZR_METADATA_SIGNATURE_NODE_MEMBER_REF = 11,
    ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF = 12,
    ZR_METADATA_SIGNATURE_NODE_METHOD_SIG = 13,
    ZR_METADATA_SIGNATURE_NODE_FIELD_SIG = 14,
    ZR_METADATA_SIGNATURE_NODE_MODULE = 15
} EZrMetadataSignatureNode;

typedef struct SZrMetadataTokenRecord {
    TZrMetadataToken token;
    TZrMetadataToken relatedToken;
    TZrMetadataToken ownerToken;
    TZrUInt32 ownerIndex;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 targetSignatureHash;
    TZrUInt64 targetModuleSignatureHash;
    struct SZrString *requestedModuleVersion;
    struct SZrString *minModuleVersionInclusive;
    struct SZrString *maxModuleVersionExclusive;
} SZrMetadataTokenRecord;

typedef struct SZrMetadataStringHeapEntry {
    TZrUInt32 stringIndex;
    struct SZrString *value;
} SZrMetadataStringHeapEntry;

typedef struct SZrMetadataTokenBinding {
    TZrMetadataToken refToken;
    TZrMetadataToken refSignatureToken;
    TZrUInt64 refSignatureHash;
    TZrMetadataToken expectedMetadataToken;
    TZrMetadataToken expectedSignatureToken;
    TZrUInt64 expectedSignatureHash;
    TZrUInt64 expectedModuleSignatureHash;
    TZrUInt32 expectedLayoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 expectedLayoutHash;
    TZrMetadataToken resolvedMetadataToken;
    TZrMetadataToken resolvedSignatureToken;
    TZrUInt64 resolvedSignatureHash;
    TZrUInt64 resolvedModuleSignatureHash;
    TZrUInt32 resolvedLayoutVersion;
    TZrUInt32 reserved1;
    TZrUInt64 resolvedLayoutHash;
} SZrMetadataTokenBinding;

typedef struct SZrMetadataTypeSpecBindStatus {
    TZrUInt32 callerTypeSpecCount;
    TZrUInt32 matchedTypeSpecCount;
    TZrUInt32 unmatchedTypeSpecCount;
    TZrUInt32 definitionMismatchCount;
    TZrUInt32 layoutMismatchCount;
    TZrMetadataToken firstUnmatchedTypeSpecToken;
    TZrUInt64 firstUnmatchedSignatureHash;
    TZrMetadataToken firstDefinitionMismatchTypeSpecToken;
    TZrUInt64 firstExpectedDefinitionSignatureHash;
    TZrUInt64 firstActualDefinitionSignatureHash;
    TZrMetadataToken firstLayoutMismatchTypeSpecToken;
    TZrUInt32 firstExpectedLayoutVersion;
    TZrUInt32 firstActualLayoutVersion;
    TZrUInt64 firstExpectedLayoutHash;
    TZrUInt64 firstActualLayoutHash;
} SZrMetadataTypeSpecBindStatus;

typedef struct SZrMetadataTypeRefBindStatus {
    TZrUInt32 callerTypeRefCount;
    TZrUInt32 matchedTypeRefCount;
    TZrUInt32 unmatchedTypeRefCount;
    TZrUInt32 definitionMismatchCount;
    TZrUInt32 layoutMismatchCount;
    TZrMetadataToken firstUnmatchedTypeRefToken;
    TZrUInt64 firstUnmatchedSignatureHash;
    TZrMetadataToken firstDefinitionMismatchTypeRefToken;
    TZrUInt64 firstExpectedDefinitionSignatureHash;
    TZrUInt64 firstActualDefinitionSignatureHash;
    TZrMetadataToken firstLayoutMismatchTypeRefToken;
    TZrUInt32 firstExpectedLayoutVersion;
    TZrUInt32 firstActualLayoutVersion;
    TZrUInt64 firstExpectedLayoutHash;
    TZrUInt64 firstActualLayoutHash;
} SZrMetadataTypeRefBindStatus;

#endif // ZR_VM_CORE_METADATA_TOKEN_H
