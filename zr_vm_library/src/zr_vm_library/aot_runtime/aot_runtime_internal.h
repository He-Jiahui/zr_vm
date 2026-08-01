#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_INTERNAL_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_INTERNAL_H

#include "zr_vm_library/aot_runtime.h"

struct SZrGlobalState;
struct SZrState;

typedef struct SZrLibraryAotRuntimeState SZrLibraryAotRuntimeState;

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

SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(struct SZrGlobalState *global);

void aot_runtime_fail(struct SZrState *state,
                      SZrLibraryAotRuntimeState *runtimeState,
                      const TZrChar *format,
                      ...);

#endif
