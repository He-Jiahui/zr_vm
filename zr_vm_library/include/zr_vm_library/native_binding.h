//
// Native binding descriptors and helpers for zr_vm_library.
//

#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/function.h"

struct ZrLibModuleDescriptor;
struct ZrLibTypeDescriptor;
struct ZrLibFunctionDescriptor;
struct ZrLibMethodDescriptor;
struct ZrLibMetaMethodDescriptor;
struct ZrLibEnumMemberDescriptor;
struct SZrCallInfo;

#define ZR_VM_NATIVE_RUNTIME_ABI_VERSION 1U

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
} ZrLibFieldDescriptor;

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
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argumentBase;
    TZrSize argumentCount;
    SZrTypeValue *selfValue;
} ZrLibCallContext;

typedef struct ZrLibTempValueRoot {
    SZrState *state;
    struct SZrCallInfo *callInfo;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor slotAnchor;
    TZrBool hasSavedCallInfoTop;
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
} ZrLibFunctionDescriptor;

typedef struct ZrLibMethodDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    TZrBool isStatic;
} ZrLibMethodDescriptor;

typedef struct ZrLibMetaMethodDescriptor {
    EZrMetaType metaType;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
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
} ZrLibTypeDescriptor;

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
} ZrLibModuleDescriptor;

ZR_LIBRARY_API TZrSize ZrLib_CallContext_ArgumentCount(const ZrLibCallContext *context);
ZR_LIBRARY_API SZrTypeValue *ZrLib_CallContext_Self(const ZrLibCallContext *context);
ZR_LIBRARY_API SZrTypeValue *ZrLib_CallContext_Argument(const ZrLibCallContext *context, TZrSize index);
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
ZR_LIBRARY_API void ZrLib_CallContext_RaiseTypeError(const ZrLibCallContext *context,
                                                     TZrSize index,
                                                     const TZrChar *expectedType);
ZR_LIBRARY_API void ZrLib_CallContext_RaiseArityError(const ZrLibCallContext *context,
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
