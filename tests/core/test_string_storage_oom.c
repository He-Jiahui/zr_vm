/* Real Core storage failures: raw string buffers and intern-table/cache commits.
 * Each argv case runs in its own process; no implementation is substituted. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;
static Allocation allocations[32768];
static size_t liveCount, matches, rejected;
static int allocatorError, failures, armed;
static TZrInt64 failFlag;
static TZrSize failSize;
static size_t failOrdinal;
static void *failOld;

static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s (live=%zu rejected=%zu)\n",
            condition ? "PASS" : "FAIL", message, liveCount, rejected);
    if (!condition) ++failures;
}

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize ||
            allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) {
        allocatorError = 1;
        return NULL;
    }
    if (newSize == 0) {
        if (old != NULL) {
            free(old);
            allocations[index] = allocations[--liveCount];
        }
        return NULL;
    }
    if (armed && flag == failFlag && newSize == failSize && old == failOld) {
        if (++matches >= failOrdinal) {
            ++rejected;
            fprintf(stderr, "reject flag=%lld size=%zu ordinal=%zu\n",
                    (long long)flag, (size_t)newSize, matches);
            fflush(stderr);
            return NULL;
        }
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

typedef struct Call {
    const char *text;
    SZrString *result;
    int mode; /* 0: Create, 1: FromNumber, 2: CreateTryHitCache */
} Call;

static void invoke(SZrState *state, TZrPtr data) {
    Call *call = (Call *)data;
    if (call->mode == 1) {
        SZrTypeValue number;
        ZrCore_Value_InitAsInt(state, &number, 123456789);
        call->result = ZrCore_String_FromNumber(state, &number);
    } else if (call->mode == 2) {
        call->result = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)call->text);
    } else {
        call->result = ZrCore_String_Create(state, (TZrNativeString)call->text,
                                          (TZrSize)strlen(call->text));
    }
}

static int cache_is_nonnull(SZrGlobalState *global) {
    TZrSize bucket, depth;
    for (bucket = 0; bucket < ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT; ++bucket) {
        for (depth = 0; depth < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH; ++depth) {
            if (global->stringHashApiCache[bucket][depth] == NULL) return 0;
        }
    }
    return 1;
}

/* Use the actual table threshold, without changing capacity or GC internals. */
static int fill_to_threshold(SZrState *state) {
    SZrHashSet *set = &state->global->stringTable->stringHashSet;
    unsigned int index;
    for (index = 0; set->elementCount < set->resizeThreshold && index < 100000U; ++index) {
        char text[32];
        Call call = {text, NULL, 0};
        snprintf(text, sizeof(text), "storage-fill-%u", index);
        if (ZrCore_Exception_TryRun(state, invoke, &call) != ZR_THREAD_STATUS_FINE ||
            call.result == NULL) return 0;
    }
    return set->elementCount == set->resizeThreshold;
}

