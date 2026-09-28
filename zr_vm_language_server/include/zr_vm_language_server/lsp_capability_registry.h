/**
 * @file
 * @brief 供传输适配层和能力一致性探针共享的 LSP 能力契约。
 * @note 描述符记录协议键与入口名称，并不负责执行入口或自动发布 provider。
 */

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CAPABILITY_REGISTRY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CAPABILITY_REGISTRY_H

#include "zr_vm_language_server/conf.h"

/** @brief 区分 stdio native 与 Web/WASM 可用性；描述符可组合这两个位。 */
typedef enum EZrLspRuntimeMask {
    ZR_LSP_RUNTIME_NATIVE = 1U,
    ZR_LSP_RUNTIME_WASM = 2U,
} EZrLspRuntimeMask;

/** @brief resolve 必须补充实际内容，原样返回不能作为可发布的 resolve 能力。 */
typedef enum EZrLspCapabilityResolveBehavior {
    ZR_LSP_CAPABILITY_RESOLVE_NONE = 0,
    ZR_LSP_CAPABILITY_RESOLVE_MATERIAL,
    ZR_LSP_CAPABILITY_RESOLVE_IDENTITY,
} EZrLspCapabilityResolveBehavior;

/** @brief 标明能力由共享核心实现，还是只由 native 传输适配器实现。 */
typedef enum EZrLspCapabilityImplementationLayer {
    ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE = 1,
    ZR_LSP_CAPABILITY_IMPLEMENTATION_NATIVE_ADAPTER,
} EZrLspCapabilityImplementationLayer;

/**
 * @brief 为能力清单、native 初始化协商和一致性测试提供同一份元数据。
 * @note 各字符串均为借用的静态名称，表示协议方法、代码入口和测试标识，
 *       并非可调用函数指针；runtimeMask 描述基础能力，resolveRuntimeMask 单独描述 resolve。
 * @note native 适配层能力须为 native-only 且无 coreEntryPoint；结构体校验不证明入口实际存在。
 */
typedef struct SZrLspCapabilityDescriptor {
    const TZrChar *capabilityKey;
    const TZrChar *method;
    const TZrChar *clientCapabilityPath;
    const TZrChar *coreEntryPoint;
    const TZrChar *nativeAdapter;
    const TZrChar *wasmExport;
    const TZrChar *testId;
    TZrUInt32 runtimeMask;
    TZrUInt16 minimumMajor;
    TZrUInt16 minimumMinor;
    TZrBool hasResolve;
    TZrBool isExperimental;
    EZrLspCapabilityResolveBehavior resolveBehavior;
    /** resolve 的运行时覆盖可能小于基础 provider 的覆盖。 */
    TZrUInt32 resolveRuntimeMask;
    /** 用于约束 coreEntryPoint 与运行时导出的组合。 */
    EZrLspCapabilityImplementationLayer implementationLayer;
} SZrLspCapabilityDescriptor;

/** @brief 返回进程内静态能力表的条目数，供清单探针完整枚举。 */
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_LspCapabilityRegistry_Count(void);
/** @brief 按静态顺序借用一个描述符；越界返回空指针，调用方不得修改或释放。 */
ZR_LANGUAGE_SERVER_API const SZrLspCapabilityDescriptor *
ZrLanguageServer_LspCapabilityRegistry_At(TZrSize index);
/** @brief 按协议能力键借用描述符，供初始化协商和测试定位同一契约。 */
ZR_LANGUAGE_SERVER_API const SZrLspCapabilityDescriptor *
ZrLanguageServer_LspCapabilityRegistry_Find(const TZrChar *capabilityKey);
/**
 * @brief 校验描述符的入口归属、运行时掩码、resolve 元数据等内部约束。
 * @return 元数据自洽时为真；空指针或不完整的条目为假。
 * @note 只检查元数据形状，不验证字符串命名的入口确实可执行。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(
        const SZrLspCapabilityDescriptor *descriptor);
/**
 * @brief 在元数据校验后施加对 resolve 行为和实验性协议能力的发布策略。
 * @note 返回真仅说明条目符合静态发布约束，实际 initialize 响应仍由各传输层构造。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(
        const SZrLspCapabilityDescriptor *descriptor);
/**
 * @brief 供 native initialize 等调用方判定指定后端是否可宣告 resolveProvider。
 * @pre runtime 必须是单个 native 或 WASM 枚举值；其他值返回假。
 * @note 基础 provider 的 runtimeMask 不等于 resolveRuntimeMask，不能据前者推断后者。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
        const TZrChar *capabilityKey, EZrLspRuntimeMask runtime);

#endif
