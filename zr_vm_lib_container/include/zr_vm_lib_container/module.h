/**
 * @file
 * ## Summary
 * 向宿主公开 zr.container、zr.pooling 的静态描述符、依赖注册入口及热 Map 查找统计。
 * 描述符查询用于 provider/协议检查；Register 才改变 VM 注册状态并安装内建 array 迭代适配。
 *
 * ## Constraints and lifecycle
 * 描述符及其中表、字符串和回调由当前 provider 持有，调用方借用且不得修改或释放；
 * 注册后的 VM 使用回调期间 provider 必须保持加载。统计属于进程，而非某个容器或 VM。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/project/project.c:368 在标准模块启动链中调用 Register；
 * tests/library/test_official_provider_convergence.c:401 和 tests/library/test_official_provider_convergence.c:403 查询两种 provider；
 * docs/library-and-builtins/zr-container-module-runtime.md 记录注册顺序、失败后的状态和描述符协议。
 */
//
// Built-in zr.container native module registration.
//

#ifndef ZR_VM_LIB_CONTAINER_MODULE_H
#define ZR_VM_LIB_CONTAINER_MODULE_H

#include "zr_vm_lib_container/conf.h"

/**
 * ## Summary
 * 按值交付热 Map 查找的诊断计数，供回归测试区分缓存命中与普通扫描。
 * 这是进程统计的快照，不是容器状态、GC 根或对某次查找结果的正确性证明。
 *
 * ## Fields
 * hotHitCount 与 memberVersionHitCount 在同一成功提交点一起增加；
 * entryValidationReadCount 当前没有计数写入，保留为测试可观察的零值。
 *
 * ## Invariants and lifecycle
 * ZR_DEBUG 下 Reset 开始新的计数窗口，Get 复制当前三个数；非 DEBUG 的 Get 返回全零。
 * 快照本身不持有 VM 对象，调用方可按普通值保存。读取与计数更新没有锁或原子快照协议。
 * TODO: 多线程或多个 GlobalState 交错测量时的统计同步与窗口隔离需核对；入口为
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1154 和 tests/container/test_container_runtime.c:1535。
 *
 * ## References
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:457 保存过程计数，
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:1154 与 zr_vm_lib_container/src/zr_vm_lib_container/module.c:1155 一起更新；
 * tests/container/test_container_runtime.c:1539 读取快照，tests/container/test_container_runtime.c:1541 至 tests/container/test_container_runtime.c:1547 检查构建分支。
 */
typedef struct ZrVmLibContainerDebugHotMapLookupStats {
/**
 * 成功返回 Map 字符串键热缓存位置的累计次数；仅 ZR_DEBUG 累计，Reset 清零。
 * 不是 Map 查询总次数，也不能单凭该值证明返回元素正确。
 */
    TZrUInt64 hotHitCount;
/**
 * 当前与 hotHitCount 在同一命中提交点递增，表示版本匹配路径的计数；
 * 两数相同是现有计数方式，不代表执行了额外的 Pair.first 内容验证。
 */
    TZrUInt64 memberVersionHitCount;
/**
 * 保留的 entry 验证读取计数；当前模块没有递增它，测试要求为零。
 * 不能把零值解释为已经验证所有缓存 entry，或解释为从未访问容器元素。
 */
    TZrUInt64 entryValidationReadCount;
} ZrVmLibContainerDebugHotMapLookupStats;

