#include "zr_vm_lib_network/udp_registry.h"

#include <stdlib.h>
#include <string.h>

#include "network/network_internal.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const TZrChar *kUdpModuleName = "zr.network.udp";

static ZrNetworkVmHandle *zr_network_udp_handle(const ZrLibCallContext *context) {
    SZrObject *self = zr_network_self_object(context);
    ZrNetworkVmHandle *handle = zr_network_get_handle(context->state, self, ZR_NETWORK_VM_HANDLE_KIND_UDP_SOCKET);

    if (handle == ZR_NULL) {
        zr_network_raise_runtime_error(context->state, "invalid UdpSocket handle");
    }
    return handle;
}

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

    nativePayload = ZrCore_String_GetNativeString(payload);
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

static const ZrLibFunctionDescriptor g_udp_functions[] = {
        {"bind", 2, 2, zr_network_udp_bind, "UdpSocket", "Bind a UDP socket.", ZR_NULL, 0},
};

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
        ZR_LIB_METHOD_DESCRIPTOR_INIT("port", 0, 0, zr_network_udp_port, "int",
                                      "Return the bound socket port.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibFieldDescriptor g_udp_packet_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("payload", "string", "Packet payload as a string."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("host", "string", "Remote sender host."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("port", "int", "Remote sender port."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("length", "int", "Payload length in bytes."),
};

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

static const ZrLibTypeHintDescriptor g_udp_hints[] = {
        {"bind", "function", "bind(host: string, port: int): UdpSocket", "Bind a UDP socket."},
        {"UdpSocket", "type", "class UdpSocket", "UDP socket handle."},
        {"UdpPacket", "type", "class UdpPacket", "UDP datagram snapshot."},
};

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
