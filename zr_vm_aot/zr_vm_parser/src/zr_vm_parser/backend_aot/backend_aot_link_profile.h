#ifndef ZR_VM_PARSER_BACKEND_AOT_LINK_PROFILE_H
#define ZR_VM_PARSER_BACKEND_AOT_LINK_PROFILE_H

#include "zr_vm_parser/aot_generic_policy.h"

/** @brief 归档链接策略的结果码；fallback 表示显式降级到 dev。 */
typedef enum EZrBackendAotLinkProfileStatus {
    ZR_BACKEND_AOT_LINK_PROFILE_OK = 0,
    ZR_BACKEND_AOT_LINK_PROFILE_INVALID_ARGUMENT,
    ZR_BACKEND_AOT_LINK_PROFILE_MISMATCH,
    ZR_BACKEND_AOT_LINK_PROFILE_UNSUPPORTED,
    ZR_BACKEND_AOT_LINK_PROFILE_FALLBACK
} EZrBackendAotLinkProfileStatus;

/** @brief 已按工具链能力解析的有效构建档位及 LTO/PGO 开关。 */
typedef struct SZrBackendAotLinkProfileResult {
    EZrAotReleaseProfileKind effectiveKind;
    TZrBool ltoEnabled;
    TZrBool thinLtoEnabled;
    TZrBool pgoEnabled;
    TZrBool fallbackToDev;
} SZrBackendAotLinkProfileResult;

/** @brief 验证 release profile 与工具链能力，按需降级为 dev。
 *  @note 失败与降级都可能填写 diagnostic；只有 OK/FALLBACK 有有效 outResult。 */
ZR_PARSER_API EZrBackendAotLinkProfileStatus
backend_aot_link_profile_resolve(
        const SZrAotReleaseProfile *profile,
        const SZrAotReleaseToolchain *toolchain,
        TZrBool allowFallback,
        SZrBackendAotLinkProfileResult *outResult,
        SZrAotReleasePolicyDiagnostic *diagnostic);

/** @brief 校验保留根并写出函数保留位图。
 *  @pre retained 可容纳 functionCount 个 TZrBool；即使输入根无效也会先清零该范围。 */
ZR_PARSER_API TZrBool backend_aot_link_profile_mark_roots(
        const SZrAotReleaseRoot *roots,
        TZrUInt32 rootCount,
        TZrUInt32 functionCount,
        TZrBool *retained,
        SZrAotReleasePolicyDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_LINK_PROFILE_H */
