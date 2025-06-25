//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CONF_H
#define ZR_VM_CONF_H
#include "zr_type_conf.h"

#if ZR_IS_32_INT
#define ZR_VM_MAX_STACK 1000000
#else
#define ZR_VM_MAX_STACK		15000
#endif

#define ZR_VM_ERROR_STACK (ZR_VM_MAX_STACK + 200)
#define ZR_VM_STACK_GLOBAL_MODULE_REGISTRY (-(ZR_VM_MAX_STACK) - 1000)
#define ZR_VM_STACK_CLOSURE_MAX 65535

#endif //ZR_VM_CONF_H
