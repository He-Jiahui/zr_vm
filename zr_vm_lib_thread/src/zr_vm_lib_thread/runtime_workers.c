#include "runtime_internal.h"

#if defined(ZR_PLATFORM_WIN)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_lib_thread/module.h"

static const TZrChar *kTaskPendingWorkersField = "__zr_task_pending_workers";
static const TZrChar *kTaskWorkerIsolateIdField = "__zr_task_worker_isolate_id";

typedef struct ZrVmTaskWorkerLaunch {
    TZrChar *binaryPath;
    TZrChar *projectFile;
    TZrChar *projectDirectory;
    TZrChar *projectSource;
    TZrChar *projectBinary;
    TZrChar *projectEntry;
    TZrUInt32 captureCount;
    ZrVmTaskTransportValue *captures;
    ZrVmTaskSchedulerRuntime *ownerRuntime;
    SZrObject *ownerHandle;
    FZrAllocator allocator;
    TZrPtr userAllocationArguments;
    TZrUInt64 workerIsolateId;
    TZrBool supportMultithread;
    TZrBool autoCoroutine;
} ZrVmTaskWorkerLaunch;

typedef struct ZrVmTaskWorkerExecuteRequest {
    const SZrTypeValue *callable;
    SZrTypeValue result;
    TZrBool completed;
} ZrVmTaskWorkerExecuteRequest;

static TZrChar *zr_vm_task_worker_strdup(const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (TZrChar *)malloc(length + 1);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void zr_vm_task_worker_launch_free(ZrVmTaskWorkerLaunch *launch) {
    TZrUInt32 captureIndex;

    if (launch == ZR_NULL) {
        return;
    }

    if (launch->captures != ZR_NULL) {
        for (captureIndex = 0; captureIndex < launch->captureCount; captureIndex++) {
            zr_vm_task_transport_clear(&launch->captures[captureIndex]);
        }
        free(launch->captures);
    }

    free(launch->binaryPath);
    free(launch->projectFile);
    free(launch->projectDirectory);
    free(launch->projectSource);
    free(launch->projectBinary);
    free(launch->projectEntry);
    free(launch);
}

static SZrLibrary_Project *zr_vm_task_worker_clone_project(SZrState *state, const ZrVmTaskWorkerLaunch *launch) {
    SZrLibrary_Project *project;

    if (state == ZR_NULL || state->global == ZR_NULL || launch == ZR_NULL) {
        return ZR_NULL;
    }

    project = (SZrLibrary_Project *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                    sizeof(SZrLibrary_Project),
                                                                    ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }

    memset(project, 0, sizeof(*project));
    if (launch->projectFile != ZR_NULL) {
        project->file = ZrCore_String_CreateTryHitCache(state, launch->projectFile);
    }
    if (launch->projectDirectory != ZR_NULL) {
        project->directory = ZrCore_String_CreateTryHitCache(state, launch->projectDirectory);
    }
    if (launch->projectSource != ZR_NULL) {
        project->source = ZrCore_String_CreateTryHitCache(state, launch->projectSource);
    }
    if (launch->projectBinary != ZR_NULL) {
        project->binary = ZrCore_String_CreateTryHitCache(state, launch->projectBinary);
    }
    if (launch->projectEntry != ZR_NULL) {
        project->entry = ZrCore_String_CreateTryHitCache(state, launch->projectEntry);
    }
    project->supportMultithread = launch->supportMultithread;
    project->autoCoroutine = launch->autoCoroutine;
    project->aotRuntime = ZR_NULL;
    return project;
}

static TZrBool zr_vm_task_worker_make_temp_path(TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        char tempDirectory[MAX_PATH];
        UINT result = GetTempPathA((DWORD)sizeof(tempDirectory), tempDirectory);
        if (result == 0 || result >= sizeof(tempDirectory)) {
            return ZR_FALSE;
        }
        if (GetTempFileNameA(tempDirectory, "zrt", 0, buffer) == 0) {
            return ZR_FALSE;
        }
    }
#else
    {
        const char *pattern = "/tmp/zr_task_worker_XXXXXX";
        int fd;
        snprintf(buffer, bufferSize, "%s", pattern);
        fd = mkstemp(buffer);
        if (fd < 0) {
            return ZR_FALSE;
        }
        close(fd);
    }
#endif

    return ZR_TRUE;
}

