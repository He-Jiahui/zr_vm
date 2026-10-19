/* Real Pair construction and comparison: exact integers and UInt64/float sign. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_library/native_binding.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int failures, allocatorError;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static TZrPtr tracked_allocator(TZrPtr argument, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t i = 0;
    void *next;
    (void)argument;
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
    next = realloc(old, newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        i = liveCount++;
    }
    allocations[i] = (Allocation){next, newSize, flag};
    return next;
}

static int make_pair(SZrState *state, SZrObjectPrototype *prototype,
                     const SZrTypeValue *constructor, const SZrTypeValue components[2],
                     SZrTypeValue *result) {
    SZrObject *object = ZrLib_Type_NewInstanceWithPrototype(state, prototype);
    SZrTypeValue receiver;
    const SZrTypeValue *stored;
    CHECK(object != NULL);
    if (object == NULL) return 0;
    ZrCore_Value_InitAsRawObject(state, &receiver, &object->super);
    ZrLib_Value_SetNull(result);
    CHECK(ZrLib_CallValue(state, constructor, &receiver, components, 2, result));
    CHECK(state->threadStatus == ZR_THREAD_STATUS_FINE);
    CHECK(result->type == ZR_VALUE_TYPE_OBJECT && result->value.object != NULL);
    if (result->type != ZR_VALUE_TYPE_OBJECT || result->value.object == NULL) return 0;
    object = ZR_CAST_OBJECT(state, result->value.object);
    CHECK(ZrCore_Object_IsInstanceOfPrototype(object, prototype));
    /* Verify constructor copied the intended integer tags and exact bits. */
    for (int i = 0; i < 2; ++i) {
        stored = ZrLib_Object_GetFieldCString(state, object, i == 0 ? "first" : "second");
        CHECK(stored != NULL);
        if (stored == NULL) return 0;
        CHECK(stored->type == components[i].type);
        CHECK(stored->value.nativeObject.nativeUInt64 == components[i].value.nativeObject.nativeUInt64);
    }
    return 1;
}

static void compare_expect(SZrState *state, const SZrTypeValue *left,
                           const SZrTypeValue *right, int expected) {
    SZrTypeValue result;
    const SZrTypeValue *callable = ZrLib_Object_GetFieldCString(
            state, ZR_CAST_OBJECT(state, left->value.object), "compareTo");
    CHECK(callable != NULL);
    if (callable == NULL) return;
    ZrLib_Value_SetNull(&result);
    CHECK(ZrLib_CallValue(state, callable, left, right, 1, &result));
    CHECK(state->threadStatus == ZR_THREAD_STATUS_FINE);
    CHECK(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
        TZrInt64 value = result.value.nativeObject.nativeInt64;
        int sign = (value > 0) - (value < 0);
        fprintf(stderr, "compare sign=%d expected=%d\n", sign, expected);
        CHECK(sign == expected);
    }
}

