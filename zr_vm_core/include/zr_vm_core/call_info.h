//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_CALL_INFO_H
#define ZR_VM_CORE_CALL_INFO_H
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/stack.h"

enum EZrCallStatus {
    ZR_CALL_STATUS_ALLOW_HOOK = 1 << 0,
    ZR_CALL_STATUS_NATIVE_CALL = 1 << 1,
    ZR_CALL_STATUS_CREATE_FRAME = 1 << 2,
    ZR_CALL_STATUS_DEBUG_HOOK = 1 << 3,
    ZR_CALL_STATUS_YIELD_CALL = 1 << 4,
    ZR_CALL_STATUS_TAIL_CALL = 1 << 5,
    ZR_CALL_STATUS_HOOK_YIELD = 1 << 6,
    ZR_CALL_STATUS_DECONSTRUCTOR_CALL = 1 << 7,
    ZR_CALL_STATUS_CALL_INFO_TRANSFER = 1 << 8,
    ZR_CALL_STATUS_RELEASE_CALL = 1 << 9,
};

typedef enum EZrCallStatus EZrCallStatus;

struct SZrCallInfo {
    TZrStackIndicator functionIndex;
    TZrStackIndicator functionTop;

    EZrCallStatus callStatus;

    struct SZrCallInfo *previous;

    struct SZrCallInfo *next;
};


typedef struct SZrCallInfo SZrCallInfo;
#endif //ZR_VM_CORE_CALL_INFO_H