static TZrBool zr_vm_task_worker_append_pending_handle(SZrState *state, SZrObject *scheduler, SZrObject *handle) {
    SZrObject *pendingArray;
    SZrTypeValue pendingValue;
    SZrTypeValue handleValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    pendingArray = zr_vm_task_get_object_field(state, scheduler, kTaskPendingWorkersField);
    if (pendingArray == ZR_NULL || pendingArray->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        pendingArray = ZrLib_Array_New(state);
        if (pendingArray == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrLib_Value_SetObject(state, &pendingValue, pendingArray, ZR_VALUE_TYPE_ARRAY);
        zr_vm_task_set_value_field(state, scheduler, kTaskPendingWorkersField, &pendingValue);
    }

	    ZrLib_Value_SetObject(state, &handleValue, handle, ZR_VALUE_TYPE_OBJECT);
	    if (!ZrLib_Array_PushValue(state, pendingArray, &handleValue)) {
	        return ZR_FALSE;
	    }
	    return ZR_TRUE;
	}

static TZrBool zr_vm_task_worker_load_function(SZrState *state, const TZrChar *path, SZrFunction **outFunction) {
    SZrLibrary_File_Reader *reader;
    SZrIo io;
    SZrIoSource *source;

    if (state == ZR_NULL || path == ZR_NULL || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFunction = ZR_NULL;
    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)path, ZR_TRUE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, &io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io.isBinary = ZR_TRUE;
    source = ZrCore_Io_ReadSourceNew(&io);
    if (source == ZR_NULL) {
        ZrLibrary_File_CloseRead(state->global, reader);
        return ZR_FALSE;
    }

    *outFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, source);
    ZrCore_Io_ReadSourceFree(state->global, source);
    ZrLibrary_File_CloseRead(state->global, reader);
    return *outFunction != ZR_NULL;
}

static TZrBool zr_vm_task_worker_build_callable(SZrState *state,
                                                SZrFunction *function,
                                                const ZrVmTaskWorkerLaunch *launch,
                                                SZrTypeValue *callableValue) {
    SZrClosure *closure;
    TZrUInt32 captureIndex;

    if (state == ZR_NULL || function == ZR_NULL || launch == ZR_NULL || callableValue == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = ZrCore_Closure_New(state, launch->captureCount);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);
    for (captureIndex = 0; captureIndex < launch->captureCount; captureIndex++) {
        SZrTypeValue decodedValue;

        ZrLib_Value_SetNull(&decodedValue);
        if (!zr_vm_task_transport_decode_value(state, &launch->captures[captureIndex], &decodedValue)) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, &closure->closureValuesExtend[captureIndex]->link.closedValue, &decodedValue);
    }

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    callableValue->type = ZR_VALUE_TYPE_CLOSURE;
    callableValue->isGarbageCollectable = ZR_TRUE;
    callableValue->isNative = ZR_FALSE;
    return ZR_TRUE;
}

static void zr_vm_task_worker_execute_body(SZrState *state, TZrPtr arguments) {
    ZrVmTaskWorkerExecuteRequest *request = (ZrVmTaskWorkerExecuteRequest *)arguments;
    SZrFunction *function;
    TZrStackValuePointer base;
    TZrStackValuePointer resultBase;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureSlot;

    if (state == ZR_NULL || request == ZR_NULL || request->callable == ZR_NULL) {
        return;
    }

    function = ZrCore_Closure_GetMetadataFunctionFromValue(state, request->callable);
    if (function == ZR_NULL) {
        return;
    }

    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, function->stackSize + 1, base, base, &anchor);
    closureSlot = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    if (closureSlot == ZR_NULL) {
        return;
    }

    ZrCore_Value_Copy(state, closureSlot, request->callable);
    state->stackTop.valuePointer++;
    resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || resultBase == ZR_NULL) {
        return;
    }

    ZrCore_Value_Copy(state, &request->result, ZrCore_Stack_GetValue(resultBase));
    request->completed = ZR_TRUE;
}

