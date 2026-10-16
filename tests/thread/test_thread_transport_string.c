/* Actual Thread transport must preserve VM string bytes, including NUL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "runtime/runtime_internal.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int failures, allocatorError;

static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize || allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[index] = allocations[--liveCount]; }
        return NULL;
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
    return next;
}

typedef struct RoundTrip {
    const char *bytes;
    TZrSize length;
    SZrGcRootLease inputLease;
    ZrVmTaskTransportValue encoded;
    ZrVmTaskTransportValue cloned;
    SZrTypeValue output;
    int completed;
} RoundTrip;

static void round_trip(SZrState *state, TZrPtr data) {
    RoundTrip *trip = (RoundTrip *)data;
    SZrTypeValue input;
    SZrString *source = ZrCore_String_Create(state, (TZrNativeString)trip->bytes, trip->length);
    SZrString *decoded;
    check(source != NULL, "actual VM source string is created");
    if (source == NULL) return;
    if (!ZrCore_Gc_RootLeaseAcquire(state, &source->super, &trip->inputLease)) {
        check(0, "caller roots actual input across transport allocations");
        return;
    }
    ZrCore_Value_InitAsRawObject(state, &input, &source->super);
    input.type = ZR_VALUE_TYPE_STRING;
    if (!zr_vm_task_transport_encode_value(state, &input, &trip->encoded, "string round trip")) {
        check(0, "real transport encodes the VM string");
        return;
    }
    check(trip->encoded.kind == ZR_VM_TASK_TRANSPORT_KIND_STRING, "encoded kind remains STRING");
    if (!zr_vm_task_transport_clone_value(&trip->encoded, &trip->cloned)) {
        check(0, "real transport clones its envelope");
        return;
    }
    check(trip->cloned.kind == ZR_VM_TASK_TRANSPORT_KIND_STRING, "cloned kind remains STRING");
    check(trip->encoded.as.stringValue != trip->cloned.as.stringValue,
          "clone owns independent native string storage");
    /* Destroy the first envelope before decode, exercising independent ownership. */
    zr_vm_task_transport_clear(&trip->encoded);
    if (!zr_vm_task_transport_decode_value(state, &trip->cloned, &trip->output)) {
        check(0, "real transport decodes the cloned envelope");
        return;
    }
    check(trip->output.type == ZR_VALUE_TYPE_STRING && ZrCore_Value_GetRawObject(&trip->output) != NULL,
          "decoded output is a real VM string");
    if (trip->output.type != ZR_VALUE_TYPE_STRING || ZrCore_Value_GetRawObject(&trip->output) == NULL) return;
    decoded = (SZrString *)ZrCore_Value_GetRawObject(&trip->output);
    check(ZrCore_String_GetByteLength(decoded) == trip->length, "round trip preserves explicit byte length");
    if (ZrCore_String_GetByteLength(decoded) == trip->length) {
        check(memcmp(ZrCore_String_GetNativeString(decoded), trip->bytes, (size_t)trip->length) == 0,
              "round trip preserves every byte including bytes after NUL");
    }
    trip->completed = 1;
}

int main(int argc, char **argv) {
    static const char embedded[] = {'a', '\0', 'b', '\0', 'c'};
    RoundTrip trip = {0};
    SZrGlobalState *global;
    TZrSize baselineRoots;
    EZrThreadStatus status;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "embedded_nul") == 0) { trip.bytes = embedded; trip.length = sizeof(embedded); }
    else if (strcmp(argv[1], "empty") == 0) { trip.bytes = ""; trip.length = 0; }
    else if (strcmp(argv[1], "plain") == 0) { trip.bytes = "plain transport text"; trip.length = strlen(trip.bytes); }
    else return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "Core initializes before transport");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            baselineRoots = global->garbageCollector->ignoredObjectCount;
            status = ZrCore_Exception_TryRun(global->mainThreadState, round_trip, &trip);
            check(status == ZR_THREAD_STATUS_FINE && trip.completed, "round trip returns normally");
            zr_vm_task_transport_clear(&trip.encoded);
            zr_vm_task_transport_clear(&trip.cloned);
            ZrCore_Gc_RootLeaseRelease(global, &trip.inputLease);
            check(global->garbageCollector->ignoredObjectCount == baselineRoots,
                  "transport leaves no additional caller roots");
            check(global->mainThreadState->exceptionRecoverPoint == NULL, "recovery chain is restored");
        }
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "all VM allocations release with matching size and category");
    return failures == 0 ? 0 : 1;
}
