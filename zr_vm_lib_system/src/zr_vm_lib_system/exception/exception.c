//
// zr.system.exception callbacks.
//

#include "zr_vm_lib_system/exception.h"

#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"

static TZrBool system_exception_get_self_object(ZrLibCallContext *context, SZrObject **outObject) {
    SZrTypeValue *selfValue;

    if (context == ZR_NULL || outObject == ZR_NULL) {
        return ZR_FALSE;
    }

    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT || selfValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outObject = ZR_CAST_OBJECT(context->state, selfValue->value.object);
    return *outObject != ZR_NULL;
}

/* 构造异常时把任意消息转成脚本字符串，供统一异常诊断读取 message 字段。 */
static void system_exception_set_message_field(SZrState *state,
                                               SZrObject *object,
                                               const SZrTypeValue *value,
                                               const TZrChar *fieldName) {
    SZrTypeValue fieldValue;
    SZrTypeValue stableValue;
    SZrString *messageString;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
        ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
        return;
    }

    stableValue = *value;
    messageString = ZrCore_Value_ConvertToString(state, &stableValue);
    if (messageString == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetStringObject(state, &fieldValue, messageString);
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

TZrBool ZrSystem_Exception_RegisterUnhandledException(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *handlerValue = ZR_NULL;
    SZrGlobalState *global;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_CheckArity(context, 1, 1) ||
        !ZrLib_CallContext_ReadFunction(context, 0, &handlerValue) ||
        handlerValue == ZR_NULL) {
        return ZR_FALSE;
    }

    /* TODO: 当前全仓生产执行出口未见读取该 handler；仅 tests/harness/runtime_support.c
     * 显式分发。需核对 CLI/嵌入宿主的未处理异常路径是否应接入此回调。 */
    global = context->state->global;
    ZrCore_Value_Copy(context->state, &global->unhandledExceptionHandler, handlerValue);
    global->hasUnhandledExceptionHandler = ZR_TRUE;

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Exception_Constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = ZR_NULL;
    SZrTypeValue *selfValue;
    SZrObject *stacksArray;
    SZrTypeValue stacksValue;
    SZrTypeValue exceptionValue;
    SZrTypeValue *messageArgument = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_CheckArity(context, 0, 1) ||
        !system_exception_get_self_object(context, &selfObject)) {
        return ZR_FALSE;
    }

    selfValue = ZrLib_CallContext_Self(context);
    messageArgument = ZrLib_CallContext_Argument(context, 0);

    /* 与核心异常规范化共享字段形状；stacks 由后续抛错路径填充。 */
    stacksArray = ZrLib_Array_New(context->state);
    if (stacksArray == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, &stacksValue, stacksArray, ZR_VALUE_TYPE_ARRAY);
    ZrLib_Object_SetFieldCString(context->state, selfObject, "stacks", &stacksValue);
    system_exception_set_message_field(context->state, selfObject, messageArgument, "message");

    if (selfValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&exceptionValue);
        ZrCore_Value_Copy(context->state, &exceptionValue, selfValue);
        ZrLib_Object_SetFieldCString(context->state, selfObject, "exception", &exceptionValue);
    }

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}
