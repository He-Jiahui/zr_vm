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

/**
 * @fn TZrBool zr_state_map_storage_shape_valid(const SZrExecIrStateMap *map)
 *
 * ## Summary
 * 私有存储层检查 state-map 四个池的 count/capacity、指针配对、字节尺寸和容量区间互斥关系，供 Core、Parser 与 materialization 共用。
 *
 * ## Parameters
 * @param[in] map 借用的待检查 map；NULL 或结构不一致时拒绝。
 *
 * ## Returns
 * @return 仅当存储形状满足内部不变量时返回 ZR_TRUE；不分配、不修改 map，也不生成诊断。
 *
 * ## Constraints and lifecycle
 * 该接口只验证存储结构，不验证 checkpoint 内容或池指针是否实际可读/可释放。调用期间 map 与池须保持稳定；map 记录及池的所有权仍归调用方。
 *
 * ## References
 * 声明对应的实现位于本目录 exec_ir_state_map_storage.c；Clone 和 StateMapStorageValid 从同一实现复用该检查。
 */
