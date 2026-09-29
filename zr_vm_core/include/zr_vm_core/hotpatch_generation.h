#ifndef ZR_VM_CORE_HOTPATCH_GENERATION_H
#define ZR_VM_CORE_HOTPATCH_GENERATION_H

#include "zr_vm_core/capability_manifest.h"

#include <stdatomic.h>

/** @brief Prepare、Publish、租约和回收的结构化状态；host 按状态选择重试或拒绝。 */
typedef enum EZrHotPatchGenerationStatus {
    ZR_HOT_PATCH_GENERATION_OK = 0,
    ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,
    ZR_HOT_PATCH_GENERATION_CAPACITY,
    ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
    ZR_HOT_PATCH_GENERATION_STALE_LINK,
    ZR_HOT_PATCH_GENERATION_INVALID_STATE,
    ZR_HOT_PATCH_GENERATION_OVERFLOW
} EZrHotPatchGenerationStatus;

/** @brief 槽位状态；旧 active 退役后仍可由现存租约读取，直到安全回收。 */
typedef enum EZrHotPatchVersionState {
    ZR_HOT_PATCH_VERSION_FREE = 0,
    ZR_HOT_PATCH_VERSION_PREPARED,
    ZR_HOT_PATCH_VERSION_ACTIVE,
    ZR_HOT_PATCH_VERSION_RETIRED
} EZrHotPatchVersionState;

/** @brief 单个版本的身份和租约计数；records 数组由调用方分配并持有。 */
typedef struct SZrHotPatchVersionRecord {
    TZrUInt64 generation;
    TZrUInt64 moduleHash;
    TZrUInt64 contentHash;
    TZrUInt64 publicContractHash;
    TZrUInt32 targetProfile;
    TZrUInt32 state;
    atomic_uint_fast32_t leaseCount;
} SZrHotPatchVersionRecord;

/** @brief 在同一锁下协调槽位与发布，在 active 指针上公布当前版本。
 * TODO: 回收不递减 count；核查它表示高水位还是当前占用数。 */
typedef struct SZrHotPatchGenerationManager {
    atomic_flag lock;
    _Atomic(SZrHotPatchVersionRecord *) active;
    atomic_uint_fast64_t nextGeneration;
    SZrHotPatchVersionRecord *records;
    TZrUInt32 capacity;
    TZrUInt32 count;
} SZrHotPatchGenerationManager;

/** @brief Prepare 返回未租用句柄；Acquire 返回租用句柄，后者必须 Release。
 * 句柄绑定创建它的 manager；其他 manager 会在读取记录前拒绝该句柄。 */
typedef struct SZrHotPatchGenerationHandle {
    SZrHotPatchVersionRecord *record;
    TZrUInt64 generation;
    TZrBool leased;
} SZrHotPatchGenerationHandle;

/** @brief 持有有效租约时读取的版本快照，不延长原记录生命周期。 */
typedef struct SZrHotPatchVersionView {
    TZrUInt64 generation;
    TZrUInt64 moduleHash;
    TZrUInt64 contentHash;
    TZrUInt64 publicContractHash;
    TZrUInt32 targetProfile;
    EZrHotPatchVersionState state;
    TZrUInt32 leaseCount;
} SZrHotPatchVersionView;

/** @brief 失败类别及期望/实际代际；输出参数可省略但返回状态不可忽略。 */
typedef struct SZrHotPatchGenerationDiagnostic {
    EZrHotPatchGenerationStatus status;
    TZrUInt64 expectedGeneration;
    TZrUInt64 actualGeneration;
    TZrUInt32 leaseCount;
} SZrHotPatchGenerationDiagnostic;

/** @brief 借用并初始化调用方的 records 槽位；成功后由 Deinit 结束借用。
 * @pre capacity 非零，records 至少有 capacity 项且无并发使用。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_GenerationManager_Init(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchVersionRecord *records,
        TZrUInt32 capacity,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 结束 manager 借用；host 先停止并发操作、释放所有租约，再释放 records。 */
ZR_CORE_API void ZrCore_HotPatch_GenerationManager_Deinit(
        SZrHotPatchGenerationManager *manager);
/** @brief 从已验证候选建立 PREPARED 版本；不会改变新调用看到的 active。
 * @pre validated 的借用内容稳定，且 moduleHash、内容哈希均非零。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Prepare(
        SZrHotPatchGenerationManager *manager,
        const SZrValidatedHotPatch *validated,
        TZrUInt64 moduleHash,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 原子切换新调用使用的 active；已有租约继续保留旧版本。
 * @pre prepared 来自同一 manager 的 Prepare/Rollback，且尚未发布或租用。
 * @note 外 manager 句柄返回 NOT_PREPARED，actualGeneration 为零。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Publish(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 复制目标版本身份到新代际；调用方随后 Publish 才切换 active。
 * @note 回滚需要空槽位，不复活原代际编号。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Rollback(
        SZrHotPatchGenerationManager *manager,
        TZrUInt64 targetGeneration,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 为一次新调用固定当前 active；成功取得的租约须 Release。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_AcquireActive(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 按编号租用仍在槽位中的版本，包括未回收的 RETIRED 版本。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Acquire(
        SZrHotPatchGenerationManager *manager,
        TZrUInt64 generation,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 在有效租约内取得版本快照；返回视图不拥有记录。
 * @note 外 manager 句柄返回 STALE_LINK，actualGeneration 为零。
 * BUG: 与 Publish 并发时会无锁读取其修改的非原子 state。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Resolve(
        const SZrHotPatchGenerationManager *manager,
        const SZrHotPatchGenerationHandle *handle,
        SZrHotPatchVersionView *outView,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 结束 Acquire 的一次租约；最后一位持有者释放后旧版本才可回收。
 * @note 外 manager 句柄返回 STALE_LINK，且不改变句柄或租约计数。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Release(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *handle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 仅回收非 active 且 leaseCount 为零的退役槽位，经 outCollected 写出本次回收数。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_CollectRetired(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief 返回诊断文本；回收或重试策略应使用 EZrHotPatchGenerationStatus。 */
ZR_CORE_API const TZrChar *ZrCore_HotPatch_Generation_StatusName(
        EZrHotPatchGenerationStatus status);

#endif
