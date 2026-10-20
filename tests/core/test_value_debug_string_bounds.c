/* Public Core formatter regression. Each case is a separate process because
 * the old formatter can overwrite its own stack after snprintf truncates. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError, failures;

static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s (live=%zu)\n", condition ? "PASS" : "FAIL", message, liveCount);
    if (!condition) ++failures;
}

static TZrPtr tracked_allocator(TZrPtr argument, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)argument;
    if (old != NULL) {
        while (index < liveCount && allocations[index].pointer != old) ++index;
        if (index == liveCount || allocations[index].size != oldSize || allocations[index].flag != flag) {
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

static int add_field(SZrState *state, SZrObject *object, const char *name,
                     const char *text, int isArray, size_t index) {
    SZrTypeValue key, value;
    SZrString *string = ZrCore_String_CreateFromNative(state, (TZrNativeString)text);
    if (string == NULL) return 0;
    ZrCore_Value_InitAsRawObject(state, &value, &string->super);
    if (isArray) {
        ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    } else {
        string = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
        if (string == NULL) return 0;
        ZrCore_Value_InitAsRawObject(state, &key, &string->super);
    }
    return ZrCore_Object_TrySetValue(state, object, &key, &value) != ZR_FALSE;
}

static void run_case(SZrState *state, const char *name) {
    enum { CAPACITY = ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH };
    char longText[CAPACITY * 4U + 1U];
    char expected[CAPACITY];
    const char *typeName = "object", *keyName = "k", *text = "v";
    SZrObjectPrototype *prototype = NULL;
    SZrObject *object;
    SZrString *formatted;
    SZrTypeValue input;
    const char *actual;
    const char *terminator;
    size_t count = 1U, index;
    int isArray = strcmp(name, "long_array") == 0 || strcmp(name, "short_array") == 0 ||
                  strncmp(name, "preview_", 8U) == 0;

    /* Isolate bounded formatting from moving-GC/metamethod behavior. All values
     * are actual managed Core objects; no host callback runs in this fixture. */
    state->global->garbageCollector->stopGcFlag = ZR_TRUE;
    memset(longText, 'L', sizeof(longText) - 1U);
    longText[sizeof(longText) - 1U] = '\0';
    if (strcmp(name, "long_type") == 0) { typeName = longText; count = 0U; }
    else if (strcmp(name, "long_key") == 0) keyName = longText;
    else if (strcmp(name, "long_value") == 0 || strcmp(name, "long_array") == 0) text = longText;
    else if (strncmp(name, "object_exact_", 13U) == 0) {
        size_t desired = strcmp(name, "object_exact_fit") == 0 ? CAPACITY - 1U : CAPACITY;
        size_t overhead = strlen("<object type=object>{k : }");
        longText[desired - overhead] = '\0';
        text = longText;
    } else if (strcmp(name, "preview_10") == 0 || strcmp(name, "preview_11") == 0) {
        count = strcmp(name, "preview_10") == 0 ? 10U : 11U;
        text = "x";
    }
    if (!isArray) {
        SZrString *type = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeName);
        check(type != NULL, "actual prototype name created");
        if (type == NULL) return;
        prototype = ZrCore_ObjectPrototype_New(state, type, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        check(prototype != NULL, "actual object prototype created");
        if (prototype == NULL) return;
        object = ZrCore_Object_New(state, prototype);
    } else {
        object = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    }
    check(object != NULL, "actual object created");
    if (object == NULL) return;
    ZrCore_Object_Init(state, object);
    for (index = 0; index < count; ++index) {
        if (!add_field(state, object, keyName, text, isArray, index)) {
            check(0, "actual public field insertion succeeds");
            return;
        }
    }
    check(object->nodeMap.elementCount == count, "fixture stores the requested field count");
    if (count >= 10U) {
        (void)snprintf(expected, sizeof(expected),
                "[x, x, x, x, x, x, x, x, x, x%s | count = %lu]",
                count > 10U ? ", ..." : "", (unsigned long)count);
    } else if (isArray) {
        (void)snprintf(expected, sizeof(expected), "[%s]", text);
    } else if (count == 0U) {
        (void)snprintf(expected, sizeof(expected), "<object type=%s>{}", typeName);
    } else {
        (void)snprintf(expected, sizeof(expected), "<object type=%s>{%s : %s}", typeName, keyName, text);
    }
    ZrCore_Value_InitAsRawObject(state, &input, &object->super);
    input.type = isArray ? ZR_VALUE_TYPE_ARRAY : ZR_VALUE_TYPE_OBJECT;
    fprintf(stderr, "Calling actual public ToDebugString for %s\n", name);
    fflush(stderr);
    formatted = ZrCore_Value_ToDebugString(state, &input);
    check(formatted != NULL, "formatter returns managed text");
    if (formatted == NULL) return;
    actual = ZrCore_String_GetNativeString(formatted);
    check(actual != NULL, "managed text exposes native bytes");
    if (actual == NULL) return;
    /* Bound the search to the managed string's actual payload, including its
     * terminator, instead of allowing the regression oracle to overread. */
    check(ZrCore_String_GetByteLength(formatted) < CAPACITY, "formatted length stays within fixed diagnostic capacity");
    if (ZrCore_String_GetByteLength(formatted) >= CAPACITY) return;
    terminator = (const char *)memchr(actual, '\0', (size_t)ZrCore_String_GetByteLength(formatted) + 1U);
    check(terminator == actual + ZrCore_String_GetByteLength(formatted), "formatted payload is exactly NUL terminated");
    if (terminator != actual + ZrCore_String_GetByteLength(formatted)) return;
    check(strcmp(expected, actual) == 0, "format retains its bounded original prefix and ordinary delimiters");
}

int main(int argc, char **argv) {
    static const char *cases[] = {"long_type", "long_key", "long_value", "long_array",
            "object_exact_fit", "object_exact_capacity", "short_object", "short_array", "preview_10", "preview_11"};
    SZrGlobalState *global;
    size_t index;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
        if (strcmp(argv[1], cases[index]) == 0) break;
    if (index == sizeof(cases) / sizeof(cases[0])) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    check(global != NULL && ZrCore_GlobalState_IsInitialized(global), "Core initializes");
    if (global != NULL) {
        if (ZrCore_GlobalState_IsInitialized(global)) run_case(global->mainThreadState, argv[1]);
        ZrCore_GlobalState_Free(global);
    }
    check(liveCount == 0 && !allocatorError, "Core teardown releases tracked allocations with matching size/category");
    return failures == 0 ? 0 : 1;
}
