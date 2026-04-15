#if !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#include "zr_vm_lib_network/network.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET ZrNetworkSocket;
typedef int ZrNetworkSockLen;
#define ZR_NETWORK_INVALID_SOCKET INVALID_SOCKET
#define ZR_NETWORK_SHUT_RDWR SD_BOTH
#define ZR_NETWORK_SOCKET_WOULD_BLOCK(errorCode) ((errorCode) == WSAEWOULDBLOCK || (errorCode) == WSAEINPROGRESS)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int ZrNetworkSocket;
typedef socklen_t ZrNetworkSockLen;
#define ZR_NETWORK_INVALID_SOCKET (-1)
#define ZR_NETWORK_SHUT_RDWR SHUT_RDWR
#define ZR_NETWORK_SOCKET_WOULD_BLOCK(errorCode) ((errorCode) == EWOULDBLOCK || (errorCode) == EAGAIN || (errorCode) == EINPROGRESS)
#endif

#define ZR_NETWORK_FRAME_HEADER_SIZE 4U
typedef enum EZrNetworkIoResult {
    ZR_NETWORK_IO_RESULT_SUCCESS = 0,
    ZR_NETWORK_IO_RESULT_TIMEOUT = 1,
    ZR_NETWORK_IO_RESULT_CLOSED = 2,
    ZR_NETWORK_IO_RESULT_ERROR = 3
} EZrNetworkIoResult;

static void network_write_error(TZrChar *buffer, TZrSize bufferSize, const TZrChar *message) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    snprintf(buffer, bufferSize, "%s", message != ZR_NULL ? message : "");
    buffer[bufferSize - 1] = '\0';
}

static int network_last_error(void) {
#if defined(_WIN32)
    return (int)WSAGetLastError();
#else
    return errno;
#endif
}

static void network_write_socket_error(TZrChar *buffer,
                                       TZrSize bufferSize,
                                       const TZrChar *prefix,
                                       int errorCode) {
    TZrChar message[256];
#if defined(_WIN32)
    snprintf(message, sizeof(message), "%s: socket error %d", prefix != ZR_NULL ? prefix : "network error", errorCode);
#else
    snprintf(message, sizeof(message), "%s: %s", prefix != ZR_NULL ? prefix : "network error", strerror(errorCode));
#endif
    network_write_error(buffer, bufferSize, message);
}

static TZrBool network_initialize(TZrChar *errorBuffer, TZrSize errorBufferSize) {
#if defined(_WIN32)
    static TZrBool initialized = ZR_FALSE;
    if (!initialized) {
        WSADATA data;
        int status = WSAStartup(MAKEWORD(2, 2), &data);
        if (status != 0) {
            network_write_socket_error(errorBuffer, errorBufferSize, "failed to initialize Winsock", status);
            return ZR_FALSE;
        }
        initialized = ZR_TRUE;
    }
#else
    ZR_UNUSED_PARAMETER(errorBuffer);
    ZR_UNUSED_PARAMETER(errorBufferSize);
#endif
    return ZR_TRUE;
}

static TZrPtr network_store_socket(ZrNetworkSocket socketHandle) {
    return (TZrPtr)(uintptr_t)((uintptr_t)socketHandle + 1u);
}

static ZrNetworkSocket network_load_socket(TZrPtr handle) {
    uintptr_t raw = (uintptr_t)handle;
    return raw == 0 ? ZR_NETWORK_INVALID_SOCKET : (ZrNetworkSocket)(raw - 1u);
}

static void network_close_socket(ZrNetworkSocket socketHandle) {
    if (socketHandle == ZR_NETWORK_INVALID_SOCKET) {
        return;
    }
#if defined(_WIN32)
    closesocket(socketHandle);
#else
    close(socketHandle);
#endif
}

