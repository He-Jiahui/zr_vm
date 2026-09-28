#ifndef ZR_VM_CORE_TASK_RUNTIME_H
#define ZR_VM_CORE_TASK_RUNTIME_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"

/**
 * @brief 注册的 guest task runtime 对外暴露的整数状态。
 * @note core task、task 库和 thread runtime 会将这些值写入 guest task 对象；
 *       数值必须保持稳定。
 *       这些状态描述 guest task 对象，与 core frame 生命周期状态不同。
 */
typedef enum EZrVmTaskStatus {
    ZR_VM_TASK_STATUS_CREATED = 0,  /**< 已创建，尚未提交给调度器。 */
    ZR_VM_TASK_STATUS_QUEUED = 1,   /**< 已被调度器接收，等待执行。 */
    ZR_VM_TASK_STATUS_RUNNING = 2,  /**< 当前正在 worker 上执行。 */
    ZR_VM_TASK_STATUS_SUSPENDED = 3, /**< TODO: 请确认此未使用的状态值是预留值还是已废弃。 */
    ZR_VM_TASK_STATUS_COMPLETED = 4, /**< 已成功结束。 */
    ZR_VM_TASK_STATUS_FAULTED = 5   /**< 已因错误结束。 */
} EZrVmTaskStatus;

/**
 * @brief 挂接 native registry，并注册 core task 模块 provider。
 * @pre 设置 runtime provider 时，global 必须是有效的全局状态。
 * @return global 为空，或挂接 registry / 注册模块失败时返回 false。
 * @note 每个 global state 应选择一个 task 模块 provider；为同一 guest module
 *       注册第二个 provider 可能违反 provider 唯一性。
 */
ZR_CORE_API TZrBool ZrCore_TaskRuntime_RegisterBuiltins(SZrGlobalState *global);

#endif
