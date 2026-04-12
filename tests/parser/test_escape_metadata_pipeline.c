#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "module_fixture_support.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

void test_compiler_escape_metadata_summarizes_capture_return_and_exports(void);
void test_binary_roundtrip_preserves_escape_metadata_summaries(void);
void test_runtime_global_binding_marks_returned_object_as_global_root(void);
void test_runtime_native_callback_capture_marks_returned_object_as_native_handle(void);
void test_binary_roundtrip_runtime_global_binding_preserves_escape_flags(void);
void test_binary_roundtrip_runtime_native_callback_preserves_escape_flags(void);
void test_runtime_returned_callable_capture_marks_closed_capture_object(void);
void test_runtime_global_callable_capture_marks_closed_capture_object(void);
void test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags(void);
void test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags(void);

static void fixture_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *compile_source_fixture(SZrState *state,
                                           const TZrChar *source,
                                           const TZrChar *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_escape_metadata_fixture(SZrState *state) {
    static const TZrChar *kSource =
            "pub returnLocal(): int {\n"
            "    var value = 9;\n"
            "    return value;\n"
            "}\n"
            "pub returnTemp(): int {\n"
            "    var value = 9;\n"
            "    return value + 1;\n"
            "}\n"
            "pub capture(): int {\n"
            "    var seed = 41;\n"
            "    read(): int {\n"
            "        return seed;\n"
            "    }\n"
            "    return read();\n"
            "}\n"
            "pub bindGlobalLocal(): string {\n"
            "    var payload = \"cache\";\n"
            "    globalPayload = payload;\n"
            "    return payload;\n"
            "}\n"
            "pub bindGlobalTemp(): int {\n"
            "    var seed = 5;\n"
            "    globalNumber = seed + 1;\n"
            "    return seed;\n"
            "}\n"
            "pub bindGlobalCaptured(): string {\n"
            "    var payload = \"cache\";\n"
            "    anchoredReader(): string {\n"
            "        var relayedPayload = payload;\n"
            "        globalRelay = relayedPayload;\n"
            "        anchoredLeaf(): string {\n"
            "            return relayedPayload;\n"
            "        }\n"
            "        return anchoredLeaf();\n"
            "    }\n"
            "    globalPayload = payload;\n"
            "    globalReader = anchoredReader;\n"
            "    return anchoredReader();\n"
            "}\n"
            "pub returnCallableCapture() {\n"
            "    var returnSeed = \"returned\";\n"
            "    returnedReader(): string {\n"
            "        return returnSeed;\n"
            "    }\n"
            "    return returnedReader;\n"
            "}\n"
            "pub bindGlobalCallableCapture(): int {\n"
            "    var globalSeed = \"global\";\n"
            "    globalCallableReader(): string {\n"
            "        return globalSeed;\n"
            "    }\n"
            "    globalCallable = globalCallableReader;\n"
            "    return 1;\n"
            "}\n"
            "pub returnRelayedCallableCapture() {\n"
            "    var relayedSeed = \"relay\";\n"
            "    relayedReader(): string {\n"
            "        return relayedSeed;\n"
            "    }\n"
            "    relayCarrier() {\n"
            "        var relayedCallable = relayedReader;\n"
            "        return relayedCallable;\n"
            "    }\n"
            "    return relayCarrier();\n"
            "}\n"
            "pub bindGlobalRelayedCallableCapture(): int {\n"
            "    var globalRelayedSeed = \"globalRelay\";\n"
            "    globalRelayedReader(): string {\n"
            "        return globalRelayedSeed;\n"
            "    }\n"
            "    globalRelayCarrier(): int {\n"
            "        var relayedCallable = globalRelayedReader;\n"
            "        globalRelayedCallable = relayedCallable;\n"
            "        return 1;\n"
            "    }\n"
            "    return globalRelayCarrier();\n"
            "}\n"
            "var ffiNative = %import(\"zr.ffi\");\n"
            "pub makeNativeCallbackHandle() {\n"
            "    var callbackSeed = 2.0;\n"
            "    nativeCallback(value: float): float {\n"
            "        return value + callbackSeed;\n"
            "    }\n"
            "    var callbackHandle = ffiNative.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, nativeCallback);\n"
            "    return callbackHandle;\n"
            "}\n"
            "var moduleSeed = \"module\";\n"
            "exportedCallableImpl(): string {\n"
            "    return moduleSeed;\n"
            "}\n"
            "pub var exportedCallable = exportedCallableImpl;\n";

    return compile_source_fixture(state, kSource, "escape_metadata_fixture.zr");
}

static SZrFunction *load_runtime_entry_from_binary_file(SZrState *state, const TZrChar *binaryPath) {
    TZrSize binaryLength = 0;
    TZrByte *binaryBytes;
    ZrTestsFixtureReader reader;
    SZrIo *io;
    SZrIoSource *sourceObject;
    SZrFunction *runtimeFunction;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(binaryPath);

    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    reader.consumed = ZR_FALSE;

    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, fixture_reader_close_noop, &reader);
    io->isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    return runtimeFunction;
}

