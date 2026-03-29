#include "runtime_support.h"

#include <stdlib.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/stack.h"

TZrPtr ZrTests_Runtime_Allocator_Default(TZrPtr userData,
                                         TZrPtr pointer,
                                         TZrSize originalSize,
                                         TZrSize newSize,
                                         TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

SZrState *ZrTests_Runtime_State_Create(FZrPanicHandlingFunction panicHandler) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(ZrTests_Runtime_Allocator_Default, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    SZrState *mainState = global->mainThreadState;
    if (mainState != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        global->panicHandlingFunction = panicHandler;
    }

    return mainState;
}

void ZrTests_Runtime_State_Destroy(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    ZrCore_GlobalState_Free(state->global);
}

TZrBool ZrTests_Runtime_Function_Execute(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrClosure *closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    TZrStackValuePointer base = state->stackTop.valuePointer;
    SZrFunctionStackAnchor baseAnchor;
    base = ZrCore_Function_CheckStackAndAnchor(state, function->stackSize + 1, base, base, &baseAnchor);

    SZrTypeValue *closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    base = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    return ZR_TRUE;
}

TZrBool ZrTests_Runtime_Function_ExecuteExpectInt64(SZrState *state, SZrFunction *function, TZrInt64 *result) {
    SZrTypeValue returnValue;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Runtime_Function_Execute(state, function, &returnValue)) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_INT(returnValue.type)) {
        return ZR_FALSE;
    }

    *result = returnValue.value.nativeObject.nativeInt64;
    return ZR_TRUE;
}
