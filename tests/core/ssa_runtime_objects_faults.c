#include "ssa_runtime_objects_faults.h"
#include <stdlib.h>
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/state.h"
#include "../../zr_vm_core/src/zr_vm_core/gc/gc_internal.h"

static TZrUInt32 failObjectOrdinal, objectCount, reserveCount, failReserveOrdinal;
static TZrBool throwOomEnabled, collectEnabled;
static size_t failNativeOrdinal, nativeCount;
static TZrBool nativeFailed;
static TZrBool failRemembered;
static TZrBool failPause;
static void (*barrierHook)(SZrState *, void *);
static void *barrierContext;

void ssa_runtime_objects_faults(TZrUInt32 failObject, TZrBool throwOom,
                                TZrBool collectEachObject, TZrUInt32 failReserve) {
    failObjectOrdinal = failObject;
    objectCount = reserveCount = 0u;
    throwOomEnabled = throwOom;
    collectEnabled = collectEachObject;
    failReserveOrdinal = failReserve;
    failRemembered = ZR_FALSE;
    failPause = ZR_FALSE;
    barrierHook = ZR_NULL;
    barrierContext = ZR_NULL;
    ssa_runtime_objects_fail_native_allocation(0u);
}

TZrUInt32 ssa_runtime_objects_allocation_count(void) { return objectCount; }

void ssa_runtime_objects_fail_native_allocation(size_t ordinal) {
    failNativeOrdinal = ordinal;
    nativeCount = 0u;
    nativeFailed = ZR_FALSE;
}

TZrBool ssa_runtime_objects_native_allocation_failed(void) { return nativeFailed; }

void ssa_runtime_objects_fail_remembered_reservation(void) { failRemembered = ZR_TRUE; }
void ssa_runtime_objects_fail_pause(void) { failPause = ZR_TRUE; }
void ssa_runtime_objects_set_barrier_hook(void (*hook)(SZrState *, void *), void *context) {
    barrierHook = hook;
    barrierContext = context;
}

static TZrBool fail_native(void) {
    if (++nativeCount != failNativeOrdinal) return ZR_FALSE;
    nativeFailed = ZR_TRUE;
    return ZR_TRUE;
}

static void *test_calloc(size_t count, size_t size) {
    return fail_native() ? NULL : calloc(count, size);
}

static TZrBool fail_object(SZrState *state) {
    ++objectCount;
    if (collectEnabled) ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    if (failObjectOrdinal != objectCount) return ZR_FALSE;
    if (throwOomEnabled) ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    return ZR_TRUE;
}

static SZrObject *test_object_new(SZrState *state, SZrObjectPrototype *prototype) {
    if (fail_object(state)) return ZR_NULL;
    return ZrCore_Object_New(state, prototype);
}

static TZrBool test_reserve(SZrState *state, SZrHashSet *set, TZrSize count) {
    if (collectEnabled) ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    if (++reserveCount == failReserveOrdinal) return ZR_FALSE;
    return ZrCore_HashSet_EnsurePairPoolForElementCount(state, set, count);
}

static TZrBool test_remembered_reserve(SZrGlobalState *global, TZrSize count) {
    return failRemembered ? ZR_FALSE : garbage_collector_ensure_remembered_registry_capacity(global, count);
}

static TZrBool test_pause(SZrState *state, TZrUInt32 timeout, SZrGcDomainPauseDiagnostic *diagnostic) {
    return failPause ? ZR_FALSE : ZrCore_GcDomain_StopTheWorldBegin(state, timeout, diagnostic);
}

static void test_barrier(SZrState *state, SZrRawObject *object, SZrRawObject *value) {
    void (*hook)(SZrState *, void *) = barrierHook;
    barrierHook = ZR_NULL;
    if (hook != ZR_NULL) hook(state, barrierContext);
    ZrCore_RawObject_Barrier(state, object, value);
}

/* Fault only the production call boundaries; objects, GC and storage remain
 * the real runtime. A distinct symbol coexists with the unwrapped library API. */
#define ZrCore_ExecIr_MaterializeObjects ssa_runtime_objects_materialize
#define ZrCore_Object_New test_object_new
#define ZrCore_HashSet_EnsurePairPoolForElementCount test_reserve
#define garbage_collector_ensure_remembered_registry_capacity test_remembered_reserve
#define ZrCore_GcDomain_StopTheWorldBegin test_pause
#define ZrCore_RawObject_Barrier test_barrier
#define calloc test_calloc
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c"