static TZrBool zr_vm_task_worker_execute_callable(SZrState *state,
                                                  const SZrTypeValue *callableValue,
                                                  SZrTypeValue *resultValue) {
    ZrVmTaskWorkerExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || callableValue == ZR_NULL || resultValue == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    request.callable = callableValue;
    ZrLib_Value_SetNull(&request.result);
    status = ZrCore_Exception_TryRun(state, zr_vm_task_worker_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE || !request.completed || state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, resultValue, &request.result);
    return ZR_TRUE;
}

static void zr_vm_task_worker_enqueue_message(ZrVmTaskSchedulerRuntime *runtime,
                                              TZrUInt32 kind,
                                              SZrObject *handle,
                                              ZrVmTaskTransportValue *payload) {
    ZrVmTaskSchedulerMessage *message;

    if (runtime == ZR_NULL || handle == ZR_NULL || payload == ZR_NULL) {
        zr_vm_task_transport_clear(payload);
        return;
    }

    message = (ZrVmTaskSchedulerMessage *)malloc(sizeof(*message));
    if (message == ZR_NULL) {
        zr_vm_task_transport_clear(payload);
        return;
    }

    memset(message, 0, sizeof(*message));
    message->kind = kind;
    message->handle = handle;
    message->payload = *payload;
    memset(payload, 0, sizeof(*payload));

    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    if (runtime->tail != ZR_NULL) {
        runtime->tail->next = message;
    } else {
        runtime->head = message;
    }
    runtime->tail = message;
    zr_vm_task_sync_condition_signal(&runtime->condition);
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);
}

static void zr_vm_task_worker_queue_error_message(ZrVmTaskSchedulerRuntime *runtime,
                                                  SZrObject *handle,
                                                  const TZrChar *message) {
    ZrVmTaskTransportValue payload;

    memset(&payload, 0, sizeof(payload));
    payload.kind = ZR_VM_TASK_TRANSPORT_KIND_STRING;
    payload.as.stringValue = zr_vm_task_worker_strdup(message != ZR_NULL ? message : "Worker task fault");
    zr_vm_task_worker_enqueue_message(runtime, ZR_VM_TASK_SCHEDULER_MESSAGE_FAULT, handle, &payload);
}

static void zr_vm_task_worker_run_launch(ZrVmTaskWorkerLaunch *launch) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *workerGlobal;
    SZrState *workerState;
    SZrLibrary_Project *project = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue callableValue;
    SZrTypeValue resultValue;
    TZrBool completed = ZR_FALSE;

    if (launch == ZR_NULL) {
        return;
    }

    workerGlobal =
            ZrCore_GlobalState_New(launch->allocator, launch->userAllocationArguments, launch->workerIsolateId, &callbacks);
    if (workerGlobal == ZR_NULL) {
        zr_vm_task_worker_queue_error_message(launch->ownerRuntime, launch->ownerHandle, "Failed to create worker isolate");
        return;
    }

    workerState = workerGlobal->mainThreadState;
    if (workerState == ZR_NULL) {
        zr_vm_task_worker_queue_error_message(launch->ownerRuntime, launch->ownerHandle, "Worker isolate has no main state");
        ZrCore_GlobalState_Free(workerGlobal);
        return;
    }

    ZrParser_ToGlobalState_Register(workerState);
    if (!ZrCore_TaskRuntime_RegisterBuiltins(workerGlobal) || !ZrVmThread_Register(workerGlobal)) {
        zr_vm_task_worker_queue_error_message(launch->ownerRuntime,
                                              launch->ownerHandle,
                                              "Failed to register task/thread runtimes in worker isolate");
        ZrCore_GlobalState_Free(workerGlobal);
        return;
    }

    project = zr_vm_task_worker_clone_project(workerState, launch);
    if (project != ZR_NULL) {
        workerGlobal->userData = project;
        workerGlobal->sourceLoader = ZrLibrary_Project_SourceLoadImplementation;
    }

    if (!zr_vm_task_worker_load_function(workerState, launch->binaryPath, &function)) {
        zr_vm_task_worker_queue_error_message(launch->ownerRuntime, launch->ownerHandle, "Failed to load worker callable");
        goto cleanup;
    }

    ZrLib_Value_SetNull(&callableValue);
    if (!zr_vm_task_worker_build_callable(workerState, function, launch, &callableValue)) {
        zr_vm_task_worker_queue_error_message(launch->ownerRuntime,
                                              launch->ownerHandle,
                                              "Failed to materialize worker callable captures");
        goto cleanup;
    }

    ZrLib_Value_SetNull(&resultValue);
    completed = zr_vm_task_worker_execute_callable(workerState, &callableValue, &resultValue);
    if (!completed || workerState->threadStatus != ZR_THREAD_STATUS_FINE) {
        if (workerState->hasCurrentException) {
            SZrTypeValue errorCopy = workerState->currentException;
            SZrString *message = ZrCore_Value_ConvertToString(workerState, &errorCopy);
            zr_vm_task_worker_queue_error_message(launch->ownerRuntime,
                                                  launch->ownerHandle,
                                                  message != ZR_NULL ? ZrCore_String_GetNativeString(message)
                                                                     : "Worker task fault");
        } else {
            zr_vm_task_worker_queue_error_message(launch->ownerRuntime, launch->ownerHandle, "Worker task fault");
        }
        goto cleanup;
    }

    {
        ZrVmTaskTransportValue payload;

        memset(&payload, 0, sizeof(payload));
        if (!zr_vm_task_transport_encode_value(workerState,
                                               &resultValue,
                                               &payload,
                                               "spawnThread only returns sendable values and thread transport handles")) {
            zr_vm_task_worker_queue_error_message(launch->ownerRuntime,
                                                  launch->ownerHandle,
                                                  "spawnThread only returns sendable values and thread transport handles");
            goto cleanup;
        }

        zr_vm_task_worker_enqueue_message(launch->ownerRuntime,
                                          ZR_VM_TASK_SCHEDULER_MESSAGE_COMPLETE,
                                          launch->ownerHandle,
                                          &payload);
    }

