/* Actual public FS calls and actual production CRT allocations, no FS mocks. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "support/fs_resource_crt_observer.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/file.h"
#include "zr_vm_lib_system/module.h"

enum { PAYLOAD_LENGTH = 257 };
_Static_assert(PAYLOAD_LENGTH > ZR_VM_SHORT_STRING_MAX, "payload must use actual long String storage");
enum Case { MODULE_TEXT, FILE_TEXT, BYTES_NEW, BYTES_PUSH, RAW_APPEND, ENTRIES, GLOB };
typedef struct VmBlock { void *pointer; TZrSize size; TZrInt64 flag; } VmBlock;
static VmBlock blocks[65536];
static size_t blockCount;
static int failures, allocatorError, armed, rejected;
static enum Case selectedCase;
static TZrSize failureSize;
static TZrInt64 failureFlag;
static char fixturePath[ZR_LIBRARY_MAX_PATH_LENGTH];
static unsigned char payload[PAYLOAD_LENGTH];
#ifdef _WIN32
static BY_HANDLE_FILE_INFORMATION identity;
static int openObserved;
#endif
static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}
#ifdef _WIN32
static int file_is_open(void) {
    HANDLE handle = CreateFileA(fixturePath, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_SHARING_VIOLATION;
    CloseHandle(handle);
    return 0;
}
#endif
static void probe_file(int initial) {
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION current;
    HANDLE handle = CreateFileA(fixturePath, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    check(handle != INVALID_HANDLE_VALUE, "exact fixture permits exclusive open after call");
    if (handle == INVALID_HANDLE_VALUE) return;
    int gotIdentity = GetFileInformationByHandle(handle, &current) != 0;
    check(gotIdentity, "read exact fixture identity");
    if (!gotIdentity) { CloseHandle(handle); return; }
    if (initial) identity = current;
    else check(current.dwVolumeSerialNumber == identity.dwVolumeSerialNumber &&
               current.nFileIndexHigh == identity.nFileIndexHigh &&
               current.nFileIndexLow == identity.nFileIndexLow,
               "probe identifies the original fixture file");
    CloseHandle(handle);
#else
    (void)initial;
#endif
}
static TZrPtr allocate(TZrPtr arguments, TZrPtr old, TZrSize oldSize,
                       TZrSize size, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)arguments;
    if (old != NULL) {
        while (index < blockCount && blocks[index].pointer != old) ++index;
        if (index == blockCount || blocks[index].size != oldSize || blocks[index].flag != flag) {
            allocatorError = 1; return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (size == 0) {
        if (old != NULL) { free(old); blocks[index] = blocks[--blockCount]; }
        return NULL;
    }
    /* A selected native resource must already exist before any fault is armed.
     * Persistent rejection includes production GC retries and normalization. */
    if (armed && old == NULL && flag == failureFlag && size == failureSize) {
        if (rejected == 0) {
            if (fs_crt_live() == 0) goto normal_allocation;
            if (selectedCase < ENTRIES) {
#ifdef _WIN32
                if (!file_is_open()) goto normal_allocation;
                openObserved = 1;
#endif
            }
        }
        ++rejected;
        return NULL;
    }
