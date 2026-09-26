#include "network/network_internal.h"

#include <stdlib.h>
#include <string.h>

/* TCP/UDP VM 对象共用隐藏字段保存 native handle；类型标签阻止错用不同协议的句柄。 */
static const TZrChar *kNetworkHandleField = "__zr_network_handle";

/** @brief 从 native 方法的 self 提取 VM 对象，供 TCP/UDP 方法共享接收者校验。 */
SZrObject *zr_network_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);

    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT || selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

/** @brief 将底层传输失败写入脚本异常状态；调用者仍须以 false 停止当前 native 调用。 */
TZrBool zr_network_raise_runtime_error(SZrState *state, const TZrChar *message) {
    SZrTypeValue errorValue;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(state, &errorValue, message != ZR_NULL ? message : "Network runtime error");
    /* 原生回调以 false 报告失败前，把系统错误转成可被脚本 catch 的异常状态。 */
    if (!ZrCore_Exception_NormalizeThrownValue(state,
                                               &errorValue,
                                               state->callInfoList,
                                               ZR_THREAD_STATUS_EXCEPTION_ERROR) &&
        !ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
        ZrCore_Debug_RunError(state, (TZrNativeString) (message != ZR_NULL ? message : "Network runtime error"));
    }

    state->threadStatus = state->currentExceptionStatus != ZR_THREAD_STATUS_FINE
                                  ? state->currentExceptionStatus
                                  : ZR_THREAD_STATUS_EXCEPTION_ERROR;
    return ZR_FALSE;
}

/** @brief 为 TCP/UDP 返回值创建 VM 对象；模块未装载时保留普通对象回退供原生回调返回。 */
SZrObject *zr_network_new_typed_object(SZrState *state, const TZrChar *moduleName, const TZrChar *typeName) {
    SZrObject *object;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    /* 指定模块尚未 materialize 时创建普通对象；已加载时尝试按类型名取得原型。
     * TODO: 若同名全局类型与指定模块类型不一致，非限定名查找可能取错原型；
     * 需用冲突类型名和延迟导入测试核对具体分派路径。 */
    if (moduleName != ZR_NULL &&
        ZrLib_Module_GetLoaded(state, moduleName) == ZR_NULL &&
        ZrLib_Module_GetExport(state, moduleName, typeName) == ZR_NULL) {
        return ZrLib_Object_New(state);
    }

    object = ZrLib_Type_NewInstance(state, typeName);
    if (object == ZR_NULL) {
        object = ZrLib_Object_New(state);
    }
    return object;
}

/** @brief 将新对象交给 native 调用结果槽，供脚本侧接收 socket 或 UDP 数据包。 */
TZrBool zr_network_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

/* BUG: 成功返回的 calloc 句柄被作为非 GC native pointer 存入 VM 对象，
 * TCP/UDP close 只关闭 socket，且无终结器或成功路径 free；反复创建并关闭会持续泄漏句柄堆内存。
 * 证据：value.c 的 native pointer 不参与 GC；仅 finish_* 失败分支调用 free(handle)。 */
ZrNetworkVmHandle *zr_network_alloc_handle(EZrNetworkVmHandleKind kind) {
    ZrNetworkVmHandle *handle = (ZrNetworkVmHandle *) calloc(1, sizeof(*handle));

    if (handle != ZR_NULL) {
        handle->kind = kind;
    }
    return handle;
}

/** @brief 将协议句柄附到 VM 对象私有字段；返回值只覆盖入参校验，不保证字段写入成功。 */
TZrBool zr_network_store_handle(SZrState *state, SZrObject *object, ZrNetworkVmHandle *handle) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: Object_SetFieldCString 在对象 pin 或字段名分配失败时静默返回，本函数仍返回 true。
     * TCP/UDP finish_* 随后将对象写入结果槽，不回滚已打开的 socket 与 calloc 句柄；
     * 私有字段缺失使后续方法无法取回句柄。用 pin/分配故障注入验证该失败路径。 */
    ZrLib_Value_SetNativePointer(state, &value, handle);
    ZrLib_Object_SetFieldCString(state, object, kNetworkHandleField, &value);
    return ZR_TRUE;
}

