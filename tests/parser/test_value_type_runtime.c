#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
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

void setUp(void) {}

void tearDown(void) {}

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static TZrUInt32 count_inline_struct_slots(const SZrFunction *function) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        if (function->frameSlotLayouts[index].slotKind == ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            count++;
        }
    }

    return count;
}

static TZrBool point_field_value_equals(const SZrStackFramePlace *place,
                                        const SZrFunctionFrameFieldLayout *fieldLayout,
                                        TZrInt64 expected) {
    TZrInt64 actual;

    if (place == ZR_NULL || place->address == ZR_NULL || fieldLayout == ZR_NULL ||
        !fieldLayout->isPrimitivePod || fieldLayout->valueType != ZR_VALUE_TYPE_INT64 ||
        fieldLayout->byteSize != sizeof(TZrInt64) || fieldLayout->byteOffset > place->byteSize ||
        fieldLayout->byteSize > place->byteSize - fieldLayout->byteOffset) {
        return ZR_FALSE;
    }

    memcpy(&actual, ((const TZrByte *)place->address) + fieldLayout->byteOffset, sizeof(actual));
    return (TZrBool)(actual == expected);
}

static TZrBool point_payload_equals(const SZrStackFramePlace *place,
                                    const SZrFunctionFrameFieldLayout *xLayout,
                                    const SZrFunctionFrameFieldLayout *yLayout,
                                    TZrInt64 x,
                                    TZrInt64 y) {
    return (TZrBool)(point_field_value_equals(place, xLayout, x) &&
                     point_field_value_equals(place, yLayout, y));
}

static TZrBool inline_point_frame_has_expected_payloads(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrStackValuePointer frameBase) {
    TZrBool foundOriginal = ZR_FALSE;
    TZrBool foundMutatedCopy = ZR_FALSE;
    SZrString *xName;
    SZrString *yName;

    if (state == ZR_NULL || function == ZR_NULL || frameBase == ZR_NULL) {
        return ZR_FALSE;
    }

    xName = ZrCore_String_Create(state, (TZrNativeString) "x", 1);
    yName = ZrCore_String_Create(state, (TZrNativeString) "y", 1);
    if (xName == ZR_NULL || yName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        SZrFunctionFrameFieldLayout xLayout;
        SZrFunctionFrameFieldLayout yLayout;
        SZrStackFramePlace place;

        if (slotLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            !ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, function, slotLayout->typeLayoutId, xName, &xLayout) ||
            !ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, function, slotLayout->typeLayoutId, yName, &yLayout) ||
            !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, &place)) {
            continue;
        }

        if (point_payload_equals(&place, &xLayout, &yLayout, 1, 2)) {
            foundOriginal = ZR_TRUE;
        }
        if (point_payload_equals(&place, &xLayout, &yLayout, 7, 2)) {
            foundMutatedCopy = ZR_TRUE;
        }
    }

    return (TZrBool)(foundOriginal && foundMutatedCopy);
}

static TZrBool inline_label_frame_has_expected_payloads(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrStackValuePointer frameBase) {
    TZrBool foundOriginal = ZR_FALSE;
    TZrBool foundCopied = ZR_FALSE;
    SZrString *textName;

    if (state == ZR_NULL || function == ZR_NULL || frameBase == ZR_NULL) {
        return ZR_FALSE;
    }

    textName = ZrCore_String_Create(state, (TZrNativeString) "text", 4);
    if (textName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        SZrFunctionFrameFieldLayout textLayout;
        SZrStackFramePlace place;
        const SZrTypeValue *textValue;
        const TZrChar *text;

        if (slotLayout->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            !ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, function, slotLayout->typeLayoutId, textName, &textLayout) ||
            !textLayout.isValueSlot ||
            textLayout.byteSize != sizeof(SZrTypeValue) ||
            !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, &place) ||
            textLayout.byteOffset > place.byteSize ||
            textLayout.byteSize > place.byteSize - textLayout.byteOffset) {
            continue;
        }

        textValue = (const SZrTypeValue *)((const TZrByte *)place.address + textLayout.byteOffset);
        if (textValue->type != ZR_VALUE_TYPE_STRING || textValue->value.object == ZR_NULL) {
            continue;
        }

        text = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, textValue->value.object));
        if (strcmp(text, "left") == 0) {
            foundOriginal = ZR_TRUE;
        } else if (strcmp(text, "right") == 0) {
            foundCopied = ZR_TRUE;
        }
    }

    return (TZrBool)(foundOriginal && foundCopied);
}

static TZrInt64 probe_inline_point_frame_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer resultSlot;
    TZrBool matched = ZR_FALSE;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo->previous;
    resultSlot = nativeCallInfo->functionBase.valuePointer;

    if (callerCallInfo != ZR_NULL && resultSlot != ZR_NULL) {
        SZrFunction *callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
        TZrStackValuePointer callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;

        matched = inline_point_frame_has_expected_payloads(state, callerFunction, callerFrameBase);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(resultSlot), matched ? 1 : 0);
        state->stackTop.valuePointer = resultSlot + 1;
        return 1;
    }

    return 0;
}

