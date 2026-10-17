#ifndef ZR_VM_CORE_EXEC_IR_GC_MAP_INTERNAL_H
#define ZR_VM_CORE_EXEC_IR_GC_MAP_INTERNAL_H

#include "zr_vm_core/exec_ir.h"

/* Only the optional owned header's outer shape is checked here. Its entries
 * may be empty or contain multiple records; its three pools remain independent.
 * Live readable storage and proper ownership remain caller preconditions. */
static inline TZrBool zr_exec_ir_gc_map_outer_shape_is_valid(
        const SZrExecIrFunction *function) {
    if (function == ZR_NULL) return ZR_FALSE;
    if (function->gcMap == ZR_NULL) {
        return function->gcMapCount == 0u && function->gcMapCapacity == 0u;
    }
    return function->gcMapCount == 1u && function->gcMapCapacity == 1u;
}

#endif
