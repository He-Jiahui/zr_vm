/* Call the registered descriptor constructor, never schedule a job. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_lib_thread/module.h"

static int failures;
static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}
static TZrPtr allocate(TZrPtr context, TZrPtr old, TZrSize oldSize, TZrSize newSize, TZrInt64 flag) {
    (void)context; (void)oldSize; (void)flag;
    if (newSize == 0) { free(old); return NULL; }
    return realloc(old, newSize);
}
typedef struct Construct {
    ZrLibCallContext context;
    SZrTypeValue self, argument, result;
    TZrBool ok;
    int returned;
} Construct;
static void invoke(SZrState *state, TZrPtr data) {
    Construct *call = data;
    (void)state;
    call->ok = call->context.metaMethodDescriptor->callback(&call->context, &call->result);
    call->returned = 1;
}
static void run(SZrState *state, TZrInt64 count) {
    const ZrLibModuleDescriptor *module = ZrVmThread_GetModuleDescriptor();
    const ZrLibTypeDescriptor *type = NULL;
    const ZrLibMetaMethodDescriptor *constructor = NULL;
    SZrGcRootLease lease = {0};
    SZrObject *receiver;
    SZrObjectPrototype *prototype;
    EZrThreadStatus status;
    Construct call;
    for (TZrSize index = 0; index < module->typeCount; ++index)
        if (strcmp(module->types[index].name, "ThreadScheduler") == 0) type = &module->types[index];
    if (type != NULL)
        for (TZrSize index = 0; index < type->metaMethodCount; ++index)
            if (type->metaMethods[index].metaType == ZR_META_CONSTRUCTOR) constructor = &type->metaMethods[index];
    check(constructor != NULL && constructor->callback != NULL, "find real public descriptor constructor");
    if (constructor == NULL || constructor->callback == NULL) return;
    prototype = ZrLib_Type_FindPrototype(state, "zr.thread.ThreadScheduler");
    check(prototype != NULL, "import registered scheduler prototype");
    if (prototype == NULL) return;
    receiver = ZrLib_Type_NewInstanceWithPrototype(state, prototype);
    check(receiver != NULL, "create real empty receiver before constructor");
    if (receiver == NULL) return;
    check(ZrCore_Gc_RootLeaseAcquire(state, &receiver->super, &lease), "lease constructor receiver");
    if (lease.object == NULL) return;
    check(receiver->nodeMap.elementCount == 0, "receiver starts without scheduler configuration");
    memset(&call, 0, sizeof(call));
    ZrLib_Value_SetObject(state, &call.self, receiver, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetInt(state, &call.argument, count);
    call.context.state = state;
    call.context.moduleDescriptor = module;
    call.context.typeDescriptor = type;
    call.context.metaMethodDescriptor = constructor;
    call.context.ownerPrototype = prototype;
    call.context.constructTargetPrototype = prototype;
    call.context.selfValue = &call.self;
    call.context.argumentValues = &call.argument;
    call.context.argumentCount = 1;
    status = ZrCore_Exception_TryRun(state, invoke, &call);
    if (count == 1) {
        check(status == ZR_THREAD_STATUS_FINE && call.returned && call.ok,
              "one-worker configuration constructs normally without scheduling");
        check(call.result.type == ZR_VALUE_TYPE_OBJECT &&
              ZrCore_Value_GetRawObject(&call.result) == &receiver->super,
              "constructor returns its actual receiver");
        const SZrTypeValue *stored = ZrLib_Object_GetFieldCString(state, receiver,
            "__zr_thread_scheduler_worker_count");
        check(stored != NULL && stored->type == ZR_VALUE_TYPE_INT64 &&
              stored->value.nativeObject.nativeInt64 == 1, "normal worker count is retained");
    } else {
        /* This descriptor returns false after NormalizeThrownValue(EXCEPTION_ERROR);
         * it does not Throw on ordinary validation failure. TryRun stays FINE. */
        check(status == ZR_THREAD_STATUS_FINE && call.returned && !call.ok &&
              state->threadStatus == ZR_THREAD_STATUS_EXCEPTION_ERROR &&
              state->hasCurrentException && state->currentExceptionStatus == ZR_THREAD_STATUS_EXCEPTION_ERROR,
              "invalid count preserves descriptor false plus ExceptionError validation contract");
        check(receiver->nodeMap.elementCount == 0,
              "reject before queue, count, policy or native provider publication");
    }
    check(state->exceptionRecoverPoint == NULL, "restore caller recovery chain");
    ZrCore_Gc_RootLeaseRelease(state->global, &lease);
    ZrCore_Exception_ClearCurrent(state);
}
int main(int argc, char **argv) {
    TZrInt64 count;
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "wrap_zero") == 0) count = INT64_C(4294967296);
    else if (strcmp(argv[1], "wrap_one") == 0) count = INT64_C(4294967297);
    else if (strcmp(argv[1], "int64_max") == 0) count = INT64_MAX;
    else if (strcmp(argv[1], "normal_one") == 0) count = 1;
    else if (strcmp(argv[1], "zero") == 0) count = 0;
    else if (strcmp(argv[1], "negative") == 0) count = -1;
    else return 2;
    global = ZrCore_GlobalState_New(allocate, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "initialize actual Core");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            check(ZrCore_TaskRuntime_RegisterBuiltins(global) && ZrVmThread_Register(global), "register real task/thread modules");
            run(global->mainThreadState, count);
        }
        ZrCore_GlobalState_Free(global);
    }
    /* No schedule call: both real backends defer OS worker start to schedule.
     * Existing native provider destruction gap is not asserted fixed here. */
    return failures == 0 ? 0 : 1;
}