static TZrInt64 probe_inline_label_frame_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer resultSlot;
    TZrBool matched = ZR_FALSE;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo->previous;
    resultSlot = nativeCallInfo->functionBase.valuePointer;

    if (callerCallInfo != ZR_NULL && resultSlot != ZR_NULL) {
        SZrFunction *callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
        TZrStackValuePointer callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;

        matched = inline_label_frame_has_expected_payloads(state, callerFunction, callerFrameBase);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(resultSlot), matched ? 1 : 0);
        state->stackTop.valuePointer = resultSlot + 1;
        return 1;
    }

    return 0;
}

static TZrInt64 force_gc_and_probe_inline_label_frame_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer resultSlot;
    TZrBool matchedBefore = ZR_FALSE;
    TZrBool matchedAfter = ZR_FALSE;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    nativeCallInfo = state->callInfoList;
    callerCallInfo = nativeCallInfo->previous;
    resultSlot = nativeCallInfo->functionBase.valuePointer;

    if (callerCallInfo != ZR_NULL && resultSlot != ZR_NULL) {
        SZrFunction *callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callerCallInfo);
        TZrStackValuePointer callerFrameBase = callerCallInfo->functionBase.valuePointer + 1;

        matchedBefore = inline_label_frame_has_expected_payloads(state, callerFunction, callerFrameBase);
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        matchedAfter = inline_label_frame_has_expected_payloads(state, callerFunction, callerFrameBase);

        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(resultSlot), (matchedBefore && matchedAfter) ? 1 : 0);
        state->stackTop.valuePointer = resultSlot + 1;
        return 1;
    }

    return 0;
}

static void install_zr_native_probe(SZrState *state, const char *name, FZrNativeFunction nativeFunction) {
    SZrObject *globalObject;
    SZrClosureNative *closure;
    SZrString *nameString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(nativeFunction);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, state->global->zrObject.type);
    TEST_ASSERT_NOT_NULL(state->global->zrObject.value.object);

    globalObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    TEST_ASSERT_NOT_NULL(globalObject);

    closure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    nameString = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    TEST_ASSERT_NOT_NULL(nameString);

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value.type = ZR_VALUE_TYPE_CLOSURE;
    value.isNative = ZR_TRUE;

    ZrCore_Object_SetValue(state, globalObject, &key, &value);
}

