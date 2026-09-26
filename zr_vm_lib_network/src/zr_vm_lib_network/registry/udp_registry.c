#include "zr_vm_lib_network/udp_registry.h"

#include <stdlib.h>
#include <string.h>

#include "network/network_internal.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/* packet/socket 实例化要使用与叶子模块描述符相同的注册名。 */
static const TZrChar *kUdpModuleName = "zr.network.udp";

/* UDP 方法只接受本模块的 socket 句柄；类型或接收者错误转为脚本异常。 */
static ZrNetworkVmHandle *zr_network_udp_handle(const ZrLibCallContext *context) {
    SZrObject *self = zr_network_self_object(context);
    ZrNetworkVmHandle *handle = zr_network_get_handle(context->state, self, ZR_NETWORK_VM_HANDLE_KIND_UDP_SOCKET);

    if (handle == ZR_NULL) {
        zr_network_raise_runtime_error(context->state, "invalid UdpSocket handle");
    }
    return handle;
}

/* 将 bind 得到的 socket 交给脚本对象；对象或句柄分配失败时关闭未交出的 socket。 */
static TZrBool zr_network_udp_finish_socket(SZrState *state, SZrTypeValue *result, const SZrNetworkUdpSocket *socket) {
    ZrNetworkVmHandle *handle = zr_network_alloc_handle(ZR_NETWORK_VM_HANDLE_KIND_UDP_SOCKET);
    SZrObject *object = zr_network_new_typed_object(state, kUdpModuleName, "UdpSocket");

    if (handle == ZR_NULL || object == ZR_NULL) {
        if (handle != ZR_NULL) {
            free(handle);
        }
        if (socket != ZR_NULL) {
            SZrNetworkUdpSocket copy = *socket;
            ZrNetwork_UdpSocketClose(&copy);
        }
        return zr_network_raise_runtime_error(state, "failed to allocate UdpSocket object");
    }

    handle->value.udpSocket = *socket;
    zr_network_store_handle(state, object, handle);
    return zr_network_finish_object(state, result, object);
}

/* 将接收缓存复制到带来源端点和原始字节数的脚本 packet；缓存仍归调用者释放。 */
static TZrBool zr_network_udp_finish_packet(SZrState *state,
                                            SZrTypeValue *result,
                                            const SZrNetworkEndpoint *remoteEndpoint,
                                            const TZrByte *buffer,
                                            TZrSize readLength) {
    SZrObject *packet;
    SZrTypeValue fieldValue;

    packet = zr_network_new_typed_object(state, kUdpModuleName, "UdpPacket");
    if (packet == ZR_NULL) {
        return zr_network_raise_runtime_error(state, "failed to allocate UdpPacket object");
    }

    /* BUG: datagram 可含 NUL；SetString 按 C 字符串长度复制，payload 被截短，
     * 而 length 仍为 readLength。证据：UdpSocketReceive/recvfrom 返回真实字节数，
     * native_binding_create_string 用原生字符串长度；修复应显式传入 readLength。 */
    ZrLib_Value_SetString(state, &fieldValue, (const TZrChar *) buffer);
    ZrLib_Object_SetFieldCString(state, packet, "payload", &fieldValue);
    ZrLib_Value_SetInt(state, &fieldValue, (TZrInt64) readLength);
    ZrLib_Object_SetFieldCString(state, packet, "length", &fieldValue);
    ZrLib_Value_SetString(state,
                          &fieldValue,
                          remoteEndpoint != ZR_NULL && remoteEndpoint->host[0] != '\0'
                                  ? remoteEndpoint->host
                                  : "127.0.0.1");
    ZrLib_Object_SetFieldCString(state, packet, "host", &fieldValue);
    ZrLib_Value_SetInt(state, &fieldValue, remoteEndpoint != ZR_NULL ? remoteEndpoint->port : 0);
    ZrLib_Object_SetFieldCString(state, packet, "port", &fieldValue);
    return zr_network_finish_object(state, result, packet);
}

/* 脚本 bind 校验端点后创建 socket，再由 finish_socket 交接资源。 */
static TZrBool zr_network_udp_bind(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrNetworkEndpoint endpoint;
    SZrNetworkUdpSocket socket;
    TZrChar error[256];

    if (!zr_network_read_endpoint_args(context, 0, 1, &endpoint)) {
        return ZR_FALSE;
    }

    memset(&socket, 0, sizeof(socket));
    if (!ZrNetwork_UdpSocketBind(&endpoint, &socket, error, sizeof(error))) {
        return zr_network_raise_runtime_error(context->state, error);
    }

    return zr_network_udp_finish_socket(context->state, result, &socket);
}

