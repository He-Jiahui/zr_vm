---
related_code:
  - zr_vm_lib_network/include/zr_vm_lib_network/module.h
  - zr_vm_lib_network/include/zr_vm_lib_network/network.h
  - zr_vm_lib_network/src/zr_vm_lib_network/module.c
  - zr_vm_lib_network/src/zr_vm_lib_network/module_support.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network_internal.h
implementation_files:
  - zr_vm_lib_network/src/zr_vm_lib_network/module.c
  - zr_vm_lib_network/src/zr_vm_lib_network/module_support.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/parser/test_compiler_regressions.c
  - tests/fixtures/projects/network_loopback/network_loopback.zrp
  - tests/fixtures/projects/network_loopback/src/main.zr
  - tests/fixtures/projects/network_import_root_probe/network_import_root_probe.zrp
  - tests/language_server/test_lsp_project_features.c
doc_type: module-detail
---

# `zr.network`

**状态：`experimental`。顶层 CMake 的 `BUILD_NETWORK_LIB` 默认开启；关闭后不构建此模块。
根模块及 TCP/UDP 叶模块的 descriptor 版本当前均为 `1.0.0`。**

`zr.network` 是仅链接 `zr.network.tcp` 和 `zr.network.udp` 的根模块。
`ZrVmLibNetwork_Register` 先把两个叶 descriptor 注册进同一 global registry，再注册根
descriptor；如果后续注册失败，函数返回失败，但没有回滚此前的注册。传输层直接调用 BSD
socket/Winsock 和 `select`，当前调用链没有使用仓库中的 libuv 副本。

## TCP

```zr
let net = import("zr.network");
let listener = net.tcp.listen("127.0.0.1", 0);
let client = net.tcp.connect("127.0.0.1", listener.port());
let server = listener.accept(3000);
client.write("ping");
let text = server.read(4, 3000);
server.close();
client.close();
listener.close();
```

`TcpListener` 方法是 `accept(timeoutMs)`、`close()`、`isClosed()`、`host()` 和 `port()`；模块函数
`listen(host, port)` 创建监听器。`TcpStream` 提供 `read(count, timeoutMs)`、`write(text)`、
`close()`、`isClosed()` 和四个本地/远端 endpoint 查询方法。连接函数
`connect(host, port, timeoutMs?)` 允许省略超时，默认值为 **5000 毫秒**。

| API | 精确调用形状 | 返回 |
| --- | --- | --- |
| TCP | `listen(host: string, port: int)`；`connect(host: string, port: int, timeoutMs?: int)` | `TcpListener` / `TcpStream`；连接失败抛异常 |
| `TcpListener` | `accept(timeoutMs: int)`；`close()`；`isClosed()`；`host()`；`port()` | `accept` 成功返回 `TcpStream`；超时、监听关闭或 socket 错误均返回 `null` |
| `TcpStream` | `read(count: int, timeoutMs: int)`；`write(text: string)`；`close()`；端点查询四方法 | `read` 成功返回字符串；超时、EOF、关闭或 socket 错误均返回 `null`；`write` 返回整数 |

`accept` 和 `read` 的回调虽预置了无限等待值，其方法 descriptor 仍要求显式超时参数。
`timeoutMs=0` 表示立即轮询，`4294967295` 表示无限等待。`read` 要求 `count>0`，底层一次
`recv` 最多读取请求的字节数，不负责读满。`null` 不能单独判定为超时或 EOF。`write("")`
返回 0，不表示 TCP 消息或 EOF。

## UDP

`UdpSocket` 通过 `bind(host, port)` 创建，提供 `send(host, port, payload)`、
`receive(maxBytes, timeoutMs)`、`close()`、`isClosed()`、`host()` 和 `port()`。`receive` 的
descriptor 要求显式超时参数。成功接收时 `UdpPacket` 有 `payload`、`host`、`port`、`length`
字段；`length` 是本次底层接收字节数。超时、关闭和 socket 错误都可能返回 `null`，不能把
`null` 只当作超时。发送成功只表示系统接受数据报，不保证到达、顺序或不重复。

