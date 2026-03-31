//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_VM_CORE_IO_H
#define ZR_VM_CORE_IO_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrGlobalState;
struct SZrString;
struct SZrFunction;

typedef TZrBytePtr (*FZrIoRead)(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size);

typedef enum EZrIoNativeHelperId {
    ZR_IO_NATIVE_HELPER_NONE = 0,
    ZR_IO_NATIVE_HELPER_MODULE_IMPORT = 1,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_UNIQUE = 2,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED = 3,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_WEAK = 4,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_USING = 5
} EZrIoNativeHelperId;

typedef EZrThreadStatus (*FZrIoWrite)(struct SZrState *state, TZrBytePtr buffer, TZrSize size, TZrPtr customData);

typedef void (*FZrIoClose)(struct SZrState *state, TZrPtr customData);

struct ZR_STRUCT_ALIGN SZrIo {
    struct SZrState *state;
    FZrIoRead read;
    TZrSize remained;
    TZrBytePtr pointer;
    TZrPtr customData;
    FZrIoClose close;
    TZrBool isBinary;
};

typedef struct SZrIo SZrIo;

typedef TZrBool (*FZrIoLoadSource)(struct SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io);


struct SZrIoImport {
    struct SZrString *name;
    struct SZrString *md5;
};

typedef struct SZrIoImport SZrIoImport;


struct SZrIoReference {
    struct SZrString *referenceModuleName;
    struct SZrString *referenceModuleMd5;
    TZrSize referenceIndex;
};

typedef struct SZrIoReference SZrIoReference;

// 前向声明，用于SZrIoClass和SZrIoStruct
struct SZrIoFunction;

struct SZrIoFunctionLocalVariable {
    TZrUInt64 instructionStartIndex;
    TZrUInt64 instructionEndIndex;
    TZrUInt64 startLine; // debug
    TZrUInt64 endLine; // debug
};

typedef struct SZrIoFunctionLocalVariable SZrIoFunctionLocalVariable;

struct SZrIoFunctionConstantVariable {
    EZrValueType type;
    TZrPureValue value;
    TZrBool hasFunctionValue;
    struct SZrIoFunction *functionValue;
    TZrUInt64 startLine; // debug
    TZrUInt64 endLine; // debug
};

typedef struct SZrIoFunctionConstantVariable SZrIoFunctionConstantVariable;

struct SZrIoFunctionExportedVariable {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
};

typedef struct SZrIoFunctionExportedVariable SZrIoFunctionExportedVariable;

typedef struct SZrIoFunctionTypedTypeRef {
    EZrValueType baseType;
    TZrBool isNullable;
    TZrUInt32 ownershipQualifier;
    TZrBool isArray;
    struct SZrString *typeName;
    EZrValueType elementBaseType;
    struct SZrString *elementTypeName;
} SZrIoFunctionTypedTypeRef;

typedef struct SZrIoFunctionTypedLocalBinding {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    SZrIoFunctionTypedTypeRef type;
} SZrIoFunctionTypedLocalBinding;

typedef struct SZrIoFunctionTypedExportSymbol {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
    TZrUInt8 symbolKind;
    SZrIoFunctionTypedTypeRef valueType;
    TZrSize parameterCount;
    SZrIoFunctionTypedTypeRef *parameterTypes;
} SZrIoFunctionTypedExportSymbol;

struct SZrIoFunction;

struct SZrIoFunctionClosure {
    struct SZrIoFunction *subFunction;
    // todo:
};

typedef struct SZrIoFunctionClosure SZrIoFunctionClosure;

struct SZrIoFunctionDebugInfo {
    TZrSize instructionsLength;
    TZrUInt64 *instructionsLine;
    // todo:
};

typedef struct SZrIoFunctionDebugInfo SZrIoFunctionDebugInfo;

// 前向声明，用于SZrIoClass和SZrIoStruct
struct SZrIoMemberDeclare;
typedef struct SZrIoMemberDeclare SZrIoMemberDeclare;

struct SZrIoClass {
    struct SZrString *name;
    TZrSize superClassLength;
    SZrIoReference *superClasses;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    struct SZrIoMemberDeclare *declares;
};

typedef struct SZrIoClass SZrIoClass;

struct SZrIoStruct {
    struct SZrString *name;
    TZrSize superStructLength;
    SZrIoReference *superStructs;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    struct SZrIoMemberDeclare *declares;
};

typedef struct SZrIoStruct SZrIoStruct;

struct SZrIoFunction {
    struct SZrString *name;
    TZrUInt64 startLine;
    TZrUInt64 endLine;
    TZrSize parametersLength;
    TZrUInt64 hasVarArgs;
    TZrUInt32 stackSize;
    TZrSize instructionsLength;
    TZrInstruction *instructions;
    TZrSize localVariablesLength;
    SZrIoFunctionLocalVariable *localVariables;
    TZrSize constantVariablesLength;
    SZrIoFunctionConstantVariable *constantVariables;
    TZrSize exportedVariablesLength;
    SZrIoFunctionExportedVariable *exportedVariables;
    TZrSize typedLocalBindingsLength;
    SZrIoFunctionTypedLocalBinding *typedLocalBindings;
    TZrSize typedExportedSymbolsLength;
    SZrIoFunctionTypedExportSymbol *typedExportedSymbols;
    TZrSize prototypesLength;                // prototype 数量
    TZrSize classesLength;
    SZrIoClass *classes;                      // class prototype 数组（如果 type 是 CLASS）
    TZrSize structsLength;
    SZrIoStruct *structs;                     // struct prototype 数组（如果 type 是 STRUCT）
    TZrSize closuresLength;
    SZrIoFunctionClosure *closures;
    TZrSize debugInfosLength;
    SZrIoFunctionDebugInfo *debugInfos;
};

