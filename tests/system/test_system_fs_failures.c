/* Public native calls against real filesystem implementations; no FS mocks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/file.h"
#include "zr_vm_lib_system/module.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[65536];
static size_t liveCount;
static int failures, allocatorError, armed;
static char fixturePath[ZR_LIBRARY_MAX_PATH_LENGTH];
#ifdef _WIN32
static int rejected, openObserved;
static BY_HANDLE_FILE_INFORMATION fixtureIdentity;
/* A unique fixture file, not process-wide handle counts. An exclusive open is
 * blocked by the CRT read handle even though that handle permits sharing. */
static int fixture_is_open(void) {
    HANDLE handle = CreateFileA(fixturePath, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_SHARING_VIOLATION;
    CloseHandle(handle);
    return 0;
}
#endif
static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}
static TZrPtr allocator(TZrPtr context, TZrPtr old, TZrSize oldSize,
                        TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)context;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize || allocations[index].flag != flag) {
            allocatorError = 1; return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[index] = allocations[--liveCount]; }
        return NULL;
    }
#ifdef _WIN32
    if (armed && old == NULL && flag == ZR_MEMORY_NATIVE_TYPE_OBJECT &&
        newSize == sizeof(SZrObject) && fixture_is_open()) {
        openObserved = 1; ++rejected; return NULL;
    }
#endif
    next = realloc(old, newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index] = (Allocation){next, newSize, flag};
    return next;
}

