//
// Runtime reflection helpers for `%type`.
//

#ifndef ZR_VM_CORE_REFLECTION_H
#define ZR_VM_CORE_REFLECTION_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"

#define ZR_REFLECTION_MODULE_NAME "zr.reflection"

struct SZrState;
struct SZrCallInfo;
struct SZrClosureNative;
struct SZrObject;
struct SZrObjectPrototype;
struct SZrObjectModule;
struct SZrFunction;
struct SZrMetadataRuntime;
struct SZrTypeLayout;
struct SZrString;
struct SZrTypeValue;
struct SZrZrpMetadataFieldDefRow;
struct SZrZrpMetadataGenericParamRow;
struct SZrZrpMetadataGenericParamConstraintRow;
struct SZrZrpMetadataTypeDefRow;
struct SZrZrpMetadataTypeSpecRow;

typedef enum EZrReflectionResolvedTokenKind {
    ZR_REFLECTION_RESOLVED_TOKEN_NONE = 0,
    ZR_REFLECTION_RESOLVED_TOKEN_TYPE = 1,
    ZR_REFLECTION_RESOLVED_TOKEN_METHOD = 2,
    ZR_REFLECTION_RESOLVED_TOKEN_FIELD = 3
} EZrReflectionResolvedTokenKind;

typedef struct SZrReflectionResolvedGenericArgument {
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken genericSignatureToken;
    TZrUInt64 genericSignatureHash;
    TZrMetadataToken genericBaseToken;
    const SZrMetadataTokenRecord *genericBaseRecord;
    TZrUInt32 argumentIndex;
    TZrUInt32 argumentNodeKind;
    TZrUInt32 argumentPayload0;
    TZrUInt32 argumentPayload1;
    TZrMetadataToken argumentToken;
    const SZrMetadataTokenRecord *argumentRecord;
} SZrReflectionResolvedGenericArgument;

typedef enum EZrReflectionGenericInstanceRoute {
    ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE = 0,
    ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT = 1,
    ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT = 2
} EZrReflectionGenericInstanceRoute;

typedef enum EZrReflectionGenericTypeArgumentKind {
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE = 0,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE = 1,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN = 2,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY = 3,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE = 4,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP = 5,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE = 6,
    ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION = 7
} EZrReflectionGenericTypeArgumentKind;

typedef enum EZrReflectionOwnershipQualifier {
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_NONE = 0,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_UNIQUE = 1,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_SHARED = 2,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_WEAK = 3,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_BORROWED = 4,
    ZR_REFLECTION_OWNERSHIP_QUALIFIER_LOANED = 5
} EZrReflectionOwnershipQualifier;

typedef struct SZrReflectionGenericTypeArgument {
    EZrReflectionGenericTypeArgumentKind kind;
    TZrUInt32 primitiveValueType;
    TZrMetadataToken typeToken;
    TZrUInt32 arrayRank;
    const struct SZrReflectionGenericTypeArgument *elementType;
    EZrReflectionOwnershipQualifier ownershipQualifier;
    TZrUInt32 childCount;
    const struct SZrReflectionGenericTypeArgument *childTypes;
    TZrUInt32 unionValueType;
    TZrUInt32 unionNameStringOffset;
} SZrReflectionGenericTypeArgument;

typedef struct SZrReflectionDynamicGenericTypeInstance {
    EZrReflectionGenericInstanceRoute route;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken genericSignatureToken;
    TZrUInt64 genericSignatureHash;
    TZrMetadataToken genericBaseToken;
    const SZrMetadataTokenRecord *genericBaseRecord;
    TZrUInt32 genericArgumentCount;
    TZrUInt32 genericArgumentListBlobOffset;
    /* Borrowed from ResolveConstructedGenericType() input; null for token-only resolution. */
    const SZrReflectionGenericTypeArgument *requestedArguments;
    TZrUInt32 typeLayoutId;
    const struct SZrTypeLayout *typeLayout;
} SZrReflectionDynamicGenericTypeInstance;

typedef struct SZrReflectionResolvedGenericMethodSpec {
    TZrMetadataToken methodSpecToken;
    const SZrMetadataTokenRecord *methodSpecRecord;
    TZrMetadataToken genericMethodToken;
    const SZrMetadataTokenRecord *genericMethodRecord;
    TZrUInt64 genericSignatureHash;
    TZrUInt32 genericArgumentCount;
    TZrUInt32 genericArgumentListBlobOffset;
    /* Borrowed from ResolveConstructedGenericMethod() input. */
    const SZrReflectionGenericTypeArgument *requestedArguments;
} SZrReflectionResolvedGenericMethodSpec;

