#ifndef ZR_VM_CORE_SESSION_CHECKPOINT_H
#define ZR_VM_CORE_SESSION_CHECKPOINT_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrSessionCheckpoint;

/** @brief 保存对象身份及可恢复状态的会话快照；借用创建时的 state，自己持有 GC 根句柄。 */
typedef struct SZrSessionCheckpoint SZrSessionCheckpoint;

/** @brief 在静止的 VM 边界保存全局根可达的可恢复对象图，供项目会话回滚。
 * @pre state 为存活的同一 VM 线程，且没有活动执行帧、待关闭值或异常处理器；跨 FFI 的 live Value 由上层排除。
 * @return 成功时将快照所有权写入 outCheckpoint；失败时不修改原输出值。
 * @note 只保存受支持的逻辑状态，不复制完整堆；快照必须在 state/global 释放前销毁。
 */
ZR_CORE_API TZrBool ZrCore_SessionCheckpoint_Create(
        struct SZrState *state,
        SZrSessionCheckpoint **outCheckpoint);
/** @brief 在原有对象上恢复快照，使外部持有的对象身份与环/别名保持有效。
 * @pre 只能在创建快照的同一存活 state 上调用，且会话须静止；成功后快照仍可复用。
 * @return 成功为真；失败可能已经修改部分全局根和对象，调用方不能假定原状态仍完整。
 * @note 当前实现未恢复 SET_CONSTANT 对函数常量槽位的修改，见实现中的 BUG 记录。
 */
ZR_CORE_API TZrBool ZrCore_SessionCheckpoint_Rollback(
        struct SZrState *state,
        const SZrSessionCheckpoint *checkpoint);
/** @brief 释放快照持有的 GC 根与原生副本，结束其对旧对象图的保留。
 * @pre 非空 checkpoint 必须与创建时的同一存活 state 配对；调用后不可再次使用。
 */
ZR_CORE_API void ZrCore_SessionCheckpoint_Free(
        struct SZrState *state,
        SZrSessionCheckpoint *checkpoint);

#endif
