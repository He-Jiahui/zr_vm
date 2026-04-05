#ifndef ZR_VM_DEBUG_DEBUG_H
#define ZR_VM_DEBUG_DEBUG_H

#include "zr_vm_lib_debug/conf.h"

struct SZrFunction;

typedef struct ZrDebugAgent ZrDebugAgent;

typedef enum EZrDebugStopReason {
    ZR_DEBUG_STOP_REASON_NONE = 0,
    ZR_DEBUG_STOP_REASON_ENTRY = 1,
    ZR_DEBUG_STOP_REASON_BREAKPOINT = 2,
    ZR_DEBUG_STOP_REASON_PAUSE = 3,
    ZR_DEBUG_STOP_REASON_STEP = 4,
    ZR_DEBUG_STOP_REASON_EXCEPTION = 5,
    ZR_DEBUG_STOP_REASON_TERMINATED = 6
} EZrDebugStopReason;

typedef enum EZrDebugScopeKind {
    ZR_DEBUG_SCOPE_KIND_LOCALS = 1,
    ZR_DEBUG_SCOPE_KIND_CLOSURES = 2,
    ZR_DEBUG_SCOPE_KIND_GLOBALS = 3
} EZrDebugScopeKind;

typedef struct ZrDebugAgentConfig {
    const TZrChar *address;
    TZrBool suspend_on_start;
    TZrBool wait_for_client;
    const TZrChar *auth_token;
    TZrBool stop_on_uncaught_exception;
} ZrDebugAgentConfig;

typedef struct ZrDebugBreakpointSpec {
    const TZrChar *module_name;
    const TZrChar *source_file;
    TZrUInt32 line;
    const TZrChar *function_name;
} ZrDebugBreakpointSpec;

typedef struct ZrDebugStopEvent {
    EZrDebugStopReason reason;
    TZrUInt32 line;
    TZrUInt32 instruction_index;
    TZrUInt64 state_id;
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
} ZrDebugStopEvent;

typedef struct ZrDebugFrameSnapshot {
    TZrUInt32 frame_id;
    TZrUInt32 line;
    TZrUInt32 instruction_index;
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugFrameSnapshot;

typedef struct ZrDebugScopeSnapshot {
    TZrUInt32 scope_id;
    TZrUInt32 frame_id;
    EZrDebugScopeKind kind;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
} ZrDebugScopeSnapshot;

typedef struct ZrDebugValuePreview {
    TZrUInt32 variables_reference;
    EZrDebugScopeKind scope_kind;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar type_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar value_text[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugValuePreview;

ZR_DEBUG_API TZrBool ZrDebug_AgentStart(SZrState *state, struct SZrFunction *entryFunction, const TZrChar *moduleName,
                                        const ZrDebugAgentConfig *config, ZrDebugAgent **outAgent, TZrChar *errorBuffer,
                                        TZrSize errorBufferSize);

ZR_DEBUG_API void ZrDebug_AgentStop(ZrDebugAgent *agent);

ZR_DEBUG_API TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize);

ZR_DEBUG_API TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count);

ZR_DEBUG_API void ZrDebug_Continue(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_Pause(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_StepInto(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_StepOver(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_StepOut(ZrDebugAgent *agent);

ZR_DEBUG_API TZrBool ZrDebug_ReadStack(ZrDebugAgent *agent, ZrDebugFrameSnapshot **outFrames, TZrSize *outCount);

ZR_DEBUG_API TZrBool ZrDebug_ReadScopes(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugScopeSnapshot **outScopes,
                                        TZrSize *outCount);

ZR_DEBUG_API TZrBool ZrDebug_ReadVariables(ZrDebugAgent *agent, TZrUInt32 scopeId, ZrDebugValuePreview **outValues,
                                           TZrSize *outCount);

ZR_DEBUG_API void ZrDebug_Free(void *pointer);

ZR_DEBUG_API void ZrDebug_NotifyException(ZrDebugAgent *agent);

ZR_DEBUG_API void ZrDebug_NotifyTerminated(ZrDebugAgent *agent, TZrBool success);

#endif
