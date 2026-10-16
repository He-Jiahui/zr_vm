/* Public Library readers with a valid direct-argument context and actual Core.
 * ReadInt keeps its exception contract; Math helpers are not used. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_library/native_binding.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError, failures;
#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

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

typedef struct Read {
    ZrLibCallContext context;
    SZrTypeValue argument;
    TZrInt64 integer;
    TZrFloat64 number;
    TZrBool returned;
    int nullOutput;
    int readFloat;
} Read;
static const ZrLibFunctionDescriptor descriptor = {.name = "numericProbe"};

static void read_value(SZrState *state, TZrPtr data) {
    Read *read = (Read *)data;
    (void)state;
    if (read->readFloat)
        read->returned = ZrLib_CallContext_ReadFloat(&read->context, 0,
                read->nullOutput ? NULL : &read->number);
    else
        read->returned = ZrLib_CallContext_ReadInt(&read->context, 0,
                read->nullOutput ? NULL : &read->integer);
}
static void initialize_read(SZrState *state, Read *read) {
    memset(read, 0, sizeof(*read));
    read->context.state = state;
    read->context.functionDescriptor = &descriptor;
    read->context.argumentValues = &read->argument;
    read->context.argumentCount = 1;
    read->integer = 12345;
    read->number = 12345;
}
static void set_double(Read *read, double number) {
    read->argument.type = ZR_VALUE_TYPE_DOUBLE;
    read->argument.value.nativeObject.nativeDouble = number;
}
static const char *exception_message(SZrState *state) {
    const SZrTypeValue *message;
    if (!state->hasCurrentException || state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == NULL) return NULL;
    message = ZrLib_Object_GetFieldCString(state,
            (SZrObject *)state->currentException.value.object, "message");
    if (message == NULL || message->type != ZR_VALUE_TYPE_STRING ||
        message->value.object == NULL) return NULL;
    return ZrCore_String_GetNativeString((SZrString *)message->value.object);
}
static void expect_error(SZrState *state, Read *read, const char *fragment) {
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, read_value, read);
    const char *message;
    EXPECT(status == ZR_THREAD_STATUS_RUNTIME_ERROR);
    EXPECT(!read->returned && read->integer == 12345 && read->number == 12345);
    EXPECT(state->exceptionRecoverPoint == NULL);
    message = exception_message(state);
    EXPECT(message != NULL && strstr(message, "numericProbe") != NULL &&
           strstr(message, fragment) != NULL);
    if (read->context.argumentCount != 0)
        EXPECT(message != NULL && strstr(message, "expected int") != NULL);
    ZrCore_Exception_ClearCurrent(state);
}
static void rejected_double(SZrState *state, double number, int nullOutput) {
    Read read;
    initialize_read(state, &read);
    set_double(&read, number);
    read.nullOutput = nullOutput;
    expect_error(state, &read, "argument 1");
}
static void accepted_double(SZrState *state, double number, TZrInt64 expected) {
    Read read;
    initialize_read(state, &read);
    set_double(&read, number);
    EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
    EXPECT(read.returned && read.integer == expected && !state->hasCurrentException);
}
static void valid_ints(SZrState *state) {
    Read read;
    accepted_double(state, 7.9, 7);
    accepted_double(state, -7.9, -7);
    accepted_double(state, -0.75, 0);
    accepted_double(state, -0x1p63, INT64_MIN);
    accepted_double(state, 0x1.fffffffffffffp62, INT64_MAX - INT64_C(1023));
    initialize_read(state, &read);
    read.argument.type = ZR_VALUE_TYPE_INT64;
    read.argument.value.nativeObject.nativeInt64 = INT64_MAX;
    EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
    EXPECT(read.returned && read.integer == INT64_MAX);
    initialize_read(state, &read);
    read.argument.type = ZR_VALUE_TYPE_UINT64;
    read.argument.value.nativeObject.nativeUInt64 = 42;
    EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
    EXPECT(read.returned && read.integer == 42);
}
static void float_controls(SZrState *state) {
    const double inputs[] = {NAN, INFINITY, -INFINITY, 1e300, -7.9};
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        Read read;
        initialize_read(state, &read);
        set_double(&read, inputs[i]);
        read.readFloat = 1;
        EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
        EXPECT(read.returned && !state->hasCurrentException);
        EXPECT(isnan(inputs[i]) ? isnan(read.number) : read.number == inputs[i]);
    }
}
static void run_case(SZrState *state, const char *name) {
    if (strcmp(name, "int_controls") == 0) valid_ints(state);
    else if (strcmp(name, "finite_range") == 0) {
        rejected_double(state, 1e300, 0);
        rejected_double(state, -1e300, 0);
    } else if (strcmp(name, "nonfinite") == 0) {
        rejected_double(state, NAN, 0);
        rejected_double(state, INFINITY, 0);
        rejected_double(state, -INFINITY, 0);
    } else if (strcmp(name, "boundaries") == 0) {
        rejected_double(state, 0x1p63, 0);
        rejected_double(state, -0x1.0000000000001p63, 0);
    } else if (strcmp(name, "null_output") == 0) {
        Read read;
        initialize_read(state, &read);
        set_double(&read, 3.9);
        read.nullOutput = 1;
        EXPECT(ZrCore_Exception_TryRun(state, read_value, &read) == ZR_THREAD_STATUS_FINE);
        EXPECT(read.returned && read.integer == 12345);
        rejected_double(state, 1e300, 1);
        rejected_double(state, NAN, 1);
    } else if (strcmp(name, "type_diagnostic") == 0) {
        Read read;
        initialize_read(state, &read);
        read.argument.type = ZR_VALUE_TYPE_BOOL;
        expect_error(state, &read, "expected int");
    } else if (strcmp(name, "arity_diagnostic") == 0) {
        Read read;
        initialize_read(state, &read);
        read.context.argumentCount = 0;
        expect_error(state, &read, "expected at least 1 arguments");
    } else if (strcmp(name, "float_controls") == 0) float_controls(state);
    else ++failures;
}
int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    EXPECT(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) {
            global->garbageCollector->stopGcFlag = ZR_TRUE;
            run_case(global->mainThreadState, argv[1]);
        }
        ZrCore_GlobalState_Free(global);
    }
    EXPECT(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}