| API | 精确调用形状 | 返回 |
| --- | --- | --- |
| UDP | `bind(host: string, port: int)` | `UdpSocket` |
| `UdpSocket` | `send(host: string, port: int, payload: string)`；`receive(maxBytes: int, timeoutMs: int)`；`close()`；`isClosed()`；`host()`；`port()` | `int`、`UdpPacket?`、`null`、`bool`、`string/int` |
| `UdpPacket` | `payload: string`；`host: string`；`port: int`；`length: int` | 创建时保存数据与来源；字段元数据未标为只读 |

`send(host, port, "")` 可以发出零字节数据报；当前实现把 `recvfrom` 成功返回的 0 判为失败，
所以脚本 `receive` 会返回 `null`，丢失这个报文。`UdpPacket` 的 descriptor 文案称其为
immutable snapshot，但字段描述符未标为只读；脚本赋值路径仍需测试。

## C 接口与资源生命周期

```c
const ZrLibModuleDescriptor *d = ZrVmLibNetwork_GetModuleDescriptor();
TZrBool ok = ZrVmLibNetwork_Register(global);
```

脚本入口接受 `0..65535` 端口、数值 IPv4/IPv6 或 `localhost`；空主机和 `localhost`
会解析为 `127.0.0.1`。普通 `listen`、`connect`、`bind`、`send` 不限制为回环地址。
只有显式调用 C 接口 `ZrNetwork_ListenerOpenLoopback` 或
`ZrNetwork_StreamConnectLoopback` 才检查回环主机。

脚本对象的私有字段保存 `calloc` 分配的 `ZrNetworkVmHandle`，其类型是**不参与 GC 的
native pointer**。显式 `close()` 会关闭系统 socket，可重复调用；当前没有 socket
finalizer。丢弃未关闭的对象不会自动关闭 socket；即使调用 `close()`，包装体堆内存也没有
释放路径。调用方应及时 `close()`，并将包装体泄漏视为现有 **BUG**。

## 已知实现问题

- **BUG：** `TcpStream.write` 和 `UdpSocket.send` 用 `strlen` 计算脚本字符串长度，嵌入 NUL
  后只发送前缀；`TcpStream.read` 与 `UdpPacket.payload` 按 C 字符串构造返回值，嵌入 NUL
  后内容被截短，而 `UdpPacket.length` 保留底层接收长度。
- **BUG：** TCP 分批发送已有前缀成功、后续失败时，底层 `outWritten` 仍为 0，脚本 `write`
  返回 0；调用方重试可能重复发送已写前缀。
- **BUG：** 已关闭的 UDP socket 调用 `send` 时，底层提前失败而不写错误缓冲区，脚本包装器
  却读取未初始化的错误文本；该异常内容不可靠。
- **BUG：** `accept`、`read`、`receive` 的 descriptor 参数下限与回调中的省略超时默认值
  不一致；省略参数在 native dispatch 阶段被拒绝。
- **BUG：** `UdpPacket` 类型文案声称 immutable snapshot，字段元数据却将四个字段标为
  可写。**TODO：** 仍需通过脚本赋值测试核实运行时实际是否允许修改这些字段。
- **BUG：** C 层 `ZrNetwork_StreamReadFrame` 在 32 位 `size_t` 上收到帧长
  `UINT32_MAX` 时，`frameLength+1` 回绕使容量检查放行超限帧；后续接收与写入的具体
  后果仍需在 32 位目标核实。**TODO：** 精确读取把剩余 `size_t` 长度转为 `int`
  传给 `recv`，大帧的目标平台行为需单独核查。

现有 `network_loopback` 运行时及项目回归测试覆盖普通 TCP/UDP 回环文本收发；LSP 测试
覆盖根模块链接与成员元数据。这些测试没有覆盖上述资源、空报文或嵌入 NUL 边界。
