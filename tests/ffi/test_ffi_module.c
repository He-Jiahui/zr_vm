#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_parser.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0x4646495F54455354ULL, &callbacks);
    SZrState *mainState;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    mainState = global->mainThreadState;
    if (mainState != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        ZrParser_ToGlobalState_Register(mainState);
        ZrVmLibMath_Register(global);
        ZrVmLibSystem_Register(global);
        ZrVmLibFfi_Register(global);
    }

    return mainState;
}

static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static SZrFunction *compile_source(SZrState *state, const TZrChar *source, const TZrChar *sourceNameText) {
    SZrAstNode *ast;
    SZrString *sourceName;
    SZrFunction *compiled;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_CreateFromNative(state, sourceNameText);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        fprintf(stderr,
                "parse_source failed for %s (threadStatus=%d hasCurrentException=%d)\n",
                sourceNameText,
                (int)state->threadStatus,
                (int)state->hasCurrentException);
        if (state->hasCurrentException) {
            ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        }
        return ZR_NULL;
    }

    compiled = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    if (compiled == ZR_NULL) {
        fprintf(stderr,
                "compile_source failed for %s (threadStatus=%d hasCurrentException=%d)\n",
                sourceNameText,
                (int)state->threadStatus,
                (int)state->hasCurrentException);
        if (state->hasCurrentException) {
            ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        }
    }

    return compiled;
}

static const TZrChar *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

static const SZrTypeValue *object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static void escape_for_zr_string_literal(char *destination, size_t destinationSize, const char *source) {
    size_t writeIndex = 0;
    size_t readIndex = 0;

    if (destination == NULL || destinationSize == 0) {
        return;
    }

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }

    while (source[readIndex] != '\0' && writeIndex + 1 < destinationSize) {
        if (source[readIndex] == '\\' || source[readIndex] == '"') {
            if (writeIndex + 2 >= destinationSize) {
                break;
            }
            destination[writeIndex++] = '\\';
        }
        destination[writeIndex++] = source[readIndex++];
    }

    destination[writeIndex] = '\0';
}

typedef struct ZrFfiExecuteCaptureRequest {
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrFfiExecuteCaptureRequest;

static void zr_ffi_execute_capture_body(SZrState *state, TZrPtr arguments) {
    ZrFfiExecuteCaptureRequest *request = (ZrFfiExecuteCaptureRequest *)arguments;
    SZrClosure *closure;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return;
    }

    closure->function = request->function;
    request->closure = closure;
    ZrCore_Closure_InitValue(state, closure);

    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &baseAnchor);

    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    request->callCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static EZrThreadStatus execute_function_capture_status(SZrState *state,
                                                       SZrFunction *function,
                                                       SZrTypeValue *result,
                                                       char *errorBuffer,
                                                       size_t errorBufferSize) {
    ZrFfiExecuteCaptureRequest request;
    EZrThreadStatus status;

    if (errorBuffer != NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    memset(&request, 0, sizeof(request));
    request.function = function;
    status = ZrCore_Exception_TryRun(state, zr_ffi_execute_capture_body, &request);

    if (status == ZR_THREAD_STATUS_FINE && request.callCompleted && request.resultBase != ZR_NULL) {
        if (result != ZR_NULL) {
            ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
        }
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return ZR_THREAD_STATUS_FINE;
    }

    if (errorBuffer != NULL && errorBufferSize > 0 && state->stackTop.valuePointer > state->stackBase.valuePointer) {
        const SZrTypeValue *errorValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1);
        const TZrChar *nativeError = string_value_native(state, errorValue);
        if (nativeError != ZR_NULL) {
            strncpy(errorBuffer, nativeError, errorBufferSize - 1);
            errorBuffer[errorBufferSize - 1] = '\0';
        }
    }

    if (request.closure != ZR_NULL) {
        request.closure->function = ZR_NULL;
    }

    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_State_ResetThread(state, status);
        return status;
    }
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        status = state->threadStatus;
        ZrCore_State_ResetThread(state, status);
        return status;
    }

    return ZR_THREAD_STATUS_RUNTIME_ERROR;
}

void setUp(void) {}

void tearDown(void) {}

