#include "zr_vm_lib_network/tcp_registry.h"

#include <stdlib.h>
#include <string.h>

#include "network/network_internal.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/* 对象创建时用模块名探测叶模块是否已加载或导出类型；名称须与描述符一致。 */
static const TZrChar *kTcpModuleName = "zr.network.tcp";

/* 将脚本接收者限制为本模块的监听句柄；无效接收者转为运行时异常。 */
static ZrNetworkVmHandle *zr_network_tcp_listener_handle(const ZrLibCallContext *context) {
    SZrObject *self = zr_network_self_object(context);
    ZrNetworkVmHandle *handle = zr_network_get_handle(context->state, self, ZR_NETWORK_VM_HANDLE_KIND_TCP_LISTENER);

    if (handle == ZR_NULL) {
        zr_network_raise_runtime_error(context->state, "invalid TcpListener handle");
    }
    return handle;
}

/* 流方法共用句柄类型检查，避免把其他网络对象解释成 TCP 流。 */
static ZrNetworkVmHandle *zr_network_tcp_stream_handle(const ZrLibCallContext *context) {
    SZrObject *self = zr_network_self_object(context);
    ZrNetworkVmHandle *handle = zr_network_get_handle(context->state, self, ZR_NETWORK_VM_HANDLE_KIND_TCP_STREAM);

    if (handle == ZR_NULL) {
        zr_network_raise_runtime_error(context->state, "invalid TcpStream handle");
    }
    return handle;
}

/* 把已打开的监听 socket 装入脚本对象；对象或句柄分配失败时关闭原 socket。 */
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

/* 连接和 accept 共用流对象交接；对象或句柄分配失败时关闭未交出的 socket。 */
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

/* 脚本 listen 先校验 host/port，成功后交由包装体保存实际绑定端点。 */
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

/* 未传 timeoutMs 时使用 5000 毫秒；连接失败由 transport 错误文本转为脚本异常。 */
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
    /* TODO: transport 的 network_set_nonblocking 失败分支不写 errorBuffer；若该分支
     * 可在受支持平台触发，此处将读取未初始化的 error。需故障注入非阻塞切换失败核查。 */
    if (!ZrNetwork_TcpStreamConnect(&endpoint, timeoutMs, &stream, error, sizeof(error))) {
        return zr_network_raise_runtime_error(context->state, error);
    }

    return zr_network_tcp_finish_stream(context->state, result, &stream);
}

/* transport 的等待超时、监听关闭和 accept 错误均返回 false，此处统一映射为 null。 */
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

    /* BUG: listen 成功后 close 只关闭 socket，calloc 分配的 ZrNetworkVmHandle 未释放；
     * native pointer 字段不参与 GC，丢弃对象也无法回收包装体。证据：
     * module_support.c 的 alloc/store_handle、value.c 的 InitAsNativePointer。修复须统一
     * close 与对象最终化的所有权路径，避免释放后仍可从脚本对象取到悬空指针。 */
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

/* read 的 transport false 同时覆盖超时、EOF 和 socket 错误，脚本侧均收到 null。 */
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
    /* BUG: TCP 可读出含 NUL 的字节，但 SetString 走 C 字符串长度，首个 NUL 后的
     * 已读字节会从返回值消失；应按 readLength 构造有显式长度的脚本字符串。证据：
     * StreamRead 按 recv 长度返回，native_binding_create_string 使用原生字符串长度。 */
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

    /* BUG: 脚本字符串可保存嵌入 NUL，但 strlen 使 write 只发送其前缀；应使用
     * ZrCore_String_GetByteLength(text) 指定真实字节数。 */
    nativeText = ZrCore_String_GetNativeString(text);
    /* BUG: transport 分批 send 成功后若后续 send 失败，outWritten 仍为 0；本回调
     * 返回 0 掩盖了已发送的前缀，调用方重试可能重复发送。证据：network.c 的
     * StreamWrite 仅在全部成功后写出 total。 */
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

    /* BUG: 已创建的 TcpStream 在 close 后仍保留 calloc 包装体；GC 不管理 native
     * pointer 字段。证据：finish_stream -> store_handle -> InitAsNativePointer，
     * 本路径仅调用 StreamClose；须配套 close 与最终化释放。 */
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

/* 函数表同时规定脚本导出名、参数区间和 native 回调入口。 */
static const ZrLibFunctionDescriptor g_tcp_functions[] = {
        {"listen", 2, 2, zr_network_tcp_listen, "TcpListener", "Bind and listen on a TCP endpoint.", ZR_NULL, 0},
        {"connect", 2, 3, zr_network_tcp_connect, "TcpStream", "Connect to a TCP endpoint.", ZR_NULL, 0},
};

/* BUG: accept 回调和 API 文档支持省略 timeoutMs，但 minArgumentCount=1 令
 * accept() 在 native dispatch 的参数检查处被拒绝，默认无限等待分支不可达。
 * 证据：zr_network_read_timeout_arg、native_binding_dispatch.c 的 arity check。 */
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

/* BUG: read 回调支持缺省 timeoutMs，表中的 2..2 参数区间却拒绝 read(maxBytes)；
 * 默认无限等待分支不可达。证据：zr_network_read_timeout_arg 与 dispatch arity check。 */
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

/* 类型表把监听和流的方法表交给 zr.network.tcp 的脚本类型原型。 */
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

/* 类型提示供导入后的元数据和编辑器展示，名称需对应上面的导出表。 */
static const ZrLibTypeHintDescriptor g_tcp_hints[] = {
        {"listen", "function", "listen(host: string, port: int): TcpListener", "Bind and listen on a TCP endpoint."},
        {"connect", "function", "connect(host: string, port: int, timeoutMs?: int): TcpStream", "Connect to a TCP endpoint."},
        {"TcpListener", "type", "class TcpListener", "TCP listener handle."},
        {"TcpStream", "type", "class TcpStream", "TCP stream handle."},
};

/* 叶子模块的提示文档与静态表共同交给模块描述符。 */
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
