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

/**
 * @fn TZrBool zr_state_map_input_span_disjoint(const SZrExecIrFunction *function, const SZrExecIrStateMap *map, const void *storage, size_t bytes)
 *
 * ## Summary
 * 声明 Core 内部的容量区间隔离检查，供 materialization 和 interpreter resume 拒绝目标缓冲区与输入存储重叠。
 *
 * ## Parameters
 * @param[in] function 调用期间保持稳定且可读取的函数记录。
 * @param[in] map 调用期间保持稳定且可读取的 state-map 记录。
 * @param[in] storage 候选范围起始地址；bytes 为零时可为 NULL。
 * @param[in] bytes 候选范围字节数；零长度仍会检查输入容量尺寸。
 *
 * ## Returns
 * @return 输入为空、非零候选地址无效/末端溢出、容量尺寸不可表示或区间重叠时返回 ZR_FALSE；否则返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 仅比较地址和按 capacity 计算的字节范围，不证明输入内存可读或有有效分配来源，也不提供同步；函数/map 与嵌套记录须由调用方保证有效并稳定。
 *
 * ## References
 * 定义位于同目录 exec_ir_state_map_storage.c 236 行；直接调用位于该 C 文件 345 行和 exec_ir_interpreter_resume.c 256 行。
 */

/**
 * @fn TZrBool zr_state_map_target_valid(const SZrExecIrMaterializedState *target, const SZrExecIrStateMap *map, const SZrExecIrFunction *function)
 *
 * ## Summary
 * 声明 materialized-state 目标池的内部形状和输入别名检查。
 *
 * ## Parameters
 * @param[in] target 待检查的调用方目标记录及其旧池。
 * @param[in] map 待读取的 state-map 输入。
 * @param[in] function 待读取的 ExecIR 函数输入。
 *
 * ## Returns
 * @return 记录为空、容量形状不合法、目标池互相重叠/覆盖目标记录或覆盖函数/map 输入存储时返回 ZR_FALSE；否则返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 该 helper 不验证缓冲区实际分配来源或可释放性，也不检查 target 记录自身与输入记录的别名；MaterializeState 成功会释放旧 target 池，因此调用方须保证目标记录独立可写、五个旧池独立可释放，且函数/map 输入稳定可读。接口不分配、不修改记录、无内部同步。
 *
 * ## References
 * 定义位于同目录 exec_ir_state_map_storage.c 308 行；唯一生产调用位于 exec_ir_materialize.c 720 行。
 */
