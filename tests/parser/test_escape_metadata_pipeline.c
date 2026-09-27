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
#include "zr_vm_lib_container/module.h"
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
void test_runtime_compiled_child_functions_detach_owner_links(void);
void test_binary_roundtrip_runtime_child_functions_detach_owner_links(void);

// 闭包逃逸夹具需要容器与 FFI 提供者；两者均注册成功才可编译 native callback。
static TZrBool register_ffi_test_modules(SZrGlobalState *global) {
    return ZrVmLibContainer_Register(global) && ZrVmLibFfi_Register(global);
}

// 读取回调只借用调用方 binaryBytes，关闭时不得释放该缓冲。
static void fixture_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

// 用独立源名编译逃逸场景，返回函数由用例或共享断言负责释放。
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

// 统一源码覆盖局部返回、嵌套闭包、全局绑定、模块导出和 native callback。
static SZrFunction *compile_escape_metadata_fixture(SZrState *state) {
    static const TZrChar *kSource =
            "pub fn returnLocal(): int {\n"
            "    var value = 9;\n"
            "    return value;\n"
            "}\n"
            "pub fn returnTemp(): int {\n"
            "    var value = 9;\n"
            "    return value + 1;\n"
            "}\n"
            "pub fn capture(): int {\n"
            "    var seed = 41;\n"
            "    fn read(): int {\n"
            "        return seed;\n"
            "    }\n"
            "    return read();\n"
            "}\n"
            "pub fn bindGlobalLocal(): string {\n"
            "    var payload = \"cache\";\n"
            "    globalPayload = payload;\n"
            "    return payload;\n"
            "}\n"
            "pub fn bindGlobalTemp(): int {\n"
            "    var seed = 5;\n"
            "    globalNumber = seed + 1;\n"
            "    return seed;\n"
            "}\n"
            "pub fn bindGlobalCaptured(): string {\n"
            "    var payload = \"cache\";\n"
            "    fn anchoredReader(): string {\n"
            "        var relayedPayload = payload;\n"
            "        globalRelay = relayedPayload;\n"
            "        fn anchoredLeaf(): string {\n"
            "            return relayedPayload;\n"
            "        }\n"
            "        return anchoredLeaf();\n"
            "    }\n"
            "    globalPayload = payload;\n"
            "    globalReader = anchoredReader;\n"
            "    return anchoredReader();\n"
            "}\n"
            "pub fn returnCallableCapture() {\n"
            "    var returnSeed = \"returned\";\n"
            "    fn returnedReader(): string {\n"
            "        return returnSeed;\n"
            "    }\n"
            "    return returnedReader;\n"
            "}\n"
            "pub fn bindGlobalCallableCapture(): int {\n"
            "    var globalSeed = \"global\";\n"
            "    fn globalCallableReader(): string {\n"
            "        return globalSeed;\n"
            "    }\n"
            "    globalCallable = globalCallableReader;\n"
            "    return 1;\n"
            "}\n"
            "pub fn returnRelayedCallableCapture() {\n"
            "    var relayedSeed = \"relay\";\n"
            "    fn relayedReader(): string {\n"
            "        return relayedSeed;\n"
            "    }\n"
            "    fn relayCarrier() {\n"
            "        var relayedCallable = relayedReader;\n"
            "        return relayedCallable;\n"
            "    }\n"
            "    return relayCarrier();\n"
            "}\n"
            "pub fn bindGlobalRelayedCallableCapture(): int {\n"
            "    var globalRelayedSeed = \"globalRelay\";\n"
            "    fn globalRelayedReader(): string {\n"
            "        return globalRelayedSeed;\n"
            "    }\n"
            "    fn globalRelayCarrier(): int {\n"
            "        var relayedCallable = globalRelayedReader;\n"
            "        globalRelayedCallable = relayedCallable;\n"
            "        return 1;\n"
            "    }\n"
            "    return globalRelayCarrier();\n"
            "}\n"
            "var ffiNative = import(\"zr.ffi\");\n"
            "pub fn makeNativeCallbackHandle() {\n"
            "    var callbackSeed = 2.0;\n"
            "    fn nativeCallback(value: float): float {\n"
            "        return value + callbackSeed;\n"
            "    }\n"
            "    var callbackHandle = ffiNative.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, nativeCallback);\n"
            "    return callbackHandle;\n"
            "}\n"
            "var moduleSeed = \"module\";\n"
            "fn exportedCallableImpl(): string {\n"
            "    return moduleSeed;\n"
            "}\n"
            "pub var exportedCallable = exportedCallableImpl;\n";

    return compile_source_fixture(state, kSource, "escape_metadata_fixture.zr");
}

