/* Actual decode/VM allocator, legal caller-owned native cells, no workers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "runtime/runtime_internal.h"
#include "zr_vm_lib_thread/module.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)
typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
typedef struct Cell {
    ZrVmTaskSharedCell *shared;
    ZrVmTaskMutexCell *mutex;
    TZrUInt32 kind;
} Cell;
typedef struct Counts { TZrUInt64 strong, weak, refs; } Counts;
typedef struct Decode {
    ZrVmTaskTransportValue input;
    SZrTypeValue output;
    int returned;
    TZrBool ok;
} Decode;
static Allocation allocations[32768];
static size_t liveCount, rejected;
static int failures, allocatorError, armed;
static Cell *activeCell;
static Counts firstRejectedCounts;

static Counts counts(Cell *cell) {
    Counts result = {0};
    if (cell->shared != NULL) {
        zr_vm_task_sync_mutex_lock(&cell->shared->mutex);
        result.strong = cell->shared->strongCount;
        result.weak = cell->shared->weakCount;
        zr_vm_task_sync_mutex_unlock(&cell->shared->mutex);
    } else {
        zr_vm_task_sync_mutex_lock(&cell->mutex->mutex);
        result.refs = cell->mutex->refCount;
        zr_vm_task_sync_mutex_unlock(&cell->mutex->mutex);
    }
    return result;
}
static void check_counts(Cell *cell, Counts expected) {
    Counts actual = counts(cell);
    fprintf(stderr, "counts strong=%llu weak=%llu mutex=%llu expected=%llu/%llu/%llu\n",
            (unsigned long long)actual.strong, (unsigned long long)actual.weak,
            (unsigned long long)actual.refs, (unsigned long long)expected.strong,
            (unsigned long long)expected.weak, (unsigned long long)expected.refs);
    CHECK(actual.strong == expected.strong && actual.weak == expected.weak && actual.refs == expected.refs);
}
static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (i < liveCount && allocations[i].pointer != old) ++i;
        if (i == liveCount || allocations[i].size != oldSize || allocations[i].flag != flag) {
            allocatorError = 1; return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[i] = allocations[--liveCount]; }
        return NULL;
    }
    if (armed && old == NULL && flag == ZR_MEMORY_NATIVE_TYPE_OBJECT && newSize == sizeof(SZrObject)) {
        if (rejected++ == 0) firstRejectedCounts = counts(activeCell);
        return NULL; /* persistent: also reject GcMalloc retry/fallback */
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        i = liveCount++;
    }
    allocations[i] = (Allocation){next, newSize, flag};
    return next;
}
static void invoke(SZrState *state, TZrPtr data) {
    Decode *call = (Decode *)data;
    call->ok = zr_vm_task_transport_decode_value(state, &call->input, &call->output);
    call->returned = 1;
}
static void prime_error(SZrState *state, TZrPtr data) {
    (void)data;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
}
static int init_cell(Cell *cell, TZrUInt32 kind) {
    memset(cell, 0, sizeof(*cell));
    cell->kind = kind;
    if (kind == ZR_VM_TASK_TRANSPORT_KIND_SHARED || kind == ZR_VM_TASK_TRANSPORT_KIND_WEAK_SHARED) {
        cell->shared = (ZrVmTaskSharedCell *)calloc(1, sizeof(*cell->shared));
        if (cell->shared == NULL) return 0;
        zr_vm_task_sync_mutex_init(&cell->shared->mutex);
        cell->shared->strongCount = 1; /* original caller owner */
        cell->shared->weakCount = 1;   /* original caller observer */
        cell->shared->lifecycleState = ZR_VM_TASK_SHARED_CELL_LIFECYCLE_ALIVE;
        cell->shared->value.kind = ZR_VM_TASK_TRANSPORT_KIND_INT;
        cell->shared->value.as.intValue = 41;
    } else {
        cell->mutex = (ZrVmTaskMutexCell *)calloc(1, sizeof(*cell->mutex));
        if (cell->mutex == NULL) return 0;
        zr_vm_task_sync_mutex_init(&cell->mutex->mutex);
        zr_vm_task_sync_condition_init(&cell->mutex->condition);
        cell->mutex->refCount = 1;
        cell->mutex->kind = (TZrUInt8)(kind == ZR_VM_TASK_TRANSPORT_KIND_SHARED_MUTEX
                ? ZR_VM_TASK_MUTEX_KIND_SHARED : ZR_VM_TASK_MUTEX_KIND_UNIQUE);
        cell->mutex->value.kind = ZR_VM_TASK_TRANSPORT_KIND_INT;
        cell->mutex->value.as.intValue = 41;
    }
    return 1;
}
static Counts delivered_counts(Cell *cell, Counts baseline) {
    if (cell->kind == ZR_VM_TASK_TRANSPORT_KIND_SHARED) ++baseline.strong;
    else if (cell->kind == ZR_VM_TASK_TRANSPORT_KIND_WEAK_SHARED) ++baseline.weak;
    else ++baseline.refs;
    return baseline;
}
static void clear_delivered_facade(SZrState *state, Cell *cell, SZrTypeValue *value) {
    SZrObject *handle;
    CHECK(value->type == ZR_VALUE_TYPE_OBJECT && value->value.object != NULL);
    if (value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == NULL) return;
    handle = ZR_CAST_OBJECT(state, value->value.object);
    if (cell->shared != NULL) {
        ZrVmTaskSharedCell *found = NULL;
        TZrBool weak = ZR_FALSE;
        CHECK(zr_vm_task_shared_try_get_cell(state, value, &found, &weak));
        CHECK(found == cell->shared && weak == (cell->kind == ZR_VM_TASK_TRANSPORT_KIND_WEAK_SHARED));
        if (found != cell->shared) return;
        zr_vm_task_set_null_field(state, handle, ZR_VM_TASK_SHARED_NATIVE_CELL_FIELD);
        if (cell->kind == ZR_VM_TASK_TRANSPORT_KIND_WEAK_SHARED) zr_vm_task_shared_cell_release_weak(found);
        else zr_vm_task_shared_cell_release_strong(found);
    } else {
        ZrVmTaskMutexCell *found = NULL;
        TZrUInt32 kind = 0;
        CHECK(zr_vm_task_mutex_try_get_cell(state, value, &found, &kind));
        CHECK(found == cell->mutex && kind == cell->mutex->kind);
        if (found != cell->mutex) return;
        zr_vm_task_set_null_field(state, handle, ZR_VM_TASK_MUTEX_NATIVE_CELL_FIELD);
        zr_vm_task_mutex_cell_release(found);
    }
    ZrLib_Value_SetNull(value);
}
static void run_cell(SZrState *state, TZrUInt32 kind, int fault, SZrString *sentinel) {
    Cell cell;
    Decode call = {0};
    Counts baseline, retained;
    TZrSize roots;
    EZrThreadStatus status;
    CHECK(init_cell(&cell, kind));
    if (cell.shared == NULL && cell.mutex == NULL) return;
    baseline = counts(&cell); retained = delivered_counts(&cell, baseline);
    call.input.kind = kind;
    call.input.as.pointerValue = cell.shared != NULL ? (TZrPtr)cell.shared : (TZrPtr)cell.mutex;
    /* Input envelope borrows the caller's known reference; it is not cleared. */
    ZrLib_Value_SetStringObject(state, &call.output, sentinel);
    roots = state->global->garbageCollector->ignoredObjectCount;
    if (fault) {
        activeCell = &cell; rejected = 0; armed = 1;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
        armed = 0;
        CHECK(rejected > 0);
        CHECK(firstRejectedCounts.strong == retained.strong && firstRejectedCounts.weak == retained.weak &&
                firstRejectedCounts.refs == retained.refs);
        CHECK(status == ZR_THREAD_STATUS_MEMORY_ERROR && !call.returned);
        CHECK(call.output.type == ZR_VALUE_TYPE_STRING && call.output.value.object == &sentinel->super);
        CHECK(state->exceptionRecoverPoint == NULL);
        CHECK(state->global->garbageCollector->ignoredObjectCount == roots);
        check_counts(&cell, baseline); /* primary RED; never adopt leaked count as baseline */
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
    }
    call.returned = 0;
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    CHECK(status == ZR_THREAD_STATUS_FINE && call.returned && call.ok);
    check_counts(&cell, retained);
    if (call.returned && call.ok) clear_delivered_facade(state, &cell, &call.output);
    check_counts(&cell, baseline);
    if (cell.shared != NULL) {
        CHECK(cell.shared->lifecycleState == ZR_VM_TASK_SHARED_CELL_LIFECYCLE_ALIVE);
        CHECK(cell.shared->value.kind == ZR_VM_TASK_TRANSPORT_KIND_INT && cell.shared->value.as.intValue == 41);
        /* Release only the original caller strong and weak, even on RED.
         * Do not compensate for an unowned leaked failure reference. */
        zr_vm_task_shared_cell_release_strong(cell.shared);
        zr_vm_task_shared_cell_release_weak(cell.shared);
    } else {
        CHECK(!cell.mutex->writerActive && cell.mutex->readerCount == 0);
        CHECK(cell.mutex->value.kind == ZR_VM_TASK_TRANSPORT_KIND_INT && cell.mutex->value.as.intValue == 41);
        zr_vm_task_mutex_cell_release(cell.mutex);
    }
    activeCell = NULL;
}
int main(int argc, char **argv) {
    static const TZrUInt32 kinds[] = {ZR_VM_TASK_TRANSPORT_KIND_SHARED, ZR_VM_TASK_TRANSPORT_KIND_WEAK_SHARED,
            ZR_VM_TASK_TRANSPORT_KIND_UNIQUE_MUTEX, ZR_VM_TASK_TRANSPORT_KIND_SHARED_MUTEX};
    static const char *const names[] = {"shared", "weak_shared", "unique_mutex", "shared_mutex"};
    SZrGlobalState *global;
    SZrString *sentinel;
    SZrGcRootLease sentinelLease = {0};
    int selection = -1, normal;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    normal = strcmp(argv[1], "normal_controls") == 0;
    for (int i = 0; i < 4; ++i) if (strcmp(argv[1], names[i]) == 0) selection = i;
    if (!normal && selection < 0) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL && ZrCore_GlobalState_IsInitialized(global)) {
        SZrState *state = global->mainThreadState;
        global->garbageCollector->stopGcFlag = ZR_TRUE;
        CHECK(ZrVmThread_Register(global));
        CHECK(ZrCore_Module_ImportByPath(state, ZrCore_String_CreateFromNative(state, "zr.thread")) != NULL);
        CHECK(ZrCore_Stack_Grow(state, 128, ZR_FALSE));
        CHECK(ZrCore_Exception_TryRun(state, prime_error, NULL) == ZR_THREAD_STATUS_MEMORY_ERROR);
        ZrCore_Exception_ClearCurrent(state); state->threadStatus = ZR_THREAD_STATUS_FINE;
        sentinel = ZrCore_String_CreateFromNative(state, "thread-facade-oom-sentinel");
        CHECK(sentinel != NULL);
        if (sentinel != NULL) {
            CHECK(ZrCore_Gc_RootLeaseAcquire(state, &sentinel->super, &sentinelLease));
            if (sentinelLease.object != NULL) {
                for (int i = 0; i < 4; ++i) if (normal || selection == i) run_cell(state, kinds[i], !normal, sentinel);
                ZrCore_Gc_RootLeaseRelease(global, &sentinelLease);
            }
        }
    }
    armed = 0;
    if (global != NULL) ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}