static TZrBool zr_network_udp_close(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_udp_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: bind 成功后 close 只关闭 socket；calloc 包装体保存在不参与 GC 的
     * native pointer 字段中，close 或丢弃对象都不释放它。证据：module_support.c
     * 的 alloc/store_handle、value.c 的 InitAsNativePointer；须统一最终化路径。 */
    ZrNetwork_UdpSocketClose(&handle->value.udpSocket);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_network_udp_is_closed(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_udp_handle(context);

    ZrLib_Value_SetBool(context->state, result, handle == ZR_NULL || !handle->value.udpSocket.isOpen ? ZR_TRUE : ZR_FALSE);
    return ZR_TRUE;
}

static TZrBool zr_network_udp_host(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_udp_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(context->state,
                          result,
                          handle->value.udpSocket.endpoint.host[0] != '\0' ? handle->value.udpSocket.endpoint.host
                                                                            : "127.0.0.1");
    return ZR_TRUE;
}

static TZrBool zr_network_udp_port(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_udp_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, result, handle->value.udpSocket.endpoint.port);
    return ZR_TRUE;
}

static TZrBool zr_network_udp_send(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_udp_handle(context);
    SZrNetworkEndpoint target;
    SZrString *payload = ZR_NULL;
    const TZrChar *nativePayload;
    TZrSize written = 0;
    TZrChar error[256];

    if (handle == ZR_NULL || !zr_network_read_endpoint_args(context, 0, 1, &target) ||
        !ZrLib_CallContext_ReadString(context, 2, &payload)) {
        return ZR_FALSE;
    }

    /* BUG: 脚本字符串允许嵌入 NUL，此处 strlen 使 datagram 仅发送前缀；
     * 应从 ZrCore_String_GetByteLength(payload) 获取字节数。 */
    nativePayload = ZrCore_String_GetNativeString(payload);
    /* BUG: 对已 close 的 socket 调用 send 时，UdpSocketSend 在 isOpen 检查处
     * 直接返回 false，不写 error；此处把未初始化栈数组当 C 字符串抛出，
     * 可读取越界或泄露栈内容。证据：network.c 的 UdpSocketSend 前置检查。
     * 修复需让 transport 所有失败路径写入有界错误，或在此先初始化错误缓冲。 */
    if (!ZrNetwork_UdpSocketSend(&handle->value.udpSocket,
                                 &target,
                                 (const TZrByte *)nativePayload,
                                 nativePayload != ZR_NULL ? strlen(nativePayload) : 0,
                                 &written,
                                 error,
                                 sizeof(error))) {
        return zr_network_raise_runtime_error(context->state, error);
    }

    ZrLib_Value_SetInt(context->state, result, (TZrInt64)written);
    return ZR_TRUE;
}

