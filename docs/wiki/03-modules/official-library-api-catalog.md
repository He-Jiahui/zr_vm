---
related_code:
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_math/include/zr_vm_lib_math/module.h
  - zr_vm_lib_network/include/zr_vm_lib_network/module.h
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/module.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_network/src/zr_vm_lib_network/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_task/src/zr_vm_lib_task/module.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/module.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍内置库接口和 C native 调用方案
  - docs/library-and-builtins/index.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
  - tests/container/test_container_runtime.c
  - tests/iterator/test_enumerator_protocol.c
  - tests/parser/test_value_type_runtime.c
  - tests/parser/test_aot_c_provider_shared_library_smoke.c
  - tests/task/test_task_runtime.c
  - tests/thread/test_thread_runtime.c
  - tests/testing/test_test_role_binding.c
  - tests/ffi/test_ffi_module.c
doc_type: api-catalog
---

# 官方内置库接口总目录

本页是标准库的“查找入口”，把当前 registry 可识别的 25 个官方 module identity、源级
接口分组、C 注册函数和实现边界放在同一处。每个领域仍有更细的 API 页面；这里的签名
用于选择模块和理解依赖，不替代 descriptor 中的参数数量、generic constraint、dispatch
flag 与 contract hash。

## 1. 模块 identity 与阶段

官方 inventory 是 resolver 的身份表，不是文件夹列表。名称、phase、tier 和 provider role
共同参与准入；第三方不能用相同 identity 覆盖官方 provider。

| Tier/phase | 官方 module | 主要职责 |
| --- | --- | --- |
| N0 / Runtime | `zr.builtin` | Object、TypeInfo、基础 equality/hash/index 协议和 primitive wrapper |
| N1 / Runtime | `zr.container` | Array、Map、Set、LinkedList、Span、Pool |
| N1 / Runtime | `zr.iteration` | Iterable、Enumerator、Iterator、AsyncIterator 协议 |
| N1 / Runtime | `zr.math` | scalar、Vector2/3/4、Complex、Quaternion、Matrix、Tensor |
| N1 / Runtime | `zr.task` | Task、Job、Scheduler、Channel、Shared、Atomic |
| N2 / Runtime | `zr.debug` | debug agent、coverage、profile、evaluation policy |
| N2 / Runtime | `zr.ffi` | native extern、Library/Symbol/Pointer/Buffer、callback |
| N2 / Runtime | `zr.network` | endpoint 根模块和 TCP/UDP link |
| N2 / Runtime | `zr.network.tcp` | listener、stream、framing |
| N2 / Runtime | `zr.network.udp` | datagram socket、packet |
| N2 / Runtime | `zr.pooling` | stable slab、generation handle、borrow guard |
| N2 / Runtime | `zr.reflection` | TypeId、metadata token、member/constructor query |
| N2 / Runtime | `zr.system` | system 根 module link |
| N2 / Runtime | `zr.system.assembly` | assembly resource 查询 |
| N2 / Runtime | `zr.system.console` | stdout/stderr/input |
| N2 / Runtime | `zr.system.env` | 环境变量 |
| N2 / Runtime | `zr.system.exception` | exception 类型和 unhandled handler |
| N2 / Runtime | `zr.system.fs` | path、File、Folder、FileStream |
| N2 / Runtime | `zr.system.gc` | GC 控制和统计 |
| N2 / Runtime | `zr.system.process` | arguments、sleep、exit |
| N2 / Runtime | `zr.system.vm` | loaded module、state 和 export 查询 |
| N2 / Runtime | `zr.thread` | worker scheduler、Send/Sync、mutex、transfer |
| N3 / CompileTool | `zr.compile` | build feature、compile-time diagnostics、conditional call-elision |
| N3 / CompileTool | `zr.compile.declaration` | immutable declaration view、typed Patch |
| N3 / Test | `zr.testing` | assertion、equal、throws、TestManifest |