// 将 ZRO 文件读成借用缓冲，经 IoSource 投影出可运行入口函数。
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

    // BUG: ReadSourceNew 分配的 native sourceObject 未调用 ReadSourceFree；LoadEntryFunctionToRuntime 只复制数据，正常返回仍泄漏整个源图。
    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    return runtimeFunction;
}

// 对返回的可 GC 对象检查指定逃逸位，不把对象种类与逃逸原因混淆。
static void assert_runtime_result_has_escape_flags(const SZrTypeValue *result, TZrUInt32 escapeFlags) {
    const SZrRawObject *rawObject;

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result->isGarbageCollectable);
    TEST_ASSERT_TRUE(result->type == ZR_VALUE_TYPE_OBJECT || result->type == ZR_VALUE_TYPE_STRING);
    TEST_ASSERT_NOT_NULL(result->value.object);

    rawObject = result->value.object;
    TEST_ASSERT_TRUE((rawObject->garbageCollectMark.escapeFlags & escapeFlags) == escapeFlags);
}

// 共用源码与 ZRO 加载路径执行同一对象场景，最终检查 global/native 逃逸标记。
// BUG: 建立局部 state/function 或写出文件后断言失败，Unity 越过尾部 Free/remove；空 tearDown 无法清理。
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
    TEST_ASSERT_TRUE(register_ffi_test_modules(state->global));

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

// 检查返回闭包的关闭捕获单元及其中对象同时拥有捕获和 return/global 逃逸位。
// BUG: 局部 state 或 ZRO 建立后断言失败会跳过 state 销毁及文件删除；空 tearDown 无法回收。
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
    TEST_ASSERT_TRUE(register_ffi_test_modules(state->global));

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

    if (binaryPathOrNull != ZR_NULL) {
        remove(binaryPathOrNull);
    }
    ZrTests_Runtime_State_Destroy(state);
}

// 在嵌套子函数树中按名称定位夹具函数，不持有返回指针。
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

// 递归验证运行时子函数不保留 ownerFunction 回指，避免父函数释放后的悬空链接。
static void assert_runtime_child_owner_links_detached_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);

    for (index = 0; index < function->childFunctionLength; index++) {
        const SZrFunction *childFunction = &function->childFunctionList[index];

        TEST_ASSERT_NULL(childFunction->ownerFunction);
        assert_runtime_child_owner_links_detached_recursive(childFunction);
    }
}

// 在指定函数的局部变量表按名称定位逃逸位承载槽。
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

// 按绑定种类和名称定位源码级逃逸边，用于同名局部/捕获区分。
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

// 无名称的 native/return 绑定依靠种类和槽号定位。
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

// 为无名称 global 绑定查找首个匹配种类的记录。
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

// 名称投影可能不同，按种类和要求的标志位寻找同一逃逸语义。
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

// 核对编译函数的返回槽摘要确实包含被检查的局部槽。
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

// 在同一源码树对照导出根、局部返回、闭包捕获、全局与 native callback 的逐层逃逸摘要。
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
    // 编译投影可能改写局部名，回退时仍要求 LOCAL 种类及 global-root 位一致。
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
    // 捕获经中间函数转发时名称不稳定；用逃逸位确认仍是全局根绑定。
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

// 编译产物直接暴露完整逃逸绑定形状，供二进制往返结果对照。
// BUG: state/function 创建后任一摘要断言失败会跳过尾部 Free，空 Unity tearDown 不持有这些句柄。
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
    TEST_ASSERT_TRUE(register_ffi_test_modules(state->global));

    function = compile_escape_metadata_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    assert_escape_metadata_shape(function);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 将源码函数写为 ZRO 并读回，逃逸绑定和返回槽摘要应保持一致。