typedef struct Call {
    SZrTypeValue callable, receiver, arguments[1], result;
    TZrSize count;
    int module, ok;
} Call;
static void invoke(SZrState *state, TZrPtr arguments) {
    Call *call = arguments;
    call->ok = call->module
        ? ZrLib_CallModuleExport(state, "zr.system.fs", "readText", call->arguments, 1, &call->result)
        : ZrLib_CallValue(state, &call->callable, &call->receiver, call->arguments, call->count, &call->result);
}
static int method(SZrState *state, Call *call, SZrTypeValue receiver, const char *name) {
    memset(call, 0, sizeof(*call));
    const SZrTypeValue *value = ZrLib_Object_GetFieldCString(state,
        (SZrObject *)ZrCore_Value_GetRawObject(&receiver), name);
    if (value == NULL) return 0;
    call->receiver = receiver;
    call->callable = *value;
    return 1;
}
static void string_result(Call *call, const unsigned char *bytes, size_t length) {
    check(call->ok && call->result.type == ZR_VALUE_TYPE_STRING, "public readText returns a string");
    if (call->ok && call->result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *text = (SZrString *)ZrCore_Value_GetRawObject(&call->result);
        check(text != NULL && ZrCore_String_GetByteLength(text) == length,
              "VM text retains the complete byte length including embedded NUL");
        if (text != NULL && ZrCore_String_GetByteLength(text) == length)
            check(memcmp(ZrCore_String_GetNativeString(text), bytes, length) == 0, "VM text retains every input byte");
    }
}
static void run(SZrState *state, const char *name) {
    static const unsigned char binary[] = {'A', 0, 'B'};
    const unsigned char *bytes = binary;
    size_t length = sizeof(binary);
    Call call;
    SZrTypeValue fileValue, streamValue, field;
    SZrGcRootLease fileLease = {0}, streamLease = {0};
    EZrThreadStatus status;
    FILE *output;
    int isModule = strncmp(name, "module_", 7) == 0;
    if (strstr(name, "plain") != NULL || strncmp(name, "open_", 5) == 0) {
        bytes = (const unsigned char *)"alpha"; length = 5;
    }
    if (strstr(name, "empty") != NULL) { bytes = (const unsigned char *)""; length = 0; }
    output = fopen(fixturePath, "wb");
    check(output != NULL, "create only the registered fixture file");
    if (output == NULL) return;
    check(fwrite(bytes, 1, length, output) == length, "write exact binary fixture bytes");
    check(fclose(output) == 0, "close setup writer before handle assertions");
    /* Resolve/import the real module before arming any allocation fault. */
    check(ZrLib_Module_GetExport(state, "zr.system.fs", "readText") != NULL, "import real FS descriptors");
    if (isModule) {
        memset(&call, 0, sizeof(call)); call.module = 1;
        ZrLib_Value_SetString(state, &call.arguments[0], fixturePath);
        check(ZrCore_Gc_RootLeaseAcquire(state, ZrCore_Value_GetRawObject(&call.arguments[0]), &fileLease),
              "lease path before module export lookup");
        if (fileLease.object == NULL) return;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
        check(status == ZR_THREAD_STATUS_FINE, "module readText completes without exception");
        string_result(&call, bytes, length);
        ZrCore_Gc_RootLeaseRelease(state->global, &fileLease);
        return;
    }
    /* Build the real registered File instance through the public type API;
     * open reads fullPath. This does not claim constructor-lowering coverage. */
    SZrObject *file = ZrLib_Type_NewInstance(state, "File");
    check(file != NULL, "construct registered File instance");
    if (file == NULL) return;
    check(ZrCore_Gc_RootLeaseAcquire(state, &file->super, &fileLease), "lease File across public calls");
    if (fileLease.object == NULL) return;
    ZrLib_Value_SetObject(state, &fileValue, file, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetString(state, &field, fixturePath);
    ZrLib_Object_SetFieldCString(state, file, "fullPath", &field);
    if (!method(state, &call, fileValue, "open")) {
        check(0, "resolve real inherited File.open"); goto cleanup;
    }
#ifdef _WIN32
    if (strcmp(name, "open_oom") == 0) {
        HANDLE probe = CreateFileA(fixturePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        check(probe != INVALID_HANDLE_VALUE, "exclusive fixture open succeeds before injection");
        if (probe == INVALID_HANDLE_VALUE) goto cleanup;
        check(GetFileInformationByHandle(probe, &fixtureIdentity), "record exact fixture file identity");
        CloseHandle(probe);
        armed = 1;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
        armed = 0;
        check(openObserved && rejected > 0, "reject actual wrapper OBJECT allocation only after fixture handle opened");
        check(!call.ok && (status == ZR_THREAD_STATUS_MEMORY_ERROR || state->threadStatus == ZR_THREAD_STATUS_MEMORY_ERROR ||
              (state->hasCurrentException && state->currentExceptionStatus == ZR_THREAD_STATUS_MEMORY_ERROR)),
              "public File.open reports allocation failure");
        probe = CreateFileA(fixturePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        check(probe != INVALID_HANDLE_VALUE, "failed wrapper closes its fixture file handle before returning");
        if (probe != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION identity;
            check(GetFileInformationByHandle(probe, &identity) &&
                  identity.dwVolumeSerialNumber == fixtureIdentity.dwVolumeSerialNumber &&
                  identity.nFileIndexHigh == fixtureIdentity.nFileIndexHigh &&
                  identity.nFileIndexLow == fixtureIdentity.nFileIndexLow, "exclusive probe still names the original fixture identity");
            CloseHandle(probe);
        }
        ZrCore_Exception_ClearCurrent(state); state->threadStatus = ZR_THREAD_STATUS_FINE;
        call.ok = 0;
    }
#endif
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    check(status == ZR_THREAD_STATUS_FINE && call.ok && call.result.type == ZR_VALUE_TYPE_OBJECT,
          "ordinary open or fault recovery returns a FileStream");
    if (!call.ok || call.result.type != ZR_VALUE_TYPE_OBJECT) goto cleanup;
    streamValue = call.result;
    check(ZrCore_Gc_RootLeaseAcquire(state, ZrCore_Value_GetRawObject(&streamValue), &streamLease), "lease returned stream");
    if (streamLease.object == NULL) goto cleanup;
    if (!method(state, &call, streamValue, "readText")) {
        check(0, "resolve real FileStream.readText"); goto cleanup;
    }
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    check(status == ZR_THREAD_STATUS_FINE, "stream readText completes without exception");
    string_result(&call, bytes, length);
    const SZrTypeValue *position = ZrLib_Object_GetFieldCString(state,
        (SZrObject *)ZrCore_Value_GetRawObject(&streamValue), "position");
    check(position != NULL && position->type == ZR_VALUE_TYPE_INT64 &&
          position->value.nativeObject.nativeInt64 == (TZrInt64)length,
          "stream consumes complete input despite any embedded NUL");
    if (!method(state, &call, streamValue, "close")) {
        check(0, "resolve real FileStream.close"); goto cleanup;
    }
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    check(status == ZR_THREAD_STATUS_FINE && call.ok, "close ordinary or recovered stream");
cleanup:
    ZrCore_Gc_RootLeaseRelease(state->global, &streamLease);
    ZrCore_Gc_RootLeaseRelease(state->global, &fileLease);
}
int main(int argc, char **argv) {
    SZrGlobalState *global;
    int length;
    int known = 0;
    static const char *cases[] = {"module_nul", "stream_nul", "module_plain", "module_empty",
                                 "stream_plain", "stream_empty", "open_control"
#ifdef _WIN32
                                 , "open_oom"
#endif
    };
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 3) return 2;
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
        if (strcmp(argv[1], cases[index]) == 0) known = 1;
    if (!known) return 2;
    /* The test never deletes fixtures. Root fingerprints them after exit. */
    if (ZrLibrary_File_Exist(argv[2]) != ZR_LIBRARY_FILE_NOT_EXIST) return 3;
    if (!ZrLibrary_File_CreateDirectories(argv[2])) return 4;
    length = snprintf(fixturePath, sizeof(fixturePath), "%s/payload.bin", argv[2]);
    if (length < 0 || (size_t)length >= sizeof(fixturePath)) return 5;
    global = ZrCore_GlobalState_New(allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "initialize real Core");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            check(ZrVmLibSystem_Register(global), "register actual System modules");
            run(global->mainThreadState, argv[1]);
        }
        armed = 0; ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "Core teardown matches allocator metadata; not a native handle oracle");
    return failures == 0 ? 0 : 1;
}
