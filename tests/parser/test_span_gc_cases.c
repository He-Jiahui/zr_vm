#include "test_span_gc_cases.h"

#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"

static SZrFunction *compile_span_gc_source(
        SZrState *state,
        const char *path,
        const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || path == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(
            state, (TZrNativeString)path, strlen(path));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
}

static TZrInt64 force_span_gc_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    TZrStackValuePointer resultSlot;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    nativeCallInfo = state->callInfoList;
    resultSlot = nativeCallInfo->functionBase.valuePointer;
    if (resultSlot == ZR_NULL) {
        return 0;
    }

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(resultSlot), 0);
    state->stackTop.valuePointer = resultSlot + 1;
    return 1;
}

static void install_span_gc_probe(SZrState *state) {
    SZrObject *globalObject;
    SZrClosureNative *closure;
    SZrString *nameString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, state->global->zrObject.type);
    TEST_ASSERT_NOT_NULL(state->global->zrObject.value.object);

    globalObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    TEST_ASSERT_NOT_NULL(globalObject);

    closure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = force_span_gc_native;
    ZrCore_RawObject_MarkAsPermanent(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    nameString = ZrCore_String_Create(
            state,
            (TZrNativeString)"__forceSpanGc",
            strlen("__forceSpanGc"));
    TEST_ASSERT_NOT_NULL(nameString);

    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(
            state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value.type = ZR_VALUE_TYPE_CLOSURE;
    value.isNative = ZR_TRUE;

    ZrCore_Object_SetValue(state, globalObject, &key, &value);
}

void test_span_array_source_survives_gc_compaction_while_view_is_live(void) {
    static const char kSource[] =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(41);\n"
            "var view = xs.span();\n"
            "var gcMarker = zr.__forceSpanGc();\n"
            "view[0] = 42;\n"
            "return view[0] + gcMarker;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    install_span_gc_probe(state);
    function = compile_span_gc_source(
            state, "span_array_gc_compaction.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}
