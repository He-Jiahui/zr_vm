/** 旧版 zr.task 模块的导出配置；当前产品由核心 task runtime 提供同名模块。 */

#ifndef ZR_VM_TASK_CONF_H
#define ZR_VM_TASK_CONF_H

#include "zr_vm_library.h"

/** 与宿主库共享可见性约定；仅供本目录的注册和 descriptor 入口使用。 */
#define ZR_VM_TASK_API ZR_API

#endif // ZR_VM_TASK_CONF_H
