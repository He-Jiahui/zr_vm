#ifndef ZR_VM_RUST_BINDING_H
#define ZR_VM_RUST_BINDING_H

#include "zr_vm_rust_binding/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EZrRustBindingStatus {
    ZR_RUST_BINDING_STATUS_OK = 0,
    ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT = 1,
    ZR_RUST_BINDING_STATUS_IO_ERROR = 2,
    ZR_RUST_BINDING_STATUS_NOT_FOUND = 3,
    ZR_RUST_BINDING_STATUS_ALREADY_EXISTS = 4,
    ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL = 5,
    ZR_RUST_BINDING_STATUS_COMPILE_ERROR = 6,
    ZR_RUST_BINDING_STATUS_RUNTIME_ERROR = 7,
    ZR_RUST_BINDING_STATUS_UNSUPPORTED = 8,
    ZR_RUST_BINDING_STATUS_INTERNAL_ERROR = 9
} ZrRustBindingStatus;

typedef enum EZrRustBindingExecutionMode {
    ZR_RUST_BINDING_EXECUTION_MODE_INTERP = 0,
    ZR_RUST_BINDING_EXECUTION_MODE_BINARY = 1
} ZrRustBindingExecutionMode;

typedef enum EZrRustBindingValueKind {
    ZR_RUST_BINDING_VALUE_KIND_NULL = 0,
    ZR_RUST_BINDING_VALUE_KIND_BOOL = 1,
    ZR_RUST_BINDING_VALUE_KIND_INT = 2,
    ZR_RUST_BINDING_VALUE_KIND_FLOAT = 3,
    ZR_RUST_BINDING_VALUE_KIND_STRING = 4,
    ZR_RUST_BINDING_VALUE_KIND_ARRAY = 5,
    ZR_RUST_BINDING_VALUE_KIND_OBJECT = 6,
    ZR_RUST_BINDING_VALUE_KIND_FUNCTION = 7,
    ZR_RUST_BINDING_VALUE_KIND_NATIVE_POINTER = 8,
    ZR_RUST_BINDING_VALUE_KIND_UNKNOWN = 255
} ZrRustBindingValueKind;

typedef enum EZrRustBindingOwnershipKind {
    ZR_RUST_BINDING_OWNERSHIP_KIND_NONE = 0,
    ZR_RUST_BINDING_OWNERSHIP_KIND_UNIQUE = 1,
    ZR_RUST_BINDING_OWNERSHIP_KIND_SHARED = 2,
    ZR_RUST_BINDING_OWNERSHIP_KIND_WEAK = 3,
    ZR_RUST_BINDING_OWNERSHIP_KIND_BORROWED = 4,
    ZR_RUST_BINDING_OWNERSHIP_KIND_LOANED = 5
} ZrRustBindingOwnershipKind;

typedef struct ZrRustBindingErrorInfo {
    ZrRustBindingStatus status;
    TZrChar message[ZR_RUST_BINDING_ERROR_MESSAGE_CAPACITY];
} ZrRustBindingErrorInfo;

typedef struct ZrRustBindingRuntimeOptions {
    TZrUInt64 heapLimitBytes;
    TZrUInt64 pauseBudgetUs;
    TZrUInt64 remarkBudgetUs;
    TZrUInt32 workerCount;
} ZrRustBindingRuntimeOptions;

typedef struct ZrRustBindingScaffoldOptions {
    const TZrChar *rootPath;
    const TZrChar *projectName;
    TZrBool overwriteExisting;
} ZrRustBindingScaffoldOptions;

typedef struct ZrRustBindingCompileOptions {
    TZrBool emitIntermediate;
    TZrBool incremental;
} ZrRustBindingCompileOptions;

typedef struct ZrRustBindingRunOptions {
    ZrRustBindingExecutionMode executionMode;
    const TZrChar *moduleName;
    const TZrChar *const *programArgs;
    TZrSize programArgCount;
} ZrRustBindingRunOptions;

typedef enum EZrRustBindingNativeConstantKind {
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_NULL = 0,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_BOOL = 1,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT = 2,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_FLOAT = 3,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_STRING = 4,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_ARRAY = 5
} ZrRustBindingNativeConstantKind;

typedef enum EZrRustBindingPrototypeType {
    ZR_RUST_BINDING_PROTOTYPE_TYPE_INVALID = 0,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_MODULE = 1,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_CLASS = 2,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_INTERFACE = 3,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_STRUCT = 4,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_ENUM = 5,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_NATIVE = 6
} ZrRustBindingPrototypeType;

