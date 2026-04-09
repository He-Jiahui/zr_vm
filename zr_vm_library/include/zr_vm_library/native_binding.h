//
// Native binding descriptors and helpers for zr_vm_library.
//

#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_common/zr_contract_conf.h"

struct ZrLibModuleDescriptor;
struct ZrLibTypeDescriptor;
struct ZrLibFunctionDescriptor;
struct ZrLibMethodDescriptor;
struct ZrLibMetaMethodDescriptor;
struct ZrLibEnumMemberDescriptor;
struct SZrCallInfo;

typedef enum EZrLibModuleCapability {
    ZR_LIB_MODULE_CAPABILITY_NONE = 0,
    ZR_LIB_MODULE_CAPABILITY_TYPE_HINTS = 1u << 0,
    ZR_LIB_MODULE_CAPABILITY_TYPE_METADATA = 1u << 1,
    ZR_LIB_MODULE_CAPABILITY_ENUM_INTERFACE_METADATA = 1u << 2,
    ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS = 1u << 3,
    ZR_LIB_MODULE_CAPABILITY_FFI_RUNTIME = 1u << 4
} EZrLibModuleCapability;

#define ZR_VM_NATIVE_RUNTIME_CAPABILITIES                                                                               \
    ((TZrUInt64)(ZR_LIB_MODULE_CAPABILITY_TYPE_HINTS | ZR_LIB_MODULE_CAPABILITY_TYPE_METADATA |                        \
                 ZR_LIB_MODULE_CAPABILITY_ENUM_INTERFACE_METADATA | ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS |       \
                 ZR_LIB_MODULE_CAPABILITY_FFI_RUNTIME))

typedef enum EZrLibConstantKind {
    ZR_LIB_CONSTANT_KIND_NULL = 0,
    ZR_LIB_CONSTANT_KIND_BOOL = 1,
    ZR_LIB_CONSTANT_KIND_INT = 2,
    ZR_LIB_CONSTANT_KIND_FLOAT = 3,
    ZR_LIB_CONSTANT_KIND_STRING = 4,
    ZR_LIB_CONSTANT_KIND_ARRAY = 5
} EZrLibConstantKind;

typedef struct ZrLibTypeHintDescriptor {
    const TZrChar *symbolName;
    const TZrChar *symbolKind;
    const TZrChar *signature;
    const TZrChar *documentation;
} ZrLibTypeHintDescriptor;

typedef struct ZrLibFieldDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
    TZrUInt32 contractRole;
} ZrLibFieldDescriptor;

typedef struct ZrLibParameterDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
} ZrLibParameterDescriptor;

typedef struct ZrLibGenericParameterDescriptor {
    const TZrChar *name;
    const TZrChar *documentation;
    const TZrChar *const *constraintTypeNames;
    TZrSize constraintTypeCount;
} ZrLibGenericParameterDescriptor;

typedef struct ZrLibEnumMemberDescriptor {
    const TZrChar *name;
    EZrLibConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
} ZrLibEnumMemberDescriptor;

typedef struct ZrLibValueView {
    const SZrTypeValue *value;
    EZrValueType runtimeType;
    const TZrChar *typeName;
} ZrLibValueView;

typedef struct ZrLibCallContext {
    SZrState *state;
    const struct ZrLibModuleDescriptor *moduleDescriptor;
    const struct ZrLibTypeDescriptor *typeDescriptor;
    const struct ZrLibFunctionDescriptor *functionDescriptor;
    const struct ZrLibMethodDescriptor *methodDescriptor;
    const struct ZrLibMetaMethodDescriptor *metaMethodDescriptor;
    struct SZrObjectPrototype *ownerPrototype;
    struct SZrObjectPrototype *constructTargetPrototype;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argumentBase;
    SZrTypeValue *argumentValues;
    TZrSize argumentCount;
    SZrTypeValue *selfValue;
} ZrLibCallContext;

