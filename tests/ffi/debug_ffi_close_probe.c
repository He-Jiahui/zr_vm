#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"

static SZrState *create_probe_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibMath_Register(state->global);
        ZrVmLibSystem_Register(state->global);
        ZrVmLibFfi_Register(state->global);
    }

    return state;
}

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString) sourceNameText);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const char *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

typedef struct ZrProbeExecuteCaptureRequest {
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrProbeExecuteCaptureRequest;

static void probe_execute_capture_body(SZrState *state, TZrPtr arguments) {
    ZrProbeExecuteCaptureRequest *request = (ZrProbeExecuteCaptureRequest *)arguments;
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
    ZrProbeExecuteCaptureRequest request;
    EZrThreadStatus status;

    if (errorBuffer != NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    memset(&request, 0, sizeof(request));
    request.function = function;
    status = ZrCore_Exception_TryRun(state, probe_execute_capture_body, &request);

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
        const char *nativeError = string_value_native(state, errorValue);
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

int main(int argc, char **argv) {
    static const char *kLibraryTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "return (!beforeClose && afterFirstClose && afterSecondClose) ? 1 : 0;\n";
    static const char *kLibraryBoolTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var beforeClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterFirstClose = lib.isClosed();\n"
            "lib.close();\n"
            "var afterSecondClose = lib.isClosed();\n"
            "return !beforeClose && afterFirstClose && afterSecondClose;\n";
    static const char *kPrimitiveTemplate =
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
    static const char *kPrimitiveAddTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_add_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "return add.call([7, 5]);\n";
    static const char *kPrimitiveMulTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var mul = lib.getSymbol(\"zr_ffi_mul_f64\", {\n"
            "  returnType: \"f64\",\n"
            "  parameters: [{ type: \"f64\" }, { type: \"f64\" }]\n"
            "});\n"
            "return mul.call([2.0, 4.0]);\n";
    static const char *kPrimitiveStrlenTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var strlenUtf8 = lib.getSymbol(\"zr_ffi_strlen_utf8\", {\n"
            "  returnType: \"u64\",\n"
            "  parameters: [{ type: { kind: \"string\", encoding: \"utf8\" } }]\n"
            "});\n"
            "return strlenUtf8.call([\"hello\"]);\n";
    static const char *kPrimitiveStdcallTemplate =
            "var ffi = %%import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var add = lib.getSymbol(\"zr_ffi_stdcall_add_i32\", {\n"
            "  returnType: \"i32\",\n"
            "  abi: \"stdcall\",\n"
            "  parameters: [{ type: \"i32\" }, { type: \"i32\" }]\n"
            "});\n"
            "return add.call([4, 9]);\n";
    static const char *kPrimitiveNumericFoldSource =
            "return 12 + 8.0 + 5;\n";
    static const char *kTernarySource =
            "return true ? 1 : 0;\n";
    static const char *kLoadFailureSource =
            "var ffi = %import(\"zr.ffi\");\n"
            "ffi.loadLibrary(\"__zr_ffi_missing_fixture__\");\n"
            "return 0;\n";
    static const char *kAllTemplate =
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
            "return (!beforeClose && afterFirstClose && afterSecondClose) ? 1 : 0;\n";
    static const char *kCallbackSource =
            "var ffi = %import(\"zr.ffi\");\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "return 1;\n";
    static const char *kBufferSource =
            "var ffi = %import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return 1;\n";
    static const char *kAllWithoutLibrarySource =
            "var ffi = %import(\"zr.ffi\");\n"
            "var cb = ffi.callback({ returnType: \"f64\", parameters: [{ type: \"f64\" }] }, (value) => {\n"
            "    return value;\n"
            "});\n"
            "cb.close();\n"
            "cb.close();\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.close();\n"
            "buffer.close();\n"
            "return 1;\n";
    const char *mode = argc > 1 ? argv[1] : "all-no-lib";
    const char *libraryPath = argc > 2 ? argv[2] : NULL;
    const char *source = kAllWithoutLibrarySource;
    char sourceBuffer[4096];
    char escapedPath[4096];
    char errorBuffer[512];
    SZrState *state;
    SZrFunction *function;
    SZrTypeValue result;

    if (strcmp(mode, "callback") == 0) {
        source = kCallbackSource;
    } else if (strcmp(mode, "buffer") == 0) {
        source = kBufferSource;
    } else if (strcmp(mode, "ternary") == 0) {
        source = kTernarySource;
    } else if (strcmp(mode, "load-failure") == 0) {
        source = kLoadFailureSource;
    } else if (strcmp(mode, "primitive-numeric-fold") == 0) {
        source = kPrimitiveNumericFoldSource;
    } else if (strcmp(mode, "library") == 0 || strcmp(mode, "all") == 0 || strcmp(mode, "primitive") == 0 ||
               strcmp(mode, "primitive-add") == 0 || strcmp(mode, "primitive-mul") == 0 ||
               strcmp(mode, "primitive-strlen") == 0 || strcmp(mode, "primitive-stdcall") == 0) {
        const char *templateSource = kAllTemplate;
        if (libraryPath == NULL) {
            fprintf(stderr, "library path is required for mode=%s\n", mode);
            return 1;
        }
        if (strcmp(mode, "library") == 0) {
            templateSource = kLibraryTemplate;
        } else if (strcmp(mode, "primitive") == 0) {
            templateSource = kPrimitiveTemplate;
        } else if (strcmp(mode, "primitive-add") == 0) {
            templateSource = kPrimitiveAddTemplate;
        } else if (strcmp(mode, "primitive-mul") == 0) {
            templateSource = kPrimitiveMulTemplate;
        } else if (strcmp(mode, "primitive-strlen") == 0) {
            templateSource = kPrimitiveStrlenTemplate;
        } else if (strcmp(mode, "primitive-stdcall") == 0) {
            templateSource = kPrimitiveStdcallTemplate;
        }
        escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), libraryPath);
        snprintf(sourceBuffer, sizeof(sourceBuffer), templateSource, escapedPath);
        source = sourceBuffer;
    } else if (strcmp(mode, "library-bool") == 0) {
        if (libraryPath == NULL) {
            fprintf(stderr, "library path is required for mode=%s\n", mode);
            return 1;
        }
        escape_for_zr_string_literal(escapedPath, sizeof(escapedPath), libraryPath);
        snprintf(sourceBuffer, sizeof(sourceBuffer), kLibraryBoolTemplate, escapedPath);
        source = sourceBuffer;
    }

    state = create_probe_state();
    if (state == ZR_NULL) {
        fprintf(stderr, "failed to create probe state\n");
        return 2;
    }

    function = compile_source(state, source, "debug_ffi_close_probe.zr");
    if (function == ZR_NULL) {
        fprintf(stderr, "failed to compile probe source\n");
        ZrTests_Runtime_State_Destroy(state);
        return 3;
    }

    if (strcmp(mode, "load-failure") == 0) {
        EZrThreadStatus status = execute_function_capture_status(state, function, ZR_NULL, errorBuffer, sizeof(errorBuffer));
        printf("capture mode=%s status=%d error='%s'\n", mode, (int) status, errorBuffer);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
        return status == ZR_THREAD_STATUS_FINE ? 5 : 0;
    }

    {
        EZrThreadStatus status =
                execute_function_capture_status(state, function, &result, errorBuffer, sizeof(errorBuffer));
        if (status != ZR_THREAD_STATUS_FINE) {
            fprintf(stderr,
                    "runtime execute failed: status=%d error=%s\n",
                    (int) status,
                    errorBuffer[0] != '\0' ? errorBuffer : "<non-string>");
            ZrCore_Function_Free(state, function);
            ZrTests_Runtime_State_Destroy(state);
            return 4;
        }
    }

    printf("probe mode=%s resultType=%d", mode, (int) result.type);
    if (ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        printf(" value=%f", result.value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
        printf(" value=%lld", (long long) result.value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        printf(" value=%llu", (unsigned long long) result.value.nativeObject.nativeUInt64);
    } else if (result.type == ZR_VALUE_TYPE_BOOL) {
        printf(" value=%s", result.value.nativeObject.nativeBool ? "true" : "false");
    }
    printf("\n");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
