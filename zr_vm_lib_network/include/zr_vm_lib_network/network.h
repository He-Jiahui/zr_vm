#ifndef ZR_VM_LIB_NETWORK_NETWORK_H
#define ZR_VM_LIB_NETWORK_NETWORK_H

#include "zr_vm_lib_network/conf.h"

typedef struct SZrNetworkEndpoint {
    TZrChar host[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    TZrUInt16 port;
} SZrNetworkEndpoint;

typedef struct SZrNetworkListener {
    TZrPtr nativeHandle;
    SZrNetworkEndpoint endpoint;
    TZrBool isOpen;
} SZrNetworkListener;

typedef struct SZrNetworkStream {
    TZrPtr nativeHandle;
    TZrBool isOpen;
    SZrNetworkEndpoint localEndpoint;
    SZrNetworkEndpoint remoteEndpoint;
} SZrNetworkStream;

typedef struct SZrNetworkUdpSocket {
    TZrPtr nativeHandle;
    SZrNetworkEndpoint endpoint;
    TZrBool isOpen;
} SZrNetworkUdpSocket;

ZR_NETWORK_API TZrBool ZrNetwork_ParseEndpoint(const TZrChar *text, SZrNetworkEndpoint *outEndpoint,
                                               TZrChar *errorBuffer, TZrSize errorBufferSize);

ZR_NETWORK_API TZrBool ZrNetwork_Endpoint_IsLoopbackHost(const TZrChar *host);

ZR_NETWORK_API TZrBool ZrNetwork_TcpListenerOpen(const SZrNetworkEndpoint *requested,
                                                 SZrNetworkListener *outListener,
                                                 TZrChar *errorBuffer,
                                                 TZrSize errorBufferSize);

ZR_NETWORK_API TZrBool ZrNetwork_ListenerOpenLoopback(const SZrNetworkEndpoint *requested,
                                                      SZrNetworkListener *outListener,
                                                      TZrChar *errorBuffer,
                                                      TZrSize errorBufferSize);

ZR_NETWORK_API void ZrNetwork_ListenerClose(SZrNetworkListener *listener);

ZR_NETWORK_API TZrBool ZrNetwork_ListenerAccept(SZrNetworkListener *listener,
                                                TZrUInt32 timeoutMs,
                                                SZrNetworkStream *outStream);

ZR_NETWORK_API TZrBool ZrNetwork_TcpStreamConnect(const SZrNetworkEndpoint *endpoint,
                                                  TZrUInt32 timeoutMs,
                                                  SZrNetworkStream *outStream,
                                                  TZrChar *errorBuffer,
                                                  TZrSize errorBufferSize);

ZR_NETWORK_API TZrBool ZrNetwork_StreamConnectLoopback(const SZrNetworkEndpoint *endpoint,
                                                       TZrUInt32 timeoutMs,
                                                       SZrNetworkStream *outStream,
                                                       TZrChar *errorBuffer,
                                                       TZrSize errorBufferSize);

ZR_NETWORK_API void ZrNetwork_StreamClose(SZrNetworkStream *stream);

ZR_NETWORK_API TZrBool ZrNetwork_StreamWrite(SZrNetworkStream *stream,
                                             const TZrByte *bytes,
                                             TZrSize length,
                                             TZrSize *outWritten);

ZR_NETWORK_API TZrBool ZrNetwork_StreamRead(SZrNetworkStream *stream,
                                            TZrUInt32 timeoutMs,
                                            TZrByte *buffer,
                                            TZrSize bufferSize,
                                            TZrSize *outLength);

ZR_NETWORK_API TZrBool ZrNetwork_StreamWriteFrame(SZrNetworkStream *stream, const TZrChar *text, TZrSize length);

ZR_NETWORK_API TZrBool ZrNetwork_StreamReadFrame(SZrNetworkStream *stream,
                                                 TZrUInt32 timeoutMs,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize,
                                                 TZrSize *outLength);

ZR_NETWORK_API TZrBool ZrNetwork_UdpSocketBind(const SZrNetworkEndpoint *requested,
                                               SZrNetworkUdpSocket *outSocket,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

ZR_NETWORK_API void ZrNetwork_UdpSocketClose(SZrNetworkUdpSocket *socket);

ZR_NETWORK_API TZrBool ZrNetwork_UdpSocketSend(SZrNetworkUdpSocket *socket,
                                               const SZrNetworkEndpoint *target,
                                               const TZrByte *bytes,
                                               TZrSize length,
                                               TZrSize *outLength,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

ZR_NETWORK_API TZrBool ZrNetwork_UdpSocketReceive(SZrNetworkUdpSocket *socket,
                                                  TZrUInt32 timeoutMs,
                                                  TZrByte *buffer,
                                                  TZrSize bufferSize,
                                                  TZrSize *outLength,
                                                  SZrNetworkEndpoint *outRemoteEndpoint);

ZR_NETWORK_API TZrBool ZrNetwork_FormatEndpoint(const SZrNetworkEndpoint *endpoint,
                                                TZrChar *buffer,
                                                TZrSize bufferSize);

ZR_NETWORK_API TZrBool ZrNetwork_TokenMatches(const TZrChar *expected, const TZrChar *actual);

#endif
