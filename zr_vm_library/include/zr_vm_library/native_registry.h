//
// Native module registry for built-in and plugin-backed descriptors.
//

#ifndef ZR_VM_LIBRARY_NATIVE_REGISTRY_H
#define ZR_VM_LIBRARY_NATIVE_REGISTRY_H

#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/call_binding.h"

struct SZrClosureNative;

/** @brief 区分静态描述符与可卸载插件，以决定失效时的资源处理。 */
typedef enum EZrLibNativeModuleRegistrationKind {
    ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_BUILTIN = 0,
    ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN = 1
} EZrLibNativeModuleRegistrationKind;

/** @brief 最近一次注册或加载操作的诊断类别；消息存于注册表内部。 */
typedef enum EZrLibNativeRegistryErrorCode {
    ZR_LIB_NATIVE_REGISTRY_ERROR_NONE = 0,
    ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD = 1,
    ZR_LIB_NATIVE_REGISTRY_ERROR_SYMBOL = 2,
    ZR_LIB_NATIVE_REGISTRY_ERROR_ABI_MISMATCH = 3,
    ZR_LIB_NATIVE_REGISTRY_ERROR_VERSION_MISMATCH = 4,
    ZR_LIB_NATIVE_REGISTRY_ERROR_CAPABILITY_MISMATCH = 5,
    ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_NAME_MISMATCH = 6,
    ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE = 7,
    ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH = 8,
    ZR_LIB_NATIVE_REGISTRY_ERROR_RESERVED_OFFICIAL_MODULE = 9,
    ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_OFFICIAL_PROVIDER = 10,
    ZR_LIB_NATIVE_REGISTRY_ERROR_PROVIDER_CONTRACT_MISMATCH = 11,
    ZR_LIB_NATIVE_REGISTRY_ERROR_INVALID_CANONICAL_TYPE_ROLE = 12,
    ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_PROVIDER_CONTRACT = 13
} EZrLibNativeRegistryErrorCode;

/** @brief 官方模块的分层清单，供提供者阶段与依赖检查。 */
typedef enum EZrLibOfficialModuleTier {
    ZR_LIB_OFFICIAL_MODULE_TIER_N0 = 0,
    ZR_LIB_OFFICIAL_MODULE_TIER_N1 = 1,
    ZR_LIB_OFFICIAL_MODULE_TIER_N2 = 2,
    ZR_LIB_OFFICIAL_MODULE_TIER_N3 = 3
} EZrLibOfficialModuleTier;

/** @brief 官方模块固定身份、层级和提供者角色；注册时拒绝冒名插件。 */
typedef struct ZrLibOfficialModuleInventoryEntry {
    const TZrChar *moduleName;
    EZrLibOfficialModuleTier tier;
    EZrLibrary_ProviderPhase phase;
    EZrProviderContractRole providerContractRole;
} ZrLibOfficialModuleInventoryEntry;

/** @brief 按角色查询得到的借用结果；提供者失效后不可继续使用。 */
typedef struct ZrLibRegisteredCanonicalTypeRole {
    const ZrLibModuleDescriptor *provider;
    const ZrLibCanonicalTypeRoleDescriptor *typeRole;
} ZrLibRegisteredCanonicalTypeRole;

/** @brief 注册模块的只读快照；指针字段均借用注册表存储，失效后需重查。 */
typedef struct ZrLibRegisteredModuleInfo {
    const ZrLibModuleDescriptor *descriptor;
    const TZrChar *moduleName;
    const TZrChar *sourcePath;
    EZrLibNativeModuleRegistrationKind registrationKind;
    TZrBool isDescriptorPlugin;
    TZrUInt32 ownerRefCount;
} ZrLibRegisteredModuleInfo;

/** @brief 为全局 VM 安装原生模块加载、绑定和强引用观察钩子，并注册基础提供者。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_Attach(SZrGlobalState *global);
/** @brief 释放注册表及插件句柄，恢复安装前的宿主钩子；应在全局 VM 销毁时调用。 */
ZR_LIBRARY_API void ZrLibrary_NativeRegistry_Free(SZrGlobalState *global);
/** @brief 注册静态 Native 模块；描述符及其表由调用者持续持有。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_RegisterModule(SZrGlobalState *global,
                                                               const ZrLibModuleDescriptor *descriptor);
/** @brief 按模块名借用已注册描述符，供编译器、加载器及测试查询。 */
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModule(SZrGlobalState *global,
                                                                                const TZrChar *moduleName);
/** @brief 按唯一提供者角色借用描述符，避免静态类型系统依赖模块别名。 */
ZR_LIBRARY_API const ZrLibModuleDescriptor *ZrLibrary_NativeRegistry_FindModuleByProviderRole(
        SZrGlobalState *global,
        EZrProviderContractRole providerRole);
