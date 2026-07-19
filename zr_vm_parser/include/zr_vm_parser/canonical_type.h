#ifndef ZR_VM_PARSER_CANONICAL_TYPE_H
#define ZR_VM_PARSER_CANONICAL_TYPE_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

#ifndef ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
#define ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
typedef TZrUInt32 TZrTypeId;
typedef TZrUInt32 TZrSymbolId;
typedef TZrUInt32 TZrOverloadSetId;
typedef TZrUInt32 TZrLifetimeRegionId;
#endif

struct SZrSemanticContext;
struct SZrInferredType;

enum EZrCanonicalTypeKind {
    ZR_CANONICAL_TYPE_PRIMITIVE = 0,
    ZR_CANONICAL_TYPE_NOMINAL,
    ZR_CANONICAL_TYPE_GENERIC_PARAMETER,
    ZR_CANONICAL_TYPE_GENERIC_INSTANCE,
    ZR_CANONICAL_TYPE_ARRAY,
    ZR_CANONICAL_TYPE_TUPLE,
    ZR_CANONICAL_TYPE_UNION,
    ZR_CANONICAL_TYPE_ERROR,
    ZR_CANONICAL_TYPE_NEVER,
    ZR_CANONICAL_TYPE_REF,
    ZR_CANONICAL_TYPE_OWNER,
    ZR_CANONICAL_TYPE_READONLY_VIEW,
    ZR_CANONICAL_TYPE_NULLABLE,
    ZR_CANONICAL_TYPE_FUNCTION,
};

typedef enum EZrCanonicalTypeKind EZrCanonicalTypeKind;

enum EZrCanonicalArrayStorageKind {
    ZR_CANONICAL_ARRAY_STORAGE_MANAGED = 0,
    ZR_CANONICAL_ARRAY_STORAGE_INLINE,
    ZR_CANONICAL_ARRAY_STORAGE_NATIVE,
};

typedef enum EZrCanonicalArrayStorageKind EZrCanonicalArrayStorageKind;

enum EZrCanonicalRefAccess {
    ZR_CANONICAL_REF_WRITABLE = 0,
    ZR_CANONICAL_REF_READONLY,
};

typedef enum EZrCanonicalRefAccess EZrCanonicalRefAccess;

enum EZrCanonicalOwnerKind {
    ZR_CANONICAL_OWNER_UNIQUE = 0,
    ZR_CANONICAL_OWNER_SHARED,
    ZR_CANONICAL_OWNER_WEAK,
    ZR_CANONICAL_OWNER_ATOMIC_SHARED,
};

typedef enum EZrCanonicalOwnerKind EZrCanonicalOwnerKind;

enum EZrCanonicalPassingForm {
    ZR_CANONICAL_PASSING_VALUE = 0,
    ZR_CANONICAL_PASSING_IN,
    ZR_CANONICAL_PASSING_REF,
    ZR_CANONICAL_PASSING_REF_READONLY,
    ZR_CANONICAL_PASSING_OUT,
};

typedef enum EZrCanonicalPassingForm EZrCanonicalPassingForm;

enum EZrCanonicalEscapeUpperBound {
    ZR_CANONICAL_ESCAPE_BLOCK = 0,
    ZR_CANONICAL_ESCAPE_FUNCTION,
    ZR_CANONICAL_ESCAPE_CALLER,
    ZR_CANONICAL_ESCAPE_HEAP_STATIC,
    ZR_CANONICAL_ESCAPE_UNKNOWN,
};

typedef enum EZrCanonicalEscapeUpperBound EZrCanonicalEscapeUpperBound;

enum EZrCanonicalEntryInitialization {
    ZR_CANONICAL_ENTRY_INITIALIZED = 0,
    ZR_CANONICAL_ENTRY_UNINITIALIZED,
};

typedef enum EZrCanonicalEntryInitialization EZrCanonicalEntryInitialization;