cleanup:
    if (project != ZR_NULL) {
        ZrLibrary_Project_Free(workerState, project);
        workerGlobal->userData = ZR_NULL;
    }
    ZrLibrary_NativeRegistry_Free(workerGlobal);
    if (launch->binaryPath != ZR_NULL) {
        remove(launch->binaryPath);
    }
    ZrCore_GlobalState_Free(workerGlobal);
}

#if defined(ZR_PLATFORM_WIN)
static unsigned __stdcall zr_vm_task_worker_entry(void *argument) {
    ZrVmTaskWorkerLaunch *launch = (ZrVmTaskWorkerLaunch *)argument;
    zr_vm_task_worker_run_launch(launch);
    zr_vm_task_worker_launch_free(launch);
    return 0;
}
#else
static void *zr_vm_task_worker_entry(void *argument) {
    ZrVmTaskWorkerLaunch *launch = (ZrVmTaskWorkerLaunch *)argument;
    zr_vm_task_worker_run_launch(launch);
    zr_vm_task_worker_launch_free(launch);
    return ZR_NULL;
}
#endif

static TZrBool zr_vm_task_worker_start(ZrVmTaskWorkerLaunch *launch) {
    if (launch == ZR_NULL) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        uintptr_t threadHandle = _beginthreadex(ZR_NULL, 0, zr_vm_task_worker_entry, launch, 0, ZR_NULL);
        if (threadHandle == 0) {
            return ZR_FALSE;
        }
        CloseHandle((HANDLE)threadHandle);
        return ZR_TRUE;
    }
#else
    {
        pthread_t thread;
        if (pthread_create(&thread, ZR_NULL, zr_vm_task_worker_entry, launch) != 0) {
            return ZR_FALSE;
        }
        pthread_detach(thread);
        return ZR_TRUE;
    }
#endif
}