typedef struct ZrLibTempValueRoot {
    SZrState *state;
    struct SZrCallInfo *callInfo;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor savedCallInfoReturnAnchor;
    SZrFunctionStackAnchor slotAnchor;
    TZrStackValuePointer savedStackTopPointer;
    TZrStackValuePointer savedCallInfoBasePointer;
    TZrStackValuePointer savedCallInfoTopPointer;
    TZrStackValuePointer savedCallInfoReturnPointer;
    TZrStackValuePointer slotPointer;
    TZrBool hasSavedCallInfoBase;
    TZrBool hasSavedCallInfoTop;
    TZrBool hasSavedCallInfoReturn;
    TZrBool restoreCallInfoTopFromSavedStackTop;
    TZrBool usesDirectPointers;
    TZrBool active;
} ZrLibTempValueRoot;

typedef TZrBool (*FZrLibBoundCallback)(ZrLibCallContext *context, SZrTypeValue *result);

typedef struct ZrLibFunctionDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 contractRole;
} ZrLibFunctionDescriptor;

typedef struct ZrLibMethodDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    TZrBool isStatic;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    TZrUInt32 contractRole;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} ZrLibMethodDescriptor;

typedef struct ZrLibMetaMethodDescriptor {
    EZrMetaType metaType;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} ZrLibMetaMethodDescriptor;

typedef struct ZrLibConstantDescriptor {
    const TZrChar *name;
    EZrLibConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
    const TZrChar *typeName;
} ZrLibConstantDescriptor;

typedef struct ZrLibModuleLinkDescriptor {
    const TZrChar *name;
    const TZrChar *moduleName;
    const TZrChar *documentation;
} ZrLibModuleLinkDescriptor;

typedef TZrBool (*FZrLibModuleMaterializeCallback)(SZrState *state,
                                                   struct SZrObjectModule *module,
                                                   const struct ZrLibModuleDescriptor *descriptor);

typedef struct ZrLibTypeDescriptor {
    const TZrChar *name;
    EZrObjectPrototypeType prototypeType;
    const ZrLibFieldDescriptor *fields;
    TZrSize fieldCount;
    const ZrLibMethodDescriptor *methods;
    TZrSize methodCount;
    const ZrLibMetaMethodDescriptor *metaMethods;
    TZrSize metaMethodCount;
    const TZrChar *documentation;
    const TZrChar *extendsTypeName;
    const TZrChar *const *implementsTypeNames;
    TZrSize implementsTypeCount;
    const ZrLibEnumMemberDescriptor *enumMembers;
    TZrSize enumMemberCount;
    const TZrChar *enumValueTypeName;
    TZrBool allowValueConstruction;
    TZrBool allowBoxedConstruction;
    const TZrChar *constructorSignature;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt64 protocolMask;
    const TZrChar *ffiLoweringKind;
    const TZrChar *ffiViewTypeName;
    const TZrChar *ffiUnderlyingTypeName;
    const TZrChar *ffiOwnerMode;
    const TZrChar *ffiReleaseHook;
} ZrLibTypeDescriptor;

#define ZR_LIB_FIELD_DESCRIPTOR_INIT(NAME, TYPE_NAME, DOCUMENTATION)                                                  \
    {(NAME), (TYPE_NAME), (DOCUMENTATION), 0U}

#define ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(NAME, TYPE_NAME, DOCUMENTATION, CONTRACT_ROLE)                              \
    {(NAME), (TYPE_NAME), (DOCUMENTATION), (CONTRACT_ROLE)}