/** @brief 按稳定类型角色查询提供者与类型声明；失败时清空输出。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRole(
        SZrGlobalState *global,
        EZrCanonicalTypeRole role,
        ZrLibRegisteredCanonicalTypeRole *outRole);
/** @brief 按规范类型名查询角色，供反射与类型推断复用。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByName(
        SZrGlobalState *global,
        const TZrChar *canonicalName,
        ZrLibRegisteredCanonicalTypeRole *outRole);
/** @brief 在指定提供者中按投影类别查唯一角色；歧义时失败。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_FindCanonicalTypeRoleByProjection(
        SZrGlobalState *global,
        EZrProviderContractRole providerRole,
        EZrCanonicalTypeProjectionKind projectionKind,
        ZrLibRegisteredCanonicalTypeRole *outRole);
/** @brief 在模块注册前验证官方身份、ABI、能力位及参数/类型角色契约。
 * @note BUG: 目前未检查类型 fields/fieldCount 等数组配对；通过本检查不保证可安全物化。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_ValidateModuleDescriptor(
        SZrGlobalState *global,
        const ZrLibModuleDescriptor *descriptor);
/** @brief 按模块名取得注册记录的借用视图；失败时清空输出。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfo(SZrGlobalState *global,
                                                              const TZrChar *moduleName,
                                                              ZrLibRegisteredModuleInfo *outInfo);
/** @brief 返回注册表当前记录数，供工具链枚举。 */
ZR_LIBRARY_API TZrSize ZrLibrary_NativeRegistry_GetModuleCount(SZrGlobalState *global);
/** @brief 查询模块对象的强引用计数，供插件失效前检查。 */
ZR_LIBRARY_API TZrUInt32 ZrLibrary_NativeRegistry_GetModuleRefCount(SZrGlobalState *global,
                                                                    const TZrChar *moduleName);
/** @brief 按当前索引取得注册记录；注册表变更后索引可能失效。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfoAt(SZrGlobalState *global,
                                                                TZrSize index,
                                                                ZrLibRegisteredModuleInfo *outInfo);
/** @brief 按插件来源路径查注册记录；只借用返回的字符串和描述符。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(SZrGlobalState *global,
                                                                          const TZrChar *sourcePath,
                                                                          ZrLibRegisteredModuleInfo *outInfo);
/** @brief 计算编译器调用绑定使用的稳定提供者签名，而非进程内指针身份。 */
ZR_LIBRARY_API TZrUInt64 ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(
        const ZrLibModuleDescriptor *descriptor);
/** @brief 将编译产物中的调用契约解析到当前注册的 Native 闭包。 */
ZR_LIBRARY_API EZrCallBindingStatus ZrLibrary_NativeRegistry_ResolveCallBinding(
        SZrState *state,
        const SZrCallBindingContract *contract,
        SZrCallBindingTarget *outTarget,
        SZrCallBindingDiagnostic *diagnostic);
/** @brief 从运行时闭包提取编译器可复核的调用契约身份。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_GetCallBindingIdentity(
        SZrGlobalState *global,
        struct SZrClosureNative *closure,
        SZrCallBindingContract *outContract);
/** @brief 按项目目录加载所需描述符插件，供编译期发现其公共表面。 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(SZrState *state,
                                                                              const TZrChar *projectDirectory,
                                                                              const TZrChar *moduleName);
/** @brief 清除描述符插件缓存和句柄；有模块强引用时拒绝卸载。
 * @note TODO: 当前实现匹配任一插件后会清空所有插件记录；需核实多插件热重载是否要求按 sourcePath 精确失效。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(SZrGlobalState *global,
                                                                                 const TZrChar *sourcePath);
/** @brief 返回最近一次注册或插件加载失败的错误类别。 */
ZR_LIBRARY_API EZrLibNativeRegistryErrorCode ZrLibrary_NativeRegistry_GetLastErrorCode(SZrGlobalState *global);
/** @brief 借用最近错误消息；下一次注册或卸载操作可能覆盖。 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeRegistry_GetLastErrorMessage(SZrGlobalState *global);
/** @brief 设置当前线程编译工具或运行时提供者阶段，供导入门禁使用。 */
ZR_LIBRARY_API void ZrLibrary_State_SetProviderPhase(SZrState *state,
                                                     EZrLibrary_ProviderPhase phase);
/** @brief 查询线程提供者阶段；无效状态默认运行时阶段。 */
ZR_LIBRARY_API EZrLibrary_ProviderPhase ZrLibrary_State_GetProviderPhase(const SZrState *state);
/** @brief 只允许运行时提供者或与宿主阶段一致的提供者被消费。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ProviderPhase_CanConsume(
        EZrLibrary_ProviderPhase hostPhase,
        EZrLibrary_ProviderPhase providerPhase);
/** @brief 返回官方模块固定清单长度，供注册检查及工具枚举。 */
ZR_LIBRARY_API TZrSize ZrLibrary_OfficialModuleInventory_GetCount(void);
/** @brief 按清单索引借用官方模块身份。 */
ZR_LIBRARY_API const ZrLibOfficialModuleInventoryEntry *ZrLibrary_OfficialModuleInventory_GetAt(
        TZrSize index);
/** @brief 按模块名查官方身份，供插件拒绝冒名及层级约束。 */
ZR_LIBRARY_API const ZrLibOfficialModuleInventoryEntry *ZrLibrary_OfficialModuleInventory_Find(
        const TZrChar *moduleName);

#endif // ZR_VM_LIBRARY_NATIVE_REGISTRY_H
