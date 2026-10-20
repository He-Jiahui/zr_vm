#include "unity.h"

#include "tests/harness/module_fixture_support.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct IoFailureAllocatorState {
    FZrAllocator upstream;
    TZrPtr upstreamArguments;
    TZrSize failOnIoAllocation;
    TZrSize ioAllocationAttempts;
    TZrSize ioAllocationFailures;
    TZrSize ioLiveAllocations;
    TZrSize ioLiveBytes;
} IoFailureAllocatorState;

typedef struct IoOomTrackedAllocation {
    TZrPtr pointer;
    TZrSize size;
    TZrInt64 type;
} IoOomTrackedAllocation;

typedef struct IoOomAllocatorState {
    FZrAllocator upstream;
    TZrPtr upstreamArguments;
    SZrState *state;
    TZrUInt32 matchingObjectFailures;
    TZrBool failOnlyOnce;
    TZrBool armed;
    const TZrChar *watchedLongString;
    TZrSize watchedLongStringLength;
    TZrPtr watchedLongStringBuffer;
    TZrBool watchedLongStringFreed;
    TZrBool readerCompleted;
    TZrSize watchedBufferFreedBeforeReaderCompleted;
    IoOomTrackedAllocation tracked[64];
    TZrSize trackedCount;
} IoOomAllocatorState;

typedef struct IoOomReadContext {
    SZrIo io;
    ZrTestsFixtureReader reader;
    const TZrByte *bytes;
    TZrSize length;
    SZrIoSource *source;
    TZrBool returnedNormally;
    IoOomAllocatorState *oomTracker;
} IoOomReadContext;

static TZrPtr io_failure_allocator(TZrPtr arguments,
                                   TZrPtr pointer,
                                   TZrSize originalSize,
                                   TZrSize newSize,
                                   TZrInt64 type) {
    IoFailureAllocatorState *tracker = (IoFailureAllocatorState *)arguments;
    TZrPtr result;

    if (type == ZR_MEMORY_NATIVE_TYPE_IO && pointer == ZR_NULL && newSize > 0u) {
        ++tracker->ioAllocationAttempts;
        if (tracker->failOnIoAllocation != 0u &&
            tracker->ioAllocationAttempts == tracker->failOnIoAllocation) {
            ++tracker->ioAllocationFailures;
            return ZR_NULL;
        }
    }
    /* Zero-sized arrays have no source-owned allocation and must stay null for matching cleanup. */
    if (type == ZR_MEMORY_NATIVE_TYPE_IO && pointer == ZR_NULL && newSize == 0u) {
        return ZR_NULL;
    }

    result = tracker->upstream(tracker->upstreamArguments, pointer, originalSize, newSize, type);
    if (type == ZR_MEMORY_NATIVE_TYPE_IO) {
        if (pointer != ZR_NULL && newSize == 0u && result == ZR_NULL) {
            --tracker->ioLiveAllocations;
            tracker->ioLiveBytes -= originalSize;
        } else if (pointer == ZR_NULL && newSize > 0u && result != ZR_NULL) {
            ++tracker->ioLiveAllocations;
            tracker->ioLiveBytes += newSize;
        }
    }
    return result;
}

/* 只跟踪 reader 可见的两类临时存储；用指针表避开 full GC 释放已有 VM 字符串。 */
static void io_oom_track_allocation(IoOomAllocatorState *tracker,
                                    TZrPtr pointer,
                                    TZrSize size,
                                    TZrInt64 type) {
    if (pointer == ZR_NULL || size == 0u ||
        (type != ZR_MEMORY_NATIVE_TYPE_IO && type != ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING)) {
        return;
    }
    if (tracker->trackedCount < sizeof(tracker->tracked) / sizeof(tracker->tracked[0])) {
        IoOomTrackedAllocation *entry = &tracker->tracked[tracker->trackedCount++];
        entry->pointer = pointer;
        entry->size = size;
        entry->type = type;
    }
}