static int network_wait_socket(ZrNetworkSocket socketHandle, TZrUInt32 timeoutMs, TZrBool writeSet) {
    fd_set sockets;
    struct timeval timeout;
    struct timeval *timeoutPointer = ZR_NULL;
    int status;

    FD_ZERO(&sockets);
    FD_SET(socketHandle, &sockets);
    if (timeoutMs != ZR_NETWORK_WAIT_INFINITE) {
        timeout.tv_sec = (long)(timeoutMs / 1000U);
        timeout.tv_usec = (long)((timeoutMs % 1000U) * 1000U);
        timeoutPointer = &timeout;
    }

    do {
#if defined(_WIN32)
        status = select(0, writeSet ? ZR_NULL : &sockets, writeSet ? &sockets : ZR_NULL, ZR_NULL, timeoutPointer);
#else
        status = select(socketHandle + 1,
                        writeSet ? ZR_NULL : &sockets,
                        writeSet ? &sockets : ZR_NULL,
                        ZR_NULL,
                        timeoutPointer);
#endif
#if !defined(_WIN32)
    } while (status < 0 && errno == EINTR);
#else
    } while (ZR_FALSE);
#endif

    return status;
}

static TZrBool network_set_nonblocking(ZrNetworkSocket socketHandle, TZrBool enabled) {
#if defined(_WIN32)
    u_long mode = enabled ? 1UL : 0UL;
    return ioctlsocket(socketHandle, FIONBIO, &mode) == 0 ? ZR_TRUE : ZR_FALSE;
#else
    int flags = fcntl(socketHandle, F_GETFL, 0);
    if (flags < 0) {
        return ZR_FALSE;
    }
    flags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(socketHandle, F_SETFL, flags) == 0 ? ZR_TRUE : ZR_FALSE;
#endif
}