static void test_inline_struct_local_field_get_set_executes_from_frame_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "var p: Point = $Point(1, 2);\n"
            "var q: Point = p;\n"
            "q.x = 7;\n"
            "return (p.x * 100) + (q.x * 10) + q.y;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_local_fields.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(172, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_local_field_get_set_updates_frame_bytes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "var p: Point = $Point(1, 2);\n"
            "var q: Point = p;\n"
            "q.x = 7;\n"
            "return zr.__probeInlinePointFrame();";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    install_zr_native_probe(state, "__probeInlinePointFrame", probe_inline_point_frame_native);
    function = compile_source(state, source, "value_type_runtime_frame_bytes.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_parameter_mutation_is_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "pub mutate(point: Point): int {\n"
            "    point.x = 9;\n"
            "    return (point.x * 10) + point.y;\n"
            "}\n"
            "var p: Point = $Point(1, 2);\n"
            "var mutated = mutate(p);\n"
            "return (p.x * 1000) + mutated;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_parameter_by_value.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1092, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_return_mutation_is_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "pub makePoint(seed: int): Point {\n"
            "    var local: Point = $Point(seed, seed + 1);\n"
            "    return local;\n"
            "}\n"
            "var original: Point = $Point(1, 2);\n"
            "var returned: Point = makePoint(3);\n"
            "returned.x = 8;\n"
            "return (original.x * 1000) + (original.y * 100) + (returned.x * 10) + returned.y;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_return_by_value.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1284, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_large_pod_struct_constructor_initializes_frame_fields(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct WidePoint {\n"
            "    pub var a: int;\n"
            "    pub var b: int;\n"
            "    pub var c: int;\n"
            "    pub var d: int;\n"
            "    pub var e: int;\n"
            "    pub var f: int;\n"
            "    pub var g: int;\n"
            "    pub var h: int;\n"
            "    pub var i: int;\n"
            "    pub var j: int;\n"
            "    pub @constructor(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int, j: int) {\n"
            "        this.a = a;\n"
            "        this.b = b;\n"
            "        this.c = c;\n"
            "        this.d = d;\n"
            "        this.e = e;\n"
            "        this.f = f;\n"
            "        this.g = g;\n"
            "        this.h = h;\n"
            "        this.i = i;\n"
            "        this.j = j;\n"
            "    }\n"
            "}\n"
            "var point: WidePoint = $WidePoint(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);\n"
            "return (point.a * 1000000) + (point.e * 10000) + point.j;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_large_pod_constructor.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1050010, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_large_pod_struct_return_is_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct WidePoint {\n"
            "    pub var a: int;\n"
            "    pub var b: int;\n"
            "    pub var c: int;\n"
            "    pub var d: int;\n"
            "    pub var e: int;\n"
            "    pub var f: int;\n"
            "    pub var g: int;\n"
            "    pub var h: int;\n"
            "    pub var i: int;\n"
            "    pub var j: int;\n"
            "    pub @constructor(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int, j: int) {\n"
            "        this.a = a;\n"
            "        this.b = b;\n"
            "        this.c = c;\n"
            "        this.d = d;\n"
            "        this.e = e;\n"
            "        this.f = f;\n"
            "        this.g = g;\n"
            "        this.h = h;\n"
            "        this.i = i;\n"
            "        this.j = j;\n"
            "    }\n"
            "}\n"
            "pub makeWide(seed: int): WidePoint {\n"
            "    var local: WidePoint = $WidePoint(seed, seed + 1, seed + 2, seed + 3, seed + 4, seed + 5, seed + 6, seed + 7, seed + 8, seed + 9);\n"
            "    return local;\n"
            "}\n"
            "var original: WidePoint = $WidePoint(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);\n"
            "var returned: WidePoint = makeWide(20);\n"
            "returned.e = 70;\n"
            "return (original.a * 1000000) + (original.j * 10000) + (returned.a * 100) + returned.e;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_large_pod_return.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1102070, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_string_field_copy_and_mutation_are_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var copied: Label = original;\n"
            "copied.text = \"right\";\n"
            "return original.text + \":\" + copied.text;";
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_string_field_by_value.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_EQUAL_STRING("left:right",
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(state, result.value.object)));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_string_field_updates_frame_bytes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var copied: Label = original;\n"
            "copied.text = \"right\";\n"
            "return zr.__probeInlineLabelFrame();";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    install_zr_native_probe(state, "__probeInlineLabelFrame", probe_inline_label_frame_native);
    function = compile_source(state, source, "value_type_runtime_string_field_frame_bytes.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_string_field_survives_gc_frame_scan(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var copied: Label = original;\n"
            "copied.text = \"right\";\n"
            "return zr.__forceGcAndProbeInlineLabelFrame();";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    install_zr_native_probe(state, "__forceGcAndProbeInlineLabelFrame", force_gc_and_probe_inline_label_frame_native);
    function = compile_source(state, source, "value_type_runtime_string_field_gc.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_string_field_parameter_is_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "pub mutate(label: Label): string {\n"
            "    label.text = \"right\";\n"
            "    return label.text;\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var mutated = mutate(original);\n"
            "return original.text + \":\" + mutated;";
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_string_field_parameter.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_EQUAL_STRING("left:right",
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(state, result.value.object)));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_string_field_return_is_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "pub makeLabel(text: string): Label {\n"
            "    var local: Label = $Label(text);\n"
            "    return local;\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var returned: Label = makeLabel(\"right\");\n"
            "returned.text = \"done\";\n"
            "return original.text + \":\" + returned.text;";
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_string_field_return.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_EQUAL_STRING("left:done",
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(state, result.value.object)));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_struct_constructor_copies_inline_struct_field_argument(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "struct Box {\n"
            "    pub var point: Point;\n"
            "    pub @constructor(point: Point) {\n"
            "        this.point = point;\n"
            "    }\n"
            "}\n"
            "var p: Point = $Point(1, 2);\n"
            "var box: Box = $Box(p);\n"
            "return (box.point.x * 10) + box.point.y;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_nested_struct_constructor.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(12, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_inline_nested_struct_field_copy_and_mutation_are_by_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const char *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "struct Box {\n"
            "    pub var point: Point;\n"
            "    pub @constructor(point: Point) {\n"
            "        this.point = point;\n"
            "    }\n"
            "}\n"
            "var p: Point = $Point(1, 2);\n"
            "var left: Box = $Box(p);\n"
            "var right: Box = left;\n"
            "right.point.x = 7;\n"
            "return (left.point.x * 1000) + (left.point.y * 100) + (right.point.x * 10) + right.point.y;";
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_type_runtime_nested_struct_field.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3u, count_inline_struct_slots(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1272, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_inline_struct_local_field_get_set_executes_from_frame_layout);
    RUN_TEST(test_inline_struct_local_field_get_set_updates_frame_bytes);
    RUN_TEST(test_inline_struct_parameter_mutation_is_by_value);
    RUN_TEST(test_inline_struct_return_mutation_is_by_value);
    RUN_TEST(test_inline_large_pod_struct_constructor_initializes_frame_fields);
    RUN_TEST(test_inline_large_pod_struct_return_is_by_value);
    RUN_TEST(test_inline_struct_string_field_copy_and_mutation_are_by_value);
    RUN_TEST(test_inline_struct_string_field_updates_frame_bytes);
    RUN_TEST(test_inline_struct_string_field_survives_gc_frame_scan);
    RUN_TEST(test_inline_struct_string_field_parameter_is_by_value);
    RUN_TEST(test_inline_struct_string_field_return_is_by_value);
    RUN_TEST(test_inline_struct_constructor_copies_inline_struct_field_argument);
    RUN_TEST(test_inline_nested_struct_field_copy_and_mutation_are_by_value);
    return UNITY_END();
}