enum EZrCanonicalExitInitialization {
    ZR_CANONICAL_EXIT_UNCHANGED = 0,
    ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED,
};

typedef enum EZrCanonicalExitInitialization EZrCanonicalExitInitialization;

enum EZrCanonicalCallSiteMarker {
    ZR_CANONICAL_CALL_SITE_NONE = 0,
    ZR_CANONICAL_CALL_SITE_REF,
    ZR_CANONICAL_CALL_SITE_OUT,
};

typedef enum EZrCanonicalCallSiteMarker EZrCanonicalCallSiteMarker;

enum EZrCanonicalReceiverEffect {
    ZR_CANONICAL_RECEIVER_NONE = 0,
    ZR_CANONICAL_RECEIVER_READONLY,
    ZR_CANONICAL_RECEIVER_MUTABLE,
};

typedef enum EZrCanonicalReceiverEffect EZrCanonicalReceiverEffect;

#define ZR_CANONICAL_CALLABLE_EFFECT_NONE ((TZrUInt32)0U)
#define ZR_CANONICAL_CALLABLE_EFFECT_THROWS ((TZrUInt32)1U << 0U)
#define ZR_CANONICAL_CALLABLE_EFFECT_ASYNC ((TZrUInt32)1U << 1U)
#define ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR ((TZrUInt32)1U << 2U)

#define ZR_CANONICAL_TYPE_CAPABILITY_NONE ((TZrUInt32)0U)
#define ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE ((TZrUInt32)1U << 0U)
#define ZR_CANONICAL_TYPE_CAPABILITY_GC_CLASS ((TZrUInt32)1U << 1U)
#define ZR_CANONICAL_TYPE_CAPABILITY_RESOURCE_CLASS ((TZrUInt32)1U << 2U)
#define ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE ((TZrUInt32)1U << 3U)
#define ZR_CANONICAL_TYPE_CAPABILITY_READONLY_TYPE ((TZrUInt32)1U << 4U)
#define ZR_CANONICAL_TYPE_CAPABILITY_REF_LIKE ((TZrUInt32)1U << 5U)
#define ZR_CANONICAL_TYPE_CAPABILITY_HAS_DROP ((TZrUInt32)1U << 6U)
#define ZR_CANONICAL_TYPE_CAPABILITY_HAS_GC_REFERENCES ((TZrUInt32)1U << 7U)
#define ZR_CANONICAL_TYPE_CAPABILITY_HAS_OWNERSHIP_FIELDS ((TZrUInt32)1U << 8U)
#define ZR_CANONICAL_TYPE_CAPABILITY_BLITTABLE ((TZrUInt32)1U << 9U)
#define ZR_CANONICAL_TYPE_CAPABILITY_SEND ((TZrUInt32)1U << 10U)
#define ZR_CANONICAL_TYPE_CAPABILITY_SYNC ((TZrUInt32)1U << 11U)

enum EZrCanonicalGcScanKind {
    ZR_CANONICAL_GC_SCAN_FREE = 0,
    ZR_CANONICAL_GC_SCAN_MAPPED,
    ZR_CANONICAL_GC_SCAN_BARRIERED,
};

typedef enum EZrCanonicalGcScanKind EZrCanonicalGcScanKind;

enum EZrCanonicalGenericArgumentKind {
    ZR_CANONICAL_GENERIC_ARGUMENT_TYPE = 0,
    ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT,
    ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER,
};

typedef enum EZrCanonicalGenericArgumentKind EZrCanonicalGenericArgumentKind;

typedef struct SZrCanonicalConstParameterReference {
    TZrSymbolId ownerSymbolId;
    TZrUInt32 ordinal;
    SZrString *displayName;
} SZrCanonicalConstParameterReference;

typedef struct SZrCanonicalGenericArgument {
    EZrCanonicalGenericArgumentKind kind;
    union {
        TZrTypeId typeId;
        TZrInt64 constIntValue;
        SZrCanonicalConstParameterReference constParameter;
    } data;
} SZrCanonicalGenericArgument;

