//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CONF_H
#define ZR_VM_CONF_H
#include "zr_type_conf.h"
/* 宿主可覆写 VM 栈界限；默认值随整数位宽选择，错误栈与模块 registry 使用保留区间。 */
#if !defined(ZR_VM_MAX_STACK)
#if ZR_IS_OVER_32_INT
#define ZR_VM_MAX_STACK 1000000
#else
#define ZR_VM_MAX_STACK 15000
#endif
#endif


#define ZR_VM_ERROR_STACK (ZR_VM_MAX_STACK + 200)
#define ZR_VM_STACK_GLOBAL_MODULE_REGISTRY (-(ZR_VM_MAX_STACK) - 1000)
/* BUG: value.c 的 GetStackOffsetValue 以负数 offset 比较此正数上界，检查恒成立；
 *      无法阻止过大的 closureIndex，须在调用处按换算后的索引核对捕获数量与上界。 */
#define ZR_VM_STACK_CLOSURE_MAX 65535

/* native 递归调用单独受限，避免脚本栈仍有余量时耗尽宿主 C 栈。 */
#if !defined(ZR_VM_MAX_NATIVE_CALL_STACK)
#define ZR_VM_MAX_NATIVE_CALL_STACK 200
#endif

#endif // ZR_VM_CONF_H
