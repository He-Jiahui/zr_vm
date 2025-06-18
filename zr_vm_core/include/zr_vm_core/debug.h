//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_DEBUG_H
#define ZR_VM_CORE_DEBUG_H

#include "zr_vm_core/conf.h"
struct SZrState;

enum EDebugHookEvent {
    ZR_DEBUG_HOOK_EVENT_CALL,
    ZR_DEBUG_HOOK_EVENT_RETURN,
    ZR_DEBUG_HOOK_EVENT_LINE,
    ZR_DEBUG_HOOK_EVENT_COUNT,
    ZR_DEBUG_HOOK_EVENT_MAX
};

typedef enum EDebugHookEvent EDebugHookEvent;

struct ZR_STRUCT_ALIGN SZrDebugInfo {
    EDebugHookEvent event;

    TNativeString name;
    // todo
};

typedef struct SZrDebugInfo SZrDebugInfo;

typedef void (*FZrDebugHook)(struct SZrState *state, SZrDebugInfo debugInfo);

#endif //ZR_VM_CORE_DEBUG_H
