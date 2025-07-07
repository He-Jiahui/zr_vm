//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_VM_CORE_IO_H
#define ZR_VM_CORE_IO_H

#include "value.h"
#include "zr_vm_core/conf.h"

struct SZrState;

typedef TBytePtr (*FZrIoRead)(struct SZrState *state, TZrPtr customData, TZrSize *size);

typedef EZrThreadStatus (*FZrIoWrite)(struct SZrState *state, TBytePtr buffer, TZrSize size, TZrPtr customData);

struct ZR_STRUCT_ALIGN SZrIo {
    struct SZrState *state;
    FZrIoRead read;
    TZrSize remained;
    TBytePtr pointer;
    TZrPtr customData;
};

typedef struct SZrIo SZrIo;


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
    TZrSize debugInfoLength;
    SZrIoFunctionDebugInfo *debugInfos;
};

typedef struct SZrIoFunction SZrIoFunction;

struct SZrIoClassDeclare {
    EZrIoClassDeclareType type;

    union {
        SZrIoFunction *function;
        // todo:
    };
};

typedef struct SZrIoClassDeclare SZrIoClassDeclare;

struct SZrIoClass {
    TZrString *name;
    TZrSize superClassLength;
    SZrIoReference *superClasses;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    SZrIoClassDeclare *declares;
};

typedef struct SZrIoClass SZrIoClass;

struct SZrIoModuleDeclare {
    EZrIoModuleDeclareType type;

    union {
        SZrIoClass *class;
        SZrIoFunction *function;
        // todo:
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

ZR_CORE_API void ZrIoInit(struct SZrState *state, SZrIo *io, FZrIoRead read, TZrPtr customData);

ZR_CORE_API TZrSize ZrIoRead(SZrIo *io, TBytePtr buffer, TZrSize size);


ZR_CORE_API SZrIoSource *ZrIoReadSourceNew(SZrIo *io);
#endif //ZR_VM_CORE_IO_H