`Runtime` provider 可被其它 phase 消费；`Test` 和 `CompileTool` 只有同 phase host 才可
消费。普通 runtime `import` 不会 materialize 后两者，详见 [Provider 矩阵](provider-matrix.md)
和 [Native 插件加载](../05-interop/native-plugin-loading.md)。

## 2. 导入和模块对象

源级导入返回一个 managed module object：

```zr
let container = import("zr.container");
let values = init container.Array<int>();
values.add(1);

let fs = import("zr.system.fs");
let file = init fs.File("data.txt");
```

根模块的字段是 module link，不是复制的子模块。`import("zr.network")` 与直接导入
`zr.network.tcp` 共享 registry 中的 module identity 和 handle；模块 cache 命中时不会创建
第二份 socket/type table。`zr.compile*` 的导入只在 compiler-owned scope 有效，不能把
compile-only module 存进 runtime object。

模块对象还导出隐藏但可反射查询的 `__module_info` metadata。它包括 module name、source
kind/path、registration kind、ABI、version、required capabilities、public contract hash
和 provider phase。宿主应通过 `zr.system.vm.loadedModules()` 或 C registry API 读取，不要
猜测 hidden field 的布局。

## 3. 基础协议和对象模型

`zr.builtin` 是所有其它 provider 的前置依赖。稳定源级 surface 包括：

| 类型/成员 | 典型签名 | 说明 |
| --- | --- | --- |
| `Object.type` | `type(value: object): string` | 返回显示类别标签，不是 TypeId |
| `Object.box` | `box(value: object): Object` | 把 primitive 包装为 managed wrapper |
| `TypeInfo` | `name`、`qualifiedName`、`kind`、`hash`、`owner`、`module` | reflection 的受限投影 |
| `IArrayLike<T>` | `length:int`、GET/SET item | 索引协议；不保证连续存储 |
| `IEquatable<T>` | `equals(other:T): bool` | 语义相等 |
| `IHashable` | `hashCode(): int` | Map/Set 的稳定 hash |
| `IComparable<T>` | `compareTo(other:T): int` | 只看负/零/正符号 |
| `IComparer<T>` | `compare(left:T,right:T): int` | 显式比较策略 |

Map/Set key 必须同时保持 equality/hash 一致；修改参与 hash 的字段前应先从集合移除。
`Object.box` 的 wrapper 使用隐藏 value storage，但 wrapper 内部字段不是 public contract。
更多协议、canonical role 和 C wrapper 调用见 [Builtin API](builtin-api.md)。

## 4. 容器、迭代和池化

### 4.1 容器

| 类型 | 核心接口 | 复杂度/边界 |
| --- | --- | --- |
| `Array<T>` | `add`、`insert`、`removeAt`、`clear`、`span`、index GET/SET | 随机访问摊销 O(1)，插入删除 O(n) |
| `Map<K,V>` | `containsKey`、GET/SET、`remove`、`getIterator` | hash + equality；修改 entries 使 iterator/cache 失效 |
| `Set<T>` | `add`、`contains`、`remove`、`getIterator` | add/remove 返回是否改变集合 |
| `LinkedList<T>` | `addFirst/Last`、`removeFirst/Last`、节点 iterator | 首尾 O(1)，按值查找 O(n) |
| `Span<T>` | `slice`、index GET/SET、`asReadOnly` | 借用连续 view，backing storage 不能搬迁 |
| `ReadOnlySpan<T>` | `slice`、index GET | 只读借用 view |

```zr
let map = init zr.container.Map<string, int>();
map["attempts"] = 1;
let list = init zr.container.LinkedList<string>();
list.addLast("parse");
for (let step in list) {
    import("zr.system.console").printLine(step);
}
```

### 4.2 迭代协议

`for (let item in value)` 先查找 `getEnumerator`，再循环调用 `moveNext` 和读取 `current`。
同步 `Enumerator<T>` 与异步 `AsyncIterator<T>` 不隐式转换；异步版本的
`moveNext(): zr.task.Task<bool>` 必须在允许挂起的 frame 中使用。集合改动可能使 cursor
失效，provider 应返回明确 iterator error，而不是继续读旧地址。

