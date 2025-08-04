//
// Created by HeJiahui on 2025/6/4.
//

#ifndef ZR_VM_CORE_H
#define ZR_VM_CORE_H
/**
 * conf
 * value conversion log debug hash math meta execution
 * stack(value) io(value) native(value)
 * call_info(stack) exception(stack) function(stack)
 * callback(exception) closure(stack)
 * global(callback) state(closure)
 * memory(global) gc(global)
 * hash_set(memory) array(memory)
 * string(hash_set)
 */
#include "zr_vm_core/array.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/native.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type.h"
#include "zr_vm_core/value.h"

ZR_CORE_API void Hello(void);


#endif // ZR_VM_CORE_H