/**
 * ## Summary
 * 借出 zr.container 的 provider 描述符，供宿主检查类型、泛型、字段及迭代/Span 协议。
 * 此查询不注册模块、不创建容器，也不安装内建 array 的 getIterator 适配。
 *
 * ## Returns
 * @return 当前 provider 持有的 const 静态描述符；当前实现直接返回非 NULL 地址。
 *        调用方借用其嵌套表、字符串和回调，不修改或释放。
 *
 * ## Constraints and lifecycle
 * 无需先创建 VM；描述符可独立用于协议检查。provider 必须存活到最后一个描述符或
 * 回调消费者结束。需要在 VM 中使用整个依赖链时，另调用 ZrVmLibContainer_Register。
 *
 * ## References
 * tests/iterator/test_enumerator_protocol.c:132 查询容器类型的迭代协议；
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5552 保存静态描述符，zr_vm_lib_container/src/zr_vm_lib_container/module.c:5575 返回其地址。
 */
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void);
/**
 * ## Summary
 * 借出 zr.pooling 的 provider 描述符，供宿主检查 BufferPool 与稳定槽池的类型契约。
 * 该入口仅转交 pooling provider；不会创建池、登记模块或代为加载 zr.container。
 *
 * ## Returns
 * @return pooling provider 持有的 const 静态描述符；当前实现返回非 NULL 地址。
 *        返回对象与嵌套元数据仅借用，不由调用方释放。
 *
 * ## Constraints and lifecycle
 * 查询本身无需 VM。pooling 的模块链接引用 zr.container 的 Span/ReadOnlySpan，
 * 在 VM 中使用池前须满足该依赖；Register 负责按依赖顺序登记。provider 回调仍被使用时不可卸载。
 *
 * ## References
 * tests/container/test_pooling_closed_type_runtime.c:21 从描述符找稳定槽回调角色；
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5580 转交查询，
 * zr_vm_lib_container/src/zr_vm_lib_container/pooling.c:928 声明 container 链接，zr_vm_lib_container/src/zr_vm_lib_container/pooling.c:950 返回静态表。
 */
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVmLibContainer_GetPoolingModuleDescriptor(void);
/**
 * ## Summary
 * 在宿主启动链中登记 iteration、container、pooling，并为基本 ARRAY 原型安装迭代桥接。
 * 顺序使容器先获得迭代协议，池模块再获得 Span 契约；描述符查询不承担这些副作用。
 *
 * ## Parameters
 * @param[in,out] global 宿主拥有的有效 VM 域；主线程状态和基本 ARRAY 原型须已建立。
 *                      登记链按需附着 native registry；NULL 正常返回 ZR_FALSE，不取得或释放 global。
 *
 * ## Returns
 * @return global 为 NULL 或任一阶段报告失败时为 ZR_FALSE；四个阶段均报告成功时为 ZR_TRUE。
 *        失败保留此前成功的登记/安装，调用方应处理部分状态，不能当成未曾注册。
 *
 * ## Constraints and lifecycle
 * @pre global 的主线程与基本类型原型已建立；注册与执行的域同步由宿主协调。
 * 成功报告不额外保证当前同名 getIterator 字段可调用；已有任意同名字段都会被视为已安装。
 * TODO: 该 presence 策略需结合宿主预置同名非闭包字段核对；入口为
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5604。
 * BUG: 合法宿主分配器在原型未 ignored、ignore 表已满且原先无 getIterator 时，
 * 连续拒绝 getter 与 setter 的两次 ignore 表扩容、允许中间闭包申请，安装仍可报告成功
 * 而字段缺失。void setter 无成功凭据；完整静态调用路径见模块文档，尚未进行故障注入验证。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/project/project.c:368 传播返回值；tests/container/container_test_common.c:60 的 state 工厂忽略该值；
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5667 至 zr_vm_lib_container/src/zr_vm_lib_container/module.c:5671 建立依赖顺序，
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5617 调用 void setter，zr_vm_lib_container/src/zr_vm_lib_container/module.c:5622 仍返回成功；
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385 与 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1429 是两次固定失败入口；
 * docs/library-and-builtins/zr-container-module-runtime.md 记录合法双失败条件及部分登记边界。
 */
ZR_VM_LIB_CONTAINER_API TZrBool ZrVmLibContainer_Register(SZrGlobalState *global);
/**
 * ## Summary
 * 为热缓存回归测试开始新的进程计数窗口，使随后执行脚本的命中次数可单独观察。
 * 它仅重置诊断计数，不清空真正的字段字符串或 Map 查找缓存。
 *
 * ## Returns
 * 无返回值；ZR_DEBUG 下三个计数归零，非 DEBUG 下不产生动作。
 *
 * ## Constraints and lifecycle
 * 不要求传入或初始化 VM。调用方需隔离测量窗口；此入口不暂停其它执行线程，
 * 与并行计数更新的同步限制沿用 ZrVmLibContainerDebugHotMapLookupStats 的 TODO。
 *
 * ## References
 * tests/container/test_container_runtime.c:1535 在待测脚本执行前重置；
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:521 仅清除计数结构。
 */
ZR_VM_LIB_CONTAINER_API void ZrVmLibContainer_Debug_ResetHotMapLookupStats(void);
/**
 * ## Summary
 * 向诊断调用者交付当前进程热 Map 查找计数快照，供已隔离的测试窗口判断是否命中。
 *
 * ## Returns
 * @return 按值复制的三个整数，不携带对象所有权。ZR_DEBUG 返回当前过程计数，
 *        非 DEBUG 返回全零；读取本身不重置计数或改变缓存。
 *
 * ## Constraints and lifecycle
 * 结果作为普通 C 值保存即可，不依赖 VM 对象寿命。没有多线程一致性快照保证；
 * 统计不能替代脚本结果检查，entryValidationReadCount 当前为零也不能证明查询结果正确。
 *
 * ## References
 * tests/container/test_container_runtime.c:1539 在脚本执行后查询；
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:544 复制 DEBUG 计数，zr_vm_lib_container/src/zr_vm_lib_container/module.c:546 构造非 DEBUG 零值。
 */
ZR_VM_LIB_CONTAINER_API ZrVmLibContainerDebugHotMapLookupStats ZrVmLibContainer_Debug_GetHotMapLookupStats(void);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/**
 * ## Summary
 * 以原生插件 v1 的固定导出符号向动态加载器提供 zr.container 描述符。
 * 它等价于描述符查询，不执行宿主 Register 的依赖登记或基本 ARRAY 适配步骤。
 *
 * ## Returns
 * @return 当前 provider 的 const 静态描述符，当前实现非 NULL；加载器只借用其数据和回调。
 *
 * ## Constraints and lifecycle
 * 仅 ZR_LIBRARY_TYPE_SHARED 构建公开此入口；ABI 名称须保持与加载器查找约定一致。
 * 加载器读取并验证 ABI 后才登记 provider；共享库须保持加载到全部描述符/回调消费者结束。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:370 查找固定符号，
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:383 调用它；
 * zr_vm_lib_container/src/zr_vm_lib_container/module.c:5677 转交描述符查询。
 */
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_CONTAINER_MODULE_H
