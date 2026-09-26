#ifndef ZR_VM_DEBUG_CONF_H
#define ZR_VM_DEBUG_CONF_H

#include "zr_vm_common.h"
#include "zr_vm_core.h"
#include "zr_vm_lib_network/network.h"

#define ZR_DEBUG_API ZR_API

/* 协议快照、名称和异常栈按值传递；这些上限也决定了调用方可见的截断界限。 */
#define ZR_DEBUG_TEXT_CAPACITY 256U
#define ZR_DEBUG_NAME_CAPACITY 128U
#define ZR_DEBUG_EXCEPTION_STACK_CAPACITY 4096U

#endif
