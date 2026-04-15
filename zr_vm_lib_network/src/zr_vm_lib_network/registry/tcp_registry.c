#include "zr_vm_lib_network/tcp_registry.h"

#include <stdlib.h>
#include <string.h>

#include "network/network_internal.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const TZrChar *kTcpModuleName = "zr.network.tcp";

static ZrNetworkVmHandle *zr_network_tcp_listener_handle(const ZrLibCallContext *context) {
    SZrObject *self = zr_network_self_object(context);
    ZrNetworkVmHandle *handle = zr_network_get_handle(context->state, self, ZR_NETWORK_VM_HANDLE_KIND_TCP_LISTENER);

    if (handle == ZR_NULL) {
        zr_network_raise_runtime_error(context->state, "invalid TcpListener handle");
    }
    return handle;
}

static ZrNetworkVmHandle *zr_network_tcp_stream_handle(const ZrLibCallContext *context) {
    SZrObject *self = zr_network_self_object(context);
    ZrNetworkVmHandle *handle = zr_network_get_handle(context->state, self, ZR_NETWORK_VM_HANDLE_KIND_TCP_STREAM);

    if (handle == ZR_NULL) {
        zr_network_raise_runtime_error(context->state, "invalid TcpStream handle");
    }
    return handle;
}

static TZrBool zr_network_tcp_finish_listener(SZrState *state,
                                              SZrTypeValue *result,
                                              const SZrNetworkListener *listener) {
    ZrNetworkVmHandle *handle;
    SZrObject *object;

    handle = zr_network_alloc_handle(ZR_NETWORK_VM_HANDLE_KIND_TCP_LISTENER);
    object = zr_network_new_typed_object(state, kTcpModuleName, "TcpListener");
    if (handle == ZR_NULL || object == ZR_NULL) {
        if (handle != ZR_NULL) {
            free(handle);
        }
        if (listener != ZR_NULL) {
            SZrNetworkListener copy = *listener;
            ZrNetwork_ListenerClose(&copy);
        }
        return zr_network_raise_runtime_error(state, "failed to allocate TcpListener object");
    }

    handle->value.listener = *listener;
    zr_network_store_handle(state, object, handle);
    return zr_network_finish_object(state, result, object);
}

static TZrBool zr_network_tcp_finish_stream(SZrState *state,
                                            SZrTypeValue *result,
                                            const SZrNetworkStream *stream) {
    ZrNetworkVmHandle *handle;
    SZrObject *object;

    handle = zr_network_alloc_handle(ZR_NETWORK_VM_HANDLE_KIND_TCP_STREAM);
    object = zr_network_new_typed_object(state, kTcpModuleName, "TcpStream");
    if (handle == ZR_NULL || object == ZR_NULL) {
        if (handle != ZR_NULL) {
            free(handle);
        }
        if (stream != ZR_NULL) {
            SZrNetworkStream copy = *stream;
            ZrNetwork_StreamClose(&copy);
        }
        return zr_network_raise_runtime_error(state, "failed to allocate TcpStream object");
    }

    handle->value.stream = *stream;
    zr_network_store_handle(state, object, handle);
    return zr_network_finish_object(state, result, object);
}

static TZrBool zr_network_tcp_listen(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrNetworkEndpoint endpoint;
    SZrNetworkListener listener;
    TZrChar error[256];

    if (!zr_network_read_endpoint_args(context, 0, 1, &endpoint)) {
        return ZR_FALSE;
    }

    memset(&listener, 0, sizeof(listener));
    if (!ZrNetwork_TcpListenerOpen(&endpoint, &listener, error, sizeof(error))) {
        return zr_network_raise_runtime_error(context->state, error);
    }

    return zr_network_tcp_finish_listener(context->state, result, &listener);
}