typedef enum EZrRustBindingMetaMethodType {
    ZR_RUST_BINDING_META_METHOD_CONSTRUCTOR = 0,
    ZR_RUST_BINDING_META_METHOD_DESTRUCTOR = 1,
    ZR_RUST_BINDING_META_METHOD_ADD = 2,
    ZR_RUST_BINDING_META_METHOD_SUB = 3,
    ZR_RUST_BINDING_META_METHOD_MUL = 4,
    ZR_RUST_BINDING_META_METHOD_DIV = 5,
    ZR_RUST_BINDING_META_METHOD_MOD = 6,
    ZR_RUST_BINDING_META_METHOD_POW = 7,
    ZR_RUST_BINDING_META_METHOD_NEG = 8,
    ZR_RUST_BINDING_META_METHOD_COMPARE = 9,
    ZR_RUST_BINDING_META_METHOD_TO_BOOL = 10,
    ZR_RUST_BINDING_META_METHOD_TO_STRING = 11,
    ZR_RUST_BINDING_META_METHOD_TO_INT = 12,
    ZR_RUST_BINDING_META_METHOD_TO_UINT = 13,
    ZR_RUST_BINDING_META_METHOD_TO_FLOAT = 14,
    ZR_RUST_BINDING_META_METHOD_CALL = 15,
    ZR_RUST_BINDING_META_METHOD_GETTER = 16,
    ZR_RUST_BINDING_META_METHOD_SETTER = 17,
    ZR_RUST_BINDING_META_METHOD_SHIFT_LEFT = 18,
    ZR_RUST_BINDING_META_METHOD_SHIFT_RIGHT = 19,
    ZR_RUST_BINDING_META_METHOD_BIT_AND = 20,
    ZR_RUST_BINDING_META_METHOD_BIT_OR = 21,
    ZR_RUST_BINDING_META_METHOD_BIT_XOR = 22,
    ZR_RUST_BINDING_META_METHOD_BIT_NOT = 23,
    ZR_RUST_BINDING_META_METHOD_GET_ITEM = 24,
    ZR_RUST_BINDING_META_METHOD_SET_ITEM = 25,
    ZR_RUST_BINDING_META_METHOD_CLOSE = 26,
    ZR_RUST_BINDING_META_METHOD_DECORATE = 27
} ZrRustBindingMetaMethodType;

typedef struct ZrRustBindingNativeTypeHintDescriptor {
    const TZrChar *symbolName;
    const TZrChar *symbolKind;
    const TZrChar *signature;
    const TZrChar *documentation;
} ZrRustBindingNativeTypeHintDescriptor;

typedef struct ZrRustBindingNativeFieldDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
    TZrUInt32 contractRole;
} ZrRustBindingNativeFieldDescriptor;

typedef struct ZrRustBindingNativeParameterDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
} ZrRustBindingNativeParameterDescriptor;

typedef struct ZrRustBindingNativeGenericParameterDescriptor {
    const TZrChar *name;
    const TZrChar *documentation;
    const TZrChar *const *constraintTypeNames;
    TZrSize constraintTypeCount;
} ZrRustBindingNativeGenericParameterDescriptor;

typedef struct ZrRustBindingNativeEnumMemberDescriptor {
    const TZrChar *name;
    ZrRustBindingNativeConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
} ZrRustBindingNativeEnumMemberDescriptor;

typedef struct ZrRustBindingNativeConstantDescriptor {
    const TZrChar *name;
    ZrRustBindingNativeConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
    const TZrChar *typeName;
} ZrRustBindingNativeConstantDescriptor;

typedef struct ZrRustBindingNativeModuleLinkDescriptor {
    const TZrChar *name;
    const TZrChar *moduleName;
    const TZrChar *documentation;
} ZrRustBindingNativeModuleLinkDescriptor;

typedef struct ZrRustBindingRuntime ZrRustBindingRuntime;
typedef struct ZrRustBindingProjectWorkspace ZrRustBindingProjectWorkspace;
typedef struct ZrRustBindingCompileResult ZrRustBindingCompileResult;
typedef struct ZrRustBindingManifestSnapshot ZrRustBindingManifestSnapshot;
typedef struct ZrRustBindingNativeCallContext ZrRustBindingNativeCallContext;
typedef struct ZrRustBindingNativeModuleBuilder ZrRustBindingNativeModuleBuilder;
typedef struct ZrRustBindingNativeModule ZrRustBindingNativeModule;
typedef struct ZrRustBindingRuntimeNativeModuleRegistration ZrRustBindingRuntimeNativeModuleRegistration;
typedef struct ZrRustBindingValue ZrRustBindingValue;

