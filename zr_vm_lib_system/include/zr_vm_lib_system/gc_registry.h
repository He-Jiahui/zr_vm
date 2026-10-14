//
// zr.system.gc descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_GC_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_GC_REGISTRY_H

#include "zr_vm_lib_system/gc.h"

/**
 * ## Summary
 * 提供 `zr.system.gc` 的静态模块描述符，供 System 注册器安装 GC 控制回调、统计类型和工具提示。
 *
 * ## Returns
 * @return 返回进程映像内静态存储的模块描述符；调用方借用该指针，不释放或修改其内容。
 *
 * ## Constraints and lifecycle
 * 描述符内部还借用静态函数表、字段表和提示字符串。注册器及运行时只要可能读取这些指针或调用其中
 * 的 callback，就必须保持定义它们的 provider 映像驻留；返回指针本身不延长映像或任一嵌套数据的寿命。
 *
 * ## References
 * `ZrVmLibSystem_Register` 将本描述符交给 native module registry；定义与各静态表位于
 * `gc_registry.c`。
 */
const ZrLibModuleDescriptor *ZrSystem_GcRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_GC_REGISTRY_H
