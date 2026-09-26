//
// zr.system.console descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_CONSOLE_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_CONSOLE_REGISTRY_H

#include "zr_vm_lib_system/console.h"

/** @brief 返回将脚本 console 导出绑定到宿主日志及 stdin 的静态描述符。 */
const ZrLibModuleDescriptor *ZrSystem_ConsoleRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_CONSOLE_REGISTRY_H
