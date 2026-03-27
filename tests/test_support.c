#include "test_support.h"

#include <stdlib.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/stack.h"

TZrPtr zr_test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
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

SZrState* zr_test_create_state(FZrPanicHandlingFunction panicHandler) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState* global = ZrGlobalStateNew(zr_test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    SZrState* mainState = global->mainThreadState;
    if (mainState != ZR_NULL) {
        ZrGlobalStateInitRegistry(mainState, global);
        global->panicHandlingFunction = panicHandler;
    }

    return mainState;
}

void zr_test_destroy_state(SZrState* state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    ZrGlobalStateFree(state->global);
}

TBool zr_test_execute_function(SZrState* state, SZrFunction* function, SZrTypeValue* result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrClosure* closure = ZrClosureNew(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = function;
    ZrClosureInitValue(state, closure);

    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrFunctionCheckStackAndGc(state, function->stackSize + 1, base);

    SZrTypeValue* closureValue = ZrStackGetValue(state->stackTop.valuePointer);
    ZrValueInitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    ZrFunctionCall(state, base, 1);

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    ZrValueCopy(state, result, ZrStackGetValue(base));
    return ZR_TRUE;
}

TBool zr_test_execute_function_expect_int64(SZrState* state, SZrFunction* function, TInt64* result) {
    SZrTypeValue returnValue;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_test_execute_function(state, function, &returnValue)) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_INT(returnValue.type)) {
        return ZR_FALSE;
    }

    *result = returnValue.value.nativeObject.nativeInt64;
    return ZR_TRUE;
}
