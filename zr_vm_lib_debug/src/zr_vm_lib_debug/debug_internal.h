#ifndef ZR_VM_DEBUG_DEBUG_INTERNAL_H
#define ZR_VM_DEBUG_DEBUG_INTERNAL_H

#include "zr_vm_lib_debug/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "cJSON/cJSON.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#define ZR_DEBUG_WAIT_INFINITE ((TZrUInt32) 0xFFFFFFFFu)
#define ZR_DEBUG_PROTOCOL_NAME "zrdbg/1"

static ZR_FORCE_INLINE void zr_debug_memory_barrier(void) {
#if defined(_WIN32)
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

static ZR_FORCE_INLINE void zr_debug_bool_store(volatile TZrBool *slot, TZrBool value) {
    if (slot == ZR_NULL) {
        return;
    }

    *slot = value;
    zr_debug_memory_barrier();
}

static ZR_FORCE_INLINE TZrBool zr_debug_bool_load(const volatile TZrBool *slot) {
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_memory_barrier();
    return *slot;
}

typedef enum EZrDebugRunMode {
    ZR_DEBUG_RUN_MODE_RUNNING = 0,
    ZR_DEBUG_RUN_MODE_PAUSED = 1,
    ZR_DEBUG_RUN_MODE_STEP_INTO = 2,
    ZR_DEBUG_RUN_MODE_STEP_OVER = 3,
    ZR_DEBUG_RUN_MODE_STEP_OUT = 4
} EZrDebugRunMode;

typedef enum EZrDebugPrototypeViewKind {
    ZR_DEBUG_PROTOTYPE_VIEW_NONE = 0,
    ZR_DEBUG_PROTOTYPE_VIEW_METADATA = 1,
    ZR_DEBUG_PROTOTYPE_VIEW_MEMBERS = 2,
    ZR_DEBUG_PROTOTYPE_VIEW_METHODS = 3,
    ZR_DEBUG_PROTOTYPE_VIEW_PROPERTIES = 4,
    ZR_DEBUG_PROTOTYPE_VIEW_STATICS = 5,
    ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS = 6,
    ZR_DEBUG_PROTOTYPE_VIEW_MANAGED_FIELDS = 7
} EZrDebugPrototypeViewKind;

typedef struct ZrDebugBreakpoint {
    EZrDebugBreakpointKind kind;
    TZrChar module_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar condition[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar hit_condition[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar log_message[ZR_DEBUG_TEXT_CAPACITY];
    TZrUInt32 line;
    TZrUInt32 hit_count;
    SZrFunction *resolved_function;
    TZrUInt32 resolved_instruction_index;
    TZrBool resolved;
} ZrDebugBreakpoint;

typedef enum EZrDebugVariableHandleKind {
    ZR_DEBUG_VARIABLE_HANDLE_KIND_NONE = 0,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_VALUE = 1,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_GLOBAL_STATE = 2,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE = 3,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE_VIEW = 4,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_EXCEPTION = 5,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_INDEX_WINDOW = 6
} EZrDebugVariableHandleKind;

typedef struct ZrDebugVariableHandle {
    TZrUInt32 handle_id;
    TZrUInt64 state_id;
    EZrDebugVariableHandleKind kind;
    EZrDebugPrototypeViewKind prototype_view_kind;
    TZrSize window_start;
    TZrSize window_count;
    SZrTypeValue value;
    SZrObjectPrototype *prototype;
} ZrDebugVariableHandle;

struct ZrDebugAgent {
    SZrState *state;
    SZrFunction *entryFunction;
    ZrDebugAgentConfig config;
    SZrNetworkListener listener;
    SZrNetworkStream client;
    TZrBool hasClient;
    TZrBool clientInitialized;
    TZrBool waitForClientPending;
    TZrBool startStopPending;
    volatile TZrBool pauseRequested;
    volatile TZrBool exceptionStopPending;
    TZrBool exceptionBreakOnCaught;
    TZrBool exceptionBreakOnUncaught;
    TZrBool terminatedNotified;
    TZrBool clientDisconnectContinue;
    EZrDebugRunMode runMode;
    TZrUInt32 stepDepth;
    SZrFunction *resumeFunction;
    TZrUInt32 resumeInstruction;
    TZrUInt64 stopStateId;
    ZrDebugStopEvent lastStopEvent;
    ZrDebugBreakpoint *breakpoints;
    TZrSize breakpointCount;
    TZrSize lineBreakpointCount;
    TZrSize functionBreakpointCount;
    ZrDebugVariableHandle *variableHandles;
    TZrSize variableHandleCount;
    TZrSize variableHandleCapacity;
    TZrUInt32 nextVariableHandleId;
    TZrUInt32 previousDebugHookSignal;
    TZrChar moduleName[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
};

void zr_debug_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text);
const TZrChar *zr_debug_string_native(SZrString *value);
const TZrChar *zr_debug_function_name(SZrFunction *function);
const TZrChar *zr_debug_function_source(SZrFunction *function);
TZrUInt32 zr_debug_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function);
TZrUInt32 zr_debug_frame_depth(SZrState *state);
const TZrChar *zr_debug_stop_reason_name(EZrDebugStopReason reason);
const TZrChar *zr_debug_exception_filter_name(EZrDebugExceptionFilter filter);
const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind);
const TZrChar *zr_debug_value_type_name(EZrValueType type);
const TZrChar *zr_debug_prototype_type_name(EZrObjectPrototypeType type);
TZrUInt32 zr_debug_scope_id(TZrUInt32 frameId, EZrDebugScopeKind kind);
TZrBool zr_debug_exception_read_frame(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugFrameSnapshot *outFrame);
TZrSize zr_debug_exception_frame_count(ZrDebugAgent *agent);
SZrCallInfo *zr_debug_find_call_info_by_frame_id(ZrDebugAgent *agent, TZrUInt32 frameId, SZrFunction **outFunction);
SZrObjectPrototype *zr_debug_resolve_value_prototype(SZrState *state, const SZrTypeValue *value);
void zr_debug_format_value_text_safe(SZrState *state,
                                     const SZrTypeValue *value,
                                     TZrChar *buffer,
                                     TZrSize bufferSize);
TZrBool zr_debug_resolve_identifier_value(ZrDebugAgent *agent,
                                          TZrUInt32 frameId,
                                          const TZrChar *name,
                                          SZrTypeValue *outValue,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize);
TZrBool zr_debug_safe_get_member_value(ZrDebugAgent *agent,
                                       const SZrTypeValue *receiver,
                                       const TZrChar *memberName,
                                       SZrTypeValue *outValue,
                                       TZrChar *errorBuffer,
                                       TZrSize errorBufferSize);
TZrBool zr_debug_try_resolve_indexed_storage(SZrState *state,
                                             const SZrTypeValue *value,
                                             SZrObject **outStorage,
                                             const TZrChar **outSyntheticName,
                                             TZrSize *outLength);
TZrBool zr_debug_safe_get_index_value(ZrDebugAgent *agent,
                                      const SZrTypeValue *receiver,
                                      const SZrTypeValue *key,
                                      SZrTypeValue *outValue,
                                      TZrChar *errorBuffer,
                                      TZrSize errorBufferSize);

void zr_debug_agent_close_client(ZrDebugAgent *agent);
void zr_debug_agent_fill_stop_event(ZrDebugAgent *agent,
                                    EZrDebugStopReason reason,
                                    EZrDebugExceptionFilter exceptionFilter,
                                    SZrFunction *function,
                                    TZrUInt32 instructionIndex,
                                    TZrUInt32 sourceLine);
void zr_debug_agent_poll_messages(ZrDebugAgent *agent, TZrUInt32 timeoutMs, TZrBool *outDisconnected);
void zr_debug_agent_pause_loop(ZrDebugAgent *agent);
void zr_debug_agent_emit_output(ZrDebugAgent *agent, const TZrChar *category, const TZrChar *outputText);

TZrUInt32 zr_debug_call_info_line(ZrDebugAgent *agent, SZrCallInfo *callInfo, SZrFunction *function,
                                  TZrBool isTopFrame);
void zr_debug_variable_handles_clear(ZrDebugAgent *agent);
TZrBool zr_debug_evaluate_expression(ZrDebugAgent *agent,
                                     TZrUInt32 frameId,
                                     const TZrChar *expression,
                                     SZrTypeValue *outValue,
                                     TZrChar *errorBuffer,
                                     TZrSize errorBufferSize);

#endif
