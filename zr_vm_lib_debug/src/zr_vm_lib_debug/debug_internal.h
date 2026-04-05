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

typedef struct ZrDebugBreakpoint {
    TZrChar module_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrUInt32 line;
    SZrFunction *resolved_function;
    TZrUInt32 resolved_instruction_index;
    TZrBool resolved;
} ZrDebugBreakpoint;

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
const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind);
const TZrChar *zr_debug_value_type_name(EZrValueType type);
TZrUInt32 zr_debug_scope_id(TZrUInt32 frameId, EZrDebugScopeKind kind);
TZrBool zr_debug_exception_read_frame(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugFrameSnapshot *outFrame);
TZrSize zr_debug_exception_frame_count(ZrDebugAgent *agent);

void zr_debug_agent_close_client(ZrDebugAgent *agent);
void zr_debug_agent_fill_stop_event(ZrDebugAgent *agent, EZrDebugStopReason reason, SZrFunction *function,
                                    TZrUInt32 instructionIndex, TZrUInt32 sourceLine);
void zr_debug_agent_poll_messages(ZrDebugAgent *agent, TZrUInt32 timeoutMs, TZrBool *outDisconnected);
void zr_debug_agent_pause_loop(ZrDebugAgent *agent);

TZrUInt32 zr_debug_call_info_line(ZrDebugAgent *agent, SZrCallInfo *callInfo, SZrFunction *function,
                                  TZrBool isTopFrame);

#endif