normal_allocation:
    next = realloc(old, size);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (blockCount == sizeof(blocks) / sizeof(blocks[0])) abort();
        index = blockCount++;
    }
    blocks[index] = (VmBlock){next, size, flag};
    return next;
}
typedef struct Call {
    SZrTypeValue callable, receiver, arguments[2], result;
    TZrSize count;
    int module, ok;
} Call;
static void invoke(SZrState *state, TZrPtr arguments) {
    Call *call = arguments;
    call->ok = call->module
        ? ZrLib_CallModuleExport(state, "zr.system.fs", "readText", call->arguments, 1, &call->result)
        : ZrLib_CallValue(state, &call->callable, &call->receiver, call->arguments, call->count, &call->result);
}
static int prepare_method(SZrState *state, Call *call, SZrObject *receiver, const char *name) {
    const SZrTypeValue *value = ZrLib_Object_GetFieldCString(state, receiver, name);
    if (value == NULL) return 0;
    call->callable = *value;
    ZrLib_Value_SetObject(state, &call->receiver, receiver, ZR_VALUE_TYPE_OBJECT);
    return 1;
}
static TZrSize pool_bytes(TZrSize capacity) {
    return sizeof(SZrHashPairPoolBlock) +
        (capacity > ZR_HASH_PAIR_POOL_INLINE_COUNT ? capacity - ZR_HASH_PAIR_POOL_INLINE_COUNT : 0) *
        sizeof(SZrHashKeyValuePair);
}
static void verify_payload(size_t repeats) {
    unsigned char bytes[PAYLOAD_LENGTH * 2 + 1];
    FILE *input = fopen(fixturePath, "rb");
    check(input != NULL, "open fixture content oracle");
    if (input == NULL) return;
    size_t length = fread(bytes, 1, sizeof(bytes), input);
    check(fclose(input) == 0, "close content oracle");
    check(length == PAYLOAD_LENGTH * repeats, "fixture has exactly the expected byte length");
    if (length == PAYLOAD_LENGTH * repeats)
        for (size_t i = 0; i < repeats; ++i)
            check(memcmp(bytes + i * PAYLOAD_LENGTH, payload, PAYLOAD_LENGTH) == 0,
                  "fixture retains exact registered bytes");
}
static void verify_result(SZrState *state, Call *call) {
    check(call->ok, "ordinary or recovered public call succeeds");
    if (!call->ok) return;
    if (selectedCase <= FILE_TEXT) {
        check(call->result.type == ZR_VALUE_TYPE_STRING, "text result is a real STRING");
        if (call->result.type == ZR_VALUE_TYPE_STRING) {
            SZrString *string = (SZrString *)ZrCore_Value_GetRawObject(&call->result);
            check(string != NULL && ZrCore_String_GetByteLength(string) == PAYLOAD_LENGTH &&
                  memcmp(ZrCore_String_GetNativeString(string), payload, PAYLOAD_LENGTH) == 0,
                  "text publication retains all fixture bytes");
        }
    } else if (selectedCase == RAW_APPEND) {
        check(call->result.type == ZR_VALUE_TYPE_INT64 &&
              call->result.value.nativeObject.nativeInt64 == PAYLOAD_LENGTH,
              "append publishes the complete written count");
        verify_payload(2);
    } else {
        check(call->result.type == ZR_VALUE_TYPE_ARRAY, "result is a real ARRAY");
        if (call->result.type == ZR_VALUE_TYPE_ARRAY) {
            SZrObject *array = (SZrObject *)ZrCore_Value_GetRawObject(&call->result);
            size_t length = selectedCase >= ENTRIES ? 1 : PAYLOAD_LENGTH;
            check(ZrLib_Array_Length(array) == length, "array contains all expected entries");
            if (selectedCase < ENTRIES) {
                int allBytesCorrect = 1;
                for (size_t i = 0; i < length; ++i) {
                    const SZrTypeValue *value = ZrLib_Array_Get(state, array, i);
                    if (value == NULL || value->type != ZR_VALUE_TYPE_INT64 ||
                        value->value.nativeObject.nativeInt64 != payload[i]) allBytesCorrect = 0;
                }
                check(allBytesCorrect, "readBytes retains every actual input byte");
            }
        }
    }
}
static void run(SZrState *state, const char *root, int inject) {
    Call call;
    SZrGcRootLease receiverLease = {0}, argumentLease = {0};
    SZrObject *receiver;
    SZrTypeValue path;
    enum FsCrtTarget target;
    size_t targetSize;
    memset(&call, 0, sizeof(call));
    check(ZrLib_Module_GetExport(state, "zr.system.fs", "readText") != NULL,
          "import actual FS module before preparing fault");
    if (selectedCase == MODULE_TEXT) {
        call.module = 1;
        ZrLib_Value_SetString(state, &call.arguments[0], fixturePath);
        check(ZrCore_Gc_RootLeaseAcquire(state, ZrCore_Value_GetRawObject(&call.arguments[0]), &argumentLease),
              "root module path argument");
        if (argumentLease.object == NULL) goto cleanup;
    } else {
        receiver = ZrLib_Type_NewInstance(state, selectedCase >= ENTRIES ? "Folder" : "File");
        check(receiver != NULL, "create real registered receiver");
        if (receiver == NULL) return;
        check(ZrCore_Gc_RootLeaseAcquire(state, &receiver->super, &receiverLease), "root receiver");
        if (receiverLease.object == NULL) return;
        ZrLib_Value_SetString(state, &path, selectedCase >= ENTRIES ? root : fixturePath);
        ZrLib_Object_SetFieldCString(state, receiver, "fullPath", &path);
        const char *method = selectedCase == FILE_TEXT ? "readText" :
            selectedCase == RAW_APPEND ? "appendBytes" : selectedCase == ENTRIES ? "entries" :
            selectedCase == GLOB ? "glob" : "readBytes";
        int resolved = prepare_method(state, &call, receiver, method);
        check(resolved, "resolve real public receiver method");
        if (!resolved) goto cleanup;
        if (selectedCase == GLOB) {
            ZrLib_Value_SetString(state, &call.arguments[0], "*");
            check(ZrCore_Gc_RootLeaseAcquire(state, ZrCore_Value_GetRawObject(&call.arguments[0]), &argumentLease),
                  "root glob pattern");
            ZrLib_Value_SetBool(state, &call.arguments[1], ZR_FALSE);
            call.count = 2;
        } else if (selectedCase == RAW_APPEND) {
            SZrObject *raw = ZrLib_Array_New(state);
            check(raw != NULL, "create actual raw byte-array owner");
            if (raw == NULL) goto cleanup;
            check(ZrCore_Gc_RootLeaseAcquire(state, &raw->super, &argumentLease), "root raw array");
            if (argumentLease.object == NULL) goto cleanup;
            check(raw->nodeMap.elementCount == 0 && raw->nodeMap.pairPoolCapacity == 0,
                  "raw fixture starts with a valid empty unmaterialized node map");
            int rawReady = ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, raw, PAYLOAD_LENGTH);
            check(rawReady, "allocate real Core raw backing through its public API");
            if (!rawReady) goto cleanup;
            for (size_t i = 0; i < PAYLOAD_LENGTH; ++i) raw->superArrayRawIntData[i] = payload[i];
            raw->superArrayRawIntLength = PAYLOAD_LENGTH;
            raw->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
            raw->superArrayRawIntDirty = ZR_TRUE;
            ZrLib_Value_SetObject(state, &call.arguments[0], raw, ZR_VALUE_TYPE_ARRAY);
            check(ZrLib_Array_Length(raw) == PAYLOAD_LENGTH && raw->nodeMap.elementCount == 0,
                  "public write receives a legal RAW_CANONICAL contiguous byte array");
            call.count = 1;
        }
    }
    target = selectedCase <= FILE_TEXT ? FS_CRT_TEXT : selectedCase == RAW_APPEND
        ? FS_CRT_WRITE_BYTES : selectedCase >= ENTRIES ? FS_CRT_LIST : FS_CRT_READ_BYTES;
    targetSize = selectedCase <= FILE_TEXT ? PAYLOAD_LENGTH + 1 : selectedCase == RAW_APPEND
        ? PAYLOAD_LENGTH : selectedCase >= ENTRIES ? 8 * sizeof(SZrLibrary_File_ListEntry) : 4096;
    failureFlag = selectedCase <= FILE_TEXT ? ZR_MEMORY_NATIVE_TYPE_STRING :
        selectedCase == BYTES_PUSH || selectedCase == RAW_APPEND ? ZR_MEMORY_NATIVE_TYPE_HASH_PAIR :
        ZR_MEMORY_NATIVE_TYPE_OBJECT;
    failureSize = selectedCase <= FILE_TEXT ? PAYLOAD_LENGTH + 1 :
        selectedCase == BYTES_PUSH ? pool_bytes(1) : selectedCase == RAW_APPEND
        ? pool_bytes(PAYLOAD_LENGTH) : sizeof(SZrObject);
    probe_file(1);
    const TZrUInt32 rootDepth = state->aotGcRootFrameDepth;
    fs_crt_begin(target, targetSize);
    armed = inject;
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, invoke, &call);
    armed = 0;
    if (inject) {
        check(rejected > 0, "persistent exact-category/size failure hit after observed native allocation");
#ifdef _WIN32
        if (selectedCase < ENTRIES) check(openObserved, "fault hit with this exact fixture handle open");
#endif
        check(!call.ok && (status == ZR_THREAD_STATUS_MEMORY_ERROR ||
              state->threadStatus == ZR_THREAD_STATUS_MEMORY_ERROR ||
              (state->hasCurrentException && state->currentExceptionStatus == ZR_THREAD_STATUS_MEMORY_ERROR)),
              "public call exposes the injected MemoryError");
        probe_file(0);
        if (selectedCase == RAW_APPEND) verify_payload(1);
        check(fs_crt_seen() > 0 && fs_crt_live() == 0 && fs_crt_valid(),
              "failure frees every exact observed native buffer/list block once");
        check(state->exceptionRecoverPoint == NULL && state->aotGcRootFrameDepth == rootDepth,
              "failure restores caller recovery/root-frame chain");
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        call.ok = 0;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
    }
    fs_crt_end();
    check(status == ZR_THREAD_STATUS_FINE, "ordinary/retry call returns normally");
    verify_result(state, &call);
    probe_file(0);
    check(fs_crt_seen() > 0 && fs_crt_live() == 0 && fs_crt_valid(),
          "normal/retry observes actual native allocation and one free per block");
