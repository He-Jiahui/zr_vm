//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_DEBUG_H
#define ZR_VM_CORE_DEBUG_H

#include "zr_vm_core/conf.h"
struct SZrState;
struct SZrCallInfo;
struct SZrTypeValue;
struct SZrObjectPrototype;
struct SZrObject;
struct SZrFunction;
enum EZrDebugHookEvent {
    ZR_DEBUG_HOOK_EVENT_CALL,
    ZR_DEBUG_HOOK_EVENT_RETURN,
    ZR_DEBUG_HOOK_EVENT_LINE,
    ZR_DEBUG_HOOK_EVENT_COUNT,
    ZR_DEBUG_HOOK_EVENT_MAX
};

typedef enum EZrDebugHookEvent EZrDebugHookEvent;

enum EZrDebugHookMask {
    ZR_DEBUG_HOOK_MASK_CALL = 1 << ZR_DEBUG_HOOK_EVENT_CALL,
    ZR_DEBUG_HOOK_MASK_RETURN = 1 << ZR_DEBUG_HOOK_EVENT_RETURN,
    ZR_DEBUG_HOOK_MASK_LINE = 1 << ZR_DEBUG_HOOK_EVENT_LINE,
    ZR_DEBUG_HOOK_MASK_COUNT = 1 << ZR_DEBUG_HOOK_EVENT_COUNT,
    ZR_DEBUG_HOOK_MASK_MAX = 1 << ZR_DEBUG_HOOK_EVENT_MAX
};

typedef enum EZrDebugHookMask EZrDebugHookMask;

enum EZrDebugInfoType {
    ZR_DEBUG_INFO_SOURCE_FILE = 1,
    ZR_DEBUG_INFO_LINE_NUMBER = 2,
    ZR_DEBUG_INFO_CLOSURE = 4,
    ZR_DEBUG_INFO_TAIL_CALL = 8,
    ZR_DEBUG_INFO_FUNCTION_NAME = 16,
    ZR_DEBUG_INFO_RETURN_VALUE = 32,
    ZR_DEBUG_INFO_LINE_TABLE = 64,
    ZR_DEBUG_INFO_PUSH_FUNCTION = 128,
    ZR_DEBUG_INFO_MAX = 256
};

typedef enum EZrDebugInfoType EZrDebugInfoType;

enum EZrDebugScope {
    ZR_DEBUG_SCOPE_GLOBAL,
    ZR_DEBUG_SCOPE_LOCAL,
    ZR_DEBUG_SCOPE_CLOSURE,
    ZR_DEBUG_SCOPE_FUNCTION,
    ZR_DEBUG_SCOPE_MAX
};

typedef enum EZrDebugScope EZrDebugScope;


struct ZR_STRUCT_ALIGN SZrDebugInfo {
    EZrDebugHookEvent event;

    TZrNativeString name;
    // todo
    EZrDebugScope scope;

    TZrBool isNative;

    TZrNativeString source;

    TZrSize sourceLength;

    TZrSize currentLine;
    TZrSize definedLineStart;
    TZrSize definedLineEnd;
    TZrSize closureValuesCount;
    TZrSize parametersCount;

    TZrBool hasVariableParameters;
    TZrBool isTailCall;

    TZrUInt32 transferStart;
    TZrUInt32 transferCount;
    struct SZrCallInfo *callInfo;
};

typedef struct SZrDebugInfo SZrDebugInfo;

typedef void (*FZrDebugHook)(struct SZrState *state, SZrDebugInfo *debugInfo);

typedef TZrDebugSignal (*FZrDebugTraceObserver)(struct SZrState *state,
                                                struct SZrFunction *function,
                                                const TZrInstruction *programCounter,
                                                TZrUInt32 instructionOffset,
                                                TZrUInt32 sourceLine,
                                                TZrPtr userData);


ZR_CORE_API TZrBool ZrCore_DebugInfo_Get(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo);

ZR_CORE_API void ZrCore_Debug_SetTraceObserver(struct SZrState *state,
                                               FZrDebugTraceObserver observer,
                                               TZrPtr userData);

ZR_CORE_API ZR_NO_RETURN void ZrCore_Debug_CallError(struct SZrState *state, struct SZrTypeValue *value);

ZR_CORE_API TZrDebugSignal ZrCore_Debug_TraceExecution(struct SZrState *state, const TZrInstruction *programCounter);

ZR_CORE_API ZR_NO_RETURN void ZrCore_Debug_RunError(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API ZR_NO_RETURN void ZrCore_Debug_ErrorWhenHandlingError(struct SZrState *state);

ZR_CORE_API void ZrCore_Debug_Hook(struct SZrState *state, EZrDebugHookEvent event, TZrUInt32 line, TZrUInt32 transferStart,
                             TZrUInt32 transferCount);

ZR_CORE_API void ZrCore_Debug_HookReturn(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount);

// Debug工具：输出Prototype信息（类、结构体等）
// 输出prototype的名称、类型、继承关系、字段、Meta方法等信息
ZR_CORE_API void ZrCore_Debug_PrintPrototype(struct SZrState *state, struct SZrObjectPrototype *prototype, FILE *output);

// Debug工具：输出Object信息
// 输出object的prototype、字段值等信息
ZR_CORE_API void ZrCore_Debug_PrintObject(struct SZrState *state, struct SZrObject *object, FILE *output);

// Debug工具：从prototypeData中解析并字符串化prototype信息
// 输出类似zri中间表示的格式，用于调试编译后的.zri文件
ZR_CORE_API void ZrCore_Debug_PrintPrototypesFromData(struct SZrState *state, struct SZrFunction *entryFunction,
                                                FILE *output);

#endif // ZR_VM_CORE_DEBUG_H
