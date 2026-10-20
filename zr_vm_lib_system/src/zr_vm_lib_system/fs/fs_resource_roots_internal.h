#ifndef ZR_VM_LIB_SYSTEM_FS_RESOURCE_ROOTS_INTERNAL_H
#define ZR_VM_LIB_SYSTEM_FS_RESOURCE_ROOTS_INTERNAL_H

#include <string.h>
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_common/zr_aot_abi.h"

/* These slots and their frame live in the TryRun caller. Its snapshot retains
 * the frame on Throw; the caller then pops it after freeing its native data. */
typedef struct ZrSystemFsResourceRoots {
    SZrRawObject *objects[2];
    SZrAotGcRootFrame frame;
} ZrSystemFsResourceRoots;

static inline TZrBool system_fs_resource_roots_begin(SZrState *state, ZrSystemFsResourceRoots *roots) {
    static const SZrAotGcRootSlot slots[] = {
        {0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, sizeof(SZrRawObject *), 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u}
    };
    static const SZrAotGcRootMap map = {2u, slots};
    memset(roots, 0, sizeof(*roots));
    return ZrCore_Gc_AotRootFramePush(state, &roots->frame,
                                    (TZrStackValuePointer)roots->objects, &map);
}

static inline SZrObject *system_fs_resource_object(SZrState *state,
                                                  ZrSystemFsResourceRoots *roots,
                                                  TZrUInt32 index) {
    return ZR_CAST_OBJECT(state, roots->objects[index]);
}

static inline void system_fs_resource_roots_end(SZrState *state, ZrSystemFsResourceRoots *roots) {
    (void)ZrCore_Gc_AotRootFramePop(state, &roots->frame);
}

#endif
