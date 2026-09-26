---
related_code:
  - zr_vm_lib_network/include/zr_vm_lib_network/network.h
  - zr_vm_lib_network/include/zr_vm_lib_network/tcp_registry.h
  - zr_vm_lib_network/include/zr_vm_lib_network/udp_registry.h
  - zr_vm_lib_network/include/zr_vm_lib_network/conf.h
  - zr_vm_lib_network/src/zr_vm_lib_network/module_support.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
implementation_files:
  - zr_vm_lib_network/src/zr_vm_lib_network/module_support.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/parser/test_compiler_regressions.c
  - tests/fixtures/projects/network_loopback/network_loopback.zrp
  - tests/fixtures/projects/network_loopback/src/main.zr
  - tests/language_server/test_lsp_project_features.c
doc_type: api-reference
---

# `zr.network` API 参考

根模块 `zr.network` 只链接 `tcp`、`udp` 两个叶模块。脚本对象借助私有 native pointer
字段持有 C 包装体；包装体包含底层 socket 的所有权结构。该字段不参与 GC，也没有 socket
finalizer。`close()` 会关闭系统 socket，**不会释放包装体**；丢弃未关闭对象会遗留 socket
和包装体。底层传输直接使用 BSD socket/Winsock 与 `select`。

## endpoint 规则

`ZrNetwork_ParseEndpoint` 接受 `host:port` 或 `[IPv6]:port`，例如 `127.0.0.1:8080`
和 `[::1]:8080`。`SZrNetworkEndpoint` 的 host 容量为 96 字节，端口范围为 `0..65535`。
解析器检查文本形状和端口；建立 socket 时仅接受数值 IP 或 `localhost`，不做 DNS 查询。
空 host 和 `localhost` 被规范化为 `127.0.0.1`。

`ZrNetwork_Endpoint_IsLoopbackHost` 只接受 `localhost`、`127.0.0.1`、`::1`。仅 C 接口
`ZrNetwork_ListenerOpenLoopback` 与 `ZrNetwork_StreamConnectLoopback` 使用这个检查；
脚本的普通 `listen`、`connect`、`bind`、`send` 可以使用其他数值地址。

## TCP

### 脚本导出

| 导出 | 签名 | 语义 |
| --- | --- | --- |
| `listen` | `listen(host: string, port: int): TcpListener` | 绑定并监听；port=0 允许 OS 选择端口。 |
| `connect` | `connect(host: string, port: int, timeoutMs?: int): TcpStream` | 连接；timeout 缺省为 5000 毫秒，失败时抛异常。 |
| `TcpListener.accept` | `accept(timeoutMs: int): TcpStream/null` | 等待连接；超时、关闭或 socket 错误均返回 null。 |
| `TcpListener.close` | `close(): null` | 关闭监听 socket；可重复调用。 |
| `TcpListener.isClosed` | `isClosed(): bool` | 读取关闭状态。 |
| `TcpListener.host/port` | `host()` / `port()` | 返回实际绑定 endpoint。 |
| `TcpStream.read` | `read(maxBytes: int, timeoutMs: int): string/null` | 一次读取最多 maxBytes；超时、EOF、关闭或 socket 错误均返回 null。 |
| `TcpStream.write` | `write(text: string): int` | 尝试写字符串的 C 字符串前缀，返回报告的写入字节数；失败也可能返回 0。 |
| `TcpStream.close/isClosed` | `close()` / `isClosed()` | 关闭 socket / 返回状态。 |
| `TcpStream.localHost/localPort` | `localHost()` / `localPort()` | 未关闭时返回建立连接时的本地 endpoint 快照。 |
| `TcpStream.remoteHost/remotePort` | `remoteHost()` / `remotePort()` | 未关闭时返回建立连接时的对端 endpoint 快照。 |

```zr
let tcp = import("zr.network.tcp");
let listener = tcp.listen("127.0.0.1", 0);
let port = listener.port();
let client = tcp.connect("127.0.0.1", port);
let peer = listener.accept(1000);
client.write("ping");
let text = peer.read(64, 1000);
peer.close();
client.close();
listener.close();
```

