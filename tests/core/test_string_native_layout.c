/* Real Core regression: native views must select long-string payload storage. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static int failures;

static TZrPtr host_allocator(TZrPtr data, TZrPtr pointer, TZrSize oldSize,
                            TZrSize newSize, TZrInt64 flag) {
    (void)data;
    (void)oldSize;
    (void)flag;
    if (newSize == 0U) {
        free(pointer);
        return ZR_NULL;
    }
    return realloc(pointer, (size_t)newSize);
}

static void check(TZrBool condition, const char *message) {
    printf("%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) failures++;
}

static void test_format(SZrState *state, TZrNativeString text, TZrSize length) {
    TZrNativeString view = ZrCore_NativeString_Format(state, "%s", text);
    const SZrTypeValue *top = ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1);
    SZrString *result;
    TZrNativeString payload;

    check(top->type == ZR_VALUE_TYPE_STRING, "format retains its result as a string on the VM stack");
    if (top->type != ZR_VALUE_TYPE_STRING) return;
    result = (SZrString *)ZrCore_Value_GetRawObject(top);
    check(result != ZR_NULL && !ZrCore_String_IsShort(result), "format exercises the long-string layout");
    if (result == ZR_NULL || ZrCore_String_IsShort(result)) return;
    payload = ZrCore_String_GetNativeString(result);
    check(ZrCore_String_GetByteLength(result) == length &&
                  memcmp(payload, text, (size_t)length + 1U) == 0,
          "formatted VM string contains the complete text and terminator");
    /* Do not read beyond an invalid inline-slot view while reproducing the bug. */
    check(view == payload, "format returns the long-string payload view");
    if (view == payload) {
        check(memcmp(view, text, (size_t)length + 1U) == 0,
              "native format result contains the complete text and terminator");
    }
}

static void test_short_format(SZrState *state) {
    TZrNativeString view = ZrCore_NativeString_Format(
            state, "prefix:%s:%d:%%", (TZrNativeString)"ok", (TZrInt64)7);
    check(view != ZR_NULL && strcmp(view, "prefix:ok:7:%") == 0,
          "short format preserves string/integer/escaped-percent placeholders");
}

static void test_short_cache(SZrState *state) {
    TZrChar text[] = "short-cache";
    SZrString *first = ZrCore_String_CreateTryHitCache(state, text);
    SZrString *again = ZrCore_String_CreateTryHitCache(state, text);
    check(first != ZR_NULL && ZrCore_String_IsShort(first) && again == first,
          "short text still hits the original cached string");
    if (again != ZR_NULL) {
        check(strcmp(ZrCore_String_GetNativeString(again), text) == 0,
              "short cache preserves its text");
    }
}

static void test_cache(SZrState *state, TZrNativeString text, TZrSize length) {
    SZrString *first = ZrCore_String_CreateTryHitCache(state, text);
    check(first != ZR_NULL && !ZrCore_String_IsShort(first), "cache begins with a real long string");
    if (first == ZR_NULL || ZrCore_String_IsShort(first)) return;
    for (TZrSize index = 0U; index < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH + 1U; index++) {
        SZrString *cached = ZrCore_String_CreateTryHitCache(state, text);
        check(cached == first, "repeated calls at the same native address hit the original cached string");
        check(cached != ZR_NULL, "cache lookup returns a string");
        if (cached != ZR_NULL) {
            check(ZrCore_String_GetByteLength(cached) == length &&
                          memcmp(ZrCore_String_GetNativeString(cached), text, (size_t)length + 1U) == 0,
                  "each cache result preserves the complete long text and terminator");
        }
    }
}

int main(int argc, char **argv) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    TZrChar text[ZR_VM_SHORT_STRING_MAX + 129U];
    const TZrSize length = sizeof(text) - 1U;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || (strcmp(argv[1], "format") != 0 && strcmp(argv[1], "cache") != 0)) return 2;
    for (TZrSize index = 0U; index < length; index++) text[index] = (TZrChar)('a' + index % 26U);
    text[length] = '\0';
    global = ZrCore_GlobalState_New(host_allocator, ZR_NULL, 0U, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) return 1;
    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    if (strcmp(argv[1], "format") == 0) {
        test_short_format(global->mainThreadState);
        test_format(global->mainThreadState, text, length);
    } else {
        test_short_cache(global->mainThreadState);
        test_cache(global->mainThreadState, text, length);
    }
    ZrCore_GlobalState_Free(global);
    return failures == 0 ? 0 : 1;
}