cleanup:
    ZrCore_Gc_RootLeaseRelease(state->global, &argumentLease);
    ZrCore_Gc_RootLeaseRelease(state->global, &receiverLease);
    ZrCore_Exception_ClearCurrent(state);
}
int main(int argc, char **argv) {
    static const char *names[] = {"module_text", "file_text", "bytes_new", "bytes_push", "raw_append", "entries", "glob"};
    int found = 0, inject, length;
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 4) return 2;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (strcmp(argv[1], names[i]) == 0) { selectedCase = (enum Case)i; found = 1; }
    if (!found || (strcmp(argv[2], "oom") != 0 && strcmp(argv[2], "normal") != 0)) return 2;
    inject = strcmp(argv[2], "oom") == 0;
    if (ZrLibrary_File_Exist(argv[3]) != ZR_LIBRARY_FILE_NOT_EXIST) return 3;
    if (!ZrLibrary_File_CreateDirectories(argv[3])) return 4;
    length = snprintf(fixturePath, sizeof(fixturePath), "%s/payload.bin", argv[3]);
    if (length < 0 || (size_t)length >= sizeof(fixturePath)) return 5;
    memset(payload, 'Q', sizeof(payload));
    FILE *output = fopen(fixturePath, "wb");
    if (output == NULL) return 6;
    check(fwrite(payload, 1, sizeof(payload), output) == sizeof(payload), "write registered fixture bytes");
    check(fclose(output) == 0, "close fixture setup writer");
    global = ZrCore_GlobalState_New(allocate, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "initialize actual Core");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            check(ZrVmLibSystem_Register(global), "register actual System module");
            run(global->mainThreadState, argv[3], inject);
        }
        armed = 0; fs_crt_end(); ZrCore_GlobalState_Free(global);
    }
    check(blockCount == 0 && !allocatorError, "real Core allocator teardown is balanced (not a CRT/handle oracle)");
    return failures == 0 ? 0 : 1;
}