static void assert_runtime_result_has_escape_flags(const SZrTypeValue *result, TZrUInt32 escapeFlags) {
    const SZrRawObject *rawObject;

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result->isGarbageCollectable);
    TEST_ASSERT_TRUE(result->type == ZR_VALUE_TYPE_OBJECT || result->type == ZR_VALUE_TYPE_STRING);
    TEST_ASSERT_NOT_NULL(result->value.object);

    rawObject = result->value.object;
    TEST_ASSERT_TRUE((rawObject->garbageCollectMark.escapeFlags & escapeFlags) == escapeFlags);
}

static void assert_runtime_escape_for_source(const TZrChar *testSource,
                                             const TZrChar *sourceNameText,
                                             TZrUInt32 expectedEscapeFlags,
                                             const TZrChar *binaryPathOrNull) {
    SZrState *state;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(testSource);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(state->global));

    function = compile_source_fixture(state, testSource, sourceNameText);
    TEST_ASSERT_NOT_NULL(function);

    if (binaryPathOrNull != ZR_NULL) {
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPathOrNull));
        ZrCore_Function_Free(state, function);
        function = load_runtime_entry_from_binary_file(state, binaryPathOrNull);
    }

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    assert_runtime_result_has_escape_flags(&result, expectedEscapeFlags);

    ZrCore_Function_Free(state, function);
    if (binaryPathOrNull != ZR_NULL) {
        remove(binaryPathOrNull);
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void assert_runtime_returned_closure_capture_has_escape_flags(const TZrChar *testSource,
                                                                     const TZrChar *sourceNameText,
                                                                     TZrUInt32 expectedEscapeFlags,
                                                                     const TZrChar *binaryPathOrNull) {
    SZrState *state;
    SZrFunction *function;
    SZrTypeValue result;
    SZrClosure *closure;
    SZrClosureValue *captureCell;
    SZrTypeValue *captureValue;
    TZrUInt32 expectedCaptureFlags = expectedEscapeFlags | ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;

    TEST_ASSERT_NOT_NULL(testSource);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(state->global));

    function = compile_source_fixture(state, testSource, sourceNameText);
    TEST_ASSERT_NOT_NULL(function);

    if (binaryPathOrNull != ZR_NULL) {
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPathOrNull));
        ZrCore_Function_Free(state, function);
        function = load_runtime_entry_from_binary_file(state, binaryPathOrNull);
    }

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_CLOSURE, result.type);
    TEST_ASSERT_FALSE(result.isNative);

    closure = ZR_CAST_VM_CLOSURE(state, result.value.object);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_TRUE(closure->closureValueCount > 0u);
    captureCell = closure->closureValuesExtend[0];
    TEST_ASSERT_NOT_NULL(captureCell);
    TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureCell));
    TEST_ASSERT_TRUE((ZR_CAST_RAW_OBJECT_AS_SUPER(captureCell)->garbageCollectMark.escapeFlags &
                      expectedEscapeFlags) == expectedEscapeFlags);

    captureValue = ZrCore_ClosureValue_GetValue(captureCell);
    TEST_ASSERT_NOT_NULL(captureValue);
    assert_runtime_result_has_escape_flags(captureValue, expectedCaptureFlags);

    ZrCore_Function_Free(state, function);
    if (binaryPathOrNull != ZR_NULL) {
        remove(binaryPathOrNull);
    }
    ZrTests_Runtime_State_Destroy(state);
}