static TZrBool network_normalize_host(const TZrChar *host, TZrChar *buffer, TZrSize bufferSize) {
    const TZrChar *resolved = (host == ZR_NULL || host[0] == '\0' || strcmp(host, "localhost") == 0) ? "127.0.0.1" : host;
    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    if (snprintf(buffer, bufferSize, "%s", resolved) >= (int)bufferSize) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool network_endpoint_from_sockaddr(const struct sockaddr *address, SZrNetworkEndpoint *outEndpoint) {
    if (address == ZR_NULL || outEndpoint == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outEndpoint, 0, sizeof(*outEndpoint));
    if (address->sa_family == AF_INET) {
        const struct sockaddr_in *address4 = (const struct sockaddr_in *)address;
        if (inet_ntop(AF_INET, &address4->sin_addr, outEndpoint->host, sizeof(outEndpoint->host)) == ZR_NULL) {
            return ZR_FALSE;
        }
        outEndpoint->port = ntohs(address4->sin_port);
        return ZR_TRUE;
    }
    if (address->sa_family == AF_INET6) {
        const struct sockaddr_in6 *address6 = (const struct sockaddr_in6 *)address;
        if (inet_ntop(AF_INET6, &address6->sin6_addr, outEndpoint->host, sizeof(outEndpoint->host)) == ZR_NULL) {
            return ZR_FALSE;
        }
        outEndpoint->port = ntohs(address6->sin6_port);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool network_sockaddr_from_endpoint(const SZrNetworkEndpoint *endpoint,
                                              struct sockaddr_storage *storage,
                                              ZrNetworkSockLen *outLength,
                                              TZrChar *errorBuffer,
                                              TZrSize errorBufferSize) {
    TZrChar hostBuffer[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];

    if (endpoint == ZR_NULL || storage == ZR_NULL || outLength == ZR_NULL || !network_normalize_host(endpoint->host, hostBuffer, sizeof(hostBuffer))) {
        network_write_error(errorBuffer, errorBufferSize, "endpoint is invalid");
        return ZR_FALSE;
    }

    memset(storage, 0, sizeof(*storage));
    if (strchr(hostBuffer, ':') == ZR_NULL) {
        struct sockaddr_in *address4 = (struct sockaddr_in *)storage;
        address4->sin_family = AF_INET;
        address4->sin_port = htons(endpoint->port);
        if (inet_pton(AF_INET, hostBuffer, &address4->sin_addr) != 1) {
            network_write_error(errorBuffer, errorBufferSize, "endpoint host must be numeric IPv4/IPv6 or localhost");
            return ZR_FALSE;
        }
        *outLength = (ZrNetworkSockLen)sizeof(*address4);
        return ZR_TRUE;
    }

    {
        struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)storage;
        address6->sin6_family = AF_INET6;
        address6->sin6_port = htons(endpoint->port);
        if (inet_pton(AF_INET6, hostBuffer, &address6->sin6_addr) != 1) {
            network_write_error(errorBuffer, errorBufferSize, "endpoint host must be numeric IPv4/IPv6 or localhost");
            return ZR_FALSE;
        }
        *outLength = (ZrNetworkSockLen)sizeof(*address6);
    }
    return ZR_TRUE;
}

static void network_update_endpoints(ZrNetworkSocket socketHandle,
                                     SZrNetworkEndpoint *outLocalEndpoint,
                                     SZrNetworkEndpoint *outRemoteEndpoint) {
    struct sockaddr_storage storage;
    ZrNetworkSockLen storageLength = (ZrNetworkSockLen)sizeof(storage);

    if (outLocalEndpoint != ZR_NULL) {
        memset(&storage, 0, sizeof(storage));
        if (getsockname(socketHandle, (struct sockaddr *)&storage, &storageLength) == 0) {
            network_endpoint_from_sockaddr((const struct sockaddr *)&storage, outLocalEndpoint);
        }
    }
    if (outRemoteEndpoint != ZR_NULL) {
        storageLength = (ZrNetworkSockLen)sizeof(storage);
        memset(&storage, 0, sizeof(storage));
        if (getpeername(socketHandle, (struct sockaddr *)&storage, &storageLength) == 0) {
            network_endpoint_from_sockaddr((const struct sockaddr *)&storage, outRemoteEndpoint);
        }
    }
}

static EZrNetworkIoResult network_read_exact(ZrNetworkSocket socketHandle,
                                             TZrUInt32 timeoutMs,
                                             TZrByte *buffer,
                                             TZrSize length) {
    TZrSize total = 0;
    while (total < length) {
        int waitStatus = network_wait_socket(socketHandle, timeoutMs, ZR_FALSE);
        int received;
        if (waitStatus == 0) {
            return ZR_NETWORK_IO_RESULT_TIMEOUT;
        }
        if (waitStatus < 0) {
            return ZR_NETWORK_IO_RESULT_ERROR;
        }
        received = recv(socketHandle, (char *)(buffer + total), (int)(length - total), 0);
        if (received == 0) {
            return ZR_NETWORK_IO_RESULT_CLOSED;
        }
        if (received < 0) {
            return ZR_NETWORK_IO_RESULT_ERROR;
        }
        total += (TZrSize)received;
    }
    return ZR_NETWORK_IO_RESULT_SUCCESS;
}

static TZrBool network_query_available_bytes(ZrNetworkSocket socketHandle, TZrSize *outLength) {
#if defined(_WIN32)
    u_long pendingBytes = 0;

    if (outLength == ZR_NULL || ioctlsocket(socketHandle, FIONREAD, &pendingBytes) != 0) {
        return ZR_FALSE;
    }

    *outLength = (TZrSize)pendingBytes;
    return ZR_TRUE;
#else
    int pendingBytes = 0;

    if (outLength == ZR_NULL || ioctl(socketHandle, FIONREAD, &pendingBytes) != 0 || pendingBytes < 0) {
        return ZR_FALSE;
    }

    *outLength = (TZrSize)pendingBytes;
    return ZR_TRUE;
#endif
}

static TZrBool network_peek_exact(ZrNetworkSocket socketHandle, TZrByte *buffer, TZrSize length) {
    int received;

    if (buffer == ZR_NULL || length == 0 || length > INT_MAX) {
        return ZR_FALSE;
    }

    received = recv(socketHandle,
                    (char *)buffer,
                    (int)length,
#if defined(MSG_PEEK)
                    MSG_PEEK
#else
                    0
#endif
    );
    return received >= 0 && (TZrSize)received >= length ? ZR_TRUE : ZR_FALSE;
}

TZrBool ZrNetwork_ParseEndpoint(const TZrChar *text, SZrNetworkEndpoint *outEndpoint, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    const TZrChar *hostStart = text;
    const TZrChar *hostEnd = ZR_NULL;
    const TZrChar *portText = ZR_NULL;
    TZrChar normalizedHost[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    char *endPointer = ZR_NULL;
    unsigned long portValue;

    if (outEndpoint == ZR_NULL || text == ZR_NULL || text[0] == '\0') {
        network_write_error(errorBuffer, errorBufferSize, "endpoint text is required");
        return ZR_FALSE;
    }

    memset(outEndpoint, 0, sizeof(*outEndpoint));
    if (text[0] == '[') {
        hostStart = text + 1;
        hostEnd = strchr(hostStart, ']');
        if (hostEnd == ZR_NULL || hostEnd[1] != ':') {
            network_write_error(errorBuffer, errorBufferSize, "IPv6 endpoints must use [host]:port");
            return ZR_FALSE;
        }
        portText = hostEnd + 2;
    } else {
        const TZrChar *firstColon = strchr(text, ':');
        hostEnd = strrchr(text, ':');
        if (hostEnd == ZR_NULL) {
            network_write_error(errorBuffer, errorBufferSize, "endpoint must use host:port");
            return ZR_FALSE;
        }
        if (firstColon != hostEnd) {
            network_write_error(errorBuffer, errorBufferSize, "IPv6 endpoints must use [host]:port");
            return ZR_FALSE;
        }
        portText = hostEnd + 1;
    }

    if ((TZrSize)(hostEnd - hostStart) >= sizeof(outEndpoint->host)) {
        network_write_error(errorBuffer, errorBufferSize, "endpoint host is too long");
        return ZR_FALSE;
    }
    memcpy(outEndpoint->host, hostStart, (size_t)(hostEnd - hostStart));
    outEndpoint->host[hostEnd - hostStart] = '\0';
    portValue = strtoul(portText, &endPointer, 10);
    if (endPointer == portText || endPointer == ZR_NULL || *endPointer != '\0' || portValue > 65535UL) {
        network_write_error(errorBuffer, errorBufferSize, "endpoint port must be between 0 and 65535");
        return ZR_FALSE;
    }
    if (!network_normalize_host(outEndpoint->host, normalizedHost, sizeof(normalizedHost))) {
        network_write_error(errorBuffer, errorBufferSize, "endpoint host is too long");
        return ZR_FALSE;
    }
    snprintf(outEndpoint->host, sizeof(outEndpoint->host), "%s", normalizedHost);
    outEndpoint->port = (TZrUInt16)portValue;
    return ZR_TRUE;
}

TZrBool ZrNetwork_Endpoint_IsLoopbackHost(const TZrChar *host) {
    return host != ZR_NULL && (strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0 || strcmp(host, "::1") == 0);
}

TZrBool ZrNetwork_TcpListenerOpen(const SZrNetworkEndpoint *requested, SZrNetworkListener *outListener, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    struct sockaddr_storage storage;
    ZrNetworkSockLen storageLength = 0;
    ZrNetworkSocket socketHandle;
    int reuseAddress = 1;
    if (outListener == ZR_NULL || !network_initialize(errorBuffer, errorBufferSize) ||
        !network_sockaddr_from_endpoint(requested, &storage, &storageLength, errorBuffer, errorBufferSize)) {
        return ZR_FALSE;
    }
    memset(outListener, 0, sizeof(*outListener));
    socketHandle = socket(storage.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (socketHandle == ZR_NETWORK_INVALID_SOCKET) {
        network_write_socket_error(errorBuffer, errorBufferSize, "failed to create TCP listener", network_last_error());
        return ZR_FALSE;
    }
    setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR,
#if defined(_WIN32)
               (const char *)&reuseAddress,
#else
               &reuseAddress,
#endif
               (ZrNetworkSockLen)sizeof(reuseAddress));
    if (bind(socketHandle, (const struct sockaddr *)&storage, storageLength) != 0 || listen(socketHandle, 16) != 0) {
        network_write_socket_error(errorBuffer, errorBufferSize, "failed to open TCP listener", network_last_error());
        network_close_socket(socketHandle);
        return ZR_FALSE;
    }
    outListener->nativeHandle = network_store_socket(socketHandle);
    outListener->isOpen = ZR_TRUE;
    network_update_endpoints(socketHandle, &outListener->endpoint, ZR_NULL);
    return ZR_TRUE;
}

TZrBool ZrNetwork_ListenerOpenLoopback(const SZrNetworkEndpoint *requested, SZrNetworkListener *outListener, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    if (requested == ZR_NULL || !ZrNetwork_Endpoint_IsLoopbackHost(requested->host)) {
        network_write_error(errorBuffer, errorBufferSize, "listener loopback host must be localhost, 127.0.0.1, or ::1");
        return ZR_FALSE;
    }
    return ZrNetwork_TcpListenerOpen(requested, outListener, errorBuffer, errorBufferSize);
}

void ZrNetwork_ListenerClose(SZrNetworkListener *listener) {
    if (listener == ZR_NULL) {
        return;
    }
    network_close_socket(network_load_socket(listener->nativeHandle));
    memset(listener, 0, sizeof(*listener));
}

TZrBool ZrNetwork_ListenerAccept(SZrNetworkListener *listener, TZrUInt32 timeoutMs, SZrNetworkStream *outStream) {
    ZrNetworkSocket listenerSocket;
    ZrNetworkSocket streamSocket;
    if (listener == ZR_NULL || outStream == ZR_NULL || !listener->isOpen) {
        return ZR_FALSE;
    }
    listenerSocket = network_load_socket(listener->nativeHandle);
    if (listenerSocket == ZR_NETWORK_INVALID_SOCKET || network_wait_socket(listenerSocket, timeoutMs, ZR_FALSE) <= 0) {
        return ZR_FALSE;
    }
    streamSocket = accept(listenerSocket, ZR_NULL, ZR_NULL);
    if (streamSocket == ZR_NETWORK_INVALID_SOCKET) {
        return ZR_FALSE;
    }
    memset(outStream, 0, sizeof(*outStream));
    outStream->nativeHandle = network_store_socket(streamSocket);
    outStream->isOpen = ZR_TRUE;
    network_update_endpoints(streamSocket, &outStream->localEndpoint, &outStream->remoteEndpoint);
    return ZR_TRUE;
}

TZrBool ZrNetwork_TcpStreamConnect(const SZrNetworkEndpoint *endpoint, TZrUInt32 timeoutMs, SZrNetworkStream *outStream, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    struct sockaddr_storage storage;
    ZrNetworkSockLen storageLength = 0;
    ZrNetworkSocket socketHandle;
    int connectError = 0;
    ZrNetworkSockLen connectErrorLength = (ZrNetworkSockLen)sizeof(connectError);
    if (outStream == ZR_NULL || !network_initialize(errorBuffer, errorBufferSize) ||
        !network_sockaddr_from_endpoint(endpoint, &storage, &storageLength, errorBuffer, errorBufferSize)) {
        return ZR_FALSE;
    }
    memset(outStream, 0, sizeof(*outStream));
    socketHandle = socket(storage.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (socketHandle == ZR_NETWORK_INVALID_SOCKET) {
        network_write_socket_error(errorBuffer, errorBufferSize, "failed to create TCP stream", network_last_error());
        return ZR_FALSE;
    }
    if (!network_set_nonblocking(socketHandle, ZR_TRUE)) {
        network_close_socket(socketHandle);
        return ZR_FALSE;
    }
    if (connect(socketHandle, (const struct sockaddr *)&storage, storageLength) != 0) {
        int errorCode = network_last_error();
        if (!ZR_NETWORK_SOCKET_WOULD_BLOCK(errorCode) || network_wait_socket(socketHandle, timeoutMs, ZR_TRUE) <= 0 ||
            getsockopt(socketHandle, SOL_SOCKET, SO_ERROR,
#if defined(_WIN32)
                       (char *)&connectError,
#else
                       &connectError,
#endif
                       &connectErrorLength) != 0 || connectError != 0) {
            network_write_socket_error(errorBuffer, errorBufferSize, "failed to connect TCP stream", connectError != 0 ? connectError : errorCode);
            network_close_socket(socketHandle);
            return ZR_FALSE;
        }
    }
    network_set_nonblocking(socketHandle, ZR_FALSE);
    outStream->nativeHandle = network_store_socket(socketHandle);
    outStream->isOpen = ZR_TRUE;
    network_update_endpoints(socketHandle, &outStream->localEndpoint, &outStream->remoteEndpoint);
    return ZR_TRUE;
}

TZrBool ZrNetwork_StreamConnectLoopback(const SZrNetworkEndpoint *endpoint, TZrUInt32 timeoutMs, SZrNetworkStream *outStream, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    if (endpoint == ZR_NULL || !ZrNetwork_Endpoint_IsLoopbackHost(endpoint->host)) {
        network_write_error(errorBuffer, errorBufferSize, "stream loopback host must be localhost, 127.0.0.1, or ::1");
        return ZR_FALSE;
    }
    return ZrNetwork_TcpStreamConnect(endpoint, timeoutMs, outStream, errorBuffer, errorBufferSize);
}

void ZrNetwork_StreamClose(SZrNetworkStream *stream) {
    ZrNetworkSocket socketHandle;
    if (stream == ZR_NULL) {
        return;
    }
    socketHandle = network_load_socket(stream->nativeHandle);
    if (socketHandle != ZR_NETWORK_INVALID_SOCKET) {
        shutdown(socketHandle, ZR_NETWORK_SHUT_RDWR);
        network_close_socket(socketHandle);
    }
    memset(stream, 0, sizeof(*stream));
}

TZrBool ZrNetwork_StreamWrite(SZrNetworkStream *stream, const TZrByte *bytes, TZrSize length, TZrSize *outWritten) {
    ZrNetworkSocket socketHandle;
    TZrSize total = 0;
    if (outWritten != ZR_NULL) {
        *outWritten = 0;
    }
    if (stream == ZR_NULL || !stream->isOpen || bytes == ZR_NULL) {
        return ZR_FALSE;
    }
    socketHandle = network_load_socket(stream->nativeHandle);
    while (total < length) {
        int sent = send(socketHandle, (const char *)(bytes + total), (int)(length - total),
#if defined(MSG_NOSIGNAL)
                        MSG_NOSIGNAL
#else
                        0
#endif
        );
        if (sent <= 0) {
            return ZR_FALSE;
        }
        total += (TZrSize)sent;
    }
    if (outWritten != ZR_NULL) {
        *outWritten = total;
    }
    return ZR_TRUE;
}

TZrBool ZrNetwork_StreamRead(SZrNetworkStream *stream, TZrUInt32 timeoutMs, TZrByte *buffer, TZrSize bufferSize, TZrSize *outLength) {
    ZrNetworkSocket socketHandle;
    int received;
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (stream == ZR_NULL || !stream->isOpen || buffer == ZR_NULL || bufferSize == 0 || bufferSize > INT_MAX) {
        return ZR_FALSE;
    }
    socketHandle = network_load_socket(stream->nativeHandle);
    if (network_wait_socket(socketHandle, timeoutMs, ZR_FALSE) <= 0) {
        return ZR_FALSE;
    }
    received = recv(socketHandle, (char *)buffer, (int)bufferSize, 0);
    if (received <= 0) {
        return ZR_FALSE;
    }
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)received;
    }
    return ZR_TRUE;
}

TZrBool ZrNetwork_StreamWriteFrame(SZrNetworkStream *stream, const TZrChar *text, TZrSize length) {
    TZrUInt32 frameLength = htonl((TZrUInt32)length);
    TZrSize written = 0;
    return stream != ZR_NULL && text != ZR_NULL && length <= UINT32_MAX &&
           ZrNetwork_StreamWrite(stream, (const TZrByte *)&frameLength, sizeof(frameLength), &written) &&
           written == sizeof(frameLength) &&
           ZrNetwork_StreamWrite(stream, (const TZrByte *)text, length, &written) && written == length;
}

TZrBool ZrNetwork_StreamReadFrame(SZrNetworkStream *stream, TZrUInt32 timeoutMs, TZrChar *buffer, TZrSize bufferSize, TZrSize *outLength) {
    ZrNetworkSocket socketHandle;
    TZrUInt32 frameLength = 0;
    int waitStatus;
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (stream == ZR_NULL || !stream->isOpen || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    socketHandle = network_load_socket(stream->nativeHandle);
    waitStatus = network_wait_socket(socketHandle, timeoutMs, ZR_FALSE);
    if (waitStatus == 0) {
        return ZR_FALSE;
    }
    if (waitStatus < 0) {
        ZrNetwork_StreamClose(stream);
        return ZR_FALSE;
    }

    if (timeoutMs == 0) {
        TZrSize availableBytes = 0;

        if (!network_query_available_bytes(socketHandle, &availableBytes)) {
            ZrNetwork_StreamClose(stream);
            return ZR_FALSE;
        }

        if (availableBytes == 0) {
            ZrNetwork_StreamClose(stream);
            return ZR_FALSE;
        }

        /* Zero-timeout polling is used by the debugger trace hook. If only part of a frame
         * has arrived, leave the bytes queued and try again on the next safepoint instead of
         * consuming a partial frame and tearing down the control stream. */
        if (availableBytes < sizeof(frameLength) ||
            !network_peek_exact(socketHandle, (TZrByte *)&frameLength, sizeof(frameLength))) {
            return ZR_FALSE;
        }

        frameLength = ntohl(frameLength);
        if ((TZrSize)frameLength + 1 > bufferSize) {
            ZrNetwork_StreamClose(stream);
            return ZR_FALSE;
        }

        if (availableBytes < sizeof(frameLength) + (TZrSize)frameLength) {
            return ZR_FALSE;
        }
    }

    if (network_read_exact(socketHandle, timeoutMs, (TZrByte *)&frameLength, sizeof(frameLength)) !=
        ZR_NETWORK_IO_RESULT_SUCCESS) {
        ZrNetwork_StreamClose(stream);
        return ZR_FALSE;
    }
    frameLength = ntohl(frameLength);
    if ((TZrSize)frameLength + 1 > bufferSize ||
        network_read_exact(socketHandle, timeoutMs, (TZrByte *)buffer, frameLength) !=
                ZR_NETWORK_IO_RESULT_SUCCESS) {
        ZrNetwork_StreamClose(stream);
        return ZR_FALSE;
    }
    buffer[frameLength] = '\0';
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)frameLength;
    }
    return ZR_TRUE;
}

TZrBool ZrNetwork_UdpSocketBind(const SZrNetworkEndpoint *requested, SZrNetworkUdpSocket *outSocket, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    struct sockaddr_storage storage;
    ZrNetworkSockLen storageLength = 0;
    ZrNetworkSocket socketHandle;
    int reuseAddress = 1;
    if (outSocket == ZR_NULL || !network_initialize(errorBuffer, errorBufferSize) ||
        !network_sockaddr_from_endpoint(requested, &storage, &storageLength, errorBuffer, errorBufferSize)) {
        return ZR_FALSE;
    }
    memset(outSocket, 0, sizeof(*outSocket));
    socketHandle = socket(storage.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == ZR_NETWORK_INVALID_SOCKET) {
        network_write_socket_error(errorBuffer, errorBufferSize, "failed to create UDP socket", network_last_error());
        return ZR_FALSE;
    }
    setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR,
#if defined(_WIN32)
               (const char *)&reuseAddress,
#else
               &reuseAddress,
#endif
               (ZrNetworkSockLen)sizeof(reuseAddress));
    if (bind(socketHandle, (const struct sockaddr *)&storage, storageLength) != 0) {
        network_write_socket_error(errorBuffer, errorBufferSize, "failed to bind UDP socket", network_last_error());
        network_close_socket(socketHandle);
        return ZR_FALSE;
    }
    outSocket->nativeHandle = network_store_socket(socketHandle);
    outSocket->isOpen = ZR_TRUE;
    network_update_endpoints(socketHandle, &outSocket->endpoint, ZR_NULL);
    return ZR_TRUE;
}

