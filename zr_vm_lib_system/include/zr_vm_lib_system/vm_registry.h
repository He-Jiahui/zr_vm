//
// zr.system.vm descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_VM_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_VM_REGISTRY_H

#include "zr_vm_lib_system/vm.h"

/**
 * ## Summary
 * 返回 `zr.system.vm` 的静态模块描述符，供 System 模块注册器暴露 VM 快照类型和跨模块调用函数。
 *
 * ## Returns
 * @return 指向 provider 静态存储的只读描述符；调用方仅借用该指针，不得释放或修改。
 *
 * ## Constraints and lifecycle
 * 描述符、函数/类型/hint 数组及其中的字符串均为静态存储，注册器会浅引用这些地址；包含它们的 provider image 必须保持驻留，直到所有注册器引用和 callback 使用结束。
 *
 * ## References
 * `ZrVmLibSystem_Register` 将此描述符加入 System leaf registry；实现表见 `vm_registry.c`，回调定义见 `vm.h`。
 */
const ZrLibModuleDescriptor *ZrSystem_VmRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_VM_REGISTRY_H
