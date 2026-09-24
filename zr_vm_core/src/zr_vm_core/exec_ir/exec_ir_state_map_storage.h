#ifndef ZR_VM_CORE_EXEC_IR_STATE_MAP_STORAGE_H
#define ZR_VM_CORE_EXEC_IR_STATE_MAP_STORAGE_H

#include "zr_vm_core/exec_ir_state_map.h"

TZrBool zr_state_map_storage_shape_valid(const SZrExecIrStateMap *map);
/* Reject base/interior aliasing with all function and state-map input storage.
 * The byte span is checked before comparing capacity-based input spans. */
TZrBool zr_state_map_input_span_disjoint(
        const SZrExecIrFunction *function, const SZrExecIrStateMap *map,
        const void *storage, size_t bytes);
TZrBool zr_state_map_target_valid(const SZrExecIrMaterializedState *target,
                                    const SZrExecIrStateMap *map,
                                    const SZrExecIrFunction *function);

#endif