static void io_oom_untrack_allocation(IoOomAllocatorState *tracker, TZrPtr pointer, TZrInt64 type) {
    for (TZrSize index = 0u; index < tracker->trackedCount; ++index) {
        IoOomTrackedAllocation *entry = &tracker->tracked[index];
        if (entry->pointer == pointer && entry->type == type) {
            tracker->tracked[index] = tracker->tracked[--tracker->trackedCount];
            return;
        }
    }
}

/* Fail the initial String object request and its post-GC retry, then allow MEMORY_ERROR normalization. */
static TZrPtr io_oom_upstream_allocator(TZrPtr arguments,
                                        TZrPtr pointer,
                                        TZrSize originalSize,
                                        TZrSize newSize,
                                        TZrInt64 type) {
    IoOomAllocatorState *tracker = (IoOomAllocatorState *)arguments;
    TZrPtr result;

    if (tracker->armed && pointer == ZR_NULL && originalSize == 0u &&
        newSize == sizeof(SZrString) && type == ZR_MEMORY_NATIVE_TYPE_OBJECT &&
        !tracker->state->global->garbageCollector->isImmediateGcFlag &&
        (tracker->failOnlyOnce ? tracker->matchingObjectFailures == 0u
                               : tracker->matchingObjectFailures < 2u)) {
        ++tracker->matchingObjectFailures;
        if (tracker->failOnlyOnce || tracker->matchingObjectFailures >= 2u) {
            tracker->armed = ZR_FALSE;
        }
        return ZR_NULL;
    }

    if (pointer != ZR_NULL && newSize == 0u) {
        if (type == ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING &&
            pointer == tracker->watchedLongStringBuffer) {
            tracker->watchedLongStringFreed = ZR_TRUE;
            if (!tracker->readerCompleted) {
                ++tracker->watchedBufferFreedBeforeReaderCompleted;
            }
        }
        io_oom_untrack_allocation(tracker, pointer, type);
    }
    result = tracker->upstream(tracker->upstreamArguments, pointer, originalSize, newSize, type);
    if (pointer == ZR_NULL && newSize > 0u && result != ZR_NULL) {
        io_oom_track_allocation(tracker, result, newSize, type);
        if (type == ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING &&
            tracker->watchedLongStringBuffer == ZR_NULL &&
            newSize == tracker->watchedLongStringLength + 1u &&
            memcmp(result, tracker->watchedLongString, tracker->watchedLongStringLength) == 0) {
            tracker->watchedLongStringBuffer = result;
        }
    }
    return result;
}

static void io_oom_read_body(SZrState *state, TZrPtr arguments) {
    IoOomReadContext *context = (IoOomReadContext *)arguments;
    ZR_UNUSED_PARAMETER(state);
    context->source = io_failure_read(context->io.state,
                                      context->bytes,
                                      context->length,
                                      &context->io,
                                      &context->reader);
    context->returnedNormally = ZR_TRUE;
    if (context->oomTracker != ZR_NULL) {
        context->oomTracker->readerCompleted = ZR_TRUE;
    }
}

static void io_force_gc_during_decode_body(SZrState *state, TZrPtr arguments) {
    IoOomReadContext *context = (IoOomReadContext *)arguments;
    context->source = io_failure_read(state,
                                      context->bytes,
                                      context->length,
                                      &context->io,
                                      &context->reader);
    context->returnedNormally = ZR_TRUE;
    if (context->oomTracker != ZR_NULL) {
        context->oomTracker->readerCompleted = ZR_TRUE;
    }
}

static void io_failure_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrIoSource *io_failure_read(SZrState *state,
                                    const TZrByte *bytes,
                                    TZrSize length,
                                    SZrIo *io,
                                    ZrTestsFixtureReader *reader) {
    memset(reader, 0, sizeof(*reader));
    reader->bytes = bytes;
    reader->length = length;
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, io_failure_close_noop, reader);
    io->isBinary = ZR_TRUE;
    return ZrCore_Io_ReadSourceNew(io);
}