typedef ZrRustBindingStatus (*FZrRustBindingNativeCallback)(ZrRustBindingNativeCallContext *context,
                                                            TZrPtr userData,
                                                            ZrRustBindingValue **outResult);
typedef void (*FZrRustBindingDestroyCallback)(TZrPtr userData);

typedef struct ZrRustBindingNativeFunctionDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrRustBindingNativeParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 contractRole;
} ZrRustBindingNativeFunctionDescriptor;

typedef struct ZrRustBindingNativeMethodDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    TZrBool isStatic;
    const ZrRustBindingNativeParameterDescriptor *parameters;
    TZrSize parameterCount;
    TZrUInt32 contractRole;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} ZrRustBindingNativeMethodDescriptor;

typedef struct ZrRustBindingNativeMetaMethodDescriptor {
    ZrRustBindingMetaMethodType metaType;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrRustBindingNativeCallback callback;
    TZrPtr userData;
    FZrRustBindingDestroyCallback destroyUserData;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrRustBindingNativeParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
} ZrRustBindingNativeMetaMethodDescriptor;

typedef struct ZrRustBindingNativeTypeDescriptor {
    const TZrChar *name;
    ZrRustBindingPrototypeType prototypeType;
    const ZrRustBindingNativeFieldDescriptor *fields;
    TZrSize fieldCount;
    const ZrRustBindingNativeMethodDescriptor *methods;
    TZrSize methodCount;
    const ZrRustBindingNativeMetaMethodDescriptor *metaMethods;
    TZrSize metaMethodCount;
    const TZrChar *documentation;
    const TZrChar *extendsTypeName;
    const TZrChar *const *implementsTypeNames;
    TZrSize implementsTypeCount;
    const ZrRustBindingNativeEnumMemberDescriptor *enumMembers;
    TZrSize enumMemberCount;
    const TZrChar *enumValueTypeName;
    TZrBool allowValueConstruction;
    TZrBool allowBoxedConstruction;
    const TZrChar *constructorSignature;
    const ZrRustBindingNativeGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt64 protocolMask;
    const TZrChar *ffiLoweringKind;
    const TZrChar *ffiViewTypeName;
    const TZrChar *ffiUnderlyingTypeName;
    const TZrChar *ffiOwnerMode;
    const TZrChar *ffiReleaseHook;
} ZrRustBindingNativeTypeDescriptor;