typedef struct SZrReflectionResolvedGenericParameter {
    TZrMetadataToken ownerToken;
    const SZrMetadataTokenRecord *ownerRecord;
    const struct SZrZrpMetadataGenericParamRow *genericParamRow;
    TZrUInt32 genericParamIndex;
    TZrUInt32 parameterIndex;
    TZrUInt32 nameStringOffset;
    TZrUInt32 firstConstraintIndex;
    TZrUInt32 constraintCount;
    TZrUInt32 flags;
} SZrReflectionResolvedGenericParameter;

typedef struct SZrReflectionResolvedGenericParameterConstraint {
    SZrReflectionResolvedGenericParameter genericParameter;
    const struct SZrZrpMetadataGenericParamConstraintRow *constraintRow;
    TZrUInt32 constraintIndex;
    TZrMetadataToken constraintTypeToken;
    const SZrMetadataTokenRecord *constraintTypeRecord;
    const TZrByte *signatureBlobData;
    TZrSize signatureBlobByteLength;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrReflectionResolvedGenericParameterConstraint;

typedef struct SZrReflectionResolvedMethodSpecGenericArgument {
    TZrMetadataToken methodSpecToken;
    TZrMetadataToken methodToken;
    const SZrMetadataTokenRecord *methodRecord;
    TZrUInt64 genericSignatureHash;
    TZrUInt32 argumentIndex;
    TZrUInt32 argumentNodeKind;
    TZrUInt32 argumentPayload0;
    TZrUInt32 argumentPayload1;
    TZrMetadataToken argumentToken;
    const SZrMetadataTokenRecord *argumentRecord;
} SZrReflectionResolvedMethodSpecGenericArgument;

typedef struct SZrReflectionResolvedToken {
    EZrReflectionResolvedTokenKind kind;
    TZrMetadataToken token;
    const SZrMetadataTokenRecord *record;
    TZrMetadataToken methodToken;
    const SZrMetadataTokenRecord *methodRecord;
    TZrMetadataToken methodSignatureToken;
    const SZrMetadataTokenRecord *methodSignatureRecord;
    TZrUInt64 methodSignatureHash;
    TZrUInt32 methodFunctionIndex;
    const SZrAotMethodInfo *methodInfo;
    FZrAotEntryThunk methodFunctionPointer;
    FZrAotReflectionInvoker methodInvoker;
    const struct SZrZrpMetadataTypeDefRow *typeDefRow;
    const struct SZrZrpMetadataTypeSpecRow *typeSpecRow;
    TZrMetadataToken genericSignatureToken;
    TZrUInt64 genericSignatureHash;
    TZrMetadataToken genericBaseToken;
    const SZrMetadataTokenRecord *genericBaseRecord;
    TZrUInt32 genericArgumentCount;
    TZrUInt32 genericArgumentListBlobOffset;
    TZrUInt32 typeLayoutId;
    TZrUInt32 cTypeId;
    const struct SZrTypeLayout *typeLayout;
    const struct SZrZrpMetadataFieldDefRow *fieldDefRow;
    TZrMetadataToken ownerTypeToken;
    const SZrMetadataTokenRecord *ownerTypeRecord;
    const struct SZrZrpMetadataTypeDefRow *ownerTypeDefRow;
    TZrMetadataToken fieldTypeToken;
    const SZrMetadataTokenRecord *fieldTypeRecord;
    TZrUInt32 byteOffset;
    TZrUInt32 fieldTypeLayoutId;
    TZrUInt32 ownerTypeLayoutId;
    const struct SZrTypeLayout *fieldTypeLayout;
    const struct SZrTypeLayout *ownerTypeLayout;
} SZrReflectionResolvedToken;

ZR_CORE_API TZrInt64 ZrCore_Reflection_TypeOfNativeEntry(struct SZrState *state);

ZR_CORE_API TZrBool ZrCore_Reflection_TypeOfValue(struct SZrState *state,
                                                  const struct SZrTypeValue *targetValue,
                                                  struct SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Reflection_IsReflectionObject(struct SZrState *state, struct SZrObject *object);

ZR_CORE_API struct SZrString *ZrCore_Reflection_FormatObject(struct SZrState *state, struct SZrObject *object);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildTypeLiteralObject(struct SZrState *state,
                                                                       struct SZrString *typeName);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildCallableTypeLiteralObject(
        struct SZrState *state,
        struct SZrString *callableName,
        struct SZrString *returnTypeName,
        struct SZrString *const *parameterNames,
        struct SZrString *const *parameterTypeNames,
        struct SZrString *const *parameterModeNames,
        TZrUInt32 parameterCount,
        struct SZrString *const *genericParameterNames,
        TZrUInt32 genericParameterCount,
        TZrBool isVariadic);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildFieldInfoTokenObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectValue(
        struct SZrState *state,
        struct SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        struct SZrTypeValue *outValue);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedValue(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const struct SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveToken(struct SZrMetadataRuntime *runtime,
                                                   TZrMetadataToken token,
                                                   SZrReflectionResolvedToken *outResolved);

ZR_CORE_API TZrBool ZrCore_Reflection_InvokeMethodToken(struct SZrState *state,
                                                        struct SZrMetadataRuntime *runtime,
                                                        TZrMetadataToken methodToken,
                                                        struct SZrTypeValue *self,
                                                        struct SZrTypeValue *args,
                                                        struct SZrTypeValue *outReturn);

ZR_CORE_API TZrBool ZrCore_Reflection_InvokeMethodTokenWithArgCount(struct SZrState *state,
                                                                    struct SZrMetadataRuntime *runtime,
                                                                    TZrMetadataToken methodToken,
                                                                    struct SZrTypeValue *self,
                                                                    struct SZrTypeValue *args,
                                                                    TZrUInt32 argCount,
                                                                    struct SZrTypeValue *outReturn);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveTypeSpecGenericArgument(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex,
        SZrReflectionResolvedGenericArgument *outArgument);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrReflectionDynamicGenericTypeInstance *outInstance);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
        struct SZrMetadataRuntime *requesterRuntime,
        TZrMetadataToken requesterTypeSpecToken,
        struct SZrMetadataRuntime *providerRuntime,
        SZrReflectionDynamicGenericTypeInstance *outInstance);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveConstructedGenericType(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionDynamicGenericTypeInstance *outInstance);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveConstructedGenericMethod(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionResolvedGenericMethodSpec *outMethodSpec);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildConstructedGenericMethodObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionResolvedGenericMethodSpec *methodSpec);

