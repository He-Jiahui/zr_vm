#ifndef ZR_VM_CORE_HOTPATCH_PUBLISH_H
#define ZR_VM_CORE_HOTPATCH_PUBLISH_H

#include "zr_vm_core/hotpatch_generation.h"

/** @brief 兼容 Prepare/Publish 分阶段 host；切换仍由 generation manager 串行化。
 * @pre prepared 属于同一 manager，尚未发布也未租用。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_PublishPrepared(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic);
/** @brief PublishPrepared 的短名称入口，共享同一发布协议与失败状态。 */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Publish(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic);

#endif
