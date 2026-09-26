/** 旧版 zr.task runtime descriptor 的内部到公开注册桥接入口。 */

#ifndef ZR_VM_TASK_RUNTIME_H
#define ZR_VM_TASK_RUNTIME_H

#include "zr_vm_lib_task/conf.h"

/** @brief 返回静态的旧版 descriptor，供模块注册与插件导出借用。
 *  @note 调用方不可释放返回值；接入同名官方 provider 前需先核对当前核心 contract。
 */
ZR_VM_TASK_API const ZrLibModuleDescriptor *ZrVmTask_Runtime_GetModuleDescriptor(void);

#endif // ZR_VM_TASK_RUNTIME_H