#define ZR_LIB_METHOD_DESCRIPTOR_INIT(NAME, MIN_ARGUMENT_COUNT, MAX_ARGUMENT_COUNT, CALLBACK, RETURN_TYPE_NAME,       \
                                      DOCUMENTATION, IS_STATIC, PARAMETERS, PARAMETER_COUNT)                          \
    {(NAME),                                                                                                          \
     (MIN_ARGUMENT_COUNT),                                                                                            \
     (MAX_ARGUMENT_COUNT),                                                                                            \
     (CALLBACK),                                                                                                      \
     (RETURN_TYPE_NAME),                                                                                              \
     (DOCUMENTATION),                                                                                                 \
     (IS_STATIC),                                                                                                     \
     (PARAMETERS),                                                                                                    \
     (PARAMETER_COUNT),                                                                                               \
     0U}

#define ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT(NAME, MIN_ARGUMENT_COUNT, MAX_ARGUMENT_COUNT, CALLBACK, RETURN_TYPE_NAME,  \
                                           DOCUMENTATION, IS_STATIC, PARAMETERS, PARAMETER_COUNT, CONTRACT_ROLE)      \
    {(NAME),                                                                                                          \
     (MIN_ARGUMENT_COUNT),                                                                                            \
     (MAX_ARGUMENT_COUNT),                                                                                            \
     (CALLBACK),                                                                                                      \
     (RETURN_TYPE_NAME),                                                                                              \
     (DOCUMENTATION),                                                                                                 \
     (IS_STATIC),                                                                                                     \
     (PARAMETERS),                                                                                                    \
     (PARAMETER_COUNT),                                                                                               \
     (CONTRACT_ROLE)}

#define ZR_LIB_TYPE_DESCRIPTOR_INIT(NAME, PROTOTYPE_TYPE, FIELDS, FIELD_COUNT, METHODS, METHOD_COUNT, META_METHODS,  \
                                    META_METHOD_COUNT, DOCUMENTATION, EXTENDS_TYPE_NAME, IMPLEMENTS_TYPE_NAMES,       \
                                    IMPLEMENTS_TYPE_COUNT, ENUM_MEMBERS, ENUM_MEMBER_COUNT, ENUM_VALUE_TYPE_NAME,    \
                                    ALLOW_VALUE_CONSTRUCTION, ALLOW_BOXED_CONSTRUCTION, CONSTRUCTOR_SIGNATURE,        \
                                    GENERIC_PARAMETERS, GENERIC_PARAMETER_COUNT)                                      \
    {(NAME),                                                                                                          \
     (PROTOTYPE_TYPE),                                                                                                \
     (FIELDS),                                                                                                        \
     (FIELD_COUNT),                                                                                                   \
     (METHODS),                                                                                                       \
     (METHOD_COUNT),                                                                                                  \
     (META_METHODS),                                                                                                  \
     (META_METHOD_COUNT),                                                                                             \
     (DOCUMENTATION),                                                                                                 \
     (EXTENDS_TYPE_NAME),                                                                                             \
     (IMPLEMENTS_TYPE_NAMES),                                                                                         \
     (IMPLEMENTS_TYPE_COUNT),                                                                                         \
     (ENUM_MEMBERS),                                                                                                  \
     (ENUM_MEMBER_COUNT),                                                                                             \
     (ENUM_VALUE_TYPE_NAME),                                                                                          \
     (ALLOW_VALUE_CONSTRUCTION),                                                                                      \
     (ALLOW_BOXED_CONSTRUCTION),                                                                                      \
     (CONSTRUCTOR_SIGNATURE),                                                                                         \
     (GENERIC_PARAMETERS),                                                                                            \
     (GENERIC_PARAMETER_COUNT),                                                                                       \
     (TZrUInt64)0,                                                                                                    \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL}