/* 接收缓存在 packet 构造完成后释放；transport false 映射为 null。 */
static TZrBool zr_network_udp_receive(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_udp_handle(context);
    TZrUInt32 timeoutMs = ZR_NETWORK_WAIT_INFINITE;
    TZrSize maxBytes = 0;
    TZrSize readLength = 0;
    SZrNetworkEndpoint remoteEndpoint;
    TZrByte *buffer;

    if (handle == ZR_NULL || !zr_network_read_byte_count_arg(context, 0, &maxBytes) ||
        !zr_network_read_timeout_arg(context, 1, timeoutMs, &timeoutMs)) {
        return ZR_FALSE;
    }

    buffer = (TZrByte *)malloc(maxBytes + 1);
    if (buffer == ZR_NULL) {
        return zr_network_raise_runtime_error(context->state, "failed to allocate UDP receive buffer");
    }

    memset(&remoteEndpoint, 0, sizeof(remoteEndpoint));
    /* BUG: 空 datagram 可由同一模块的 send("") 发出，recvfrom 成功返回 0 时
     * network.c 的 UdpSocketReceive 将 received <= 0 判为失败，此处把它作为
     * 超时返回 null；应区分有效零字节包、超时与 socket 错误。 */
    if (!ZrNetwork_UdpSocketReceive(&handle->value.udpSocket,
                                    timeoutMs,
                                    buffer,
                                    maxBytes,
                                    &readLength,
                                    &remoteEndpoint)) {
        free(buffer);
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    buffer[readLength] = '\0';
    if (!zr_network_udp_finish_packet(context->state, result, &remoteEndpoint, buffer, readLength)) {
        free(buffer);
        return ZR_FALSE;
    }

    free(buffer);
    return ZR_TRUE;
}

/* 绑定函数的脚本名和参数数由原生注册表统一分派。 */
static const ZrLibFunctionDescriptor g_udp_functions[] = {
        {"bind", 2, 2, zr_network_udp_bind, "UdpSocket", "Bind a UDP socket.", ZR_NULL, 0},
};

/* BUG: receive 回调允许缺省 timeoutMs，但描述符的 2..2 参数区间使
 * receive(maxBytes) 在 native dispatch 参数检查处被拒绝。证据：
 * zr_network_read_timeout_arg 与 native_binding_dispatch.c 的 arity check。 */
static const ZrLibMethodDescriptor g_udp_socket_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("send", 3, 3, zr_network_udp_send, "int",
                                      "Send a UDP datagram to host/port.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("receive", 2, 2, zr_network_udp_receive, "UdpPacket",
                                      "Receive a UDP datagram or return null on timeout.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, zr_network_udp_close, "null",
                                      "Close the UDP socket.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isClosed", 0, 0, zr_network_udp_is_closed, "bool",
                                      "Return whether the UDP socket has been closed.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("host", 0, 0, zr_network_udp_host, "string",
                                      "Return the bound socket host.", ZR_FALSE, ZR_NULL, 0),
        {
                .name = "port",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = zr_network_udp_port,
                .returnTypeName = "int",
                .documentation = "Return the bound socket port.",
                .isStatic = ZR_FALSE,
                .dispatchFlags =
                        ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
        },
};

/* BUG: g_udp_types 声明 UdpPacket 为 immutable snapshot，但字段宏初始化
 * isReadonly=false，运行时 metadata 映射成 isWritable=true；元数据允许写入，
 * 与描述不符。证据：native_binding.h 的 FIELD_DESCRIPTOR_INIT 与
 * native_binding_metadata.c 的 fieldDescriptor->isReadonly 分支；脚本赋值路径待测。 */
static const ZrLibFieldDescriptor g_udp_packet_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("payload", "string", "Packet payload as a string."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("host", "string", "Remote sender host."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("port", "int", "Remote sender port."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("length", "int", "Payload length in bytes."),
};

/* socket 方法与 packet 字段按类型分组，供叶子模块实例化及元数据使用。 */
static const ZrLibTypeDescriptor g_udp_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("UdpSocket", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                    g_udp_socket_methods, ZR_ARRAY_COUNT(g_udp_socket_methods),
                                    ZR_NULL, 0, "UDP socket handle.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("UdpPacket", ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                    g_udp_packet_fields, ZR_ARRAY_COUNT(g_udp_packet_fields),
                                    ZR_NULL, 0, ZR_NULL, 0, "Immutable UDP datagram snapshot.",
                                    ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
};

/* 类型提示名称对应绑定函数、socket 类型和 packet 类型。 */
static const ZrLibTypeHintDescriptor g_udp_hints[] = {
        {"bind", "function", "bind(host: string, port: int): UdpSocket", "Bind a UDP socket."},
        {"UdpSocket", "type", "class UdpSocket", "UDP socket handle."},
        {"UdpPacket", "type", "class UdpPacket", "UDP datagram snapshot."},
};

/* 叶子模块的提示 schema 由模块描述符暴露给导入侧。 */
static const TZrChar g_udp_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.network.udp\"\n"
        "}\n";

const ZrLibModuleDescriptor *ZrNetwork_UdpRegistry_GetModule(void) {
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.network.udp",
            ZR_NULL,
            0,
            g_udp_functions,
            ZR_ARRAY_COUNT(g_udp_functions),
            g_udp_types,
            ZR_ARRAY_COUNT(g_udp_types),
            g_udp_hints,
            ZR_ARRAY_COUNT(g_udp_hints),
            g_udp_hints_json,
            "UDP datagram primitives.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