typedef struct SZrCanonicalPrimitiveType {
    EZrValueType valueType;
} SZrCanonicalPrimitiveType;

typedef struct SZrCanonicalNominalType {
    SZrString *moduleIdentity;
    SZrString *name;
    TZrUInt32 definitionToken;
} SZrCanonicalNominalType;

typedef struct SZrCanonicalGenericParameterType {
    TZrSymbolId ownerSymbolId;
    TZrUInt32 ordinal;
} SZrCanonicalGenericParameterType;

typedef struct SZrCanonicalGenericInstanceType {
    TZrTypeId definitionTypeId;
    SZrArray arguments; // SZrCanonicalGenericArgument
} SZrCanonicalGenericInstanceType;

typedef struct SZrCanonicalArrayType {
    TZrTypeId elementTypeId;
    TZrUInt32 rank;
    EZrCanonicalArrayStorageKind storageKind;
} SZrCanonicalArrayType;

typedef struct SZrCanonicalTypeList {
    SZrArray elementTypeIds; // TZrTypeId
} SZrCanonicalTypeList;

typedef struct SZrCanonicalUnionType {
    TZrTypeId definitionTypeId;
    SZrArray variantTypeIds; // TZrTypeId
} SZrCanonicalUnionType;

typedef struct SZrCanonicalRefType {
    TZrTypeId pointeeTypeId;
    EZrCanonicalRefAccess access;
} SZrCanonicalRefType;

typedef struct SZrCanonicalOwnerType {
    TZrTypeId targetTypeId;
    EZrCanonicalOwnerKind ownerKind;
} SZrCanonicalOwnerType;

typedef struct SZrCanonicalTargetType {
    TZrTypeId targetTypeId;
} SZrCanonicalTargetType;

typedef struct SZrCanonicalParameterContract {
    TZrTypeId typeId;
    EZrCanonicalPassingForm passingForm;
    EZrCanonicalEscapeUpperBound escapeUpperBound;
    EZrCanonicalEntryInitialization entryInitialization;
    EZrCanonicalExitInitialization exitInitialization;
    TZrBool acceptsTemporary;
    EZrCanonicalCallSiteMarker callSiteMarker;
} SZrCanonicalParameterContract;

typedef struct SZrCanonicalFunctionType {
    SZrArray parameterContracts; // SZrCanonicalParameterContract
    TZrTypeId returnTypeId;
    EZrCanonicalReceiverEffect receiverEffect;
    TZrUInt32 effectFlags;
} SZrCanonicalFunctionType;

typedef struct SZrCanonicalGenericBinding {
    SZrString *name;
    EZrCanonicalGenericArgumentKind kind;
    TZrTypeId typeId;
    TZrSymbolId ownerSymbolId;
    TZrUInt32 ordinal;
} SZrCanonicalGenericBinding;

