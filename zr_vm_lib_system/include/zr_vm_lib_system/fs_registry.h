/**
 * @file fs_registry.h
 * ## Summary
 * 声明 zr.system.fs 静态 native descriptor 的注册访问入口。
 *
 * ## Constraints and lifecycle
 * 返回描述符由 provider 静态存储；调用方借用指针，不释放、不修改，也不假定支持运行时卸载。
 *
 * ## References
 * 描述符定义见 fs_registry.c；zr.system 聚合注册见 zr_vm_lib_system/src/zr_vm_lib_system/module.c。
 */
//
// zr.system.fs descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_FS_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_FS_REGISTRY_H

#include "zr_vm_lib_system/fs.h"


/**
 * ## Summary
 * 返回 zr.system.fs 的静态模块描述符，供聚合模块注册叶子 module link。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 指向 provider 静态描述符的借用指针；结果非空且不需要调用者释放。
 *
 * ## Constraints and lifecycle
 * 描述符及其嵌套表具有静态存储期；调用者只读取，不修改或释放。
 *
 * ## References
 * 定义见 fs_registry.c 的 ZrSystem_FsRegistry_GetModule；聚合注册见 zr.system module.c。
 */
const ZrLibModuleDescriptor *ZrSystem_FsRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_FS_REGISTRY_H
