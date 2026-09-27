#ifndef ZR_VM_CORE_HOTPATCH_CAPABILITY_H
#define ZR_VM_CORE_HOTPATCH_CAPABILITY_H

#include "zr_vm_core/capability_manifest.h"

/* 独立闭包入口只核对需求与 host 授权；身份和签名由主验证器承担。 */
#define ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS ((TZrUInt32)4096u)

/** @brief 先拒绝未知 manifest flag，再统一遍历逐 token 能力需求。
 * @note 不验证 artifact 身份或签名；发布前仍须调用主验证入口。 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_ValidateCapabilityClosure(
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic);

/** @brief 汇总 manifest 总需求与逐 token 需求，同时检查每项均在 host 授权内。
 * @return 成功才写出闭包；失败时可选输出为零，diagnostic 记录首个拒绝原因，
 * 逐 token 需求失败时才携带 token/sourceOffset。 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_ComputeCapabilityClosure(
        const SZrHotPatchCapabilityRequirement *requirements,
        TZrUInt32 requirementCount,
        TZrUInt64 manifestRequiredCapabilities,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic);

/** @brief 面向 host 的 manifest 形兼容入口，沿用 ValidateCapabilityClosure 的准入限制。 */
ZR_CORE_API EZrHotPatchCapabilityStatus
ZrCore_HotPatch_ComputeRequiredCapabilities(
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic);

/* 旧调用名映射到同一验证路径，避免出现第二套授权规则。 */
#define ZrCore_HotPatch_ValidateCapabilities \
    ZrCore_HotPatch_ValidateCapabilityClosure

#endif
