//
// zr.system.vm native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_VM_H
#define ZR_VM_LIB_SYSTEM_VM_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 将当前 global 的已加载模块实例及其原生注册元数据投影为脚本数组，供运行中的程序检查模块状态。
 * 这里只遍历实例缓存；仅在注册表中登记、但尚未导入的描述符不会出现在结果里。
 *
 * ## Parameters
 * @param[in] context 由原生分派器建立的有效调用上下文；回调借用其中的 state，不取得其所有权。
 * @param[out] result 可写的结果槽。正常分派会在调用前置为 null；成功时由本回调写入新建数组。
 *
 * ## Returns
 * @return 成功创建并交付数组时为真；无效上下文、结果槽或数组创建失败时为假。返回假且线程状态仍正常时，分派器将结果保留为 null。
 *
 * ## Constraints and lifecycle
 * 注册元数为零，由 dispatcher 在进入回调前检查。数组及条目归 VM 管理；创建条目期间可分配并触发 GC。
 * `ZrLib_Array_PushValue` 的失败目前被忽略，条目可能遗漏而回调仍返回成功；另见实现中的风险记录。
 * TODO: 同一 global 允许多个 mutator 时，需沿 State_MutatorLaunch/Module_AddToCache 核实并发导入与
 * 此遍历是否由调用方串行化；当前尚未闭合同步协议，不提供已验证的并发快照保证。
 *
 * ## References
 * `ZrSystem_VmRegistry_GetModule` 注册此 callback；`ZrLibrary_NativeRegistry_GetModuleInfo` 补充描述符来源信息；
 * `ZrLib_Array_PushValue` 将条目写入返回数组。模块导入入口见 `zr_vm_lib_system/src/zr_vm_lib_system/module.c`。
 */
TZrBool ZrSystem_Vm_LoadedModules(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 生成 `SystemVmState` 值快照：模块数量来自所属 global，GC 指标来自共享 collector，栈和帧深度来自当前 state。
 *
 * ## Parameters
 * @param[in] context 由原生分派器建立的有效调用上下文；快照不接管 state。
 * @param[out] result 可写的结果槽。正常分派会先置为 null，成功时接收 VM 管理的新对象。
 *
 * ## Returns
 * @return 对象分配成功并交付时为真；无效上下文、结果槽或对象创建失败时为假。字段由 void setter 写入，字段失败不改变此状态码。
 *
 * ## Constraints and lifecycle
 * 注册元数为零。读取值来自不同的当前状态位置，随后对象和字段物化可能分配或触发 GC；此接口不提供跨字段原子快照。
 * 字段写入可能因软分配失败而被静默跳过，调用者不能把返回真解释为所有字段均成功写入。
 * TODO: 同 global 的其他 mutator 可能导入模块或调整 GC；沿 State_MutatorLaunch/Module_AddToCache
 * 及 collector 写路径核实遍历和普通字段读取的同步协议；非原子快照说明不能代替该核查。
 *
 * ## References
 * `ZrSystem_VmRegistry_GetModule` 声明 `SystemVmState` 及其零参函数；实现由 `system_vm_make_state` 组装当前线程与 global 数据。
 */
TZrBool ZrSystem_Vm_State(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按名称查找并调用目标模块的公开导出，将脚本数组中的元素作为该导出的参数；用于需要动态选择模块或导出的调用方。
 *
 * ## Parameters
 * @param[in] context 由 dispatcher 建立的有效调用上下文；VM 参数按注册顺序为模块名字符串、导出名字符串和参数数组。
 * @param[out] result 可写的结果槽。正常分派先置为 null；目标调用成功时接收目标导出的 VM 值。
 *
 * ## Returns
 * @return 读取参数并完成目标调用时为真；普通参数读取、临时参数缓冲区或目标调用失败时为假。类型不匹配由 ReadString/ReadArray 的 VM 异常路径报告。
 *
 * ## Constraints and lifecycle
 * 注册元数恰为三，dispatcher 在回调前检查。导出查询可触发模块导入，目标函数可执行任意 VM 副作用；临时 C 参数数组只在本回调期间存在。
 * TODO: 导入/目标查找可在共享 helper 安装局部 panic handler 前分配或跳转；需核实浅拷贝参数和两个借用字符串在该窗口的 GC/非局部退出安全性。
 *
 * ## References
 * `ZrSystem_VmRegistry_GetModule` 注册该入口；回调通过 `ZrLib_CallModuleExport` 复用通用模块导出调用流程。
 */
TZrBool ZrSystem_Vm_CallModuleExport(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_VM_H