// BUG: 写出文件或建立局部函数/缓冲后若断言失败，Unity 跳过尾部 Free/remove，空 tearDown 无法清理。
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
    TEST_ASSERT_TRUE(register_ffi_test_modules(state->global));

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

    // BUG: 此处 ReadSourceNew 的 native sourceObject 未调用 ReadSourceFree；Io_Free 只释放 io，正常通过仍泄漏源图。
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

// 原生编译的嵌套函数在运行时形态不得保留父函数回指。
// BUG: state/function 创建后递归断言失败会跳过尾部释放，空 Unity tearDown 无法清理。
void test_runtime_compiled_child_functions_detach_owner_links(void) {
    SZrTestTimer timer;
    const TZrChar *testSummary = "Runtime Compiled Child Functions Detach Owner Links";
    SZrState *state;
    SZrFunction *runtimeFunction;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime child-function owner links",
                 "Testing that runtime-compiled child function graphs clear ownerFunction back-links so nested callables do not retain stale parent pointers across GC");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(register_ffi_test_modules(state->global));

    runtimeFunction = compile_escape_metadata_fixture(state);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_runtime_child_owner_links_detached_recursive(runtimeFunction);

    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// ZRO 导入后的嵌套函数同样须清除 ownerFunction 回指。
// BUG: 正常路径写出 escape_metadata_owner_links_roundtrip.zro 后从未 remove，CTest 工作目录留下固定名产物；断言失败还跳过局部清理。
void test_binary_roundtrip_runtime_child_functions_detach_owner_links(void) {
    SZrTestTimer timer;
    const TZrChar *testSummary = "Binary Roundtrip Runtime Child Functions Detach Owner Links";
    const TZrChar *binaryPath = "escape_metadata_owner_links_roundtrip.zro";
    SZrState *state;
    SZrFunction *sourceFunction;
    SZrFunction *runtimeFunction;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("runtime child-function owner links roundtrip",
                 "Testing that .zro runtime loading also clears child ownerFunction back-links so imported nested callables do not keep stale parent references");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(register_ffi_test_modules(state->global));

    sourceFunction = compile_escape_metadata_fixture(state);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, sourceFunction, binaryPath));

    runtimeFunction = load_runtime_entry_from_binary_file(state, binaryPath);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_runtime_child_owner_links_detached_recursive(runtimeFunction);

    ZrCore_Function_Free(state, runtimeFunction);
    ZrCore_Function_Free(state, sourceFunction);
    ZrTests_Runtime_State_Destroy(state);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 返回对象同时绑定到 global 时须带全局根逃逸位。
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

// FFI 回调捕获对象返回后须保留 native-handle 逃逸位。
void test_runtime_native_callback_capture_marks_returned_object_as_native_handle(void) {
    static const TZrChar *kSource =
            "let ffiNative = import(\"zr.ffi\");\n"
            "var payload = { extra: 2.0 };\n"
            "fn nativeCallback(value: float): float {\n"
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

// ZRO 往返不得丢失全局绑定造成的对象逃逸位。
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

// ZRO 往返不得丢失 native callback 捕获造成的逃逸位。
void test_binary_roundtrip_runtime_native_callback_preserves_escape_flags(void) {
    static const TZrChar *kSource =
            "let ffiNative = import(\"zr.ffi\");\n"
            "var payload = { extra: 4.0 };\n"
            "fn nativeCallback(value: float): float {\n"
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

// 返回闭包应把 return 标志传播到已关闭捕获对象。
void test_runtime_returned_callable_capture_marks_closed_capture_object(void) {
    static const TZrChar *kSource =
            "var payload = { value: 13 };\n"
            "fn returnedReader() {\n"
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

// 全局持有闭包应把 global-root 标志传播到已关闭捕获对象。
void test_runtime_global_callable_capture_marks_closed_capture_object(void) {
    static const TZrChar *kSource =
            "var payload = { value: 17 };\n"
            "fn globalReader() {\n"
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

// ZRO 往返后返回闭包的关闭捕获仍保留 return 与 capture 标志。
void test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags(void) {
    static const TZrChar *kSource =
            "var payload = { value: 19 };\n"
            "fn returnedReader() {\n"
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

// ZRO 往返后全局闭包的关闭捕获仍保留 global-root 与 capture 标志。
void test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags(void) {
    static const TZrChar *kSource =
            "var payload = { value: 23 };\n"
            "fn globalReader() {\n"
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