ZR_CORE_API TZrBool ZrCore_Reflection_RevalidateDynamicGenericTypeInstance(
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        SZrReflectionDynamicGenericTypeInstance *outResolved);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_MakeGenericTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_MakeGenericMethodObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_MakeGenericMethodFromObjects(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrObject *genericMethodDefinition,
        struct SZrObject *genericArguments);

ZR_CORE_API TZrInt64 ZrCore_Reflection_MakeGenericMethodNativeEntry(struct SZrState *state);

ZR_CORE_API struct SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime);

ZR_CORE_API struct SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntime(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime);

ZR_CORE_API struct SZrObjectModule *ZrCore_Reflection_GetOrCreateModuleForRuntime(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildGenericMethodDefinitionObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildMethodSpecGenericContextObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken);

ZR_CORE_API TZrBool ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrCallInfo *callInfo,
        TZrMetadataToken methodSpecToken);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
        struct SZrState *state,
        struct SZrCallInfo *callInfo);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrCallInfo *callInfo,
        TZrMetadataToken genericMethodToken,
        TZrUInt32 parameterIndex);

ZR_CORE_API TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        struct SZrFunction *function,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_NewInterpreterGenericInstanceObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        struct SZrObjectPrototype *openPrototype);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(
        struct SZrState *state,
        struct SZrObject *instanceObject);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex);

ZR_CORE_API TZrBool ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo(
        struct SZrState *state,
        struct SZrCallInfo *callInfo,
        struct SZrObject *instanceObject);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
        struct SZrState *state,
        struct SZrCallInfo *callInfo);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrCallInfo *callInfo,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex);

ZR_CORE_API TZrBool ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        struct SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        struct SZrFunction *function,
        const struct SZrTypeValue *arguments,
        TZrSize argumentCount,
        struct SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveGenericParameter(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrReflectionResolvedGenericParameter *outParameter);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveGenericParameterConstraint(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        TZrUInt32 constraintIndex,
        SZrReflectionResolvedGenericParameterConstraint *outConstraint);

ZR_CORE_API TZrBool ZrCore_Reflection_ResolveMethodSpecGenericArgument(
        struct SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        TZrUInt32 argumentIndex,
        SZrReflectionResolvedMethodSpecGenericArgument *outArgument);

ZR_CORE_API void ZrCore_Reflection_AttachModuleRuntimeMetadata(struct SZrState *state,
                                                               struct SZrObjectModule *module,
                                                               struct SZrFunction *entryFunction);

ZR_CORE_API void ZrCore_Reflection_AttachPrototypeRuntimeMetadata(struct SZrState *state,
                                                                  struct SZrObjectPrototype *prototype,
                                                                  struct SZrObjectModule *module,
                                                                  struct SZrFunction *entryFunction);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildDecoratorTargetMemberReflection(
        struct SZrState *state,
        struct SZrObjectPrototype *prototype,
        struct SZrString *memberName,
        TZrUInt32 targetKind);

#endif // ZR_VM_CORE_REFLECTION_H