`accept` / `read` 回调有省略超时的无限等待默认值，但其 descriptor 分别只接受 1 / 2 个
显式参数；`accept()` 与 `read(maxBytes)` 在 native dispatch 前置参数检查中被拒绝，这是当前
**BUG**。超时参数是 `0..4294967295` 的整数；`0` 为立即轮询，`4294967295` 为无限等待。
`read` 的 `maxBytes` 必须大于零，底层 `ZrNetwork_StreamRead` 要求缓冲区不超过 `INT_MAX`；
脚本还会先申请 `maxBytes+1` 字节缓冲区。一次 `recv` 可短读，不会自动拼接完整应用消息。

TCP 的 `null` 返回值不区分超时、EOF、关闭和 socket 错误。`write("")` 返回 0；非空写入
也可能在失败时返回 0。脚本写入以 `strlen` 截断嵌入 NUL 的字符串，脚本读取又以 C 字符串
创建返回值，丢失首个 NUL 之后的已读字节；这是当前 **BUG**。底层分批 `send` 部分成功后
失败仍把 `outWritten` 报为 0，脚本 `write` 也返回 0；调用方不能据此断定没有字节到达对端。

### C TCP API

| 入口 | 说明 |
| --- | --- |
| `ZrNetwork_ParseEndpoint` | 文本转 `SZrNetworkEndpoint`，写 error buffer。 |
| `ZrNetwork_TcpListenerOpen` / `ListenerOpenLoopback` | 绑定监听。 |
| `ZrNetwork_ListenerAccept` | 等待一个连接；超时、关闭或 socket 错误都返回 `ZR_FALSE`，不区分原因。 |
| `ZrNetwork_TcpStreamConnect` / `StreamConnectLoopback` | 非阻塞连接和超时。 |
| `ZrNetwork_StreamRead` | 等待后执行一次 `recv`；失败返回 `ZR_FALSE` 且不自动关闭流。 |
| `ZrNetwork_StreamWrite` | 目标是写完指定字节；仅全部成功后写 `outWritten=length`，部分成功后失败仍报告 0。 |
| `ZrNetwork_ListenerClose` / `StreamClose` | 关闭系统 socket 并清零 C 结构；对已清零结构重复调用安全。 |

调用 `Open` / `Connect` 前，应给输出结构初始清零，且不要把未关闭的旧 socket 当作输出结构
复用；部分前置失败路径不会清零它。成功后 C 调用方负责执行相应 `Close`。包含同一
`nativeHandle` 的结构副本不可各自关闭，否则会重复关闭同一系统描述符。

**TODO：** `ZrNetwork_StreamWrite` 将本次剩余的 `size_t` 长度直接转成 `int` 传给 `send`，
没有将大于 `INT_MAX` 的单次写入分块；大长度行为仍需在目标平台核实。脚本 `read` 和
`receive` 会先把正数 `int64` 转为 `TZrSize` 再分配缓冲区，在 32 位目标上的截断边界
也仍需核实。

## framing

`ZrNetwork_StreamWriteFrame` / `ZrNetwork_StreamReadFrame` 使用 4 字节网络字节序长度
前缀，载荷是原始字节，不验证 UTF-8，也没有 TLS、压缩或认证。`WriteFrame` 接受
`length <= UINT32_MAX`；`ReadFrame` 使用调用方提供的缓冲区，要求容量至少为载荷长度
加一个终止 NUL。`ZR_NETWORK_FRAME_BUFFER_CAPACITY=8192` 是调试代理使用的固定缓冲区
容量，不是这两个 C API 的统一帧上限。

```text
uint32_be payload_length
payload bytes
```

零长度帧合法。`ReadFrame` 初次等待超时时返回失败而保持流打开；等待错误、容量不足，
或进入精确读取后仍无法读满帧（EOF、错误、超时）时会关闭流。`timeoutMs=0` 时只消费
已完整排队的帧，未完整时
保留接收队列；**BUG：** 对端发送半帧后关闭，残留字节可使后续轮询一直返回失败但仍
保持 `isOpen`，现有调试协议路径可能因此拒绝后续客户端。**BUG：** 32 位 `size_t` 上，
帧长为 `UINT32_MAX` 时 `frameLength+1` 确定回绕，容量比较会放行超限帧；后续接收与
写入的具体后果仍需在 32 位目标核实。**TODO：** 精确读取把剩余 `size_t` 长度转为
`int` 传给 `recv`，大帧的目标平台行为需单独核查。写帧失败后可能已发送帧头或部分
载荷，连接可能失去帧边界同步。

