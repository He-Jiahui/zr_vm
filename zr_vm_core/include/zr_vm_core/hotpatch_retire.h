#ifndef ZR_VM_CORE_HOTPATCH_RETIRE_H
#define ZR_VM_CORE_HOTPATCH_RETIRE_H

#include "zr_vm_core/hotpatch_generation.h"

/** @brief host 安全点的显式回收入口；manager 只释放退役且无租约的槽位。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_RetireCollect(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief RetireCollect 的兼容名称；经 outCollected 写出本次实际回收数。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_CollectRetired(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/* 旧调用名仍落在同一安全点回收协议上。 */
#define ZrCore_HotPatch_Retire ZrCore_HotPatch_RetireCollect

#endif
