/** @file
 * ## Summary
 * 宿主注册 `zr.math` 与共享库插件发现的入口；两条路径借用同一份模块描述符。
 * 描述符由各数值类型 registry 聚合，注册本身不执行数值 callback，也不产生导入后的 VM 模块对象。
 *
 * ## Constraints and lifecycle
 * 首次调用会写进程内静态表，调用方须串行完成首次初始化；普通布尔标志不提供同步。
 * 后续读取仍要求首次初始化与读取之间存在宿主同步。返回指针不转移所有权，且只在当前模块映像
 * 仍装载时有效；静态链接通常为进程期，共享库卸载后不得使用描述符及其字段、callback 指针。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 负责聚合；CLI 的 project 注册路径和
 * `native_binding_registry_plugin.c` 的动态符号查询是两类入口。
 */

#ifndef ZR_VM_LIB_MATH_MODULE_H
#define ZR_VM_LIB_MATH_MODULE_H

#include "zr_vm_lib_math/conf.h"

/**
 * ## Summary
 * 查询 `zr.math` 的只读描述符，供 native registry 或工具读取函数、类型、常量与提示表。
 * 首次查询完成静态聚合；本函数不把描述符注册到任一 VM。
 *
 * ## Parameters
 * 无参数，不依赖 `SZrState` 或 `SZrGlobalState`。
 *
 * ## Returns
 * @return 模块映像内静态描述符的借用指针；当前聚合路径不分配 VM 对象，没有 NULL 失败分支。
 * 调用方不得释放或修改描述符、其数组和字符串，也不得在共享库卸载后继续保存它们。
 *
 * ## Constraints and lifecycle
 * 首次初始化需由宿主串行化；初始化完成后只读。固定容量与 registry 总数须一起维护，
 * 追加 helper 无容量错误返回，当前数量和后续核查入口见实现中的 TODO。
 *
 * ## References
 * @see ZrVmLibMath_Register
 * @see ZrVm_GetNativeModule_v1
 * `zr_math_initialize_module_descriptor` 聚合各 registry；native registry 借用返回结果。
 */
ZR_API const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void);
/**
 * ## Summary
 * 将静态 `zr.math` 描述符交给指定 VM 的 native registry，供后续导入、反射与 callback 绑定。
 * CLI 和测试宿主在执行依赖 Math 的程序前调用；不同 VM 使用同一份描述符和各自的注册状态。
 *
 * ## Parameters
 * @param[in,out] global 仍存活的 VM 全局状态；native registry 可由下层 Attach 创建。
 * NULL 由下层拒绝，返回 false，不构成已注册状态。
 *
 * ## Returns
 * @return 下层 `ZrLibrary_NativeRegistry_RegisterModule` 的布尔结果。失败后本模块静态描述符
 * 仍已初始化；下层可能已完成 registry Attach，不能据 false 假定整个 VM 完全未变化。
 * 有效 registry 的错误信息由下层维护，NULL global 等早期拒绝不保证有可查询的错误。
 *
 * ## Constraints and lifecycle
 * 调用方负责首次静态初始化和该 VM 注册操作的串行化。registry 借用描述符、不接管模块映像，
 * 因此静态表和 callback 地址须覆盖该 VM 的使用期；本函数不负责 VM 创建、销毁或插件卸载。
 *
 * ## References
 * @see ZrVmLibMath_GetModuleDescriptor
 * @see ZrLibrary_NativeRegistry_RegisterModule
 * `native_registry_register_module_record` 执行 Attach、契约校验及同名注册处理；
 * `zr_vm_cli/src/zr_vm_cli/project/project.c` 在注册内建模块时调用本入口。
 */
ZR_API TZrBool ZrVmLibMath_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/**
 * ## Summary
 * 共享库插件协议按固定符号名查询的发现入口，只在 `ZR_LIBRARY_TYPE_SHARED` 配置下声明。
 *
 * ## Parameters
 * 无参数；动态加载器先取得库句柄，再把符号转换为 `FZrVmGetNativeModuleV1` 调用。
 *
 * ## Returns
 * @return 与 `ZrVmLibMath_GetModuleDescriptor` 相同的借用描述符，不执行 VM 注册。
 *
 * ## Constraints and lifecycle
 * 加载器须保持共享库装载，覆盖描述符、注册表及 callback 的全部借用期，并串行化首次发现。
 * 此入口不校验请求的模块名或 ABI，也不管理句柄；加载器取得描述符后负责这些检查。
 *
 * ## References
 * @see ZrVmLibMath_GetModuleDescriptor
 * `native_binding_registry_plugin.c` 查询 `ZrVm_GetNativeModule_v1`、调用后校验 ABI 与模块名。
 */
ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_MATH_MODULE_H
