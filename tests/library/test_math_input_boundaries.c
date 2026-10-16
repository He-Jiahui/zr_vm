/* Actual Math support API boundaries. Fractional numbers retain truncation.
 * The test computes no out-of-range floating-to-integer casts or signed products. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_lib_math/math_common.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int failures, allocatorError;
#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    void *next;
    (void)data;
    if (old != NULL) {
        while (i < liveCount && allocations[i].pointer != old) ++i;
        if (i == liveCount || allocations[i].size != oldSize || allocations[i].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) { allocatorError = 1; return NULL; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[i] = allocations[--liveCount]; }
        return NULL;
    }
    next = realloc(old, (size_t)newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        i = liveCount++;
    }
    allocations[i].pointer = next;
    allocations[i].size = newSize;
    allocations[i].flag = flag;
    return next;
}

static SZrTypeValue float_value(double input) {
    SZrTypeValue value;
    memset(&value, 0, sizeof(value));
    value.type = ZR_VALUE_TYPE_DOUBLE;
    value.value.nativeObject.nativeDouble = input;
    return value;
}
static void rejected_float(double input) {
    SZrTypeValue value = float_value(input);
    TZrInt64 out = 12345;
    EXPECT(!ZrMath_IntFromValue(&value, &out));
    EXPECT(out == 12345);
}
static void accepted_float(double input, TZrInt64 expected) {
    SZrTypeValue value = float_value(input);
    TZrInt64 out = 12345;
    EXPECT(ZrMath_IntFromValue(&value, &out));
    EXPECT(out == expected);
}
static void int_controls(void) {
    SZrTypeValue value;
    TZrInt64 out = 0;
    accepted_float(7.9, 7);
    accepted_float(-7.9, -7);
    accepted_float(-0.75, 0);
    accepted_float(0.0, 0);
    memset(&value, 0, sizeof(value));
    value.type = ZR_VALUE_TYPE_INT64;
    value.value.nativeObject.nativeInt64 = INT64_MAX;
    EXPECT(ZrMath_IntFromValue(&value, &out) && out == INT64_MAX);
    value.value.nativeObject.nativeInt64 = INT64_MIN;
    EXPECT(ZrMath_IntFromValue(&value, &out) && out == INT64_MIN);
    value.type = ZR_VALUE_TYPE_BOOL;
    out = 12345;
    EXPECT(!ZrMath_IntFromValue(&value, &out) && out == 12345);
}
static void int_boundaries(void) {
    /* Double(INT64_MAX) rounds to +2^63: it must not be used as an inclusive bound. */
    accepted_float(-0x1p63, INT64_MIN);
    accepted_float(0x1.fffffffffffffp62, INT64_MAX - INT64_C(1023));
    rejected_float(0x1p63);
    rejected_float(-0x1.0000000000001p63);
}
static SZrObject *new_array(SZrState *state) {
    SZrObject *result = ZrLib_Array_New(state);
    EXPECT(result != NULL);
    return result;
}
static void push_int(SZrState *state, SZrObject *array, TZrInt64 input) {
    SZrTypeValue value;
    ZrLib_Value_SetInt(state, &value, input);
    EXPECT(ZrLib_Array_PushValue(state, array, &value));
}
static void push_float(SZrState *state, SZrObject *array, double input) {
    SZrTypeValue value = float_value(input);
    EXPECT(ZrLib_Array_PushValue(state, array, &value));
}
static SZrObject *new_tensor_shell(SZrState *state) {
    SZrObject *tensor = ZrCore_Object_New(state, NULL);
    EXPECT(tensor != NULL);
    if (tensor != NULL) ZrCore_Object_Init(state, tensor);
    return tensor;
}
static void shape_product(SZrState *state) {
    SZrObject *shape = new_array(state);
    SZrObject *data = new_array(state);
    SZrObject *tensor = new_tensor_shell(state);
    if (shape == NULL || data == NULL || tensor == NULL) return;
    push_int(state, shape, INT64_MAX);
    push_int(state, shape, 2);
    EXPECT(ZrMath_TensorTotalSize(state, shape) == -1);
    EXPECT(!ZrMath_TensorPopulate(state, tensor, shape, data));
}
static void array_conversion(SZrState *state) {
    SZrObject *shape = new_array(state);
    TZrInt64 out = 12345;
    if (shape == NULL) return;
    push_float(state, shape, 1e300);
    EXPECT(!ZrMath_ArrayReadInt(state, shape, 0, &out) && out == 12345);
    EXPECT(ZrMath_TensorTotalSize(state, shape) == -1);
}
static void tensor_data(SZrState *state) {
    SZrObject *shape = new_array(state);
    SZrTypeValue invalid[3];
    if (shape == NULL) return;
    push_int(state, shape, 1);
    ZrLib_Value_SetString(state, &invalid[0], "not-a-number");
    ZrLib_Value_SetBool(state, &invalid[1], ZR_TRUE);
    ZrCore_Value_ResetAsNull(&invalid[2]);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        SZrObject *data = new_array(state);
        SZrObject *tensor = new_tensor_shell(state);
        if (data == NULL || tensor == NULL) return;
        EXPECT(ZrLib_Array_PushValue(state, data, &invalid[i]));
        EXPECT(!ZrMath_TensorPopulate(state, tensor, shape, data));
    }
}
static void tensor_controls(SZrState *state, int invalidStorage) {
    SZrObject *shape = new_array(state);
    SZrObject *data = new_array(state);
    SZrObject *tensor = new_tensor_shell(state);
    ZrMathTensorStorage storage;
    if (shape == NULL || data == NULL || tensor == NULL) return;
    /* Existing shape policy truncates positive fractional dimensions. */
    push_float(state, shape, 2.9);
    push_float(state, shape, 1.9);
    push_int(state, data, 4);
    push_float(state, data, 5.5);
    EXPECT(ZrMath_TensorTotalSize(state, shape) == 2);
    EXPECT(ZrMath_TensorPopulate(state, tensor, shape, data));
    EXPECT(ZrMath_TensorGetStorage(state, tensor, &storage));
    EXPECT(storage.rank == 2 && storage.size == 2 && storage.shape == shape && storage.data == data);
    if (invalidStorage) {
        SZrTypeValue bad = float_value(1e300);
        /* Existing public rank/size fields feed the same support conversion. */
        ZrLib_Object_SetFieldCString(state, tensor, "rank", &bad);
        EXPECT(!ZrMath_TensorGetStorage(state, tensor, &storage));
        ZrMath_WriteIntField(state, tensor, "rank", 2);
        ZrLib_Object_SetFieldCString(state, tensor, "size", &bad);
        EXPECT(!ZrMath_TensorGetStorage(state, tensor, &storage));
    }
}
int main(int argc, char **argv) {
    SZrGlobalState *global = NULL;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "int_controls") == 0) int_controls();
    else if (strcmp(argv[1], "int_finite_range") == 0) {
        rejected_float(1e300);
        rejected_float(-1e300);
    } else if (strcmp(argv[1], "int_nonfinite") == 0) {
        rejected_float(NAN);
        rejected_float(INFINITY);
        rejected_float(-INFINITY);
    } else if (strcmp(argv[1], "int_boundaries") == 0) int_boundaries();
    else {
        global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
        EXPECT(global != NULL && ZrCore_GlobalState_IsInitialized(global));
        if (global != NULL && ZrCore_GlobalState_IsInitialized(global)) {
            /* Fixture addresses remain borrowed for the short support calls. */
            global->garbageCollector->stopGcFlag = ZR_TRUE;
            if (strcmp(argv[1], "shape_product") == 0) shape_product(global->mainThreadState);
            else if (strcmp(argv[1], "array_conversion") == 0) array_conversion(global->mainThreadState);
            else if (strcmp(argv[1], "tensor_data") == 0) tensor_data(global->mainThreadState);
            else if (strcmp(argv[1], "tensor_controls") == 0) tensor_controls(global->mainThreadState, 0);
            else if (strcmp(argv[1], "storage_conversion") == 0) tensor_controls(global->mainThreadState, 1);
            else ++failures;
        }
        if (global != NULL) ZrCore_GlobalState_Free(global);
    }
    EXPECT(liveCount == 0 && !allocatorError);
    return failures == 0 ? 0 : 1;
}