#define ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT(NAME, PROTOTYPE_TYPE, FIELDS, FIELD_COUNT, METHODS, METHOD_COUNT,       \
                                             META_METHODS, META_METHOD_COUNT, DOCUMENTATION, EXTENDS_TYPE_NAME,       \
                                             IMPLEMENTS_TYPE_NAMES, IMPLEMENTS_TYPE_COUNT, ENUM_MEMBERS,              \
                                             ENUM_MEMBER_COUNT, ENUM_VALUE_TYPE_NAME, ALLOW_VALUE_CONSTRUCTION,       \
                                             ALLOW_BOXED_CONSTRUCTION, CONSTRUCTOR_SIGNATURE, GENERIC_PARAMETERS,     \
                                             GENERIC_PARAMETER_COUNT, PROTOCOL_MASK)                                  \
    {(NAME),                                                                                                          \
     (PROTOTYPE_TYPE),                                                                                                \
     (FIELDS),                                                                                                        \
     (FIELD_COUNT),                                                                                                   \
     (METHODS),                                                                                                       \
     (METHOD_COUNT),                                                                                                  \
     (META_METHODS),                                                                                                  \
     (META_METHOD_COUNT),                                                                                             \
     (DOCUMENTATION),                                                                                                 \
     (EXTENDS_TYPE_NAME),                                                                                             \
     (IMPLEMENTS_TYPE_NAMES),                                                                                         \
     (IMPLEMENTS_TYPE_COUNT),                                                                                         \
     (ENUM_MEMBERS),                                                                                                  \
     (ENUM_MEMBER_COUNT),                                                                                             \
     (ENUM_VALUE_TYPE_NAME),                                                                                          \
     (ALLOW_VALUE_CONSTRUCTION),                                                                                      \
     (ALLOW_BOXED_CONSTRUCTION),                                                                                      \
     (CONSTRUCTOR_SIGNATURE),                                                                                         \
     (GENERIC_PARAMETERS),                                                                                            \
     (GENERIC_PARAMETER_COUNT),                                                                                       \
     (PROTOCOL_MASK),                                                                                                 \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL}

#define ZR_LIB_TYPE_DESCRIPTOR_FFI_INIT(NAME, PROTOTYPE_TYPE, FIELDS, FIELD_COUNT, METHODS, METHOD_COUNT,            \
                                        META_METHODS, META_METHOD_COUNT, DOCUMENTATION, EXTENDS_TYPE_NAME,           \
                                        IMPLEMENTS_TYPE_NAMES, IMPLEMENTS_TYPE_COUNT, ENUM_MEMBERS,                  \
                                        ENUM_MEMBER_COUNT, ENUM_VALUE_TYPE_NAME, ALLOW_VALUE_CONSTRUCTION,           \
                                        ALLOW_BOXED_CONSTRUCTION, CONSTRUCTOR_SIGNATURE, GENERIC_PARAMETERS,         \
                                        GENERIC_PARAMETER_COUNT, FFI_LOWERING_KIND, FFI_VIEW_TYPE_NAME,             \
                                        FFI_UNDERLYING_TYPE_NAME, FFI_OWNER_MODE, FFI_RELEASE_HOOK)                 \
    {(NAME),                                                                                                          \
     (PROTOTYPE_TYPE),                                                                                                \
     (FIELDS),                                                                                                        \
     (FIELD_COUNT),                                                                                                   \
     (METHODS),                                                                                                       \
     (METHOD_COUNT),                                                                                                  \
     (META_METHODS),                                                                                                  \
     (META_METHOD_COUNT),                                                                                             \
     (DOCUMENTATION),                                                                                                 \
     (EXTENDS_TYPE_NAME),                                                                                             \
     (IMPLEMENTS_TYPE_NAMES),                                                                                         \
     (IMPLEMENTS_TYPE_COUNT),                                                                                         \
     (ENUM_MEMBERS),                                                                                                  \
     (ENUM_MEMBER_COUNT),                                                                                             \
     (ENUM_VALUE_TYPE_NAME),                                                                                          \
     (ALLOW_VALUE_CONSTRUCTION),                                                                                      \
     (ALLOW_BOXED_CONSTRUCTION),                                                                                      \
     (CONSTRUCTOR_SIGNATURE),                                                                                         \
     (GENERIC_PARAMETERS),                                                                                            \
     (GENERIC_PARAMETER_COUNT),                                                                                       \
     (TZrUInt64)0,                                                                                                    \
     (FFI_LOWERING_KIND),                                                                                             \
     (FFI_VIEW_TYPE_NAME),                                                                                            \
     (FFI_UNDERLYING_TYPE_NAME),                                                                                      \
     (FFI_OWNER_MODE),                                                                                                \
     (FFI_RELEASE_HOOK)}

