---
related_code:
  - zr_vm_lib_network/include/zr_vm_lib_network/module.h
  - zr_vm_lib_network/src/zr_vm_lib_network/module.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network_internal.h
implementation_files:
  - zr_vm_lib_network/src/zr_vm_lib_network/module.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/fixtures/projects/network_loopback/network_loopback.zrp
  - tests/fixtures/projects/network_import_root_probe/network_import_root_probe.zrp
doc_type: module-detail
---

# `zr.network`

**状态：`experimental`；仅在 `BUILD_NETWORK_LIB=ON` 时提供。根 descriptor 与 TCP/UDP
叶子 descriptor 当前均为 `1.0.0`，未发布独立 public contract hash；接口可能随 provider
权限模型调整。**

网络 provider 在 CMake 选项 `BUILD_NETWORK_LIB=ON` 时构建。根模块只聚合两个叶子：
`zr.network.tcp` 和 `zr.network.udp`。底层 socket 句柄封装在 GC 对象的 finalizer payload
中，脚本不能读取或伪造操作系统句柄。

## TCP

```zr
let net = import("zr.network");
let listener = net.tcp.listen("127.0.0.1", 9000);
let stream = net.tcp.connect("127.0.0.1", 9000, 1000);
stream.write("ping");
let bytes = stream.read(4, 1000);
stream.close();
let accepted = listener.accept(1000);
if (accepted != null) { accepted.close(); }
listener.close();
```

`TcpListener` 方法是 `accept(timeoutMs)`、`close()`、`isClosed()`、`host()` 和 `port()`；模块函数
`listen(host, port)` 创建监听器。`TcpStream` 提供 `read(count, timeoutMs)`、`write(text)`、
`close()`、`isClosed()`、`localHost()`、`localPort()`、`remoteHost()` 和 `remotePort()`。连接函数
`connect(host, port, timeoutMs?)` 支持超时；超时、EOF、短写和关闭都通过网络异常类型
以 `null`/异常区分，不能用空字符串推断 EOF。`read(count, timeoutMs)` 的两个参数都必须提供，
`timeoutMs` 可使用无穷等待约定；`accept(timeoutMs)` 同样在超时或没有连接时返回 `null`。

| API | 精确调用形状 | 返回 |
| --- | --- | --- |
| TCP | `listen(host: string, port: int)`；`connect(host: string, port: int, timeoutMs?: int)` | `TcpListener` / `TcpStream` |
| `TcpListener` | `accept(timeoutMs: int)`；`close()`；`isClosed()`；`host()`；`port()` | `TcpStream?`、`null`、`bool`、`string`、`int` |
| `TcpStream` | `read(count: int, timeoutMs: int)`；`write(text: string)`；`close()`；端点查询四方法 | `string?`、写入 `int`、`null`、`bool`、端点 `string/int` |

## UDP

`UdpSocket` 通过 `bind(host, port)` 创建，提供 `send(host, port, payload)`、
`receive(maxBytes, timeoutMs)`、`close()`、`isClosed()`、`host()` 和 `port()`。`UdpPacket` 是收到的
地址、端口和 payload 快照；receive 超时返回 `null`，协议错误抛异常。发送不保证到达，
应用层必须自行处理重试和序号。

| API | 精确调用形状 | 返回 |
| --- | --- | --- |
| UDP | `bind(host: string, port: int)` | `UdpSocket` |
| `UdpSocket` | `send(host: string, port: int, payload: string)`；`receive(maxBytes: int, timeoutMs: int)`；`close()`；`isClosed()`；`host()`；`port()` | `int`、`UdpPacket?`、`null`、`bool`、`string/int` |
| `UdpPacket` | `payload: string`；`host: string`；`port: int`；`length: int` | 只读数据快照 |

当前实现公开 loopback 安全边界：`ZrNetwork_Endpoint_IsLoopbackHost` 拒绝非 loopback
地址用于受限测试入口。生产宿主可以提供更高权限的 provider，但应注册不同 contract hash，
不能悄悄改变官方 descriptor 的安全语义。

## C 接口与资源生命周期

```c
const ZrLibModuleDescriptor *d = ZrVmLibNetwork_GetModuleDescriptor();
TZrBool ok = ZrVmLibNetwork_Register(global);
```

注册函数依次注册 TCP、UDP 叶子和根 descriptor；任一叶子失败则整体失败。每个对象的
close 是幂等的，GC finalizer 只在未关闭时关闭 socket。网络 callback 不得跨 `await` 保存
`ZrLibCallContext` 或其 argument view；需要长期操作时应复制数据到 GC 对象。
