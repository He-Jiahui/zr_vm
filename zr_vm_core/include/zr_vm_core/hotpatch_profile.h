#ifndef ZR_VM_CORE_HOTPATCH_PROFILE_H
#define ZR_VM_CORE_HOTPATCH_PROFILE_H

#include "zr_vm_core/artifact_exec_ir.h"

/** @brief 只能解释执行的受限宿主档位，不能从数值推断普通 host 兼容性。 */
typedef enum EZrHotPatchRestrictedProfile {
    ZR_HOT_PATCH_PROFILE_IOS_INTERPRETER = 1,
    ZR_HOT_PATCH_PROFILE_WASM_INTERPRETER = 2
} EZrHotPatchRestrictedProfile;

/** @brief 首个违反档位限制的结构化原因。
 * TODO: MACHINE_CODE/IMPORT 当前不由受限档位验证器返回，状态名也未单独映射；
 * 核查它们是预留的公开状态，还是应纳入该入口的诊断契约。 */
typedef enum EZrHotPatchRestrictedStatus {
    ZR_HOT_PATCH_RESTRICTED_OK = 0,
    ZR_HOT_PATCH_RESTRICTED_INVALID_ARGUMENT,
    ZR_HOT_PATCH_RESTRICTED_PROFILE_MISMATCH,
    ZR_HOT_PATCH_RESTRICTED_SECTION_FORBIDDEN,
    ZR_HOT_PATCH_RESTRICTED_MACHINE_CODE,
    ZR_HOT_PATCH_RESTRICTED_IMPORT
} EZrHotPatchRestrictedStatus;

/** @brief 定位被拒绝的 artifact section；由调用方按需提供。 */
typedef struct SZrHotPatchRestrictedDiagnostic {
    EZrHotPatchRestrictedStatus status;
    TZrUInt32 sectionKind;
    TZrUInt32 sectionIndex;
} SZrHotPatchRestrictedDiagnostic;

/** @brief 按 section kind 白名单检查 iOS/WASM 解释器档位；机器码/导入标志由主验证器检查。
 * @pre artifact view 已解析，且底层 section 存储在调用期间稳定。 */
ZR_CORE_API EZrHotPatchRestrictedStatus ZrCore_HotPatch_ValidateRestrictedProfile(
        const SZrArtifactExecIrView *artifact,
        TZrUInt32 profile,
        SZrHotPatchRestrictedDiagnostic *diagnostic);
/** @brief 将受限档位拒绝状态转为日志文字；程序分支仍使用状态枚举。 */
ZR_CORE_API const TZrChar *ZrCore_HotPatch_RestrictedStatusName(
        EZrHotPatchRestrictedStatus status);

#endif
