#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_INTERNAL_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_INTERNAL_H

#include "zr_vm_library/aot_runtime.h"

struct SZrGlobalState;
struct SZrState;

typedef struct SZrLibraryAotRuntimeState SZrLibraryAotRuntimeState;

/* 静态直调在生成代码给出的 thunk 与运行时元数据函数之间做同一性门禁；
 * PrepareStaticDirectCall 只能在两张表仍对应同一个函数时跳过通用分派。 */
static inline TZrBool aot_runtime_static_direct_call_identity_matches(
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 calleeFunctionIndex,
        const struct SZrFunction *metadataFunction,
        FZrAotEntryThunk calleeThunk) {
    if (frame == ZR_NULL || metadataFunction == ZR_NULL || calleeThunk == ZR_NULL ||
        frame->functionTable == ZR_NULL || frame->functionThunks == ZR_NULL ||
        calleeFunctionIndex >= frame->functionCount ||
        calleeFunctionIndex >= frame->functionThunkCount) {
        return ZR_FALSE;
    }

    return (TZrBool)(
            frame->functionTable[calleeFunctionIndex] == metadataFunction &&
            frame->functionThunks[calleeFunctionIndex] == calleeThunk);
}

/* 项目 AOT 状态由 global->userData 中的 project 持有，返回值仅在项目释放前有效。 */
SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(struct SZrGlobalState *global);

/* 共用的诊断出口：写项目最近错误，并在 VM 尚正常时置运行错误状态。 */
void aot_runtime_fail(struct SZrState *state,
                      SZrLibraryAotRuntimeState *runtimeState,
                      const TZrChar *format,
                      ...);

#endif