/** @brief 在方法入口按协议种类取回私有句柄，避免将 listener、stream、UDP socket 混用。 */
ZrNetworkVmHandle *zr_network_get_handle(SZrState *state, SZrObject *object, EZrNetworkVmHandleKind expectedKind) {
    const SZrTypeValue *value;
    ZrNetworkVmHandle *handle;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    value = ZrLib_Object_GetFieldCString(state, object, kNetworkHandleField);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER || value->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }

    handle = (ZrNetworkVmHandle *) value->value.nativeObject.nativePointer;
    if (handle->kind != expectedKind) {
        return ZR_NULL;
    }

    return handle;
}

/** @brief 为 listen/connect/bind/send 统一读取端点；主机文本受公开结构容量约束。 */
TZrBool zr_network_read_endpoint_args(const ZrLibCallContext *context,
                                      TZrSize hostIndex,
                                      TZrSize portIndex,
                                      SZrNetworkEndpoint *outEndpoint) {
    SZrString *hostString = ZR_NULL;
    TZrInt64 portValue = 0;
    const TZrChar *hostText;
    TZrSize hostLength;

    if (outEndpoint == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outEndpoint, 0, sizeof(*outEndpoint));
    if (!ZrLib_CallContext_ReadString(context, hostIndex, &hostString)) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, portIndex, &portValue)) {
        return ZR_FALSE;
    }

    /* VM 入口在调用 getaddrinfo/bind 前先限制 host 缓冲区和端口范围。 */
    hostText = ZrCore_String_GetNativeString(hostString);
    hostLength = hostText != ZR_NULL ? strlen(hostText) : 0;
    if (hostLength >= sizeof(outEndpoint->host)) {
        return zr_network_raise_runtime_error(context->state, "network host is too long");
    }
    if (portValue < 0 || portValue > 65535) {
        return zr_network_raise_runtime_error(context->state, "network port must be between 0 and 65535");
    }

    if (hostText != ZR_NULL) {
        memcpy(outEndpoint->host, hostText, hostLength + 1);
    } else {
        outEndpoint->host[0] = '\0';
    }
    outEndpoint->port = (TZrUInt16) portValue;
    return ZR_TRUE;
}

/** @brief 读取等待时长；仅在调用描述符允许省略该参数时才保留调用者的默认值。 */
TZrBool zr_network_read_timeout_arg(const ZrLibCallContext *context,
                                    TZrSize index,
                                    TZrUInt32 defaultValue,
                                    TZrUInt32 *outTimeoutMs) {
    TZrInt64 value = 0;

    if (outTimeoutMs == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 省略 timeout 维持各调用点默认值；显式值只接受 uint32 区间。 */
    *outTimeoutMs = defaultValue;
    if (context == ZR_NULL || ZrLib_CallContext_ArgumentCount(context) <= index) {
        return ZR_TRUE;
    }

    if (!ZrLib_CallContext_ReadInt(context, index, &value)) {
        return ZR_FALSE;
    }
    if (value < 0 || value > 0xFFFFFFFFLL) {
        return zr_network_raise_runtime_error(context->state, "timeout must be between 0 and 4294967295");
    }

    *outTimeoutMs = (TZrUInt32) value;
    return ZR_TRUE;
}

/** @brief 将脚本请求的正字节数交给 read/receive 缓冲区分配路径。 */
TZrBool zr_network_read_byte_count_arg(const ZrLibCallContext *context,
                                       TZrSize index,
                                       TZrSize *outLength) {
    TZrInt64 value = 0;

    if (outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadInt(context, index, &value)) {
        return ZR_FALSE;
    }
    if (value <= 0) {
        return zr_network_raise_runtime_error(context->state, "byte count must be greater than 0");
    }

    /* TODO: 正数 int64 直接转 TZrSize；需在 32 位目标或大值测试中核对截断后
     * receive/read 的缓冲区申请与调用方期待的 maxBytes 是否一致。 */
    *outLength = (TZrSize) value;
    return ZR_TRUE;
}