static int run_case(SZrState *state, const char *name) {
    SZrObjectModule *module;
    const SZrTypeValue *exported, *constructor;
    SZrObjectPrototype *prototype;
    SZrTypeValue a[2], b[2], left, right, same;
    TZrUInt64 low = 0, high = 0;
    TZrInt64 signedLow = 0, signedHigh = 0;
    int isUnsigned = 0, second = 0, equal = 0, mixed = 0, floats = 0;
    if (strcmp(name, "signed_adjacent_53") == 0) { signedLow = INT64_C(9007199254740992); signedHigh = signedLow + 1; }
    else if (strcmp(name, "signed_adjacent_max") == 0) { signedLow = INT64_MAX - 1; signedHigh = INT64_MAX; }
    else if (strcmp(name, "signed_adjacent_min") == 0) { signedLow = INT64_MIN; signedHigh = INT64_MIN + 1; }
    else if (strcmp(name, "signed_extrema") == 0) { signedLow = INT64_MIN; signedHigh = INT64_MAX; }
    else if (strcmp(name, "unsigned_adjacent_53") == 0) { isUnsigned = 1; low = UINT64_C(9007199254740992); high = low + 1; }
    else if (strcmp(name, "unsigned_sign_boundary") == 0) { isUnsigned = 1; low = (TZrUInt64)INT64_MAX; high = low + 1; }
    else if (strcmp(name, "unsigned_adjacent_max") == 0) { isUnsigned = 1; low = UINT64_MAX - 1; high = UINT64_MAX; }
    else if (strcmp(name, "unsigned_extrema") == 0) { isUnsigned = 1; low = 0; high = UINT64_MAX; }
    else if (strcmp(name, "second_component") == 0) { second = 1; signedLow = INT64_C(9007199254740992); signedHigh = signedLow + 1; }
    else if (strcmp(name, "small_control") == 0) { signedLow = -3; signedHigh = 7; }
    else if (strcmp(name, "equal_control") == 0) { isUnsigned = 1; low = high = UINT64_MAX; equal = 1; }
    else if (strcmp(name, "mixed_negative_zero") == 0) { mixed = 1; signedLow = -1; high = 0; }
    else if (strcmp(name, "mixed_sign_boundary") == 0) { mixed = 1; signedLow = INT64_MAX; high = (TZrUInt64)INT64_MAX + 1; }
    else if (strcmp(name, "mixed_adjacent_53") == 0) { mixed = 1; signedLow = INT64_C(9007199254740992); high = UINT64_C(9007199254740993); }
    else if (strcmp(name, "mixed_equal") == 0) { mixed = 1; signedLow = INT64_MAX; high = (TZrUInt64)INT64_MAX; equal = 1; }
    else if (strcmp(name, "unsigned_float_sign") == 0) { floats = 1; }
    else if (strcmp(name, "float_control") == 0) { floats = 2; }
    else return 0;
    module = ZrCore_Module_ImportByPath(state, ZrCore_String_CreateFromNative(state, "zr.container"));
    CHECK(module != NULL);
    if (module == NULL) return 1;
    exported = ZrCore_Module_GetPubExport(state, module, ZrCore_String_CreateFromNative(state, "Pair"));
    CHECK(exported != NULL && exported->type == ZR_VALUE_TYPE_OBJECT && exported->value.object != NULL);
    if (exported == NULL || exported->type != ZR_VALUE_TYPE_OBJECT || exported->value.object == NULL) return 1;
    CHECK(ZR_CAST_OBJECT(state, exported->value.object)->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (ZR_CAST_OBJECT(state, exported->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) return 1;
    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exported->value.object);
    constructor = ZrLib_Object_GetFieldCString(state, &prototype->super, "__constructor");
    CHECK(constructor != NULL);
    if (constructor == NULL) return 1;
    for (int i = 0; i < 2; ++i) {
        ZrCore_Value_InitAsInt(state, &a[i], 17);
        ZrCore_Value_InitAsInt(state, &b[i], 17);
    }
    if (mixed) {
        ZrCore_Value_InitAsInt(state, &a[second], signedLow);
        ZrCore_Value_InitAsUInt(state, &b[second], high);
    } else if (floats) {
        ZrCore_Value_InitAsFloat(state, &a[second], floats == 1 ? 0.0 : -1.5);
        if (floats == 1) ZrCore_Value_InitAsUInt(state, &b[second], UINT64_MAX);
        else ZrCore_Value_InitAsFloat(state, &b[second], 2.25);
    } else if (isUnsigned) {
        ZrCore_Value_InitAsUInt(state, &a[second], low);
        ZrCore_Value_InitAsUInt(state, &b[second], high);
    } else {
        ZrCore_Value_InitAsInt(state, &a[second], signedLow);
        ZrCore_Value_InitAsInt(state, &b[second], signedHigh);
    }
    if (!make_pair(state, prototype, constructor, a, &left) ||
        !make_pair(state, prototype, constructor, b, &right) ||
        !make_pair(state, prototype, constructor, a, &same)) return 1;
    compare_expect(state, &left, &right, equal ? 0 : -1);
    compare_expect(state, &right, &left, equal ? 0 : 1);
    compare_expect(state, &left, &same, 0); /* distinct equal instance */
    compare_expect(state, &left, &left, 0);
    return 1;
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
    int recognized = 0;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    if (global != NULL && ZrCore_GlobalState_IsInitialized(global)) {
        SZrState *state = global->mainThreadState;
        /* Numeric semantics only: keep C-local Pair values alive without
         * invoking unrelated constructor OOM or moving-GC contracts. */
        global->garbageCollector->stopGcFlag = ZR_TRUE;
        CHECK(ZrVmLibContainer_Register(global));
        CHECK(ZrCore_Stack_Grow(state, 256, ZR_FALSE));
        recognized = run_case(state, argv[1]);
    }
    if (global != NULL) ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0 && !allocatorError);
    if (!recognized && failures == 0) return 2;
    return failures == 0 ? 0 : 1;
}