typedef struct ZrLibModuleDescriptor {
    TZrUInt32 abiVersion;
    const TZrChar *moduleName;
    const ZrLibConstantDescriptor *constants;
    TZrSize constantCount;
    const ZrLibFunctionDescriptor *functions;
    TZrSize functionCount;
    const ZrLibTypeDescriptor *types;
    TZrSize typeCount;
    const ZrLibTypeHintDescriptor *typeHints;
    TZrSize typeHintCount;
    const TZrChar *typeHintsJson;
    const TZrChar *documentation;
    const ZrLibModuleLinkDescriptor *moduleLinks;
    TZrSize moduleLinkCount;
    const TZrChar *moduleVersion;
    TZrUInt32 minRuntimeAbi;
    TZrUInt64 requiredCapabilities;
    FZrLibModuleMaterializeCallback onMaterialize;
} ZrLibModuleDescriptor;

ZR_LIBRARY_API TZrSize ZrLib_CallContext_ArgumentCount(const ZrLibCallContext *context);
ZR_LIBRARY_API SZrTypeValue *ZrLib_CallContext_Self(const ZrLibCallContext *context);
ZR_LIBRARY_API SZrTypeValue *ZrLib_CallContext_Argument(const ZrLibCallContext *context, TZrSize index);
ZR_LIBRARY_API struct SZrObjectPrototype *ZrLib_CallContext_OwnerPrototype(const ZrLibCallContext *context);
ZR_LIBRARY_API struct SZrObjectPrototype *ZrLib_CallContext_GetConstructTargetPrototype(const ZrLibCallContext *context);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_CheckArity(const ZrLibCallContext *context,
                                                    TZrSize minArgumentCount,
                                                    TZrSize maxArgumentCount);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadInt(const ZrLibCallContext *context,
                                                 TZrSize index,
                                                 TZrInt64 *outValue);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadFloat(const ZrLibCallContext *context,
                                                   TZrSize index,
                                                   TZrFloat64 *outValue);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadBool(const ZrLibCallContext *context,
                                                  TZrSize index,
                                                  TZrBool *outValue);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadString(const ZrLibCallContext *context,
                                                    TZrSize index,
                                                    SZrString **outValue);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadObject(const ZrLibCallContext *context,
                                                    TZrSize index,
                                                    SZrObject **outValue);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadArray(const ZrLibCallContext *context,
                                                   TZrSize index,
                                                   SZrObject **outValue);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadFunction(const ZrLibCallContext *context,
                                                      TZrSize index,
                                                      SZrTypeValue **outValue);
ZR_LIBRARY_API ZR_NO_RETURN void ZrLib_CallContext_RaiseTypeError(const ZrLibCallContext *context,
                                                                  TZrSize index,
                                                                  const TZrChar *expectedType);
ZR_LIBRARY_API ZR_NO_RETURN void ZrLib_CallContext_RaiseArityError(const ZrLibCallContext *context,
                                                                   TZrSize minArgumentCount,
                                                                   TZrSize maxArgumentCount);

ZR_LIBRARY_API TZrBool ZrLib_TempValueRoot_Begin(SZrState *state, ZrLibTempValueRoot *root);
ZR_LIBRARY_API TZrBool ZrLib_CallContext_BeginTempValueRoot(const ZrLibCallContext *context,
                                                            ZrLibTempValueRoot *root);