typedef struct SZrIoFunction SZrIoFunction;

struct SZrIoMeta {
    EZrMetaType metaType;
    TZrSize functionsLength;
    SZrIoFunction *functions;
};

typedef struct SZrIoMeta SZrIoMeta;

struct SZrIoMethod {
    struct SZrString *name;
    TZrSize functionsLength;
    SZrIoFunction *functions;
};

typedef struct SZrIoMethod SZrIoMethod;

struct SZrIoProperty {
    struct SZrString *name;
    // todo:
    TZrUInt32 propertyType;
    SZrIoFunction *getter;
    SZrIoFunction *setter;
};

typedef struct SZrIoProperty SZrIoProperty;

struct SZrIoField {
    struct SZrString *name;
};

typedef struct SZrIoField SZrIoField;

struct SZrIoEnumField {
    struct SZrString *name;
    TZrPureValue value;
};

typedef struct SZrIoEnumField SZrIoEnumField;

struct SZrIoMemberDeclare {
    EZrIoMemberDeclareType type;
    EZrIoMemberDeclareStatus status;
    union {
        SZrIoField *field;
        SZrIoMethod *method;
        SZrIoProperty *property;
        SZrIoMeta *meta;
        SZrIoEnumField *enumField;
        // todo:
    };
};

typedef struct SZrIoMemberDeclare SZrIoMemberDeclare;

struct SZrIoInterface {
    struct SZrString *name;
    TZrSize superInterfaceLength;
    SZrIoReference *superInterfaces;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    SZrIoMemberDeclare *declares;
};

typedef struct SZrIoInterface SZrIoInterface;

struct SZrIoEnum {
    struct SZrString *name;
    EZrValueType valueType;
    TZrSize fieldsLength;
    SZrIoEnumField *fields;
};

typedef struct SZrIoEnum SZrIoEnum;

struct SZrIoModuleDeclare {
    EZrIoModuleDeclareType type;

    union {
#ifdef __cplusplus
        SZrIoClass *class_;
#else
        SZrIoClass *class;
#endif
        SZrIoStruct *struct_;
        SZrIoInterface *interface;
        SZrIoFunction *function;
        SZrIoEnum *enum_;
        SZrIoField *field;
    };
};

typedef struct SZrIoModuleDeclare SZrIoModuleDeclare;

struct SZrIoModule {
    struct SZrString *name;
    struct SZrString *md5;
    TZrSize importsLength;
    SZrIoImport *imports;
    TZrSize declaresLength;
    SZrIoModuleDeclare *declares;
    SZrIoFunction *entryFunction;
};

typedef struct SZrIoModule SZrIoModule;

struct SZrIoSource {
    TZrChar signature[4];
    TZrUInt32 versionMajor;
    TZrUInt32 versionMinor;
    TZrUInt32 versionPatch;
    TZrUInt64 format;
    TZrUInt8 nativeIntSize;
    TZrUInt8 typeSizeSize;
    TZrUInt8 typeInstructionSize;
    TZrBool isBigEndian;
    TZrBool isDebug;
    TZrChar optional[3];
    TZrSize modulesLength;
    SZrIoModule *modules;
};

typedef struct SZrIoSource SZrIoSource;

ZR_CORE_API SZrIo *ZrCore_Io_New(struct SZrGlobalState *global);

ZR_CORE_API void ZrCore_Io_Free(struct SZrGlobalState *global, SZrIo *io);

ZR_CORE_API void ZrCore_Io_Init(struct SZrState *state, SZrIo *io, FZrIoRead read, FZrIoClose close, TZrPtr customData);

ZR_CORE_API TZrSize ZrCore_Io_Read(SZrIo *io, TZrBytePtr buffer, TZrSize size);


ZR_CORE_API SZrIoSource *ZrCore_Io_ReadSourceNew(SZrIo *io);

ZR_CORE_API void ZrCore_Io_ReadSourceFree(struct SZrGlobalState *global, SZrIoSource *source);

ZR_CORE_API SZrIoSource *ZrCore_Io_LoadSource(struct SZrState *state, TZrNativeString sourceName, TZrNativeString md5);

ZR_CORE_API struct SZrFunction *ZrCore_Io_LoadEntryFunctionToRuntime(struct SZrState *state,
                                                                     const SZrIoSource *source);
ZR_CORE_API FZrNativeFunction ZrCore_Io_GetSerializableNativeHelperFunction(TZrUInt64 helperId);
#endif // ZR_VM_CORE_IO_H