TZrBool zr_vm_task_spawn_thread_worker(ZrLibCallContext *context,
                                       const SZrTypeValue *callable,
                                       SZrTypeValue *result,
                                       SZrObject *mainScheduler) {
    SZrObject *handle;
    SZrFunction *function;
    ZrVmTaskWorkerLaunch *launch;
    SZrClosure *closure = ZR_NULL;
    SZrLibrary_Project *project;
    TZrChar tempPath[512];
    TZrUInt32 captureIndex;
    TZrUInt32 captureCount = 0;

    if (context == ZR_NULL || context->state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL ||
        mainScheduler == ZR_NULL || result->type != ZR_VALUE_TYPE_OBJECT || result->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

	    handle = ZR_CAST_OBJECT(context->state, result->value.object);
	    function = ZrCore_Closure_GetMetadataFunctionFromValue(context->state, callable);
	    if (callable->isNative || function == ZR_NULL) {
	        return zr_vm_task_raise_runtime_error(context->state, "spawnThread only supports script callables in this version");
	    }

	    if (callable->type == ZR_VALUE_TYPE_CLOSURE && !callable->isNative) {
	        closure = ZR_CAST_VM_CLOSURE(context->state, callable->value.object);
	        captureCount = closure != ZR_NULL ? (TZrUInt32)closure->closureValueCount : 0u;
	    }

    if (!zr_vm_task_worker_make_temp_path(tempPath, sizeof(tempPath)) ||
        !ZrParser_Writer_WriteBinaryFile(context->state, function, tempPath)) {
        return zr_vm_task_raise_runtime_error(context->state, "Failed to serialize worker callable");
    }

    launch = (ZrVmTaskWorkerLaunch *)malloc(sizeof(*launch));
    if (launch == ZR_NULL) {
        remove(tempPath);
        return ZR_FALSE;
    }
    memset(launch, 0, sizeof(*launch));
    launch->binaryPath = zr_vm_task_worker_strdup(tempPath);
    launch->captureCount = captureCount;
    launch->ownerRuntime = zr_vm_task_scheduler_get_runtime(context->state, mainScheduler);
    launch->ownerHandle = handle;
    launch->allocator = context->state->global->allocator;
    launch->userAllocationArguments = context->state->global->userAllocationArguments;
    launch->workerIsolateId = zr_vm_task_next_worker_isolate_id();
    launch->supportMultithread = zr_vm_task_default_support_multithread(context->state);
    launch->autoCoroutine = zr_vm_task_get_bool_field(context->state, mainScheduler, "__zr_task_auto_coroutine", ZR_TRUE);

    project = (SZrLibrary_Project *)context->state->global->userData;
    if (project != ZR_NULL) {
        launch->projectFile = project->file != ZR_NULL ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->file))
                                                       : ZR_NULL;
        launch->projectDirectory = project->directory != ZR_NULL
                                           ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->directory))
                                           : ZR_NULL;
        launch->projectSource = project->source != ZR_NULL
                                        ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->source))
                                        : ZR_NULL;
        launch->projectBinary = project->binary != ZR_NULL
                                        ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->binary))
                                        : ZR_NULL;
        launch->projectEntry = project->entry != ZR_NULL
                                       ? zr_vm_task_worker_strdup(ZrCore_String_GetNativeString(project->entry))
                                       : ZR_NULL;
    }

	    if (captureCount > 0u) {
	        launch->captures = (ZrVmTaskTransportValue *)calloc(captureCount, sizeof(ZrVmTaskTransportValue));
	        if (launch->captures == ZR_NULL) {
	            zr_vm_task_worker_launch_free(launch);
	            remove(tempPath);
	            return ZR_FALSE;
	        }

	        for (captureIndex = 0; captureIndex < captureCount; captureIndex++) {
	            const SZrTypeValue *captureValue = ZrCore_ClosureValue_GetValue(closure->closureValuesExtend[captureIndex]);
            if (!zr_vm_task_transport_encode_value(context->state,
                                                   captureValue,
                                                   &launch->captures[captureIndex],
                                                   "spawnThread only captures sendable values and thread transport handles")) {
                zr_vm_task_worker_launch_free(launch);
                remove(tempPath);
                return ZR_FALSE;
            }
        }
    }

    zr_vm_task_set_uint_field(context->state, handle, kTaskWorkerIsolateIdField, launch->workerIsolateId);
    zr_vm_task_record_last_worker_isolate(context->state, launch->workerIsolateId);
    if (!zr_vm_task_worker_append_pending_handle(context->state, mainScheduler, handle) ||
        !zr_vm_task_worker_start(launch)) {
        zr_vm_task_worker_launch_free(launch);
        remove(tempPath);
        return zr_vm_task_raise_runtime_error(context->state, "Failed to start worker isolate thread");
    }

    return ZR_TRUE;
}
