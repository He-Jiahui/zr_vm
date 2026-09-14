//
// Created by HeJiahui on 2025/6/4.
//

#ifndef ZR_VM_CORE_H
#define ZR_VM_CORE_H
/**
 * conf
 * raw_object value conversion log debug hash math meta execution
 * stack(value) io(value) native(value)
 * call_info(stack) exception(stack) function(stack)
 * callback(exception) closure(stack)
 * global(callback) state(closure)
 * memory(global) gc(global)
 * hash_set(memory) array(memory)
 * string(hash_set) object(hash_set)
 * module(object)
 */
#include "zr_vm_core/array.h"
#include "zr_vm_core/aot_ir.h"
#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/execbc_verify.h"
#include "zr_vm_core/batch_contract.h"
#include "zr_vm_core/exec_ir_numeric.h"
#include "zr_vm_core/host_baseline_jit.h"
#include "zr_vm_core/optimization_remark.h"
#include "zr_vm_core/capability_manifest.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/container_storage_contract.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/contiguous_view.h"
#include "zr_vm_core/object_layout_map.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution_contract.h"
#include "zr_vm_core/execution_backend.h"
#include "zr_vm_core/execution_call_transfer.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_young_allocation.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/gc_domain_clone.h"
#include "zr_vm_core/gc_budget_contract.h"
#include "zr_vm_core/async_frame_budget.h"
#include "zr_vm_core/hotpatch_generation.h"
#include "zr_vm_core/hotpatch_capability.h"
#include "zr_vm_core/hotpatch_publish.h"
#include "zr_vm_core/hotpatch_profile.h"
#include "zr_vm_core/hotpatch_rollback.h"
#include "zr_vm_core/hotpatch_retire.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/native_call_contract.h"
#include "zr_vm_core/native.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/string_builder.h"
#include "zr_vm_core/type.h"
#include "zr_vm_core/value.h"

ZR_CORE_API void Hello(void);


#endif // ZR_VM_CORE_H
