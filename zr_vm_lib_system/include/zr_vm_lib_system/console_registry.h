//
// zr.system.console descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_CONSOLE_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_CONSOLE_REGISTRY_H

#include "zr_vm_lib_system/console.h"

/**
 * ## Summary
 * 返回 `zr.system.console` 的静态模块描述符，列出四个日志输出 callback、两个 stdin callback 及工具类型提示。
 *
 * ## Parameters
 * 无参数；getter 不读取脚本状态、不创建模块副本。
 *
 * ## Returns
 * @return 指向模块内静态只读描述符的借用指针；调用者不得释放或修改。
 *
 * ## Constraints and lifecycle
 * 描述符、函数/提示表、字符串及 callback 地址都位于 provider image，并可被注册器浅拷贝或长期保留；provider image
 * 必须驻留至注册器结束全部嵌套指针和 callback 借用。浅拷贝 descriptor 不延长 image 寿命。函数表规定 4 个输出各需一个
 * 参数、2 个输入不接收参数；dispatcher 在 callback 前检查元数。类型提示用于静态工具投影，不取代运行时 descriptor。
 *
 * ## References
 * `ZrVmLibSystem_Register` 经系统模块注册表调用本 getter；`native_binding_registry_plugin.c` 消费 descriptor，
 * `native_binding_dispatch_lanes.h` 经注册函数指针进入 callback。
 */
const ZrLibModuleDescriptor *ZrSystem_ConsoleRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_CONSOLE_REGISTRY_H
