---
related_code:
  - zr_vm_lib_network/include/zr_vm_lib_network/network.h
  - zr_vm_lib_network/include/zr_vm_lib_network/tcp_registry.h
  - zr_vm_lib_network/include/zr_vm_lib_network/udp_registry.h
  - zr_vm_lib_network/include/zr_vm_lib_network/conf.h
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
implementation_files:
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/network/test_network_module.c
  - tests/network/test_network_runtime.c
  - tests/network/test_network_endpoint.c
doc_type: api-reference
---

# `zr.network` API 参考

网络 provider 把 endpoint 解析、TCP listener/stream 和 UDP socket 封装为带 close 状态的
managed handle。根模块 `zr.network` 只链接 `tcp`、`udp` 两个叶子 provider。网络 I/O 使用
非阻塞 socket + timeout wait；脚本层的超时/EOF 通常表现为 null，系统错误则抛异常。

## endpoint 规则

```text
hostname:port
127.0.0.1:8080
[::1]:8080
```

端口范围是 0..65535；IPv6 literal 必须用方括号。C API 的 `SZrNetworkEndpoint` 保存
`host[96]` 和 `port`，超长 host、缺失端口、负端口和多余冒号都会失败。loopback host 只
包括 `localhost`、`127.0.0.1` 和 `::1`，可用 `ListenerOpenLoopback`/`StreamConnectLoopback`
强制本机边界。

## TCP

### 脚本导出

| 导出 | 签名 | 语义 |
| --- | --- | --- |
| `listen` | `listen(host: string, port: int): TcpListener` | 绑定并监听；port=0 允许 OS 选择端口。 |
| `connect` | `connect(host: string, port: int, timeoutMs?: int): TcpStream` | 非阻塞连接；timeout 缺省为 infinite。 |
| `TcpListener.accept` | `accept(timeoutMs?: int): TcpStream/null` | 等待连接；超时返回 null。 |
| `TcpListener.close` | `(): null` | 关闭监听；幂等。 |
| `TcpListener.isClosed` | `(): bool` | 读取关闭状态。 |
| `TcpListener.host/port` | 属性 | 返回实际绑定 endpoint。 |
| `TcpStream.read` | `(maxBytes: int, timeoutMs?: int): string/null` | 读取最多 maxBytes；EOF/timeout 为 null。 |
| `TcpStream.write` | `(text: string): int` | 写 UTF-8 文本，返回实际字节数。 |
| `TcpStream.close/isClosed` | `(): null` / `(): bool` | 关闭和状态检查。 |
| `TcpStream.localHost/localPort` | 属性 | 本地 endpoint。 |
| `TcpStream.remoteHost/remotePort` | 属性 | 对端 endpoint。 |

```zr
let tcp = import("zr.network.tcp");
let listener = tcp.listen("127.0.0.1", 0);
let port = listener.port;
let client = tcp.connect("127.0.0.1", port, 1000);
let peer = listener.accept(1000);
client.write("ping");
let text = peer.read(64, 1000);
peer.close();
client.close();
listener.close();
```

`read(maxBytes)` 要求 maxBytes > 0 且不超过 provider frame/buffer 限制；部分读取是正常
行为，应用层需要自行拼包。句柄失效、connect 失败和系统 socket error 抛 network/runtime
exception，不返回负 errno。

### C TCP API

| 入口 | 说明 |
| --- | --- |
| `ZrNetwork_ParseEndpoint` | 文本转 `SZrNetworkEndpoint`，写 error buffer。 |
| `ZrNetwork_TcpListenerOpen` / `ListenerOpenLoopback` | 绑定监听。 |
| `ZrNetwork_ListenerAccept` | timeout 等待并写 `SZrNetworkStream`。 |
| `ZrNetwork_TcpStreamConnect` / `StreamConnectLoopback` | 非阻塞连接和超时。 |
| `ZrNetwork_StreamRead` / `StreamWrite` | 原始 bytes I/O，out length 报告实际数量。 |
| `ZrNetwork_ListenerClose` / `StreamClose` | 关闭并清空 native handle。 |

所有 out struct 在调用前应清零；失败时不要继续调用 close 以外的操作。close 可重复调用。

## framing

`ZrNetwork_StreamWriteFrame`/`ReadFrame` 使用 4-byte network-order length prefix，最大 frame
buffer 为 8192 bytes。它不是 TLS、压缩或消息认证协议；需要安全传输时在上层增加明确的
认证/加密层。

```text
uint32_be payload_length
payload bytes (UTF-8 text)
```

长度为 0 的 frame 合法；超过容量、short read、EOF 或 timeout 会关闭/标记 stream 并返回
失败。读取到的 payload 由调用方提供的 buffer 接收，函数额外写入 outLength。

## UDP

| 导出 | 签名 | 语义 |
| --- | --- | --- |
| `bind` | `bind(host: string, port: int): UdpSocket` | 绑定 datagram socket。 |
| `UdpSocket.send` | `(host: string, port: int, payload: string): int` | 发送一个 datagram。 |
| `UdpSocket.receive` | `(maxBytes: int, timeoutMs?: int): UdpPacket/null` | 接收一个 datagram；超时为 null。 |
| `UdpSocket.close/isClosed` | `(): null` / `(): bool` | 关闭和状态。 |
| 属性 | `host:string`、`port:int` | 实际绑定 endpoint。 |
| `UdpPacket` | `payload:string`、`host:string`、`port:int`、`length:int` | 接收数据和来源。 |

UDP 不保证顺序、可靠性或不重复；payload 超过 maxBytes 会按 provider 的截断/错误规则处理，
应用应读取 `length` 并设计重传/校验。

## timeout 和系统边界

内部 wait infinite 常量为 `0xFFFFFFFF`。脚本省略 timeout 时使用该值；显式 0 是否表示
立即轮询由 registry 定义，不能假定为无限。Windows 使用 Winsock，POSIX 使用 BSD socket；
provider 统一错误和关闭语义，但 DNS、IPv6 和权限差异仍可能导致平台不同结果。

## 安全清单

- 对外监听前显式选择 host，不要把 loopback helper 当公网绑定。
- frame length 和 UDP maxBytes 必须受限，避免把网络输入直接当数组容量。
- 关闭 stream/listener/socket 后清除脚本引用；finalizer 只是兜底。
- 不要把 socket native pointer 放入 `Ptr<T>` 或跨 isolated thread transfer。
- 将认证 token 与 endpoint 分离；`ZrNetwork_TokenMatches` 只做安全字符串比较，不提供
  密钥存储或加密。

## 注册入口

```c
const ZrLibModuleDescriptor *network = ZrVmLibNetwork_GetModuleDescriptor();
TZrBool ok = ZrVmLibNetwork_Register(global);
```

根 descriptor 注册 module links；TCP/UDP 叶子由同一个 global registry 管理。更完整的
native callback 和 handle 资源顺序见 [Native API](native-api.md)。
