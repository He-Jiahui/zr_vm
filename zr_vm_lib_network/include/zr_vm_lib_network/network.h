#ifndef ZR_VM_LIB_NETWORK_NETWORK_H
#define ZR_VM_LIB_NETWORK_NETWORK_H

#include "zr_vm_lib_network/conf.h"

/** @brief 主机文本与端口的值对象；建立 socket 时仅接受数值 IP 或 localhost。 */
typedef struct SZrNetworkEndpoint {
    TZrChar host[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    TZrUInt16 port;
} SZrNetworkEndpoint;

/** @brief 拥有一个监听 socket；nativeHandle 与 isOpen 必须一起转移或清零，结构体副本不能分别关闭。 */
typedef struct SZrNetworkListener {
    TZrPtr nativeHandle;
    SZrNetworkEndpoint endpoint;
    TZrBool isOpen;
} SZrNetworkListener;

/** @brief 拥有一个已连接 TCP socket；本地和远端地址是建立连接时取得的快照。 */
typedef struct SZrNetworkStream {
    TZrPtr nativeHandle;
    TZrBool isOpen;
    SZrNetworkEndpoint localEndpoint;
    SZrNetworkEndpoint remoteEndpoint;
} SZrNetworkStream;

/** @brief 拥有一个已绑定 UDP socket；endpoint 是绑定后的本地地址快照。 */
typedef struct SZrNetworkUdpSocket {
    TZrPtr nativeHandle;
    SZrNetworkEndpoint endpoint;
    TZrBool isOpen;
} SZrNetworkUdpSocket;

/** @brief 解析 host:port 或 [IPv6]:port，并将空主机及 localhost 规范化为 127.0.0.1。
 *  @note 本函数检查端口和文本格式；数值 IP 的有效性在建立 socket 时检查。 */
ZR_NETWORK_API TZrBool ZrNetwork_ParseEndpoint(const TZrChar *text, SZrNetworkEndpoint *outEndpoint,
                                               TZrChar *errorBuffer, TZrSize errorBufferSize);

/** @brief 判断主机文本是否恰为 localhost、127.0.0.1 或 ::1；不进行 DNS 查询。 */
ZR_NETWORK_API TZrBool ZrNetwork_Endpoint_IsLoopbackHost(const TZrChar *host);

/** @brief 建立 TCP 监听 socket，并将其所有权交给 outListener。
 *  @pre outListener 不得持有尚未关闭的 socket；成功后调用 ZrNetwork_ListenerClose。 */
ZR_NETWORK_API TZrBool ZrNetwork_TcpListenerOpen(const SZrNetworkEndpoint *requested,
                                                 SZrNetworkListener *outListener,
                                                 TZrChar *errorBuffer,
                                                 TZrSize errorBufferSize);

/** @brief 限制主机为显式回环地址后建立 TCP 监听，供调试代理使用。 */
ZR_NETWORK_API TZrBool ZrNetwork_ListenerOpenLoopback(const SZrNetworkEndpoint *requested,
                                                      SZrNetworkListener *outListener,
                                                      TZrChar *errorBuffer,
                                                      TZrSize errorBufferSize);

/** @brief 关闭监听 socket 并清零句柄；可对已清零的句柄重复调用。 */
ZR_NETWORK_API void ZrNetwork_ListenerClose(SZrNetworkListener *listener);

/** @brief 等待并接受一个 TCP 连接，成功时由 outStream 拥有新 socket。
 *  @pre outStream 不得持有尚未关闭的 socket。
 *  @return 超时、监听关闭或 socket 错误均返回 ZR_FALSE，无法从返回值区分原因。
 *  @note timeoutMs 为等待可读的毫秒数；ZR_NETWORK_WAIT_INFINITE 表示无限等待。 */
ZR_NETWORK_API TZrBool ZrNetwork_ListenerAccept(SZrNetworkListener *listener,
                                                TZrUInt32 timeoutMs,
                                                SZrNetworkStream *outStream);

/** @brief 连接数值 IP 或 localhost，连接阶段使用 timeoutMs，随后尝试恢复阻塞模式。
 *  @pre outStream 不得持有尚未关闭的 socket；成功后调用 ZrNetwork_StreamClose。 */
ZR_NETWORK_API TZrBool ZrNetwork_TcpStreamConnect(const SZrNetworkEndpoint *endpoint,
                                                  TZrUInt32 timeoutMs,
                                                  SZrNetworkStream *outStream,
                                                  TZrChar *errorBuffer,
                                                  TZrSize errorBufferSize);

/** @brief 只允许显式回环主机的 TCP 连接，供调试客户端连接本地代理。 */
ZR_NETWORK_API TZrBool ZrNetwork_StreamConnectLoopback(const SZrNetworkEndpoint *endpoint,
                                                       TZrUInt32 timeoutMs,
                                                       SZrNetworkStream *outStream,
                                                       TZrChar *errorBuffer,
                                                       TZrSize errorBufferSize);

/** @brief 双向关闭 TCP 流并清零句柄；可对已清零的句柄重复调用。 */
ZR_NETWORK_API void ZrNetwork_StreamClose(SZrNetworkStream *stream);

/** @brief 阻塞写入指定字节数，成功时 outWritten 为 length。
 *  @note 失败时 socket 可能已写入部分字节；当前实现的 outWritten 仍为 0。 */
ZR_NETWORK_API TZrBool ZrNetwork_StreamWrite(SZrNetworkStream *stream,
                                             const TZrByte *bytes,
                                             TZrSize length,
                                             TZrSize *outWritten);

/** @brief 等待可读后执行一次 recv，返回本次实际收到的字节数。
 *  @return 等待超时、EOF 或读错误返回 ZR_FALSE；此函数不会自动关闭流。 */
ZR_NETWORK_API TZrBool ZrNetwork_StreamRead(SZrNetworkStream *stream,
                                            TZrUInt32 timeoutMs,
                                            TZrByte *buffer,
                                            TZrSize bufferSize,
                                            TZrSize *outLength);

/** @brief 按四字节网络字节序长度前缀写入一帧，不额外发送结尾 NUL。
 *  @pre length 不超过 UINT32_MAX；写入失败时连接可能留下不完整帧。 */
ZR_NETWORK_API TZrBool ZrNetwork_StreamWriteFrame(SZrNetworkStream *stream, const TZrChar *text, TZrSize length);

/** @brief 读取四字节网络字节序长度前缀和完整载荷，并在 buffer 中追加 NUL。
 *  @pre bufferSize 至少为载荷长度加一。
 *  @note timeoutMs 为 0 时仅消费已经完整排队的帧；不完整帧暂留 socket。初次等待超时不关闭流，开始读取后失败则关闭流。 */
ZR_NETWORK_API TZrBool ZrNetwork_StreamReadFrame(SZrNetworkStream *stream,
                                                 TZrUInt32 timeoutMs,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize,
                                                 TZrSize *outLength);

/** @brief 绑定数值 IP 或 localhost 上的 UDP socket；端口 0 由系统分配。
 *  @pre outSocket 不得持有尚未关闭的 socket；成功后调用 ZrNetwork_UdpSocketClose。 */
ZR_NETWORK_API TZrBool ZrNetwork_UdpSocketBind(const SZrNetworkEndpoint *requested,
                                               SZrNetworkUdpSocket *outSocket,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

/** @brief 关闭 UDP socket 并清零句柄；可对已清零的句柄重复调用。 */
ZR_NETWORK_API void ZrNetwork_UdpSocketClose(SZrNetworkUdpSocket *socket);

/** @brief 向数值 IP 或 localhost 发送一个 UDP 数据报，并返回实际发送字节数。
 *  @pre bytes 非空；长度可以为 0。 */
ZR_NETWORK_API TZrBool ZrNetwork_UdpSocketSend(SZrNetworkUdpSocket *socket,
                                               const SZrNetworkEndpoint *target,
                                               const TZrByte *bytes,
                                               TZrSize length,
                                               TZrSize *outLength,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

/** @brief 等待并接收一个 UDP 数据报，可返回发送端地址。
 *  @return 超时、读错误和当前实现遇到的零字节数据报均返回 ZR_FALSE。
 *  @note 缓冲区不足时系统可能截断数据报；返回值不区分截断与完整接收。 */
ZR_NETWORK_API TZrBool ZrNetwork_UdpSocketReceive(SZrNetworkUdpSocket *socket,
                                                  TZrUInt32 timeoutMs,
                                                  TZrByte *buffer,
                                                  TZrSize bufferSize,
                                                  TZrSize *outLength,
                                                  SZrNetworkEndpoint *outRemoteEndpoint);

/** @brief 将地址格式化为 host:port，IPv6 主机自动添加方括号。 */
ZR_NETWORK_API TZrBool ZrNetwork_FormatEndpoint(const SZrNetworkEndpoint *endpoint,
                                                TZrChar *buffer,
                                                TZrSize bufferSize);

/** @brief 比较调试握手令牌；expected 为空时表示不要求认证。
 *  @note 长度不同会提前返回，相同长度才逐字节累积差异。 */
ZR_NETWORK_API TZrBool ZrNetwork_TokenMatches(const TZrChar *expected, const TZrChar *actual);

#endif
