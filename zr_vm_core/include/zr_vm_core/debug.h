//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_DEBUG_H
#define ZR_VM_CORE_DEBUG_H

#include "zr_vm_core/conf.h"
struct SZrState;

enum EZrDebugHookEvent {
    ZR_DEBUG_HOOK_EVENT_CALL,
    ZR_DEBUG_HOOK_EVENT_RETURN,
    ZR_DEBUG_HOOK_EVENT_LINE,
    ZR_DEBUG_HOOK_EVENT_COUNT,
    ZR_DEBUG_HOOK_EVENT_MAX
};

typedef enum EZrDebugHookEvent EZrDebugHookEvent;

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

struct ZR_STRUCT_ALIGN SZrDebugInfo {
    EZrDebugHookEvent event;

    TNativeString name;
    // todo
};

typedef struct SZrDebugInfo SZrDebugInfo;

typedef void (*FZrDebugHook)(struct SZrState *state, SZrDebugInfo debugInfo);


ZR_CORE_API TBool ZrDebugInfoGet(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo);

ZR_CORE_API ZR_NO_RETURN void ZrDebugRunError(struct SZrState *state, TNativeString format, ...);

ZR_CORE_API ZR_NO_RETURN void ZrDebugErrorWhenHandlingError(struct SZrState *state);

#endif // ZR_VM_CORE_DEBUG_H
