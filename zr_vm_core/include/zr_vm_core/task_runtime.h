#ifndef ZR_VM_CORE_TASK_RUNTIME_H
#define ZR_VM_CORE_TASK_RUNTIME_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"

typedef enum EZrVmTaskStatus {
    ZR_VM_TASK_STATUS_CREATED = 0,
    ZR_VM_TASK_STATUS_QUEUED = 1,
    ZR_VM_TASK_STATUS_RUNNING = 2,
    ZR_VM_TASK_STATUS_SUSPENDED = 3,
    ZR_VM_TASK_STATUS_COMPLETED = 4,
    ZR_VM_TASK_STATUS_FAULTED = 5
} EZrVmTaskStatus;

ZR_CORE_API TZrBool ZrCore_TaskRuntime_RegisterBuiltins(SZrGlobalState *global);

#endif