static void test_case(SZrGlobalState *global, const char *name) {
    SZrState *state = global->mainThreadState;
    SZrHashSet *set = &global->stringTable->stringHashSet;
    char longText[ZR_VM_SHORT_STRING_MAX + 129U];
    char cacheText[64];
    SZrString *savedCache[ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT][ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH];
    Call call = {"storage-rehash-probe", NULL, 0};
    EZrThreadStatus status;
    void *oldBuckets = NULL;
    TZrSize oldCapacity = 0, oldCount = 0;
    SZrString *oldHead = NULL;
    int rehash = strcmp(name, "short_rehash") == 0 || strcmp(name, "cache_rehash") == 0;
    size_t index;
    for (index = 0; index + 1 < sizeof(longText); ++index) longText[index] = 'x';
    longText[sizeof(longText) - 1] = '\0';
    if (strcmp(name, "cache_rehash") == 0) {
        /* Seed distinct valid entries in one bucket, using the same native
         * address. A failed miss must preserve both their identity and order. */
        Call seed = {cacheText, NULL, 2};
        for (index = 0; index < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH; ++index) {
            snprintf(cacheText, sizeof(cacheText), "storage-cache-seed-%zu", index);
            status = ZrCore_Exception_TryRun(state, invoke, &seed);
            check(status == ZR_THREAD_STATUS_FINE && seed.result != NULL,
                  "distinct valid cache entries are established before injection");
            if (status != ZR_THREAD_STATUS_FINE || seed.result == NULL) return;
        }
        memcpy(cacheText, "storage-rehash-probe", sizeof("storage-rehash-probe"));
        call.text = cacheText;
    }
    if (rehash) {
        int ready = fill_to_threshold(state);
        check(ready, "real intern table reaches its growth threshold");
        if (!ready) return;
        oldBuckets = set->buckets;
        oldCapacity = set->capacity;
        oldCount = set->elementCount;
        oldHead = global->stringTable->shortStringListHead;
        failFlag = ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET;
        failSize = set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR * sizeof(TZrPtr);
        failOld = set->buckets;
        if (strcmp(name, "cache_rehash") == 0) call.mode = 2;
    } else {
        failFlag = ZR_MEMORY_NATIVE_TYPE_STRING;
        failOld = NULL;
        if (strcmp(name, "number_buffer") == 0) {
            call.mode = 1;
            failSize = ZR_NUMBER_TO_STRING_LENGTH_MAX;
        } else {
            call.text = longText;
            failSize = sizeof(longText);
        }
    }
    check(cache_is_nonnull(global), "cache starts fully initialized");
    memcpy(savedCache, global->stringHashApiCache, sizeof(savedCache));
    failOrdinal = 1;
    armed = 1;
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    /* Persistent for this allocation class/size/address until protected return. */
    check(armed && rejected > 0, "intended storage allocation actually failed");
    check((status == ZR_THREAD_STATUS_FINE && call.result == NULL) ||
                  status == ZR_THREAD_STATUS_MEMORY_ERROR,
          "storage failure returns NULL or raises protected MemoryError");
    check(state->exceptionRecoverPoint == NULL, "protected call restores its recovery chain");
    if (rehash) {
        check(set->buckets == oldBuckets && set->capacity == oldCapacity &&
                      set->elementCount == oldCount,
              "failed rehash preserves bucket storage and element count");
        check(global->stringTable->shortStringListHead == oldHead,
              "failed intern insertion unlinks its candidate");
    }
    check(cache_is_nonnull(global), "failed creation never publishes a NULL cache entry");
    if (call.mode == 2) {
        check(memcmp(savedCache, global->stringHashApiCache, sizeof(savedCache)) == 0,
              "failed cache miss preserves all previous entries and their order");
    }
    armed = 0;
    ZrCore_Exception_ClearCurrent(state);
    /* Avoid a second known NULL dereference in the RED fixture. The invariant
     * above already fails; repaired code must take and pass the real retry. */
    if (call.mode != 2 || cache_is_nonnull(global)) {
        const char *expected = call.mode == 1 ? "123456789" : call.text;
        call.result = NULL;
        status = ZrCore_Exception_TryRun(state, invoke, &call);
        check(status == ZR_THREAD_STATUS_FINE && call.result != NULL,
              "same operation retries after removing the fault");
        if (status == ZR_THREAD_STATUS_FINE && call.result != NULL) {
            SZrString *first = call.result;
            check(ZrCore_String_GetByteLength(first) == strlen(expected) &&
                          strcmp(ZrCore_String_GetNativeString(first), expected) == 0,
                  "retry preserves complete text");
            if (call.mode == 2) {
                status = ZrCore_Exception_TryRun(state, invoke, &call);
                check(status == ZR_THREAD_STATUS_FINE && call.result == first,
                      "same native address hits the successfully retried cache entry");
            }
        }
    }
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || (strcmp(argv[1], "long_payload") != 0 &&
                      strcmp(argv[1], "number_buffer") != 0 &&
                      strcmp(argv[1], "short_rehash") != 0 &&
                      strcmp(argv[1], "cache_rehash") != 0)) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "Core initializes before injection");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) test_case(global, argv[1]);
        armed = 0;
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "Core teardown releases every tracked allocation with correct metadata");
    return failures == 0 ? 0 : 1;
}