## UDP

| 导出 | 签名 | 语义 |
| --- | --- | --- |
| `bind` | `bind(host: string, port: int): UdpSocket` | 绑定 datagram socket。 |
| `UdpSocket.send` | `send(host: string, port: int, payload: string): int` | 发送一个 datagram；成功时返回系统报告的字节数，失败时抛异常。 |
| `UdpSocket.receive` | `receive(maxBytes: int, timeoutMs: int): UdpPacket/null` | 接收一个 datagram；超时、关闭或 socket 错误都返回 null。 |
| `UdpSocket.close/isClosed` | `close()` / `isClosed()` | 关闭和状态。 |
| `UdpSocket.host/port` | `host()` / `port()` | 实际绑定 endpoint。 |
| `UdpPacket` | `payload:string`、`host:string`、`port:int`、`length:int` | 创建时保存数据与来源；`length` 是本次底层接收字节数。 |

`receive` 回调有省略超时的无限等待默认值，但 descriptor 要求 2 个显式参数；
`receive(maxBytes)` 在 native dispatch 中被拒绝，这是当前 **BUG**。`maxBytes` 必须大于零，
底层 `ZrNetwork_UdpSocketReceive` 要求缓冲区不超过 `INT_MAX`；缓冲区不足时系统可能截断
报文，返回值不标记截断。UDP 不保证到达、顺序或不重复。

`send(host, port, "")` 可以发出零字节数据报；当前 `recvfrom` 成功返回 0 时，接收实现却
把它当作失败，脚本返回 `null`，这是已复现的 **BUG**。发送和接收的脚本包装器都用 C
字符串长度处理 payload，嵌入 NUL 的数据在发送或返回时被截断；收到的
`UdpPacket.length` 仍是底层接收字节数。**BUG：** `UdpPacket` 的文字描述称
“immutable”，但字段元数据标为可写。**TODO：** 脚本实际赋值行为仍需核实，不能承诺
不可变。

C 接口 `ZrNetwork_UdpSocketSend` 拒绝长度超过 `INT_MAX` 的数据报；
`ZrNetwork_UdpSocketReceive` 要求 `1..INT_MAX` 字节的接收缓冲区。脚本层还受缓冲区
分配及操作系统报文大小限制，不能把大于 `INT_MAX` 的请求当作受支持调用。

调用已关闭 socket 的 `send` 会进入另一个 **BUG**：底层返回失败但没有写错误缓冲区，
脚本包装器把未初始化缓冲区当作异常文本。该异常内容不可靠。

## timeout 和系统边界

内部 wait infinite 常量为 `0xFFFFFFFF`。只有 `connect` 允许脚本省略 timeout，其默认值
是 5000 毫秒；`accept`、`read`、`receive` 当前必须显式提供超时，传入
`0xFFFFFFFF` 才会无限等待。显式 0 表示立即轮询。Windows 使用 Winsock，POSIX 使用
BSD socket；当前不做 DNS 查询。

## 安全清单

- 对外监听前显式选择 host；普通脚本入口不会自动限制为回环地址。
- 限制 frame length 和 UDP `maxBytes`，避免直接按不可信长度分配缓冲区。
- 显式关闭 stream、listener、socket；当前不存在 finalizer 兜底，且关闭后包装体仍泄漏。
- `ZrNetwork_TokenMatches` 只比较令牌；不提供密钥存储、加密或完整认证协议。

## 注册入口

```c
const ZrLibModuleDescriptor *network = ZrVmLibNetwork_GetModuleDescriptor();
TZrBool ok = ZrVmLibNetwork_Register(global);
```

根 descriptor 注册 module links；TCP/UDP 叶子由同一个 global registry 管理。注册过程
没有失败回滚，后续失败时可能留有已注册的叶模块。普通 TCP/UDP 回环文本收发由
`test_compiler_regressions.c` 与 `network_loopback` fixture 覆盖；LSP 测试验证模块链接和
成员元数据。这些测试不证明 GC 终结、二进制 payload、零字节 UDP、部分发送或半帧轮询
行为正确。更完整的 native callback 约定见 [Native API](native-api.md)。