static TZrBool zr_network_tcp_connect(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrNetworkEndpoint endpoint;
    SZrNetworkStream stream;
    TZrUInt32 timeoutMs = 5000u;
    TZrChar error[256];

    if (!zr_network_read_endpoint_args(context, 0, 1, &endpoint) ||
        !zr_network_read_timeout_arg(context, 2, timeoutMs, &timeoutMs)) {
        return ZR_FALSE;
    }

    memset(&stream, 0, sizeof(stream));
    if (!ZrNetwork_TcpStreamConnect(&endpoint, timeoutMs, &stream, error, sizeof(error))) {
        return zr_network_raise_runtime_error(context->state, error);
    }

    return zr_network_tcp_finish_stream(context->state, result, &stream);
}

static TZrBool zr_network_tcp_listener_accept(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_listener_handle(context);
    TZrUInt32 timeoutMs = ZR_NETWORK_WAIT_INFINITE;
    SZrNetworkStream stream;

    if (handle == ZR_NULL || !zr_network_read_timeout_arg(context, 0, timeoutMs, &timeoutMs)) {
        return ZR_FALSE;
    }

    memset(&stream, 0, sizeof(stream));
    if (!ZrNetwork_ListenerAccept(&handle->value.listener, timeoutMs, &stream)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    return zr_network_tcp_finish_stream(context->state, result, &stream);
}

static TZrBool zr_network_tcp_listener_close(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_listener_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrNetwork_ListenerClose(&handle->value.listener);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_listener_is_closed(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_listener_handle(context);
    TZrBool isClosed = ZR_TRUE;

    if (handle != ZR_NULL) {
        isClosed = handle->value.listener.isOpen ? ZR_FALSE : ZR_TRUE;
    }

    ZrLib_Value_SetBool(context->state, result, isClosed);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_listener_host(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_listener_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(context->state,
                          result,
                          handle->value.listener.endpoint.host[0] != '\0'
                                  ? handle->value.listener.endpoint.host
                                  : "127.0.0.1");
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_listener_port(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_listener_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, result, handle->value.listener.endpoint.port);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_read(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);
    TZrUInt32 timeoutMs = ZR_NETWORK_WAIT_INFINITE;
    TZrSize maxBytes = 0;
    TZrSize readLength = 0;
    TZrByte *buffer;

    if (handle == ZR_NULL || !zr_network_read_byte_count_arg(context, 0, &maxBytes) ||
        !zr_network_read_timeout_arg(context, 1, timeoutMs, &timeoutMs)) {
        return ZR_FALSE;
    }

    buffer = (TZrByte *) malloc(maxBytes + 1);
    if (buffer == ZR_NULL) {
        return zr_network_raise_runtime_error(context->state, "failed to allocate TCP read buffer");
    }

    if (!ZrNetwork_StreamRead(&handle->value.stream, timeoutMs, buffer, maxBytes, &readLength)) {
        free(buffer);
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    buffer[readLength] = '\0';
    ZrLib_Value_SetString(context->state, result, (const TZrChar *) buffer);
    free(buffer);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_write(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);
    SZrString *text = ZR_NULL;
    TZrSize written = 0;
    const TZrChar *nativeText;

    if (handle == ZR_NULL || !ZrLib_CallContext_ReadString(context, 0, &text)) {
        return ZR_FALSE;
    }

    nativeText = ZrCore_String_GetNativeString(text);
    if (!ZrNetwork_StreamWrite(&handle->value.stream,
                               (const TZrByte *) nativeText,
                               nativeText != ZR_NULL ? strlen(nativeText) : 0,
                               &written)) {
        written = 0;
    }

    ZrLib_Value_SetInt(context->state, result, (TZrInt64) written);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_close(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrNetwork_StreamClose(&handle->value.stream);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_is_closed(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);
    TZrBool isClosed = ZR_TRUE;

    if (handle != ZR_NULL) {
        isClosed = handle->value.stream.isOpen ? ZR_FALSE : ZR_TRUE;
    }

    ZrLib_Value_SetBool(context->state, result, isClosed);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_local_host(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(context->state,
                          result,
                          handle->value.stream.localEndpoint.host[0] != '\0'
                                  ? handle->value.stream.localEndpoint.host
                                  : "127.0.0.1");
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_local_port(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, result, handle->value.stream.localEndpoint.port);
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_remote_host(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(context->state,
                          result,
                          handle->value.stream.remoteEndpoint.host[0] != '\0'
                                  ? handle->value.stream.remoteEndpoint.host
                                  : "127.0.0.1");
    return ZR_TRUE;
}

static TZrBool zr_network_tcp_stream_remote_port(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrNetworkVmHandle *handle = zr_network_tcp_stream_handle(context);

    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, result, handle->value.stream.remoteEndpoint.port);
    return ZR_TRUE;
}

static const ZrLibFunctionDescriptor g_tcp_functions[] = {
        {"listen", 2, 2, zr_network_tcp_listen, "TcpListener", "Bind and listen on a TCP endpoint.", ZR_NULL, 0},
        {"connect", 2, 3, zr_network_tcp_connect, "TcpStream", "Connect to a TCP endpoint.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_tcp_listener_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("accept", 1, 1, zr_network_tcp_listener_accept, "TcpStream",
                                      "Accept a client connection or return null on timeout.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, zr_network_tcp_listener_close, "null",
                                      "Close the TCP listener.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isClosed", 0, 0, zr_network_tcp_listener_is_closed, "bool",
                                      "Return whether the listener has been closed.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("host", 0, 0, zr_network_tcp_listener_host, "string",
                                      "Return the bound listener host.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("port", 0, 0, zr_network_tcp_listener_port, "int",
                                      "Return the bound listener port.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_tcp_stream_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("read", 2, 2, zr_network_tcp_stream_read, "string",
                                      "Read bytes from the stream or return null on timeout/EOF.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("write", 1, 1, zr_network_tcp_stream_write, "int",
                                      "Write a UTF-8 string to the stream.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, zr_network_tcp_stream_close, "null",
                                      "Close the TCP stream.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isClosed", 0, 0, zr_network_tcp_stream_is_closed, "bool",
                                      "Return whether the stream has been closed.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("localHost", 0, 0, zr_network_tcp_stream_local_host, "string",
                                      "Return the local endpoint host.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("localPort", 0, 0, zr_network_tcp_stream_local_port, "int",
                                      "Return the local endpoint port.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remoteHost", 0, 0, zr_network_tcp_stream_remote_host, "string",
                                      "Return the remote endpoint host.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remotePort", 0, 0, zr_network_tcp_stream_remote_port, "int",
                                      "Return the remote endpoint port.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibTypeDescriptor g_tcp_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("TcpListener", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                    g_tcp_listener_methods, ZR_ARRAY_COUNT(g_tcp_listener_methods),
                                    ZR_NULL, 0, "TCP listener handle.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("TcpStream", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                    g_tcp_stream_methods, ZR_ARRAY_COUNT(g_tcp_stream_methods),
                                    ZR_NULL, 0, "TCP stream handle.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
};

static const ZrLibTypeHintDescriptor g_tcp_hints[] = {
        {"listen", "function", "listen(host: string, port: int): TcpListener", "Bind and listen on a TCP endpoint."},
        {"connect", "function", "connect(host: string, port: int, timeoutMs?: int): TcpStream", "Connect to a TCP endpoint."},
        {"TcpListener", "type", "class TcpListener", "TCP listener handle."},
        {"TcpStream", "type", "class TcpStream", "TCP stream handle."},
};

static const TZrChar g_tcp_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.network.tcp\"\n"
        "}\n";

const ZrLibModuleDescriptor *ZrNetwork_TcpRegistry_GetModule(void) {
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.network.tcp",
            ZR_NULL,
            0,
            g_tcp_functions,
            ZR_ARRAY_COUNT(g_tcp_functions),
            g_tcp_types,
            ZR_ARRAY_COUNT(g_tcp_types),
            g_tcp_hints,
            ZR_ARRAY_COUNT(g_tcp_hints),
            g_tcp_hints_json,
            "TCP client and server primitives.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
