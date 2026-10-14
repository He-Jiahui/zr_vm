//
// zr.system.exception callbacks.
//

#include "zr_vm_lib_system/exception.h"

#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"

/**
 * ## Summary
 * 从 native callback 的 receiver 读取异常对象，集中拒绝无效 context、非对象 self 与无法转换的对象指针。
 *
 * ## Parameters
 * @param[in] context 当前 callback 的借用调用上下文；其 state 与 self 必须在 callback 期间有效。
 * @param[out] outObject 前置检查失败时保持原值；有效 self 转换后写入结果（可能为 NULL），仅返回真时可使用。
 *
 * ## Returns
 * @return 仅在 self 是可用对象且对象转换成功时返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 返回对象不转移所有权；它只在当前 native 调用的保活边界内使用。调用方应先初始化输出槽，并在失败时停止访问 receiver。
 *
 * ## References
 * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c:ZrSystem_Exception_Constructor` 是唯一调用者，并将该对象用于初始化异常实例字段。
 */
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

/**
 * ## Summary
 * 将可选消息值转换为脚本字符串并通过字段 setter 写入异常对象的指定字段；此处用于 `message`。
 *
 * ## Parameters
 * @param[in] state 当前 VM state；仅借用到本次同步转换和字段写入结束。
 * @param[in] object 接收字段的异常对象；不取得所有权。
 * @param[in] value 可选的 VM 值指针；空指针表示调用者没有提供该值，非空值在调用期间借用。
 * @param[in] fieldName 以 NUL 结尾的字段名；调用期间保持有效，当前调用者传入静态字符串。
 *
 * ## Returns
 * 本函数没有返回值；转换结果写入字段，缺值或转换返回空指针时写 VM null。底层字段 setter 也是 void，写入失败不向这里报告。
 *
 * ## Constraints and lifecycle
 * `stableValue` 是浅拷贝，不创建独立 GC root；native dispatch 在 callback 期间固定其 receiver 与参数。非空消息会经
 * `ZrCore_Value_ConvertToString` 转换，可能执行脚本转换协议并设置 VM 异常状态；本 helper 不清除该状态。构造器只在同步
 * callback 内立即使用转换结果，不将临时值或字段名保存到 callback 之外。
 *
 * ## References
 * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c:ZrSystem_Exception_Constructor` 是唯一调用者；
 * `zr_vm_core/src/zr_vm_core/value/value.c:ZrCore_Value_ConvertToString` 承担 VM 值到字符串的转换。
 */
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

    /* TODO: handler 槽由 GC 标记/转发，session 仅保存/恢复可恢复值；当前仓内实际读取/调用只见
     * tests/harness/runtime_support.c:213-247；zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:367-380 与
     * zr_vm_cli/src/zr_vm_cli/repl/repl_session.c:38-60 只规范化、记录并重置异常。需沿生产宿主未处理异常
     * 出口确认本 API 是否仅供嵌入宿主，或 CLI/REPL 也应派发；注册成功本身不代表 handler 会被执行。 */
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
    /* BUG: 反射构造在进入 callback 前 pin 新实例。若 ignored-root count 原为 capacity-1，实例 pin
     * 占满表；ZrLib_Array_New 不 pin 新数组。此处 setter 对 stacks 值扩容时，单次 RawMallocWithType(ARRAY)
     * 软失败会被 void setter 静默吞掉；callback 仍返回 true/null，而 reflection_construction 只看调用状态并
     * 交付 receiver，故合法构造可成功返回缺少 stacks 的异常对象。限定为该 pin 扩容失败且其他分配成功的路径，未做故障注入。 */
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
