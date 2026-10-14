//
// zr.system.exception descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_EXCEPTION_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_EXCEPTION_REGISTRY_H

#include "zr_vm_lib_system/exception.h"

/**
 * ## Summary
 * 返回 `zr.system.exception` 叶模块的静态描述符，供 System 注册流程登记异常类型、构造器和 handler 注册函数。
 *
 * ## Returns
 * @return 指向进程映像内只读模块描述符的借用指针；该存储具有静态生命周期，调用方不得释放或修改。
 *
 * ## Constraints and lifecycle
 * 描述符及其表格都静态存储，必须保持包含它们的 provider image 有效；本 getter 不创建 VM 对象，也不导入模块。
 * System 注册器随后登记此描述符并导入 `zr.system.exception`，使 Error 原型可用于运行期异常匹配。
 *
 * ## References
 * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c:ZrSystem_ExceptionRegistry_GetModule` 定义描述符；
 * `zr_vm_lib_system/src/zr_vm_lib_system/module.c:88` 登记该叶模块，`:108` 随后物化其原型。
 */
const ZrLibModuleDescriptor *ZrSystem_ExceptionRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_EXCEPTION_REGISTRY_H