void ZrNetwork_UdpSocketClose(SZrNetworkUdpSocket *socket) {
    if (socket == ZR_NULL) {
        return;
    }
    network_close_socket(network_load_socket(socket->nativeHandle));
    memset(socket, 0, sizeof(*socket));
}

TZrBool ZrNetwork_UdpSocketSend(SZrNetworkUdpSocket *socket, const SZrNetworkEndpoint *target, const TZrByte *bytes, TZrSize length, TZrSize *outLength, TZrChar *errorBuffer, TZrSize errorBufferSize) {
    ZrNetworkSocket socketHandle;
    struct sockaddr_storage storage;
    ZrNetworkSockLen storageLength = 0;
    int sent;
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (socket == ZR_NULL || !socket->isOpen || target == ZR_NULL || bytes == ZR_NULL || length > INT_MAX ||
        !network_sockaddr_from_endpoint(target, &storage, &storageLength, errorBuffer, errorBufferSize)) {
        return ZR_FALSE;
    }
    socketHandle = network_load_socket(socket->nativeHandle);
    sent = sendto(socketHandle, (const char *)bytes, (int)length, 0, (const struct sockaddr *)&storage, storageLength);
    if (sent < 0) {
        network_write_socket_error(errorBuffer, errorBufferSize, "failed to send UDP payload", network_last_error());
        return ZR_FALSE;
    }
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)sent;
    }
    return ZR_TRUE;
}

