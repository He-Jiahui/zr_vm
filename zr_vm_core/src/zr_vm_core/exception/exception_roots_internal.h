#ifndef ZR_VM_CORE_EXCEPTION_ROOTS_INTERNAL_H
#define ZR_VM_CORE_EXCEPTION_ROOTS_INTERNAL_H

#include <string.h>
#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_aot_abi.h"

/* Construction must also work while the allocator is failing. These local
 * roots allocate nothing. The collector updates objects[], so callers reload
 * a pointer or tagged value after every operation that can allocate. */
typedef struct SZrExceptionRoots {
    SZrRawObject *objects[4];
    SZrTypeValue values[4];
    SZrAotGcRootFrame frame;
} SZrExceptionRoots;

static inline TZrBool exception_roots_begin(SZrState *state, SZrExceptionRoots *roots) {
    static const SZrAotGcRootSlot slots[] = {
        {0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, sizeof(SZrRawObject *), 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, 2u * sizeof(SZrRawObject *), 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, 3u * sizeof(SZrRawObject *), 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u}
    };
    static const SZrAotGcRootMap map = {4u, slots};
    memset(roots, 0, sizeof(*roots));
    return ZrCore_Gc_AotRootFramePush(state, &roots->frame,
                                   (TZrStackValuePointer)roots->objects, &map);
}

static inline void exception_roots_set_value(SZrExceptionRoots *roots,
                                              TZrUInt32 index,
                                              const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&roots->values[index]);
    } else {
        roots->values[index] = *value;
    }
    roots->objects[index] = ZrCore_Value_IsGarbageCollectable(&roots->values[index])
            ? ZrCore_Value_GetRawObject(&roots->values[index]) : ZR_NULL;
}

static inline const SZrTypeValue *exception_roots_value(SZrExceptionRoots *roots, TZrUInt32 index) {
    if (ZrCore_Value_IsGarbageCollectable(&roots->values[index])) {
        roots->values[index].value.object = roots->objects[index];
    }
    return &roots->values[index];
}

/* Ordinary returns pop explicitly. Throw restores the owning TryRun snapshot
 * before abandoning these C locals; an unwound frame must never be popped. */
static inline void exception_roots_end(SZrState *state, SZrExceptionRoots *roots) {
    (void)ZrCore_Gc_AotRootFramePop(state, &roots->frame);
}

#endif