typedef struct SZrCanonicalTypeNode {
    TZrTypeId id;
    EZrCanonicalTypeKind kind;
    TZrUInt64 structuralHash;
    union {
        SZrCanonicalPrimitiveType primitive;
        SZrCanonicalNominalType nominal;
        SZrCanonicalGenericParameterType genericParameter;
        SZrCanonicalGenericInstanceType genericInstance;
        SZrCanonicalArrayType array;
        SZrCanonicalTypeList typeList;
        SZrCanonicalUnionType unionType;
        SZrCanonicalRefType refType;
        SZrCanonicalOwnerType owner;
        SZrCanonicalTargetType target;
        SZrCanonicalFunctionType function;
    } data;
} SZrCanonicalTypeNode;

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternPrimitive(
        struct SZrSemanticContext *context,
        EZrValueType valueType);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternNominal(
        struct SZrSemanticContext *context,
        SZrString *moduleIdentity,
        SZrString *name,
        TZrUInt32 definitionToken);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternGenericInstance(
        struct SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const TZrTypeId *argumentTypeIds,
        TZrSize argumentCount);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternGenericInstanceEx(
        struct SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternGenericParameter(
        struct SZrSemanticContext *context,
        TZrSymbolId ownerSymbolId,
        TZrUInt32 ordinal);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternArray(
        struct SZrSemanticContext *context,
        TZrTypeId elementTypeId,
        TZrUInt32 rank,
        EZrCanonicalArrayStorageKind storageKind);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternTuple(
        struct SZrSemanticContext *context,
        const TZrTypeId *elementTypeIds,
        TZrSize elementCount);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternUnion(
        struct SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const TZrTypeId *variantTypeIds,
        TZrSize variantCount);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternError(struct SZrSemanticContext *context);
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternNever(struct SZrSemanticContext *context);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternRef(
        struct SZrSemanticContext *context,
        TZrTypeId pointeeTypeId,
        EZrCanonicalRefAccess access);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternOwner(
        struct SZrSemanticContext *context,
        TZrTypeId targetTypeId,
        EZrCanonicalOwnerKind ownerKind);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternReadonlyView(
        struct SZrSemanticContext *context,
        TZrTypeId targetTypeId);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternNullable(
        struct SZrSemanticContext *context,
        TZrTypeId targetTypeId);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternFunction(
        struct SZrSemanticContext *context,
        const SZrCanonicalParameterContract *parameterContracts,
        TZrSize parameterCount,
        TZrTypeId returnTypeId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags);

ZR_PARSER_API const SZrCanonicalTypeNode *ZrParser_CanonicalType_Find(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_Format(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromName(
        struct SZrSemanticContext *context,
        SZrString *qualifiedName);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromInferred(
        struct SZrSemanticContext *context,
        const struct SZrInferredType *type);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromInferredWithGenericBindings(
        struct SZrSemanticContext *context,
        const struct SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromFunctionSignature(
        struct SZrSemanticContext *context,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        const struct SZrInferredType *returnType,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromFunctionSignatureWithGenericBindings(
        struct SZrSemanticContext *context,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        const struct SZrInferredType *returnType,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterDefinition(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterGenericDefinition(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterGenericDefinitionEx(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        const EZrCanonicalGenericArgumentKind *parameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterDefinitionProjection(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterGenericDefinitionProjection(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        const EZrCanonicalGenericArgumentKind *parameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_ResolveProjection(
        struct SZrSemanticContext *context,
        TZrTypeId typeId);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_GetGcScanKind(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrCanonicalGcScanKind *outGcScanKind);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterConstructor(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId constructorSymbolId,
        const TZrTypeId *parameterTypeIds,
        TZrSize parameterCount,
        TZrBool isPublic);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_HasCapabilities(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 requiredCapabilityFlags);

ZR_PARSER_API TZrBool ZrParser_CanonicalType_ResolveValueConstructor(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const TZrTypeId *argumentTypeIds,
        TZrSize argumentCount,
        TZrSymbolId *outConstructorSymbolId);

ZR_PARSER_API void ZrParser_CanonicalType_Reset(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalType_Free(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeIndex_Init(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeIndex_Reset(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeIndex_Free(struct SZrSemanticContext *context);
ZR_PARSER_API TZrBool ZrParser_CanonicalTypeIndex_Insert(
        struct SZrSemanticContext *context,
        TZrSize nodeIndex);
ZR_PARSER_API TZrSize ZrParser_CanonicalTypeIndex_First(
        const struct SZrSemanticContext *context,
        TZrUInt64 structuralHash);
ZR_PARSER_API TZrSize ZrParser_CanonicalTypeIndex_Next(
        const struct SZrSemanticContext *context,
        TZrSize nodeIndex);
ZR_PARSER_API void ZrParser_CanonicalTypeDefinition_Init(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeDefinition_Reset(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeDefinition_Free(struct SZrSemanticContext *context);

#endif // ZR_VM_PARSER_CANONICAL_TYPE_H
