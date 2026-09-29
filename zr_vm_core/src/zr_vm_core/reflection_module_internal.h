#ifndef ZR_VM_REFLECTION_MODULE_INTERNAL_H
#define ZR_VM_REFLECTION_MODULE_INTERNAL_H

#include "zr_vm_core/reflection.h"

/* 四个公开导出键由服务模块构造与缓存完整性校验共用。 */
#define ZR_REFLECTION_MAKE_GENERIC_METHOD_EXPORT "MakeGenericMethod" /* 绑定泛型方法构造入口。 */
#define ZR_REFLECTION_RESOLVE_TYPE_ID_EXPORT "resolve" /* 解析已认证的 TypeId。 */
#define ZR_REFLECTION_REQUIRE_CONSTRUCTIBLE_EXPORT "requireConstructible" /* 校验构造能力。 */
#define ZR_REFLECTION_CREATE_INSTANCE_EXPORT "createInstance" /* 执行反射实例构造。 */
/* 按目标 runtime module 缓存服务模块的 protected 属性键。 */
#define ZR_REFLECTION_SERVICE_MODULE_CACHE_NAME "__zr_reflection_service_module" /* 目标 module 的 protected-cache 键。 */

/**
 * @brief 按全局 reflection provider 名称创建独立服务 module，四个 native closure 捕获目标 runtime。
 * @pre state、runtime 及 runtime->module 有效，module 反向持有该 runtime，且双方位于同一 GC domain。
 * @param pinRuntimeModule 成功后是否为目标 runtime module 建立持久 native-handle pin。
 * @return 四个公开导出通过校验时返回 READY module，显式失败返回 null；分配失败可由 VM 异常非局部传播。
 * @note 服务 module 不附着 MetadataRuntime；返回 module 本身不由该 pin 固定，调用方须在下次 GC 前 root。
 */
struct SZrObjectModule *ZrCore_Reflection_CreateModuleForRuntimeInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