### 4.3 stable pool

`Pool<T>` 的 handle 是 `(poolId, slotIndex, generation)`，不是 C 指针。`tryRead` 可并存，
`tryBorrow` 与其它写 guard 互斥；`recycle` 会使旧 generation 立即失效。跨 GC、reload 或
pool destroy 使用 handle 前必须重新 `isLive/Validate`。详见 [Container API](container-api.md)
和 [Reflection/Pooling API](reflection-pooling-api.md)。

## 5. 数学接口

`zr.math` 的标量和小型几何值倾向 value semantics；Tensor 是 managed object。常用 surface：

| 分组 | 接口 |
| --- | --- |
| 标量 | `abs`、`min/max`、`clamp`、`lerp`、`sqrt/rsqrt`、`pow`、`exp/log`、三角函数、`almostEqual` |
| Vector2/3/4 | `length`、`normalized`、`dot`、`distance`、`lerp`；Vector3 另有 `cross` |
| Complex | `real/imag`、`magnitude`、`phase`、`conjugate`、`normalized` |
| Quaternion | `length`、`inverse`、`dot`、`mul`、`slerp` |
| Matrix3x3/4x4 | `identity`、`transpose`、`determinant`、`inverse`、`mulVector`、`mulMatrix` |
| Tensor | `shape/rank/size`、`get/set`、`reshape`、`fill`、`sum/mean`、`matmul`、`toArray` |

向量/矩阵 meta 运算通常返回新值；Tensor 需要检查 shape、rank、index 和内存分配。NaN、
接近零长度和奇异矩阵都应显式处理，不能假定 C math 的 errno 会自动成为 ZR exception。
精确签名与数值边界见 [Math API](math-api.md)。

## 6. Task、Thread 和异步资源

### 6.1 Task

| 类型/函数 | 典型签名 | 语义 |
| --- | --- | --- |
| `Task<T>` | `result():T`、`isCompleted():bool` | 结果/状态 handle |
| `Job<T>` | `Job(fn()->T)` | cold work description，不自动执行 |
| `Scheduler` | `schedule(job):Task<T>`、`pump()`、`step()` | 驱动队列 |
| `spawn` | `spawn(fn): Async` | 当前 scheduler 排队 |
| `await` | `await(handle): value` | 在允许挂起的 frame 中恢复 |
| `Channel<T>` | `send`、`recv`、`close` | 空队列/EOF 返回 null，close 后 send 失败 |
| `Shared<T>` | `load/store/clone/downgrade/release` | 引用计数共享，不等于 transaction |
| `Atomic*` | `load/store/compareExchange/fetchAdd` | 单操作原子性 |

`async fn` 必须显式返回 `zr.task.Task<T>`；活动 `ref`、`scoped` loan、thread-affine lock
不能跨 `await`。canonical bridge 的 C 调用顺序是 `Prepare -> Execute -> Complete/Fault ->
Release`，见 [Task API](task-api.md)。

### 6.2 Thread

`zr.thread` 提供 `ThreadScheduler`、`UniqueMutex`、`SharedMutex`、`Lock`、`SharedLock` 和
attached/isolated domain 策略。Send 表示可移动 ownership，Sync 表示可通过明确协议共享；
open socket/file、native pointer、callback 和活动 borrow 默认不可 transfer。isolated domain
还受 maxObjects/maxBytes/maxDepth quota 约束。详见 [Thread API](thread-api.md)。

## 7. System 和网络

### 7.1 System 叶子模块