TZrBool ZrNetwork_UdpSocketReceive(SZrNetworkUdpSocket *socket, TZrUInt32 timeoutMs, TZrByte *buffer, TZrSize bufferSize, TZrSize *outLength, SZrNetworkEndpoint *outRemoteEndpoint) {
    ZrNetworkSocket socketHandle;
    struct sockaddr_storage storage;
    ZrNetworkSockLen storageLength = (ZrNetworkSockLen)sizeof(storage);
    int received;
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (socket == ZR_NULL || !socket->isOpen || buffer == ZR_NULL || bufferSize == 0 || bufferSize > INT_MAX) {
        return ZR_FALSE;
    }
    socketHandle = network_load_socket(socket->nativeHandle);
    if (network_wait_socket(socketHandle, timeoutMs, ZR_FALSE) <= 0) {
        return ZR_FALSE;
    }
    received = recvfrom(socketHandle, (char *)buffer, (int)bufferSize, 0, (struct sockaddr *)&storage, &storageLength);
    if (received <= 0) {
        return ZR_FALSE;
    }
    if (outRemoteEndpoint != ZR_NULL) {
        network_endpoint_from_sockaddr((const struct sockaddr *)&storage, outRemoteEndpoint);
    }
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)received;
    }
    return ZR_TRUE;
}

TZrBool ZrNetwork_FormatEndpoint(const SZrNetworkEndpoint *endpoint, TZrChar *buffer, TZrSize bufferSize) {
    int requiredLength;
    TZrBool bracketIpv6;
    if (endpoint == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    bracketIpv6 = strchr(endpoint->host, ':') != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    requiredLength = snprintf(buffer, bufferSize, bracketIpv6 ? "[%s]:%u" : "%s:%u", endpoint->host, (unsigned)endpoint->port);
    if (requiredLength < 0 || (TZrSize)requiredLength >= bufferSize) {
        buffer[0] = '\0';
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrNetwork_TokenMatches(const TZrChar *expected, const TZrChar *actual) {
    TZrSize length;
    TZrUInt8 diff = 0;
    TZrSize index;

    if (expected == ZR_NULL || expected[0] == '\0') {
        return ZR_TRUE;
    }
    if (actual == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(expected);
    if (length != strlen(actual)) {
        return ZR_FALSE;
    }
    for (index = 0; index < length; index++) {
        diff |= (TZrUInt8)(expected[index] ^ actual[index]);
    }
    return diff == 0 ? ZR_TRUE : ZR_FALSE;
}