static TZrBool io_failure_make_fixture(SZrState *state,
                                       const TZrChar *path,
                                       TZrByte **bytes,
                                       TZrSize *length) {
    static const TZrChar sourceText[] = "return 42;\n";
    static const SZrBinaryWriterOptions options = {
            "io_allocation_failure_unique_long_module_name_for_gc_oom_path",
            "io-allocation-failure-hash",
    };
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "io_allocation_failure.zr");
    SZrFunction *function;
    TZrBool result;

    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    function = ZrParser_Source_CompileTest(state, sourceText, sizeof(sourceText) - 1u, sourceName);
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    result = ZrParser_Writer_WriteBinaryFileWithOptions(state, function, path, &options);
    ZrCore_Function_Free(state, function);
    if (!result) {
        return ZR_FALSE;
    }
    *bytes = ZrTests_Fixture_ReadFileBytes(path, length);
    return *bytes != ZR_NULL;
}

void setUp(void) {}
void tearDown(void) {}

static void test_io_rejects_incompatible_fixed_header_fields(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrChar path[512];
    TZrByte *bytes = ZR_NULL;
    TZrByte *damaged = ZR_NULL;
    TZrSize length = 0u;
    SZrIo io;
    ZrTestsFixtureReader reader;
    SZrIoSource *source;
    const TZrSize headerOffsets[] = {24u, 25u, 26u, 27u};

    TEST_ASSERT_NOT_NULL(state);
    snprintf(path, sizeof(path), "%s/io_allocation_failure.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(io_failure_make_fixture(state, path, &bytes, &length));
    damaged = (TZrByte *)malloc(length);
    TEST_ASSERT_NOT_NULL(damaged);

    for (TZrSize index = 0u; index < sizeof(headerOffsets) / sizeof(headerOffsets[0]); ++index) {
        memcpy(damaged, bytes, length);
        damaged[headerOffsets[index]] ^= 1u;
        source = io_failure_read(state, damaged, length, &io, &reader);
        TEST_ASSERT_NULL(source);
        TEST_ASSERT_TRUE(io.hasReadError);
    }

    free(damaged);
    free(bytes);
    remove(path);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_io_frees_partial_source_after_native_allocation_failure(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrChar path[512];
    TZrByte *bytes = ZR_NULL;
    TZrSize length = 0u;
    SZrIo io;
    ZrTestsFixtureReader reader;
    SZrIoSource *source;
    IoFailureAllocatorState tracker;
    FZrAllocator originalAllocator;
    TZrPtr originalArguments;
    TZrBool readFailed;
    TZrBool stickyError;

    TEST_ASSERT_NOT_NULL(state);
    snprintf(path, sizeof(path), "%s/io_allocation_failure.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(io_failure_make_fixture(state, path, &bytes, &length));

    memset(&tracker, 0, sizeof(tracker));
    tracker.upstream = state->global->allocator;
    tracker.upstreamArguments = state->global->userAllocationArguments;
    tracker.failOnIoAllocation = 3u;
    originalAllocator = state->global->allocator;
    originalArguments = state->global->userAllocationArguments;
    state->global->allocator = io_failure_allocator;
    state->global->userAllocationArguments = &tracker;

    source = io_failure_read(state, bytes, length, &io, &reader);
    readFailed = source == ZR_NULL;
    stickyError = io.hasReadError;

    state->global->allocator = originalAllocator;
    state->global->userAllocationArguments = originalArguments;

    TEST_ASSERT_NULL(source);
    TEST_ASSERT_TRUE(readFailed);
    TEST_ASSERT_TRUE(stickyError);
    TEST_ASSERT_EQUAL_UINT64(3u, tracker.ioAllocationAttempts);
    TEST_ASSERT_EQUAL_UINT64(1u, tracker.ioAllocationFailures);
    TEST_ASSERT_EQUAL_UINT64(0u, tracker.ioLiveAllocations);
    TEST_ASSERT_EQUAL_UINT64(0u, tracker.ioLiveBytes);

    free(bytes);
    remove(path);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_io_gc_oom_recovery_releases_source_and_temporary_string(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrState *writerState = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrChar path[512];
    TZrByte *bytes = ZR_NULL;
    TZrSize length = 0u;
    IoOomAllocatorState tracker;
    IoOomReadContext context;
    FZrAllocator originalUpstream;
    TZrPtr originalUpstreamArguments;
    TZrSize baselineIgnoredObjects;
    TZrSize ignoredObjectsAfterTry;
    EZrThreadStatus tryStatus;
    TZrSize outstandingTrackedAllocations;
    TZrBool readReturnedNormally;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(writerState);
    snprintf(path, sizeof(path), "%s/io_allocation_failure.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(io_failure_make_fixture(writerState, path, &bytes, &length));

    memset(&tracker, 0, sizeof(tracker));
    tracker.upstream = state->global->upstreamAllocator;
    tracker.upstreamArguments = state->global->upstreamAllocationArguments;
    tracker.state = state;
    tracker.armed = ZR_TRUE;
    originalUpstream = state->global->upstreamAllocator;
    originalUpstreamArguments = state->global->upstreamAllocationArguments;
    state->global->upstreamAllocator = io_oom_upstream_allocator;
    state->global->upstreamAllocationArguments = &tracker;

    memset(&context, 0, sizeof(context));
    context.bytes = bytes;
    context.length = length;
    context.oomTracker = &tracker;
    ZrCore_Io_Init(state,
                   &context.io,
                   ZrTests_Fixture_ReaderRead,
                   io_failure_close_noop,
                   &context.reader);
    context.io.isBinary = ZR_TRUE;
    baselineIgnoredObjects = state->global->garbageCollector->ignoredObjectCount;
    tryStatus = ZrCore_Exception_TryRun(state, io_oom_read_body, &context);
    ignoredObjectsAfterTry = state->global->garbageCollector->ignoredObjectCount;
    readReturnedNormally = context.returnedNormally;
    outstandingTrackedAllocations = tracker.trackedCount;

    tracker.armed = ZR_FALSE;
    state->global->upstreamAllocator = originalUpstream;
    state->global->upstreamAllocationArguments = originalUpstreamArguments;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_MEMORY_ERROR, tryStatus);
    TEST_ASSERT_FALSE(readReturnedNormally);
    TEST_ASSERT_NULL(context.source);
    TEST_ASSERT_EQUAL_UINT32(2u, tracker.matchingObjectFailures);
    TEST_ASSERT_EQUAL_UINT64(baselineIgnoredObjects, ignoredObjectsAfterTry);
    TEST_ASSERT_EQUAL_UINT64(0u, outstandingTrackedAllocations);

    free(bytes);
    remove(path);
    ZrTests_Runtime_State_Destroy(state);
    ZrTests_Runtime_State_Destroy(writerState);
}

static void test_io_roots_decoded_long_strings_across_gc(void) {
    static const TZrChar expectedName[] = "reader_rooted_long_module_name_survives_forced_gc";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrState *writerState = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrChar path[512];
    TZrByte *bytes = ZR_NULL;
    TZrSize length = 0u;
    IoOomAllocatorState tracker;
    IoOomReadContext context;
    FZrAllocator originalUpstream;
    TZrPtr originalUpstreamArguments;
    EZrThreadStatus tryStatus;
    TZrBool nameStillMatches = ZR_FALSE;
    TZrBool watchedBufferFreed;
    TZrSize watchedBufferPrematureFreeCount;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(writerState);
    snprintf(path, sizeof(path), "%s/io_allocation_failure.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(io_failure_make_fixture(writerState, path, &bytes, &length));
    /* Re-write with a distinct long module name so the watched decoded string cannot hit the fresh-state cache. */
    {
        static const SZrBinaryWriterOptions options = {
                expectedName,
                "reader_rooted_long_module_hash_survives_forced_gc",
        };
        static const TZrChar sourceText[] = "return 42;\n";
        SZrString *sourceName = ZrCore_String_CreateFromNative(writerState, "io_force_gc.zr");
        SZrFunction *function = ZrParser_Source_CompileTest(
                writerState, sourceText, sizeof(sourceText) - 1u, sourceName);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(writerState, function, path, &options));
        ZrCore_Function_Free(writerState, function);
        free(bytes);
        bytes = ZrTests_Fixture_ReadFileBytes(path, &length);
        TEST_ASSERT_NOT_NULL(bytes);
    }

    memset(&tracker, 0, sizeof(tracker));
    tracker.upstream = state->global->upstreamAllocator;
    tracker.upstreamArguments = state->global->upstreamAllocationArguments;
    tracker.state = state;
    tracker.failOnlyOnce = ZR_TRUE;
    tracker.armed = ZR_TRUE;
    tracker.watchedLongString = expectedName;
    tracker.watchedLongStringLength = sizeof(expectedName) - 1u;
    originalUpstream = state->global->upstreamAllocator;
    originalUpstreamArguments = state->global->upstreamAllocationArguments;
    state->global->upstreamAllocator = io_oom_upstream_allocator;
    state->global->upstreamAllocationArguments = &tracker;

    memset(&context, 0, sizeof(context));
    context.bytes = bytes;
    context.length = length;
    context.oomTracker = &tracker;
    ZrCore_Io_Init(state,
                   &context.io,
                   ZrTests_Fixture_ReaderRead,
                   io_failure_close_noop,
                   &context.reader);
    context.io.isBinary = ZR_TRUE;
    tryStatus = ZrCore_Exception_TryRun(state, io_force_gc_during_decode_body, &context);
    {
        TZrBool hasSource = context.source != ZR_NULL;
    if (context.source != ZR_NULL && context.source->modulesLength > 0u &&
        context.source->modules[0].name != ZR_NULL) {
        TZrNativeString name = ZrCore_String_GetNativeString(context.source->modules[0].name);
        nameStillMatches = (TZrBool)(name != ZR_NULL && strcmp(name, expectedName) == 0);
    }
        context.returnedNormally = (TZrBool)(context.returnedNormally && hasSource);
    }
    watchedBufferFreed = tracker.watchedLongStringFreed;
    watchedBufferPrematureFreeCount = tracker.watchedBufferFreedBeforeReaderCompleted;
    tracker.readerCompleted = ZR_TRUE;

    if (context.source != ZR_NULL) {
        ZrCore_Io_ReadSourceFree(state->global, context.source);
        context.source = ZR_NULL;
    }
    state->global->upstreamAllocator = originalUpstream;
    state->global->upstreamAllocationArguments = originalUpstreamArguments;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, tryStatus);
    TEST_ASSERT_TRUE(context.returnedNormally);
    TEST_ASSERT_TRUE(nameStillMatches);
    TEST_ASSERT_FALSE(watchedBufferFreed);
    TEST_ASSERT_EQUAL_UINT64(0u, watchedBufferPrematureFreeCount);

    free(bytes);
    remove(path);
    ZrTests_Runtime_State_Destroy(state);
    ZrTests_Runtime_State_Destroy(writerState);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_io_rejects_incompatible_fixed_header_fields);
    RUN_TEST(test_io_frees_partial_source_after_native_allocation_failure);
    RUN_TEST(test_io_gc_oom_recovery_releases_source_and_temporary_string);
    RUN_TEST(test_io_roots_decoded_long_strings_across_gc);
    return UNITY_END();
}
