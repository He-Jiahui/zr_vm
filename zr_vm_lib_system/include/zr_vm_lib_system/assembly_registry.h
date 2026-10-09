/** @file
 * ## Summary
 * 声明 `zr.system.assembly` 静态模块描述符的借用入口，供系统库注册阶段发布资源 API。
 * 注册数据关联三个 callback、参数提示、返回类型和工具提示；资源读取发生在后续派发。
 *
 * ## Value model and lifecycle
 * 描述符及嵌套表位于模块静态只读存储；不拥有项目、VM、archive 或一次调用的结果槽。
 * 调用方在模块仍加载时借用它们，不修改或释放。getter 不创建 global，也不执行 callback。
 *
 * ## References
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:41` 定义描述符，`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:62` 返回其地址。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/module.c:84` 取得该描述符，`zr_vm_lib_system/src/zr_vm_lib_system/module.c:100` 登记叶模块。
 */

#ifndef ZR_VM_LIB_SYSTEM_ASSEMBLY_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_ASSEMBLY_REGISTRY_H

#include "zr_vm_lib_system/assembly.h"

/**
 * ## Summary
 * 借出 `zr.system.assembly` 的模块描述符，让 `ZrVmLibSystem_Register` 在发布根模块链接
 * 前登记三个资源导出及类型提示。这一步提供注册信息，不查询项目资源。
 *
 * ## Parameters
 * 无参数；所返回的表是模块静态数据，不依赖当前项目或某个 global。
 *
 * ## Returns
 * @return 指向静态 `const ZrLibModuleDescriptor` 的只读借用。嵌套函数表、参数表、提示表、
 *         JSON 和 callback 指针共同属于模块存储；调用方不释放或修改它们。
 *
 * ## Constraints and lifecycle
 * 调用方负责初始化并维持 native registry，以及使 descriptor/callback 所属模块在使用期
 * 保持加载。当前系统库先登记叶模块，随后登记聚合根；getter 调用不会打开 archive、创建
 * VM 对象或执行任何资源 API，也不替注册方保证动态卸载与并发访问安全。
 *
 * ## References
 * - `zr_vm_lib_system/src/zr_vm_lib_system/module.c:84` 是实际 getter 调用；`zr_vm_lib_system/src/zr_vm_lib_system/module.c:100` 消费借出的描述符。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/module.c:105` 随后登记根描述符；`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:62` 返回静态地址。
 * - `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:199` 后续取函数指针，`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277` 才执行 callback。
 */
const ZrLibModuleDescriptor *ZrSystem_AssemblyRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_ASSEMBLY_REGISTRY_H
