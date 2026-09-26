//
// Built-in zr.container native module registration.
//

#ifndef ZR_VM_LIB_CONTAINER_MODULE_H
#define ZR_VM_LIB_CONTAINER_MODULE_H

#include "zr_vm_lib_container/conf.h"

/** @brief 供容器热路径回归测试读取的进程级查找统计；只在调试构建中累计。 */
typedef struct ZrVmLibContainerDebugHotMapLookupStats {
    TZrUInt64 hotHitCount;
    TZrUInt64 memberVersionHitCount;
    TZrUInt64 entryValidationReadCount;
} ZrVmLibContainerDebugHotMapLookupStats;

/** @brief 返回 zr.container 的静态描述符，供注册表或插件加载器读取；调用方不拥有返回值。 */
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void);
/** @brief 返回 zr.pooling 的静态描述符；其类型引用 zr.container 的 Span 契约。 */
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVmLibContainer_GetPoolingModuleDescriptor(void);
/** @brief 按迭代模块、容器模块、池模块的依赖顺序注册，并为内建 array 安装迭代适配器。
 * @pre global 已创建主线程状态和内建类型原型。
 * @note 失败时本接口不回滚已注册的前序模块，调用方重试前须处理部分注册状态。
 */
ZR_VM_LIB_CONTAINER_API TZrBool ZrVmLibContainer_Register(SZrGlobalState *global);
/** @brief 重置热查找统计，供测试隔离测量区间。 */
ZR_VM_LIB_CONTAINER_API void ZrVmLibContainer_Debug_ResetHotMapLookupStats(void);
/** @brief 读取热查找统计，供测试判断缓存路径是否命中。 */
ZR_VM_LIB_CONTAINER_API ZrVmLibContainerDebugHotMapLookupStats ZrVmLibContainer_Debug_GetHotMapLookupStats(void);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 原生插件 ABI 入口；动态加载器从此取得 zr.container 描述符。 */
ZR_VM_LIB_CONTAINER_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_CONTAINER_MODULE_H
