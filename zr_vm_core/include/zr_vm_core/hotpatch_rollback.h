#ifndef ZR_VM_CORE_HOTPATCH_ROLLBACK_H
#define ZR_VM_CORE_HOTPATCH_ROLLBACK_H

#include "zr_vm_core/hotpatch_generation.h"

/** @brief registry 幂等检查、代际准备及发布的 host 可见结果。 */
typedef enum EZrHotPatchApplyStatus {
    ZR_HOT_PATCH_APPLY_OK = 0,
    ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT,
    ZR_HOT_PATCH_APPLY_ALREADY_APPLIED,
    ZR_HOT_PATCH_APPLY_ID_COLLISION,
    ZR_HOT_PATCH_APPLY_CAPACITY,
    ZR_HOT_PATCH_APPLY_PUBLISH_FAILED,
    ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND,
    ZR_HOT_PATCH_APPLY_ROLLBACK_FAILED
} EZrHotPatchApplyStatus;

/** @brief patch ID 与内容哈希绑定的发布记录；同 ID 不可指向不同内容。 */
typedef struct SZrHotPatchRegistryEntry {
    TZrUInt64 patchId;
    TZrUInt64 contentHash;
    TZrUInt64 generation;
    TZrUInt32 state;
} SZrHotPatchRegistryEntry;

/** @brief 调用方持有 entries 存储；此层不负责初始化、锁定或释放 registry。
 * TODO: 核查 host 是否串行调用同一 registry 的 ApplyValidated。 */
typedef struct SZrHotPatchRegistry {
    SZrHotPatchRegistryEntry *entries;
    TZrUInt32 capacity;
    TZrUInt32 count;
} SZrHotPatchRegistry;

/** @brief 返回冲突 ID、哈希及代际，供 host 诊断和重试判定。 */
typedef struct SZrHotPatchApplyDiagnostic {
    EZrHotPatchApplyStatus status;
    TZrUInt64 patchId;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
    TZrUInt64 generation;
} SZrHotPatchApplyDiagnostic;

/** @brief 将已验证候选按 patch ID 判重，再 Prepare + Publish 并登记代际。
 * @pre validated 借用内容保持稳定；registry 由 host 初始化并串行使用。
 * BUG: Prepare 的 INVALID_ARGUMENT/OVERFLOW 目前也会被映射为 CAPACITY。 */
ZR_CORE_API EZrHotPatchApplyStatus ZrCore_HotPatch_ApplyValidated(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchRegistry *registry,
        const SZrValidatedHotPatch *validated,
        TZrUInt64 moduleHash,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchApplyDiagnostic *diagnostic);

/** @brief 将目标旧身份复制到新代际并发布，不改动原租约所见版本。
 * BUG: 目标存在但无空槽时的 CAPACITY 目前会被映射为 NOT_FOUND。 */
ZR_CORE_API EZrHotPatchApplyStatus ZrCore_HotPatch_Rollback(
        SZrHotPatchGenerationManager *manager,
        TZrUInt64 targetGeneration,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchApplyDiagnostic *diagnostic);

/** @brief 返回部署日志文字；host 处理失败时仍应使用状态枚举与结构化诊断。 */
ZR_CORE_API const TZrChar *ZrCore_HotPatch_ApplyStatusName(EZrHotPatchApplyStatus status);

#endif
