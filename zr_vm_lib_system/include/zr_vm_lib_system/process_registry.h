/**
 * @file process_registry.h
 * @brief 声明 `zr.system.process` 静态模块描述符的取得入口。
 * @details 描述符由系统聚合注册器登记；数组、字符串和 callback pointer 均为只读静态借用数据。
 * @see process.h; zr_vm_lib_system/src/zr_vm_lib_system/module.c
 */

#ifndef ZR_VM_LIB_SYSTEM_PROCESS_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_PROCESS_REGISTRY_H

#include "zr_vm_lib_system/process.h"

/**
 * ## Summary
 * 在系统叶模块聚合注册阶段提供 `zr.system.process` 的原生模块描述符，供原生注册表登记该模块。
 *
 * ## Parameters
 * 无参数；模块选择由系统聚合注册器在注册阶段完成。
 *
 * ## Returns
 * @return 返回指向函数内静态 `const` 描述符的借用指针；始终非空，不分配、不转移所有权，调用方
 *         不得释放或修改。描述符所引用的常量、回调、提示和 JSON 表同样具有静态存储期。
 *
 * ## Constraints and lifecycle
 * descriptor 及其常量、回调、提示和 JSON 表只在提供者模块映像存续期内有效；注册方须保持映像驻留，
 * 直至注册记录结束借用；调用方不得修改或释放。`arguments` 默认 VM 数组由物化器创建；CLI 项目、inline 与 REPL
 * 入口会在执行前注入入口标识和用户参数，非 CLI embedder 不会由此函数自动注入。
 *
 * ## References
 * `ZrVmLibSystem_Register` 通过本接口取得 descriptor；原生注册表在
 * `native_binding_support.c` 中保存借用指针。
 */
const ZrLibModuleDescriptor *ZrSystem_ProcessRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_PROCESS_REGISTRY_H
