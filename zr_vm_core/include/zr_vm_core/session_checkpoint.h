#ifndef ZR_VM_CORE_SESSION_CHECKPOINT_H
#define ZR_VM_CORE_SESSION_CHECKPOINT_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrSessionCheckpoint;

/** @brief 保存对象身份及可恢复状态的会话快照；借用创建时的 state，自己持有 GC 根句柄。
 * @note core 句柄不延长 state/global 的寿命；跨语言包装层须另行保留其 owner，先释放快照再释放 owner。
 */
typedef struct SZrSessionCheckpoint SZrSessionCheckpoint;

/** @brief 在静止的 VM 边界保存全局根可达的可恢复对象图，供项目会话回滚。
 * @pre state 为存活的同一 VM 线程，callinfo 位于 base frame，stackTop 恰为 stackBase 后一格，
 *      且没有其他 live stack Value、pending control、current exception、异常恢复点/处理器、AOT root
 *      frame、活动执行预算、嵌套 native 调用或其 yield 计数、开放栈闭包或待关闭栈值；跨 FFI 的 live
 *      Value 由上层排除。
 * @return 成功时将快照所有权写入 outCheckpoint；失败时不修改原输出值。
 * @note 只保存受支持的逻辑状态，不复制完整堆；快照必须在 state/global 释放前销毁。
 */
ZR_CORE_API TZrBool ZrCore_SessionCheckpoint_Create(
        struct SZrState *state,
        SZrSessionCheckpoint **outCheckpoint);
/** @brief 在原有对象上恢复快照，使外部持有的对象身份与环/别名保持有效。
 * @pre 只能在创建快照的同一存活 state 上调用，且会话须静止；不得存在异常处理器、pending control、
 *      活动异常恢复点、执行预算、嵌套 native 调用、AOT root frame、开放栈闭包或待关闭栈值；
 *      将被线程归一化丢弃的栈槽不得持有 native ownership。
 * @return 成功为真；返回假时，不提交快照根、对象、模块、数组或线程边界状态。
 * @note 线程需要归一化时，会在全部暂存和句柄校验成功后执行；快照成功后仍可重复回滚。
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
