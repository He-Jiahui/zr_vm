//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_VM_CORE_IO_H
#define ZR_VM_CORE_IO_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrGlobalState;


typedef TBytePtr (*FZrIoRead)(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size);

typedef EZrThreadStatus (*FZrIoWrite)(struct SZrState *state, TBytePtr buffer, TZrSize size, TZrPtr customData);

typedef void (*FZrIoClose)(struct SZrState *state, TZrPtr customData);

struct ZR_STRUCT_ALIGN SZrIo {
    struct SZrState *state;
    FZrIoRead read;
    TZrSize remained;
    TBytePtr pointer;
    TZrPtr customData;
    FZrIoClose close;
};

typedef struct SZrIo SZrIo;

typedef TBool (*FZrIoLoadSource)(struct SZrState *state, TNativeString sourcePath, TNativeString md5, SZrIo *io);


struct SZrIoImport {
    TZrString *name;
    TZrString *md5;
};

typedef struct SZrIoImport SZrIoImport;


struct SZrIoReference {
    TZrString *referenceModuleName;
    TZrString *referenceModuleMd5;
    TZrSize referenceIndex;
};

typedef struct SZrIoReference SZrIoReference;

struct SZrIoFunctionLocalVariable {
    TUInt64 instructionStartIndex;
    TUInt64 instructionEndIndex;
    TUInt64 startLine; // debug
    TUInt64 endLine; // debug
};

typedef struct SZrIoFunctionLocalVariable SZrIoFunctionLocalVariable;

struct SZrIoFunctionConstantVariable {
    EZrValueType type;
    TZrPureValue value;
    TUInt64 startLine; // debug
    TUInt64 endLine; // debug
};

typedef struct SZrIoFunctionConstantVariable SZrIoFunctionConstantVariable;

struct SZrIoFunction;

struct SZrIoFunctionClosure {
    struct SZrIoFunction *subFunction;
    // todo:
};

typedef struct SZrIoFunctionClosure SZrIoFunctionClosure;

struct SZrIoFunctionDebugInfo {
    TZrSize instructionsLength;
    TUInt64 *instructionsLine;
    // todo:
};

typedef struct SZrIoFunctionDebugInfo SZrIoFunctionDebugInfo;

struct SZrIoFunction {
    TZrString *name;
    TUInt64 startLine;
    TUInt64 endLine;
    TZrSize parametersLength;
    TUInt64 hasVarArgs;
    TZrSize instructionsLength;
    TZrInstruction *instructions;
    TZrSize localVariablesLength;
    SZrIoFunctionLocalVariable *localVariables;
    TZrSize constantVariablesLength;
    SZrIoFunctionConstantVariable *constantVariables;
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
    TZrString *name;
    TZrSize functionsLength;
    SZrIoFunction *functions;
};

typedef struct SZrIoMethod SZrIoMethod;

struct SZrIoProperty {
    TZrString *name;
    // todo:
    TUInt32 propertyType;
    SZrIoFunction *getter;
    SZrIoFunction *setter;
};

typedef struct SZrIoProperty SZrIoProperty;

struct SZrIoField {
    TZrString *name;
};

typedef struct SZrIoField SZrIoField;

struct SZrIoEnumField {
    TZrString *name;
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

struct SZrIoClass {
    TZrString *name;
    TZrSize superClassLength;
    SZrIoReference *superClasses;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    SZrIoMemberDeclare *declares;
};

typedef struct SZrIoClass SZrIoClass;

struct SZrIoStruct {
    TZrString *name;
    TZrSize superStructLength;
    SZrIoReference *superStructs;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    SZrIoMemberDeclare *declares;
};

typedef struct SZrIoStruct SZrIoStruct;

struct SZrIoInterface {
    TZrString *name;
    TZrSize superInterfaceLength;
    SZrIoReference *superInterfaces;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    SZrIoMemberDeclare *declares;
};

typedef struct SZrIoInterface SZrIoInterface;

struct SZrIoEnum {
    TZrString *name;
    EZrValueType valueType;
    TZrSize fieldsLength;
    SZrIoEnumField *fields;
};

typedef struct SZrIoEnum SZrIoEnum;

struct SZrIoModuleDeclare {
    EZrIoModuleDeclareType type;

    union {
        SZrIoClass *class;
        SZrIoStruct *struct_;
        SZrIoInterface *interface;
        SZrIoFunction *function;
        SZrIoEnum *enum_;
        SZrIoField *field;
    };
};

typedef struct SZrIoModuleDeclare SZrIoModuleDeclare;

struct SZrIoModule {
    TZrString *name;
    TZrString *md5;
    TZrSize importsLength;
    SZrIoImport *imports;
    TZrSize declaresLength;
    SZrIoModuleDeclare *declares;
    SZrIoFunction *entryFunction;
};

typedef struct SZrIoModule SZrIoModule;

struct SZrIoSource {
    TChar signature[4];
    TUInt32 versionMajor;
    TUInt32 versionMinor;
    TUInt32 versionPatch;
    TUInt64 format;
    TUInt8 nativeIntSize;
    TUInt8 typeSizeSize;
    TUInt8 typeInstructionSize;
    TBool isBigEndian;
    TBool isDebug;
    TChar optional[3];
    TZrSize modulesLength;
    SZrIoModule *modules;
};

typedef struct SZrIoSource SZrIoSource;

ZR_CORE_API SZrIo *ZrIoNew(struct SZrGlobalState *global);

ZR_CORE_API void ZrIoFree(struct SZrGlobalState *global, SZrIo *io);

ZR_CORE_API void ZrIoInit(struct SZrState *state, SZrIo *io, FZrIoRead read, FZrIoClose close, TZrPtr customData);

ZR_CORE_API TZrSize ZrIoRead(SZrIo *io, TBytePtr buffer, TZrSize size);


ZR_CORE_API SZrIoSource *ZrIoReadSourceNew(SZrIo *io);

ZR_CORE_API SZrIoSource *ZrIoLoadSource(struct SZrState *state, TNativeString sourceName, TNativeString md5);
#endif // ZR_VM_CORE_IO_H
