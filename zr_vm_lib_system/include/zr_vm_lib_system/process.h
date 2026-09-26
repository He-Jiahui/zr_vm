//
// zr.system.process native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_PROCESS_H
#define ZR_VM_LIB_SYSTEM_PROCESS_H

#include "zr_vm_lib_system/conf.h"

/** @brief 暂停当前宿主线程指定毫秒数；负值按零处理。
 *  @note 该回调阻塞所在 OS 线程，不参与 VM 任务调度。 */
TZrBool ZrSystem_Process_SleepMilliseconds(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 直接终止整个宿主进程，调用后不会返回 VM；宿主应仅向可信脚本暴露。 */
TZrBool ZrSystem_Process_Exit(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_PROCESS_H