static const SZrFunction *find_named_function_recursive(const SZrFunction *function, const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->functionName != ZR_NULL) {
        const TZrChar *functionName = ZrCore_String_GetNativeString(function->functionName);
        if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
            return function;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        const SZrFunction *found = find_named_function_recursive(&function->childFunctionList[index], name);
        if (found != ZR_NULL) {
            return found;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionLocalVariable *find_local_variable_by_name(const SZrFunction *function, const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL || function->localVariableList == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];
        const TZrChar *localName = local->name != ZR_NULL ? ZrCore_String_GetNativeString(local->name) : ZR_NULL;

        if (localName != ZR_NULL && strcmp(localName, name) == 0) {
            return local;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionEscapeBinding *find_escape_binding(const SZrFunction *function,
                                                           EZrFunctionEscapeBindingKind kind,
                                                           const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL || function->escapeBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->escapeBindingLength; index++) {
        const SZrFunctionEscapeBinding *binding = &function->escapeBindings[index];
        const TZrChar *bindingName = binding->name != ZR_NULL ? ZrCore_String_GetNativeString(binding->name) : ZR_NULL;

        if ((EZrFunctionEscapeBindingKind)binding->bindingKind == kind &&
            bindingName != ZR_NULL &&
            strcmp(bindingName, name) == 0) {
            return binding;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionEscapeBinding *find_escape_binding_by_slot(const SZrFunction *function,
                                                                   EZrFunctionEscapeBindingKind kind,
                                                                   TZrUInt32 slotOrIndex) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->escapeBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->escapeBindingLength; index++) {
        const SZrFunctionEscapeBinding *binding = &function->escapeBindings[index];

        if ((EZrFunctionEscapeBindingKind)binding->bindingKind == kind &&
            binding->slotOrIndex == slotOrIndex) {
            return binding;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionEscapeBinding *find_first_escape_binding_of_kind(const SZrFunction *function,
                                                                         EZrFunctionEscapeBindingKind kind) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->escapeBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->escapeBindingLength; index++) {
        const SZrFunctionEscapeBinding *binding = &function->escapeBindings[index];

        if ((EZrFunctionEscapeBindingKind)binding->bindingKind == kind) {
            return binding;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionEscapeBinding *find_first_escape_binding_with_flag(const SZrFunction *function,
                                                                           EZrFunctionEscapeBindingKind kind,
                                                                           TZrUInt32 escapeFlagMask) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->escapeBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->escapeBindingLength; index++) {
        const SZrFunctionEscapeBinding *binding = &function->escapeBindings[index];

        if ((EZrFunctionEscapeBindingKind)binding->bindingKind == kind &&
            (binding->escapeFlags & escapeFlagMask) == escapeFlagMask) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool function_has_return_escape_slot(const SZrFunction *function, TZrUInt32 slot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->returnEscapeSlots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->returnEscapeSlotCount; index++) {
        if (function->returnEscapeSlots[index] == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void assert_escape_metadata_shape(const SZrFunction *rootFunction) {
    const SZrFunction *returnLocalFunction;
    const SZrFunction *returnTempFunction;
    const SZrFunction *captureFunction;
    const SZrFunction *readFunction;
    const SZrFunction *bindGlobalLocalFunction;
    const SZrFunction *bindGlobalTempFunction;
    const SZrFunction *bindGlobalCapturedFunction;
    const SZrFunction *anchoredReaderFunction;
    const SZrFunction *anchoredLeafFunction;
    const SZrFunction *returnCallableCaptureFunction;
    const SZrFunction *returnedReaderFunction;
    const SZrFunction *bindGlobalCallableCaptureFunction;
    const SZrFunction *globalCallableReaderFunction;
    const SZrFunction *returnRelayedCallableCaptureFunction;
    const SZrFunction *relayedReaderFunction;
    const SZrFunction *relayCarrierFunction;
    const SZrFunction *bindGlobalRelayedCallableCaptureFunction;
    const SZrFunction *globalRelayedReaderFunction;
    const SZrFunction *globalRelayCarrierFunction;
    const SZrFunction *makeNativeCallbackHandleFunction;
    const SZrFunction *nativeCallbackFunction;
    const SZrFunction *exportedCallableImplFunction;
    const SZrFunctionLocalVariable *moduleSeedLocal;
    const SZrFunctionLocalVariable *valueLocal;
    const SZrFunctionLocalVariable *seedLocal;
    const SZrFunctionLocalVariable *payloadLocal;
    const SZrFunctionLocalVariable *relayedPayloadLocal;
    const SZrFunctionLocalVariable *returnSeedLocal;
    const SZrFunctionLocalVariable *globalSeedLocal;
    const SZrFunctionLocalVariable *relayedSeedLocal;
    const SZrFunctionLocalVariable *globalRelayedSeedLocal;
    const SZrFunctionLocalVariable *callbackSeedLocal;
    const SZrFunctionLocalVariable *nativeCallbackLocal;
    const SZrFunctionEscapeBinding *binding;

    TEST_ASSERT_NOT_NULL(rootFunction);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "returnLocal");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "capture");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "returnTemp");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "bindGlobalLocal");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "bindGlobalTemp");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "bindGlobalCaptured");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "returnCallableCapture");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "bindGlobalCallableCapture");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "returnRelayedCallableCapture");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT,
                                  "bindGlobalRelayedCallableCapture");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT,
                                  "makeNativeCallbackHandle");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT, "exportedCallable");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    returnLocalFunction = find_named_function_recursive(rootFunction, "returnLocal");
    returnTempFunction = find_named_function_recursive(rootFunction, "returnTemp");
    captureFunction = find_named_function_recursive(rootFunction, "capture");
    readFunction = find_named_function_recursive(rootFunction, "read");
    bindGlobalLocalFunction = find_named_function_recursive(rootFunction, "bindGlobalLocal");
    bindGlobalTempFunction = find_named_function_recursive(rootFunction, "bindGlobalTemp");
    bindGlobalCapturedFunction = find_named_function_recursive(rootFunction, "bindGlobalCaptured");
    anchoredReaderFunction = find_named_function_recursive(rootFunction, "anchoredReader");
    anchoredLeafFunction = find_named_function_recursive(rootFunction, "anchoredLeaf");
    returnCallableCaptureFunction = find_named_function_recursive(rootFunction, "returnCallableCapture");
    returnedReaderFunction = find_named_function_recursive(rootFunction, "returnedReader");
    bindGlobalCallableCaptureFunction = find_named_function_recursive(rootFunction, "bindGlobalCallableCapture");
    globalCallableReaderFunction = find_named_function_recursive(rootFunction, "globalCallableReader");
    returnRelayedCallableCaptureFunction = find_named_function_recursive(rootFunction, "returnRelayedCallableCapture");
    relayedReaderFunction = find_named_function_recursive(rootFunction, "relayedReader");
    relayCarrierFunction = find_named_function_recursive(rootFunction, "relayCarrier");
    bindGlobalRelayedCallableCaptureFunction =
            find_named_function_recursive(rootFunction, "bindGlobalRelayedCallableCapture");
    globalRelayedReaderFunction = find_named_function_recursive(rootFunction, "globalRelayedReader");
    globalRelayCarrierFunction = find_named_function_recursive(rootFunction, "globalRelayCarrier");
    makeNativeCallbackHandleFunction = find_named_function_recursive(rootFunction, "makeNativeCallbackHandle");
    nativeCallbackFunction = find_named_function_recursive(rootFunction, "nativeCallback");
    exportedCallableImplFunction = find_named_function_recursive(rootFunction, "exportedCallableImpl");
    TEST_ASSERT_NOT_NULL(returnLocalFunction);
    TEST_ASSERT_NOT_NULL(returnTempFunction);
    TEST_ASSERT_NOT_NULL(captureFunction);
    TEST_ASSERT_NOT_NULL(readFunction);
    TEST_ASSERT_NOT_NULL(bindGlobalLocalFunction);
    TEST_ASSERT_NOT_NULL(bindGlobalTempFunction);
    TEST_ASSERT_NOT_NULL(bindGlobalCapturedFunction);
    TEST_ASSERT_NOT_NULL(anchoredReaderFunction);
    TEST_ASSERT_NOT_NULL(anchoredLeafFunction);
    TEST_ASSERT_NOT_NULL(returnCallableCaptureFunction);
    TEST_ASSERT_NOT_NULL(returnedReaderFunction);
    TEST_ASSERT_NOT_NULL(bindGlobalCallableCaptureFunction);
    TEST_ASSERT_NOT_NULL(globalCallableReaderFunction);
    TEST_ASSERT_NOT_NULL(returnRelayedCallableCaptureFunction);
    TEST_ASSERT_NOT_NULL(relayedReaderFunction);
    TEST_ASSERT_NOT_NULL(relayCarrierFunction);
    TEST_ASSERT_NOT_NULL(bindGlobalRelayedCallableCaptureFunction);
    TEST_ASSERT_NOT_NULL(globalRelayedReaderFunction);
    TEST_ASSERT_NOT_NULL(globalRelayCarrierFunction);
    TEST_ASSERT_NOT_NULL(makeNativeCallbackHandleFunction);
    TEST_ASSERT_NOT_NULL(nativeCallbackFunction);
    TEST_ASSERT_NOT_NULL(exportedCallableImplFunction);

    valueLocal = find_local_variable_by_name(returnLocalFunction, "value");
    TEST_ASSERT_NOT_NULL(valueLocal);
    TEST_ASSERT_TRUE((valueLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
    TEST_ASSERT_TRUE(function_has_return_escape_slot(returnLocalFunction, valueLocal->stackSlot));
    binding = find_escape_binding(returnLocalFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL, "value");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
    TEST_ASSERT_EQUAL_UINT32(valueLocal->stackSlot, binding->slotOrIndex);

    TEST_ASSERT_TRUE(returnTempFunction->returnEscapeSlotCount > 0);
    binding = find_escape_binding_by_slot(returnTempFunction,
                                          ZR_FUNCTION_ESCAPE_BINDING_KIND_RETURN_SLOT,
                                          returnTempFunction->returnEscapeSlots[0]);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
    TEST_ASSERT_TRUE(function_has_return_escape_slot(returnTempFunction, binding->slotOrIndex));
    TEST_ASSERT_NULL(binding->name);

    seedLocal = find_local_variable_by_name(captureFunction, "seed");
    TEST_ASSERT_NOT_NULL(seedLocal);
    TEST_ASSERT_TRUE((seedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    binding = find_escape_binding(captureFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL, "seed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(seedLocal->stackSlot, binding->slotOrIndex);

    TEST_ASSERT_TRUE(readFunction->closureValueLength > 0);
    TEST_ASSERT_NOT_NULL(readFunction->closureValueList);
    TEST_ASSERT_NOT_NULL(readFunction->closureValueList[0].name);
    TEST_ASSERT_TRUE((readFunction->closureValueList[0].escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE(ZrTests_Fixture_StringEqualsCString(readFunction->closureValueList[0].name, "seed"));
    binding = find_escape_binding(readFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "seed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, binding->slotOrIndex);

    payloadLocal = find_local_variable_by_name(bindGlobalLocalFunction, "payload");
    TEST_ASSERT_NOT_NULL(payloadLocal);
    binding = find_escape_binding(bindGlobalLocalFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL, "payload");
    if (binding == ZR_NULL) {
        binding = find_first_escape_binding_with_flag(bindGlobalLocalFunction,
                                                      ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL,
                                                      ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT);
    }
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    binding = find_first_escape_binding_of_kind(bindGlobalTempFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_GLOBAL_BINDING);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);
    TEST_ASSERT_NULL(binding->name);

    payloadLocal = find_local_variable_by_name(bindGlobalCapturedFunction, "payload");
    TEST_ASSERT_NOT_NULL(payloadLocal);
    binding = find_escape_binding(bindGlobalCapturedFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL, "payload");
    if (binding == ZR_NULL ||
        (binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) == 0u) {
        binding = find_first_escape_binding_with_flag(bindGlobalCapturedFunction,
                                                      ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL,
                                                      ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT);
    }
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    binding = find_escape_binding(anchoredReaderFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "payload");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    relayedPayloadLocal = find_local_variable_by_name(anchoredReaderFunction, "relayedPayload");
    TEST_ASSERT_NOT_NULL(relayedPayloadLocal);
    TEST_ASSERT_TRUE((relayedPayloadLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    binding = find_escape_binding(anchoredLeafFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "relayedPayload");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    returnSeedLocal = find_local_variable_by_name(returnCallableCaptureFunction, "returnSeed");
    TEST_ASSERT_NOT_NULL(returnSeedLocal);
    TEST_ASSERT_TRUE((returnSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((returnSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
    binding = find_escape_binding(returnedReaderFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "returnSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);

    globalSeedLocal = find_local_variable_by_name(bindGlobalCallableCaptureFunction, "globalSeed");
    TEST_ASSERT_NOT_NULL(globalSeedLocal);
    TEST_ASSERT_TRUE((globalSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((globalSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);
    binding = find_escape_binding(globalCallableReaderFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE,
                                  "globalSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    relayedSeedLocal = find_local_variable_by_name(returnRelayedCallableCaptureFunction, "relayedSeed");
    TEST_ASSERT_NOT_NULL(relayedSeedLocal);
    TEST_ASSERT_TRUE((relayedSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((relayedSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);
    binding = find_escape_binding(relayedReaderFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "relayedSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);

    binding = find_escape_binding(relayCarrierFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE, "relayedReader");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u);

    globalRelayedSeedLocal =
            find_local_variable_by_name(bindGlobalRelayedCallableCaptureFunction, "globalRelayedSeed");
    TEST_ASSERT_NOT_NULL(globalRelayedSeedLocal);
    TEST_ASSERT_TRUE((globalRelayedSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((globalRelayedSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);
    binding = find_escape_binding(globalRelayedReaderFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE,
                                  "globalRelayedSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    binding = find_escape_binding(globalRelayCarrierFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE,
                                  "globalRelayedReader");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u);

    callbackSeedLocal = find_local_variable_by_name(makeNativeCallbackHandleFunction, "callbackSeed");
    TEST_ASSERT_NOT_NULL(callbackSeedLocal);
    TEST_ASSERT_TRUE((callbackSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((callbackSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) != 0u);

    nativeCallbackLocal = find_local_variable_by_name(makeNativeCallbackHandleFunction, "nativeCallback");
    TEST_ASSERT_NOT_NULL(nativeCallbackLocal);
    binding = find_escape_binding_by_slot(makeNativeCallbackHandleFunction,
                                          ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING,
                                          nativeCallbackLocal->stackSlot);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) != 0u);

    binding = find_escape_binding(nativeCallbackFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE,
                                  "callbackSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) != 0u);

    moduleSeedLocal = find_local_variable_by_name(rootFunction, "moduleSeed");
    TEST_ASSERT_NOT_NULL(moduleSeedLocal);
    TEST_ASSERT_TRUE((moduleSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((moduleSeedLocal->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);
    binding = find_escape_binding(rootFunction, ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL, "moduleSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);

    binding = find_escape_binding(exportedCallableImplFunction,
                                  ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE,
                                  "moduleSeed");
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE) != 0u);
    TEST_ASSERT_TRUE((binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u);
}

void test_compiler_escape_metadata_summarizes_capture_return_and_exports(void) {
    SZrTestTimer timer;
    const TZrChar *testSummary = "Compiler Escape Metadata Summarizes Capture Return And Exports";
    SZrState *state;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("compiler escape metadata",
                 "Testing that compiled functions populate module-export, local-return, and closure-capture escape summaries");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(state->global));

    function = compile_escape_metadata_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    assert_escape_metadata_shape(function);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_binary_roundtrip_preserves_escape_metadata_summaries(void) {
    SZrTestTimer timer;
    const TZrChar *testSummary = "Binary Roundtrip Preserves Escape Metadata Summaries";
    const TZrChar *binaryPath = "escape_metadata_roundtrip_test.zro";
    SZrState *state;
    SZrFunction *sourceFunction;
    TZrSize binaryLength = 0;
    TZrByte *binaryBytes;
    ZrTestsFixtureReader reader;
    SZrIo *io;
    SZrIoSource *sourceObject;
    SZrFunction *runtimeFunction;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("escape metadata binary roundtrip",
                 "Testing that .zro roundtrip keeps function escape bindings and return-escape slots available to runtime-loaded functions");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(state->global));

    sourceFunction = compile_escape_metadata_fixture(state);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, sourceFunction, binaryPath));

    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    reader.consumed = ZR_FALSE;

    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, fixture_reader_close_noop, &reader);
    io->isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_escape_metadata_shape(runtimeFunction);

    ZrCore_Function_Free(state, runtimeFunction);
    ZrCore_Function_Free(state, sourceFunction);
    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    remove(binaryPath);
    ZrTests_Runtime_State_Destroy(state);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_runtime_global_binding_marks_returned_object_as_global_root(void) {
    static const TZrChar *kSource =
            "var payload = { value: 7 };\n"
            "globalPayload = payload;\n"
            "return payload;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary = "Runtime Global Binding Marks Returned Object As Global Root";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime global escape propagation",
                 "Testing that executing a compiled entry which binds a heap object into a global also marks the returned object with global-root escape flags");

    assert_runtime_escape_for_source(kSource,
                                     "escape_runtime_global_binding_fixture.zr",
                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT,
                                     ZR_NULL);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_runtime_native_callback_capture_marks_returned_object_as_native_handle(void) {
    static const TZrChar *kSource =
            "var ffiNative = %import(\"zr.ffi\");\n"
            "var payload = { extra: 2.0 };\n"
            "nativeCallback(value: float): float {\n"
            "    return value + payload.extra;\n"
            "}\n"
            "var callbackHandle = ffiNative.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, nativeCallback);\n"
            "return payload;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary = "Runtime Native Callback Capture Marks Returned Object As Native Handle";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime native-handle escape propagation",
                 "Testing that creating an ffi callback anchors its captured heap object with native-handle escape flags that remain visible on the returned object");

    assert_runtime_escape_for_source(kSource,
                                     "escape_runtime_native_callback_fixture.zr",
                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE,
                                     ZR_NULL);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_binary_roundtrip_runtime_global_binding_preserves_escape_flags(void) {
    static const TZrChar *kSource =
            "var payload = { value: 11 };\n"
            "globalPayload = payload;\n"
            "return payload;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary = "Binary Roundtrip Runtime Global Binding Preserves Escape Flags";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime global escape roundtrip",
                 "Testing that compile -> .zro -> runtime loading preserves global-root escape behavior for a returned heap object that is also stored into a global");

    assert_runtime_escape_for_source(kSource,
                                     "escape_runtime_global_binding_roundtrip_fixture.zr",
                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT,
                                     "escape_runtime_global_binding_roundtrip.zro");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_binary_roundtrip_runtime_native_callback_preserves_escape_flags(void) {
    static const TZrChar *kSource =
            "var ffiNative = %import(\"zr.ffi\");\n"
            "var payload = { extra: 4.0 };\n"
            "nativeCallback(value: float): float {\n"
            "    return value + payload.extra;\n"
            "}\n"
            "var callbackHandle = ffiNative.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, nativeCallback);\n"
            "return payload;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary = "Binary Roundtrip Runtime Native Callback Preserves Escape Flags";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime native-handle escape roundtrip",
                 "Testing that compile -> .zro -> runtime loading preserves native-handle escape propagation from ffi callback creation onto the captured returned heap object");

    assert_runtime_escape_for_source(kSource,
                                     "escape_runtime_native_callback_roundtrip_fixture.zr",
                                     ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE,
                                     "escape_runtime_native_callback_roundtrip.zro");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_runtime_returned_callable_capture_marks_closed_capture_object(void) {
    static const TZrChar *kSource =
            "var payload = { value: 13 };\n"
            "returnedReader() {\n"
            "    return payload;\n"
            "}\n"
            "return returnedReader;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary = "Runtime Returned Callable Capture Marks Closed Capture Object";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime returned callable capture propagation",
                 "Testing that returning a closure anchors its closed capture object with closure-capture plus return escape flags");

    assert_runtime_returned_closure_capture_has_escape_flags(kSource,
                                                             "escape_runtime_returned_callable_capture_fixture.zr",
                                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN,
                                                             ZR_NULL);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_runtime_global_callable_capture_marks_closed_capture_object(void) {
    static const TZrChar *kSource =
            "var payload = { value: 17 };\n"
            "globalReader() {\n"
            "    return payload;\n"
            "}\n"
            "globalCallable = globalReader;\n"
            "return globalReader;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary = "Runtime Global Callable Capture Marks Closed Capture Object";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime global callable capture propagation",
                 "Testing that binding a closure into a global anchors its closed capture object with closure-capture plus global-root escape flags");

    assert_runtime_returned_closure_capture_has_escape_flags(kSource,
                                                             "escape_runtime_global_callable_capture_fixture.zr",
                                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT,
                                                             ZR_NULL);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags(void) {
    static const TZrChar *kSource =
            "var payload = { value: 19 };\n"
            "returnedReader() {\n"
            "    return payload;\n"
            "}\n"
            "return returnedReader;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary =
            "Binary Roundtrip Runtime Returned Callable Capture Preserves Closed Capture Escape Flags";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime returned callable capture roundtrip",
                 "Testing that compile -> .zro -> runtime loading preserves return-driven callable capture escape propagation onto the closed capture object");

    assert_runtime_returned_closure_capture_has_escape_flags(
            kSource,
            "escape_runtime_returned_callable_capture_roundtrip_fixture.zr",
            ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN,
            "escape_runtime_returned_callable_capture_roundtrip.zro");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags(void) {
    static const TZrChar *kSource =
            "var payload = { value: 23 };\n"
            "globalReader() {\n"
            "    return payload;\n"
            "}\n"
            "globalCallable = globalReader;\n"
            "return globalReader;\n";
    SZrTestTimer timer;
    const TZrChar *testSummary =
            "Binary Roundtrip Runtime Global Callable Capture Preserves Closed Capture Escape Flags";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime global callable capture roundtrip",
                 "Testing that compile -> .zro -> runtime loading preserves global-binding callable capture escape propagation onto the closed capture object");

    assert_runtime_returned_closure_capture_has_escape_flags(
            kSource,
            "escape_runtime_global_callable_capture_roundtrip_fixture.zr",
            ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT,
            "escape_runtime_global_callable_capture_roundtrip.zro");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}