| module | 稳定接口 | 错误/资源规则 |
| --- | --- | --- |
| `zr.system.console` | `print/printLine/printError/read/readLine` | stdout/stderr 分离；EOF 为 null |
| `zr.system.env` | `getVariable(name):string/null` | 不把缺失变量当空字符串 |
| `zr.system.process` | `arguments`、`sleepMilliseconds`、`exit` | exit 是不可恢复控制转移 |
| `zr.system.assembly` | `resourceExists`、`readResourceText/Bytes` | 缺失/解码失败为 IOException |
| `zr.system.fs` | path helper、`File`、`Folder`、`FileStream` | using/close 管理 native handle |
| `zr.system.gc` | `enable/disable/collect/set_heap_limit/get_stats` | 可能触发 safepoint |
| `zr.system.vm` | `loadedModules/state/callModuleExport` | 只读 snapshot，不暴露内部指针 |
| `zr.system.exception` | Error 类型、unhandled handler | handler 自身抛错进入默认策略 |

`FileStream`、网络 socket 和 debug agent 都是 resource；推荐 `using`/finally 显式关闭，
不要依赖 finalizer 作为唯一释放路径。完整文件字段、mode、seek 和异常见
[System API](system-api.md)。

### 7.2 Network

| module | 接口 |
| --- | --- |
| `zr.network.tcp` | `listen`、`connect`、`accept`、`read`、`write`、`close` |
| `zr.network.udp` | `bind`、`send`、`receive`、`close` |
| `zr.network` | 根 module link、endpoint helper、framing |

endpoint 支持 `host:port` 和 `[IPv6]:port`，端口为 0..65535。TCP `read`/accept 超时通常为
null，系统错误抛 network exception；UDP 不保证顺序、可靠性或不重复。frame 使用 4-byte
network-order length prefix，最大 frame 受 provider buffer 限制，不是 TLS 或认证协议。详见
[Network API](network-api.md)。

## 8. FFI、Debug 和 Testing

### 8.1 FFI

`zr.ffi` 把 foreign symbol 分成可验证的 contract：

```zr
let ffi = import("zr.ffi");
let library = ffi.loadLibrary("sample");
let symbol = library.getSymbol(
    "add",
    {return: "i32", parameters: ["i32", "i32"]}
);
let sum = symbol.call([20, 22]);
symbol.close();
library.close();
```

Library/Symbol/Pointer/Buffer/Callback 都有 close 或 owner 关系；string/array/struct
marshal 会建立临时 storage、pin/root、调用和 out 写回事务。signature、calling convention、
alignment 或 foreign thread 不匹配时 fail closed。见 [FFI API](ffi-api.md) 和
[FFI Contract](../05-interop/ffi-contract.md)。

### 8.2 Debug/Testing

Debug agent 提供 breakpoints、continue/step、stack/scopes、bounded evaluate、coverage 和
sampling profile。evaluate 默认拒绝写入、阻塞、文件/网络和线程切换；需要 capability 时
显式请求并检查 effect policy。

Testing provider 只在 Test phase 可 materialize：

```zr
#zr.testing.test#
fn add_case(): void {
    zr.testing.equal(20 + 22, 42);
}
```

`assert`、`equal<T>` 和 `throws<E>` 生成 typed TestManifest；failure snapshot 可能被固定
buffer 截断，必须检查 `truncated`。详见 [Debug/Testing API](debug-testing-api.md)。

## 9. CompileTool provider

`zr.compile` 与 `zr.compile.declaration` 不是 runtime 标准库。它们只能在 compiler-owned
scope 查询 build feature、发出 compile diagnostic、读取 immutable declaration view 和
生成 typed Patch。依赖来自 `.zrm` compile-tool entry，必须验证 package hash、entry hash、
phase 和 public contract；runtime import 不会加载它们。具体阶段、effect、预算和缓存见
[Compile API](compile.md) 与 [Compile Declaration API](compile-declaration.md)。

## 10. C 注册入口速查

静态宿主先完成 core/global 初始化，再按依赖顺序注册 provider。以下函数名来自各自 public
`module.h`：