static void test_zr_ffi_import_exposes_known_types_and_functions(void) {
    SZrTestTimer timer;
    SZrState *state;
    SZrObjectModule *module;
    const TZrChar *exports[] = {"loadLibrary", "callback", "sizeof", "alignof", "nullPointer",
                                "LibraryHandle", "SymbolHandle", "CallbackHandle", "PointerHandle", "BufferHandle"};
    TZrSize index;

    ZR_TEST_START("zr.ffi import exposes known types and functions");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    if (module == ZR_NULL) {
        SZrString *modulePath = ZrCore_String_CreateFromNative(state, "zr.ffi");
        TEST_ASSERT_NOT_NULL(modulePath);
        TEST_ASSERT_NOT_NULL(ZrCore_Module_ImportByPath(state, modulePath));
        module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    }

    TEST_ASSERT_NOT_NULL(module);
    for (index = 0; index < ZR_ARRAY_COUNT(exports); index++) {
        SZrString *exportName = ZrCore_String_CreateFromNative(state, exports[index]);
        TEST_ASSERT_NOT_NULL(exportName);
        TEST_ASSERT_NOT_NULL(ZrCore_Module_GetPubExport(state, module, exportName));
    }

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi import exposes known types and functions");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_buffer_handle_allocate_is_callable(void) {
    SZrTestTimer timer;
    SZrState *state;
    SZrObjectModule *module;
    const SZrTypeValue *bufferHandleValue;
    const SZrTypeValue *allocateValue;
    SZrObject *bufferHandleObject;

    ZR_TEST_START("zr.ffi BufferHandle.allocate is callable");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    if (module == ZR_NULL) {
        SZrString *modulePath = ZrCore_String_CreateFromNative(state, "zr.ffi");
        TEST_ASSERT_NOT_NULL(modulePath);
        TEST_ASSERT_NOT_NULL(ZrCore_Module_ImportByPath(state, modulePath));
        module = ZrLib_Module_GetLoaded(state, "zr.ffi");
    }

    TEST_ASSERT_NOT_NULL(module);
    bufferHandleValue = ZrLib_Module_GetExport(state, "zr.ffi", "BufferHandle");
    TEST_ASSERT_NOT_NULL(bufferHandleValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, bufferHandleValue->type);

    bufferHandleObject = ZR_CAST_OBJECT(state, bufferHandleValue->value.object);
    TEST_ASSERT_NOT_NULL(bufferHandleObject);
    allocateValue = object_field(state, bufferHandleObject, "allocate");
    TEST_ASSERT_NOT_NULL(allocateValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(allocateValue->type) || allocateValue->type == ZR_VALUE_TYPE_CLOSURE);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi BufferHandle.allocate is callable");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_can_load_fixture_and_call_primitive_symbols(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_add_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var mul = lib.getSymbol(\"zr_ffi_mul_f64\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [{ type: \"f64\" }, { type: \"f64\" }]\n"
            "});\n"
            "var strlenUtf8 = lib.getSymbol(\"zr_ffi_strlen_utf8\", {\n"
            "  returnType: \"u64\",\n"
            "  parameters: [{ type: { kind: \"string\", encoding: \"utf8\" } }]\n"
            "});\n"
            "return add.call([7, 5]) + mul.call([2.0, 4.0]) + strlenUtf8.call([\"hello\"]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can load fixture and call primitive symbols");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_primitive_roundtrip.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type) || ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
                     ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));

    if (ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 25.0, result.value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(25, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(25, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can load fixture and call primitive symbols");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_can_roundtrip_struct_symbols(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var pointType = {\n"
            "  kind: \"struct\",\n"
            "  name: \"FixturePoint\",\n"
            "  fields: [\n"
            "    { name: \"x\", type: \"i32\" },\n"
            "    { name: \"y\", type: \"i32\" }\n"
            "  ]\n"
            "};\n"
            "var makePoint = lib.getSymbol(\"zr_ffi_make_point\", {\n"
            "  returnType: pointType,\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var sumPoint = lib.getSymbol(\"zr_ffi_sum_point\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: pointType }]\n"
            "});\n"
            "var point = makePoint.call([3, 9]);\n"
            "return sumPoint.call([point]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can roundtrip struct symbols");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_struct_roundtrip.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(12, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(12, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can roundtrip struct symbols");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_buffer_and_pointer_methods_work(void) {
    static const TZrChar *kSource =
            "var ffi = %import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var ptr = buffer.pin();\n"
            "var typed = ptr.as({ kind: \"pointer\", to: \"u8\", direction: \"inout\" });\n"
            "buffer.write(0, [1, 2, 3, 4]);\n"
            "var bytes = buffer.read(0, 4);\n"
            "return bytes[0] + bytes[3];\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi buffer and pointer methods work");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSource, "ffi_buffer_pointer_methods.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(5, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(5, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi buffer and pointer methods work");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_can_fill_buffer_via_symbol(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var fillBytes = lib.getSymbol(\"zr_ffi_fill_bytes\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [\n"
            "    { type: { kind: \"pointer\", to: \"u8\", direction: \"inout\" } },\n"
            "    { type: \"u64\" },\n"
            "    { type: \"u8\" }\n"
            "  ]\n"
            "});\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var ptr = buffer.pin();\n"
            "var written = fillBytes.call([ptr, 4, 10]);\n"
            "return written + buffer.read(0, 4)[0];\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can fill buffer via symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_fill_buffer_symbol.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(14, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(14, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can fill buffer via symbol");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_can_create_callback_handle(void) {
    static const TZrChar *kSource =
            "var ffi = %import(\"zr.ffi\");\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return cb != null;\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can create callback handle");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSource, "ffi_create_callback_handle.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can create callback handle");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_can_call_callback_symbol(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var applyCallback = lib.getSymbol(\"zr_ffi_apply_callback\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [\n"
            "    { type: \"f64\" },\n"
            "    { type: { kind: \"function\", returnType: \"f64\", parameters: [{ type: \"f64\" }] } }\n"
            "  ]\n"
            "});\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return applyCallback.call([5.0, cb]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can call callback symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_callback_symbol.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 10.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can call callback symbol");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_can_roundtrip_structs_buffers_and_callbacks(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var pointType = {\n"
            "  kind: \"struct\",\n"
            "  name: \"FixturePoint\",\n"
            "  fields: [\n"
            "    { name: \"x\", type: \"i32\" },\n"
            "    { name: \"y\", type: \"i32\" }\n"
            "  ]\n"
            "};\n"
            "var pointPtr = { kind: \"pointer\", to: pointType, direction: \"out\" };\n"
            "var makePoint = lib.getSymbol(\"zr_ffi_make_point\", {\n"
            "  returnType: pointType,\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var sumPoint = lib.getSymbol(\"zr_ffi_sum_point\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: pointType }]\n"
            "});\n"
            "var fillPoint = lib.getSymbol(\"zr_ffi_fill_point\", {\n"
            "  returnType: \"void\",\n"
            "  parameters: [{ type: pointPtr }, { type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "var fillBytes = lib.getSymbol(\"zr_ffi_fill_bytes\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [\n"
            "    { type: { kind: \"pointer\", to: \"u8\", direction: \"inout\" } },\n"
            "    { type: \"u64\" },\n"
            "    { type: \"u8\" }\n"
            "  ]\n"
            "});\n"
            "var incrementI32 = lib.getSymbol(\"zr_ffi_increment_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: { kind: \"pointer\", to: \"i32\", direction: \"inout\" } }]\n"
            "});\n"
            "var applyCallback = lib.getSymbol(\"zr_ffi_apply_callback\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [\n"
            "    { type: \"f64\" },\n"
            "    { type: { kind: \"function\", returnType: \"f64\", parameters: [{ type: \"f64\" }] } }\n"
            "  ]\n"
            "});\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "var point = makePoint.call([3, 9]);\n"
            "var sumA = sumPoint.call([point]);\n"
            "var pointBuffer = ffi.BufferHandle.allocate(8);\n"
            "var pointOutPtr = pointBuffer.pin().as(pointPtr);\n"
            "fillPoint.call([pointOutPtr, 4, 7]);\n"
            "var pointFromBuffer = pointOutPtr.read(pointType);\n"
            "var sumB = sumPoint.call([pointFromBuffer]);\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "var bufferPtr = buffer.pin();\n"
            "var filled = fillBytes.call([bufferPtr, 4, 10]);\n"
            "var first = buffer.read(0, 4)[0];\n"
            "var outBuffer = ffi.BufferHandle.allocate(4);\n"
            "outBuffer.write(0, [10, 0, 0, 0]);\n"
            "var outPtr = outBuffer.pin().as({ kind: \"pointer\", to: \"i32\", direction: \"inout\" });\n"
            "incrementI32.call([outPtr]);\n"
            "var outValue = outPtr.read(\"i32\");\n"
            "var callbackResult = applyCallback.call([5.0, cb]);\n"
            "var total = 0.0;\n"
            "total = total + sumA + sumB + filled + first + outValue + callbackResult;\n"
            "return total;\n";
    SZrTestTimer timer;
    char source[8192];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi can roundtrip structs buffers and callbacks");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_struct_buffer_callback.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 58.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi can roundtrip structs buffers and callbacks");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_library_get_version_reads_fixture_symbol(void) {
    static const char *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "return lib.getVersion();\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;
    const TZrChar *versionText;

    ZR_TEST_START("zr.ffi LibraryHandle.getVersion reads version symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_library_get_version.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    versionText = string_value_native(state, &result);
    TEST_ASSERT_NOT_NULL(versionText);
    TEST_ASSERT_EQUAL_STRING("1.2.3-fixture", versionText);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi LibraryHandle.getVersion reads version symbol");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_handle_close_methods_are_idempotent(void) {
    static const char *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return !beforeClose && afterFirstClose && afterSecondClose;\n";
    SZrTestTimer timer;
    char source[2048];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi handle close methods are idempotent");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_handle_close_idempotent.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(result.type == ZR_VALUE_TYPE_BOOL || ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
                     ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(1, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(1, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi handle close methods are idempotent");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_load_library_failure_reports_load_error(void) {
    static const char *kSource =
            "var ffi = %import(\"zr.ffi\");\n"
            "ffi.loadLibrary(\"__zr_ffi_missing_fixture__\");\n"
            "return 0;\n";
    SZrTestTimer timer;
    SZrState *state;
    SZrFunction *entryFunction;
    char errorBuffer[512];

    ZR_TEST_START("zr.ffi loadLibrary failure reports LoadError");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, kSource, "ffi_load_failure.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                              execute_function_capture_status(state, entryFunction, ZR_NULL, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[LoadError]"));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi loadLibrary failure reports LoadError");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_get_symbol_failure_reports_symbol_error(void) {
    static const char *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "lib.getSymbol(\"zr_ffi_missing_symbol\", {\n"
            "    returnType: \"i32\",\n"
            "    parameters: [{ type: \"i32\" }]\n"
            "});\n"
            "return 0;\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    char errorBuffer[512];

    ZR_TEST_START("zr.ffi getSymbol failure reports SymbolError");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_missing_symbol.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                              execute_function_capture_status(state, entryFunction, ZR_NULL, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[SymbolError]"));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi getSymbol failure reports SymbolError");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_varargs_symbol_accepts_explicit_signature(void) {
    static const char *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var sumVarargs = lib.getSymbol(\"zr_ffi_sum_varargs_i32\", {\n"
            "    returnType: \"i32\",\n"
            "    varargs: true,\n"
            "    parameters: [\n"
            "        { type: \"i32\" },\n"
            "        { type: \"i32\" },\n"
            "        { type: \"i32\" },\n"
            "        { type: \"i32\" }\n"
            "    ]\n"
            "});\n"
            "return sumVarargs.call([3, 5, 6, 7]);\n";
    SZrTestTimer timer;
    char source[2048];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi varargs symbol accepts explicit signature");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_varargs_success.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(18, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(18, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi varargs symbol accepts explicit signature");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_varargs_call_without_matching_signature_reports_marshal_error(void) {
    static const char *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var sumVarargs = lib.getSymbol(\"zr_ffi_sum_varargs_i32\", {\n"
            "    returnType: \"i32\",\n"
            "    parameters: [{ type: \"i32\" }]\n"
            "});\n"
            "return sumVarargs.call([3, 5, 6, 7]);\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    char errorBuffer[512];

    ZR_TEST_START("zr.ffi varargs call without matching signature reports MarshalError");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_varargs_marshal_error.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                              execute_function_capture_status(state, entryFunction, ZR_NULL, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[MarshalError]"));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "expected 1 arguments but got 4"));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi varargs call without matching signature reports MarshalError");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_stdcall_signature_matches_platform_support(void) {
    static const char *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_stdcall_add_i32\", {\n"
            "    returnType: \"i32\",\n"
            "    abi: \"stdcall\",\n"
            "    parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "return add.call([4, 9]);\n";
    SZrTestTimer timer;
    char source[1024];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;
    char errorBuffer[512];
    EZrThreadStatus status;

    ZR_TEST_START("zr.ffi stdcall signature matches platform support");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_stdcall_support.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    status = execute_function_capture_status(state, entryFunction, &result, errorBuffer, sizeof(errorBuffer));
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(13, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(13, result.value.nativeObject.nativeInt64);
    }
#else
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "[MarshalError]"));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "stdcall is not supported"));
#endif

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi stdcall signature matches platform support");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_foreign_thread_callback_reports_error(void) {
    static const TZrChar *kSourceTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var applyCallback = lib.getSymbol(\"zr_ffi_apply_callback_foreign_thread\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [\n"
            "    { type: \"f64\" },\n"
            "    { type: { kind: \"function\", returnType: \"f64\", parameters: [{ type: \"f64\" }] } }\n"
            "  ]\n"
            "});\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "  return 3.0;\n"
            "});\n"
            "return applyCallback.call([2.0, cb]);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi foreign thread callback reports error");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_foreign_thread_callback.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi foreign thread callback reports error");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_source_extern_can_bind_and_call_symbol(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")# Add(lhs:i32, rhs:i32): i32;\n"
            "}\n"
            "return Add(7, 5);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern can bind and call symbol");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_add.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(12, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(12, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern can bind and call symbol");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_source_extern_delegate_works_with_callback(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  delegate Unary(value:f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback\")# Apply(value:f64, cb:Unary): f64;\n"
            "}\n"
            "var ffi = %%import(\"zr.ffi\");\n"
            "var cb = ffi.callback(Unary, (value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return Apply(5.0, cb);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern delegate works with callback");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_delegate.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 10.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern delegate works with callback");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_source_extern_system_callconv_uses_platform_default(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  #zr.ffi.callconv(\"system\")#\n"
            "  Add(lhs:i32, rhs:i32): i32;\n"
            "}\n"
            "return Add(4, 9);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern system callconv uses platform default");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_system_callconv.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(13, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(13, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern system callconv uses platform default");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_source_extern_struct_pack_affects_sizeof_and_alignof(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  #zr.ffi.pack(1)#\n"
            "  struct PackedPair {\n"
            "    var tag:u8;\n"
            "    var value:u32;\n"
            "  }\n"
            "}\n"
            "var ffi = %%import(\"zr.ffi\");\n"
            "return ffi.sizeof(PackedPair) + ffi.alignof(PackedPair);\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern struct pack affects sizeof and alignof");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_pack_layout.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(6, result.value.nativeObject.nativeUInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(6, result.value.nativeObject.nativeInt64);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern struct pack affects sizeof and alignof");
    ZR_TEST_DIVIDER();
}

static void test_zr_ffi_source_extern_struct_offset_overlay_controls_pointer_read(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  struct Overlay32 {\n"
            "    #zr.ffi.offset(0)# var raw:u32;\n"
            "    #zr.ffi.offset(0)# var asFloat:f32;\n"
            "  }\n"
            "}\n"
            "var ffi = %%import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(8);\n"
            "buffer.write(0, [0, 0, 128, 63, 0, 0, 0, 0]);\n"
            "var value = buffer.pin().read(Overlay32);\n"
            "return value.raw + value.asFloat;\n";
    SZrTestTimer timer;
    char source[4096];
    char escapedPath[4096];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.ffi source extern struct offset overlay controls pointer read");
    timer.startTime = clock();

    escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    snprintf(source, sizeof(source), kSourceTemplate, escapedPath);
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "ffi_source_extern_offset_overlay.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1065353217.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.ffi source extern struct offset overlay controls pointer read");
    ZR_TEST_DIVIDER();
}

int main(void) {
    UNITY_BEGIN();
    test_zr_ffi_import_exposes_known_types_and_functions();
    test_zr_ffi_buffer_handle_allocate_is_callable();
    test_zr_ffi_can_load_fixture_and_call_primitive_symbols();
    test_zr_ffi_can_roundtrip_struct_symbols();
    test_zr_ffi_buffer_and_pointer_methods_work();
    test_zr_ffi_can_fill_buffer_via_symbol();
    test_zr_ffi_can_create_callback_handle();
    test_zr_ffi_can_call_callback_symbol();
    test_zr_ffi_can_roundtrip_structs_buffers_and_callbacks();
    test_zr_ffi_library_get_version_reads_fixture_symbol();
    test_zr_ffi_handle_close_methods_are_idempotent();
    test_zr_ffi_load_library_failure_reports_load_error();
    test_zr_ffi_get_symbol_failure_reports_symbol_error();
    test_zr_ffi_varargs_symbol_accepts_explicit_signature();
    test_zr_ffi_varargs_call_without_matching_signature_reports_marshal_error();
    test_zr_ffi_stdcall_signature_matches_platform_support();
    test_zr_ffi_foreign_thread_callback_reports_error();
    test_zr_ffi_source_extern_can_bind_and_call_symbol();
    test_zr_ffi_source_extern_delegate_works_with_callback();
    test_zr_ffi_source_extern_system_callconv_uses_platform_default();
    test_zr_ffi_source_extern_struct_pack_affects_sizeof_and_alignof();
    test_zr_ffi_source_extern_struct_offset_overlay_controls_pointer_read();
    return UNITY_END();
}
