#include "zr_vm_lib_testing/module.h"

#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_registry.h"
#include "runtime/runtime_internal.h"

/**
 * ## Summary
 * 公开查询Testing的静态签名，供宿主核查契约、注册和插件转交。
 *
 * ## Returns
 * @return 非NULL的provider所有的const描述表；调用方不释放，其嵌套数组/字符串亦为借用。
 *
 * ## Constraints and lifecycle
 * 无需GlobalState；查询不执行Register，不登记zr.task，也不修改Test phase。
 * 读取可用不代表当前host能物化TEST provider；provider须活到最后一次注册/closure消费。
 *
 * ## References
 * tests/testing/test_assertions.c:59 和 tests/library/test_official_provider_convergence.c:415 核查元数据；
 * zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:52 注册、zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:77 插件查询使用同一表。
 */
const ZrLibModuleDescriptor *ZrVmLibTesting_GetModuleDescriptor(void) {
    return ZrVmLibTesting_Runtime_GetModuleDescriptor();
}

/**
 * ## Summary
 * 为直接嵌入宿主建立测试provider的依赖顺序：先规范task，再native registry，最后Testing表。
 *
 * ## Parameters
 * @param[in,out] global 已构造的宿主VM全局状态；NULL安全拒绝。调用依赖接口会建立或复用它的注册状态，所有权仍属宿主。
 *
 * ## Returns
 * @return NULL global或任一步以bool报告失败时返回false；true表示三步均成功，不表示TEST phase已启用或测试已执行。
 *
 * ## Constraints and lifecycle
 * TaskRuntime_RegisterBuiltins自身已按需Attach，后续Attach可以复用registry；显式保留顺序供直接C宿主调用。
 * 短路不回滚前序task/registry登记；CLI初始化失败会释放整个新global，直接调用者须管理原global的残留状态。
 * 注册与模块消费分离；宿主在成功后另设TEST phase，不在此执行action或创建Task实例。
 * TODO: bool短路不捕获依赖内部异常或初始化缺陷；沿zr_vm_library/src/zr_vm_library/native_binding/native_binding.c:109 的既有Array_Init失败问题核查OOM传播。
 * TODO: tests/testing/test_assertions.c:25 的helper在Register false时先触发TEST_ASSERT跳出，未先Destroy；沿测试失败清理入口补核。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/commands/test_command.c:83 bootstrap调用，zr_vm_cli/src/zr_vm_cli/commands/test_command.c:98 清理失败global，zr_vm_cli/src/zr_vm_cli/commands/test_command.c:101 后置phase；
 * zr_vm_library/src/zr_vm_library/task_runtime.c:1389 与 zr_vm_library/src/zr_vm_library/task_runtime.c:1393 先建立task provider；tests/testing/test_assertions.c:25 直接测试宿主调用。
 */
TZrBool ZrVmLibTesting_Register(SZrGlobalState *global) {
    return global != ZR_NULL &&
           ZrCore_TaskRuntime_RegisterBuiltins(global) &&
           ZrLibrary_NativeRegistry_Attach(global) &&
           ZrLibrary_NativeRegistry_RegisterModule(
                   global, ZrVmLibTesting_GetModuleDescriptor());
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
/**
 * ## Summary
 * 满足插件加载器的固定v1查询符号，只交付静态provider表，不执行直接宿主的注册流程。
 *
 * ## Returns
 * @return 与公开getter同一const借用描述表；插件加载器另查ABI/moduleName并保存相关provider消费期。
 *
 * ## Constraints and lifecycle
 * 仅在定义ZR_LIBRARY_TYPE_SHARED时编译；当前CommonMacros共享目标将其定义为1，静态目标不定义它。
 * TODO: 自定义SHARED=0仍通过defined guard，但ZR_API导出条件按数值判断；沿自定义配置和Windows符号表核查是否需统一门禁。
 * 调用此入口不注册zr.task或切TEST phase；插件宿主须独立满足链接解析与消费phase。
 * BUG: 上游loader先登记借用descriptor再复制handle归属字符串；复制失败关闭DLL但未撤回module record，留下悬空借用。
 * 入口 zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:438 与 zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:462 已记录此加载层错误；本getter不修复它，未做运行时故障注入。
 * TODO: invalidation按module ownerRefCount门禁却未清closure绑定；沿保留export闭包后释放module再invalidate的路径核查DLL消费寿命。
 * 继续从 zr_vm_library/src/zr_vm_library/native_binding/native_binding.c:647、zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:186 及LSP调用 zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c:1402 核查。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:370 查符号，zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:383 实际调用，zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:396/zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:410 校验；
 * zr_vm_common/CommonMacros.cmake:40 共享定义和 zr_vm_common/include/zr_vm_common/zr_api_conf.h:14 导出条件。
 */
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibTesting_GetModuleDescriptor();
}
#endif