| provider | descriptor getter | register |
| --- | --- | --- |
| builtin | `ZrLibrary_BuiltinModule_GetDescriptor` (internal/public declaration used by registry) | `ZrLibrary_NativeRegistry_Attach` automatically registers builtin and reflection contracts |
| container | `ZrVmLibContainer_GetModuleDescriptor` | `ZrVmLibContainer_Register` |
| iteration | `ZrVmLibIteration_GetModuleDescriptor` | `ZrVmLibIteration_Register` |
| math | `ZrVmLibMath_GetModuleDescriptor` | `ZrVmLibMath_Register` |
| network | `ZrVmLibNetwork_GetModuleDescriptor` | `ZrVmLibNetwork_Register` |
| system | `ZrVmLibSystem_GetModuleDescriptor` | `ZrVmLibSystem_Register` |
| task | `ZrVmTask_GetModuleDescriptor` | `ZrVmTask_Register` |
| thread | `ZrVmThread_GetModuleDescriptor` | `ZrVmThread_Register` |
| ffi | `ZrVmLibFfi_GetModuleDescriptor` | `ZrVmLibFfi_Register` |
| debug | `ZrVmLibDebug_GetModuleDescriptor` | `ZrVmLibDebug_Register` / `ZrVmLibDebug_RegisterSandboxed` |
| testing | `ZrVmLibTesting_GetModuleDescriptor` | `ZrVmLibTesting_Register` |

示例：

```c
if (!ZrLibrary_NativeRegistry_Attach(global) ||
    !ZrVmLibContainer_Register(global) ||
    !ZrVmLibIteration_Register(global) ||
    !ZrVmLibMath_Register(global) ||
    !ZrVmLibSystem_Register(global) ||
    !ZrVmTask_Register(global)) {
    const TZrChar *message =
        ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
    host_log("provider registration failed", message);
    return ZR_FALSE;
}
```

真实宿主应按构建配置决定是否注册 network/debug/thread/testing，并在每次失败后立即读取
registry 错误；不要继续注册依赖失败 provider。共享库发行时由 loader 调用统一入口
`ZrVm_GetNativeModule_v1()`，静态 getter 名称不会成为动态 ABI。

## 11. descriptor 到源级接口的映射

每个公开函数最终来自 `ZrLibFunctionDescriptor`/`ZrLibMethodDescriptor`：

```text
ZR name + generic args + passing modes
    -> descriptor name/min/max/parameter descriptors
    -> canonical signature + metadata token
    -> call-binding contract (module hash + signature hash + layout)
    -> native callback or VM/AOT target
```

因此更改下列任一项都属于 contract 变化：函数名、参数数量、`in/out/ref` mode、返回类型、
generic constraint、dispatch flag、字段 layout、canonical protocol role、module link 或
public contract hash。只改 documentation 不改变调用绑定；只改 C callback 的内部实现也不应
改变 hash，但必须保持 arity、异常和 result write-back 语义。

## 12. 错误和兼容策略

| 层 | 代表错误 | 调用方动作 |
| --- | --- | --- |
| import | module not found、phase mismatch | 读取 module-load diagnostic；不要创建伪造 module |
| descriptor | ABI/version/capability/role mismatch | 拒绝注册，修正 provider 或 runtime 版本 |
| call | arity/type/layout/signature mismatch | 不执行 callback，回退 checked path 或报告错误 |
| resource | closed/stale/busy | 释放/重新获取 owner；不能重用裸地址 |
| concurrency | Send/Sync/transfer quota | 复制或显式 transfer；保留原 task 状态 |
| testing/debug | policy denied、snapshot truncated | 检查 capability/truncated，不能猜完整文本 |

标准库页面中的 `null` 是 ZR value；C API 的 `ZR_NULL` 是指针 sentinel。两者不能互换。
所有跨 state、跨 thread、跨 reload 的 object、descriptor、token、span 和 diagnostic 都
必须经过对应的 copy/root/pin/generation API。需要实现自定义 native module 时，继续阅读
[Native Module 编写](../05-interop/native-module-authoring.md)、[C 值与 GC 生命周期](../05-interop/c-api-value-lifecycle.md)
和 [Native 插件加载](../05-interop/native-plugin-loading.md)。