ZR_RUST_BINDING_API void ZrRustBinding_GetLastErrorInfo(ZrRustBindingErrorInfo *outErrorInfo);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_NewBare(const ZrRustBindingRuntimeOptions *options,
                                                                      ZrRustBindingRuntime **outRuntime);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_NewStandard(const ZrRustBindingRuntimeOptions *options,
                                                                          ZrRustBindingRuntime **outRuntime);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_Free(ZrRustBindingRuntime *runtime);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Scaffold(
        const ZrRustBindingScaffoldOptions *options,
        ZrRustBindingProjectWorkspace **outWorkspace);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Open(const TZrChar *projectPath,
                                                                   ZrRustBindingProjectWorkspace **outWorkspace);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_Free(ZrRustBindingProjectWorkspace *workspace);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetProjectPath(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetProjectRoot(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetManifestPath(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetEntryModule(
        const ZrRustBindingProjectWorkspace *workspace,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_ResolveArtifacts(
        const ZrRustBindingProjectWorkspace *workspace,
        const TZrChar *moduleName,
        TZrChar *zroBuffer,
        TZrSize zroBufferSize,
        TZrChar *zriBuffer,
        TZrSize zriBufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_LoadManifest(
        const ZrRustBindingProjectWorkspace *workspace,
        ZrRustBindingManifestSnapshot **outManifestSnapshot);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetVersion(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrUInt32 *outVersion);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryCount(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize *outEntryCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_FindEntry(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        const TZrChar *moduleName,
        TZrSize *outEntryIndex);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryModuleName(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntrySourceHash(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZroHash(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZroPath(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZriPath(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryImportCount(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrSize *outImportCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryImport(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrSize importIndex,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_Free(
        ZrRustBindingManifestSnapshot *manifestSnapshot);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Compile(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingCompileOptions *options,
        ZrRustBindingCompileResult **outCompileResult);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_CompileResult_GetCounts(
        const ZrRustBindingCompileResult *compileResult,
        TZrSize *outCompiledCount,
        TZrSize *outSkippedCount,
        TZrSize *outRemovedCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_CompileResult_Free(ZrRustBindingCompileResult *compileResult);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_Run(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingRunOptions *options,
        ZrRustBindingValue **outResult);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Project_CallModuleExport(
        ZrRustBindingRuntime *runtime,
        const ZrRustBindingProjectWorkspace *workspace,
        const ZrRustBindingRunOptions *options,
        const TZrChar *moduleName,
        const TZrChar *exportName,
        ZrRustBindingValue *const *arguments,
        TZrSize argumentCount,
        ZrRustBindingValue **outResult);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_New(
        const TZrChar *moduleName,
        ZrRustBindingNativeModuleBuilder **outBuilder);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetDocumentation(
        ZrRustBindingNativeModuleBuilder *builder,
        const TZrChar *documentation);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetModuleVersion(
        ZrRustBindingNativeModuleBuilder *builder,
        const TZrChar *moduleVersion);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetTypeHintsJson(
        ZrRustBindingNativeModuleBuilder *builder,
        const TZrChar *typeHintsJson);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_SetRuntimeRequirements(
        ZrRustBindingNativeModuleBuilder *builder,
        TZrUInt32 minRuntimeAbi,
        TZrUInt64 requiredCapabilities);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddTypeHint(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeTypeHintDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddModuleLink(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeModuleLinkDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddConstant(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeConstantDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddFunction(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeFunctionDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_AddType(
        ZrRustBindingNativeModuleBuilder *builder,
        const ZrRustBindingNativeTypeDescriptor *descriptor);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_Build(
        ZrRustBindingNativeModuleBuilder *builder,
        ZrRustBindingNativeModule **outModule);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModuleBuilder_Free(
        ZrRustBindingNativeModuleBuilder *builder);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeModule_Free(
        ZrRustBindingNativeModule *module);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Runtime_RegisterNativeModule(
        ZrRustBindingRuntime *runtime,
        ZrRustBindingNativeModule *module,
        ZrRustBindingRuntimeNativeModuleRegistration **outRegistration);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_RuntimeNativeModuleRegistration_Free(
        ZrRustBindingRuntimeNativeModuleRegistration *registration);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetModuleName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetTypeName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetCallableName(
        const ZrRustBindingNativeCallContext *context,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetArgumentCount(
        const ZrRustBindingNativeCallContext *context,
        TZrSize *outArgumentCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_CheckArity(
        const ZrRustBindingNativeCallContext *context,
        TZrSize minArgumentCount,
        TZrSize maxArgumentCount);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetArgument(
        const ZrRustBindingNativeCallContext *context,
        TZrSize index,
        ZrRustBindingValue **outArgumentValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_NativeCallContext_GetSelf(
        const ZrRustBindingNativeCallContext *context,
        ZrRustBindingValue **outSelfValue);

ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewNull(ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewBool(TZrBool boolValue, ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewInt(TZrInt64 intValue, ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewFloat(TZrFloat64 floatValue,
                                                                     ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewString(const TZrChar *stringValue,
                                                                      ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewArray(ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_NewObject(ZrRustBindingValue **outValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Free(ZrRustBindingValue *value);

ZR_RUST_BINDING_API ZrRustBindingValueKind ZrRustBinding_Value_GetKind(const ZrRustBindingValue *value);
ZR_RUST_BINDING_API ZrRustBindingOwnershipKind ZrRustBinding_Value_GetOwnershipKind(
        const ZrRustBindingValue *value);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadBool(const ZrRustBindingValue *value,
                                                                     TZrBool *outBoolValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadInt(const ZrRustBindingValue *value,
                                                                    TZrInt64 *outIntValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadFloat(const ZrRustBindingValue *value,
                                                                      TZrFloat64 *outFloatValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_ReadString(const ZrRustBindingValue *value,
                                                                       TZrChar *buffer,
                                                                       TZrSize bufferSize);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Array_Length(const ZrRustBindingValue *value,
                                                                         TZrSize *outLength);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Array_Get(const ZrRustBindingValue *value,
                                                                      TZrSize index,
                                                                      ZrRustBindingValue **outElement);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Array_Push(ZrRustBindingValue *value,
                                                                       const ZrRustBindingValue *element);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Object_Get(const ZrRustBindingValue *value,
                                                                       const TZrChar *fieldName,
                                                                       ZrRustBindingValue **outFieldValue);
ZR_RUST_BINDING_API ZrRustBindingStatus ZrRustBinding_Value_Object_Set(ZrRustBindingValue *value,
                                                                       const TZrChar *fieldName,
                                                                       const ZrRustBindingValue *fieldValue);

#ifdef __cplusplus
}
#endif

#endif