ZR_LIBRARY_API SZrTypeValue *ZrLib_TempValueRoot_Value(ZrLibTempValueRoot *root);
ZR_LIBRARY_API TZrBool ZrLib_TempValueRoot_SetValue(ZrLibTempValueRoot *root, const SZrTypeValue *value);
ZR_LIBRARY_API TZrBool ZrLib_TempValueRoot_SetObject(ZrLibTempValueRoot *root,
                                                     SZrObject *object,
                                                     EZrValueType type);
ZR_LIBRARY_API void ZrLib_TempValueRoot_SetNull(ZrLibTempValueRoot *root);
ZR_LIBRARY_API void ZrLib_TempValueRoot_End(ZrLibTempValueRoot *root);

ZR_LIBRARY_API void ZrLib_Value_SetNull(SZrTypeValue *value);
ZR_LIBRARY_API void ZrLib_Value_SetBool(SZrState *state, SZrTypeValue *value, TZrBool boolValue);
ZR_LIBRARY_API void ZrLib_Value_SetInt(SZrState *state, SZrTypeValue *value, TZrInt64 intValue);
ZR_LIBRARY_API void ZrLib_Value_SetFloat(SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue);
ZR_LIBRARY_API void ZrLib_Value_SetString(SZrState *state, SZrTypeValue *value, const TZrChar *stringValue);
ZR_LIBRARY_API void ZrLib_Value_SetStringObject(SZrState *state, SZrTypeValue *value, SZrString *stringObject);
ZR_LIBRARY_API void ZrLib_Value_SetObject(SZrState *state, SZrTypeValue *value, SZrObject *object, EZrValueType type);
ZR_LIBRARY_API void ZrLib_Value_SetNativePointer(SZrState *state, SZrTypeValue *value, TZrPtr pointerValue);

ZR_LIBRARY_API SZrObject *ZrLib_Object_New(SZrState *state);
ZR_LIBRARY_API SZrObject *ZrLib_Array_New(SZrState *state);
ZR_LIBRARY_API void ZrLib_Object_SetFieldCString(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 const SZrTypeValue *value);
ZR_LIBRARY_API const SZrTypeValue *ZrLib_Object_GetFieldCString(SZrState *state,
                                                                SZrObject *object,
                                                                const TZrChar *fieldName);
ZR_LIBRARY_API TZrBool ZrLib_Array_PushValue(SZrState *state, SZrObject *array, const SZrTypeValue *value);
ZR_LIBRARY_API TZrSize ZrLib_Array_Length(SZrObject *array);
ZR_LIBRARY_API const SZrTypeValue *ZrLib_Array_Get(SZrState *state, SZrObject *array, TZrSize index);

ZR_LIBRARY_API SZrObject *ZrLib_Type_NewInstance(SZrState *state, const TZrChar *typeName);
ZR_LIBRARY_API SZrObject *ZrLib_Type_NewInstanceWithPrototype(SZrState *state, SZrObjectPrototype *prototype);
ZR_LIBRARY_API SZrObjectPrototype *ZrLib_Type_FindPrototype(SZrState *state, const TZrChar *typeName);

ZR_LIBRARY_API SZrObjectModule *ZrLib_Module_GetLoaded(SZrState *state, const TZrChar *moduleName);
ZR_LIBRARY_API const SZrTypeValue *ZrLib_Module_GetExport(SZrState *state,
                                                          const TZrChar *moduleName,
                                                          const TZrChar *exportName);

ZR_LIBRARY_API TZrBool ZrLib_CallValue(SZrState *state,
                                       const SZrTypeValue *callable,
                                       const SZrTypeValue *receiver,
                                       const SZrTypeValue *arguments,
                                       TZrSize argumentCount,
                                       SZrTypeValue *result);
ZR_LIBRARY_API TZrBool ZrLib_CallModuleExport(SZrState *state,
                                              const TZrChar *moduleName,
                                              const TZrChar *exportName,
                                              const SZrTypeValue *arguments,
                                              TZrSize argumentCount,
                                              SZrTypeValue *result);

#endif // ZR_VM_LIBRARY_NATIVE_BINDING_H
