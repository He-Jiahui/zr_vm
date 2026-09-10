---
related_code:
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/module.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_parser/include/zr_vm_parser/ffi_contract.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
implementation_files:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/10-native-ffi-module-package/m2-v2-declarations.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/ffi/ffi_fixture.c
doc_type: api-reference
---

# `zr.ffi` Runtime Handle 与 ABI 参考

`zr.ffi` 提供动态库、符号、callback、pointer 与 native buffer 的受约束运行时接口。它与
`native extern` 共享 FFI contract 原则，但路径不同：`native extern` 在编译/产物阶段固化
`SZrNativeImportContract`；`zr.ffi` 在运行时从显式 signature/contract 构建 typed handle。
两条路径都不能从裸地址、参数数量或 C 头文件文本猜测 ABI。

Source declaration 的 grammar 与 contract 生成见[模块导入与 Native FFI](../02-language/module-import-native-ffi-reference.md)。
本页面向脚本使用者和 C 宿主/模块作者，强调 handle 生命周期、pin 和 callback 边界。

## 导入与能力前提

```zr
let ffi = import("zr.ffi");
```

`zr.ffi` native descriptor 声明了 type hints/type metadata/safe call helper/FFI runtime 等
能力要求。registry/provider phase 或目标构建缺少这些能力时，module 不应假装可用。callback
还依赖当前构建是否有可用 libffi backend；缺失时应报告 ABI/marshalling 错误，而不是创建一个
不可调用的 `CallbackHandle`。

## API 总表

| 分类 | 接口 | 签名/行为摘要 |
| --- | --- | --- |
| 模块函数 | `loadLibrary(path)` | 返回 `LibraryHandle`；按路径打开动态库。 |
|  | `callback(signature, fn)` | 返回 C-callable `CallbackHandle`；signature 和 closure 都要通过验证。 |
|  | `sizeof(type)` / `alignof(type)` | 查询能被 FFI lowering 的 runtime type descriptor。 |
|  | `nullPointer(type)` | 创建带 target type 的 null pointer wrapper。 |
| LibraryHandle | `close()` / `isClosed()` | 关闭库；`close` 可重复调用。 |
|  | `getSymbol(name, signature)` | 解析动态符号，编译 typed `SymbolHandle`。 |
|  | `getContractSymbol(contract)` | 以 retained static native-import contract 解析符号。 |
|  | `getVersion([versionSymbol])` | 读取库导出的版本字符串。 |
| SymbolHandle | `call(args)` / `symbol(...)` | 用 array 或 positional meta-call 执行已编译 ABI call。 |
| CallbackHandle | `close()` | 释放/禁用 callback trampoline；可重复。 |
| PointerHandle/`Ptr<T>` | `as(type)`、`read(type)`、`close()`、`span()`、`[]` | 有类型的 native address/view；访问仍受 pin/owner/ABI 约束。 |
| BufferHandle | `allocate(size)`、`pin()`、`read`、`write`、`slice`、`close()` | managed native byte buffer，明确 pin 后获得 pointer view。 |

上述方法由真实 `ZrLibModuleDescriptor` 中的函数/type/method/meta-method表发布；参数名称
和具体 signature object 的字段应以当前 provider 的 type hints 与源实现为准。

## Handle 生命周期

```text
loadLibrary
  -> LibraryHandle (open)
      -> getSymbol / getContractSymbol
          -> SymbolHandle (compiled signature + native entry)
              -> call / meta-call
  -> close LibraryHandle
      -> future lookup/use must fail according to handle contract

callback(signature, closure)
  -> CallbackHandle (trampoline + VM rooted closure)
  -> native code may call only while policy/lifetime permits
  -> close CallbackHandle
```

`LibraryHandle`、`SymbolHandle` 和 `CallbackHandle` 都是 managed wrapper，但“受 GC 管理”不等于
“任意时刻都可调用”。显式 `close()` 使资源生命周期可预测；close 之后的 lookup/call 应被
拒绝。Library close 与现有 SymbolHandle 的关系要由 runtime handle data/contract 处理，不能
在 native code 中缓存 symbol pointer 并假定库仍常驻。

推荐脚本结构：

```zr
let ffi = import("zr.ffi");
let library = ffi.loadLibrary("native/codec");
try {
    let symbol = library.getSymbol("codec_version", versionSignature);
    let version = symbol();
    use(version);
} finally {
    library.close();
}
```

此例依赖 `versionSignature` 是当前 provider 可接受的显式 ABI signature。不要把字符串
`"i32()"` 或对象 literal 的随意 shape 视为跨版本标准；动态 call 在 contract validation
前不可执行。

## Static contract 与动态 signature

| 情形 | 正确入口 | 最重要的验证 |
| --- | --- | --- |
| 源代码固定声明一个 C 函数 | `native extern` | parser/semantic build 的 `SZrNativeImportContract`、ABI/hash/layout。 |
| 已有 retained static contract，运行时取符号 | `LibraryHandle.getContractSymbol(contract)` | contract 与当前 library/target ABI 仍匹配。 |
| 运行时用户选择库/符号 | `getSymbol(name, signature)` | signature object 合法、符号存在、library open。 |
| C 回调 ZR closure | `ffi.callback(signature, fn)` | callback signature、lifetime/thread policy、VM state/root。 |

`ZrVmLibFfi_ValidateNativeImportContract` 是 runtime module 暴露的 C 验证辅助函数，可在
尝试 `getContractSymbol` 前把 contract 错误写入 caller buffer。它不能替代 parser 的
source-aware `ZrParser_FfiContract_Build`，后者才能给出 parameter index 和 ZR source range。

## Callback：跨 ABI 回到 ZR

```zr
let ffi = import("zr.ffi");
let callback = ffi.callback(unarySignature, fn(value: f64): f64 => value * 2.0);
try {
    nativeApi.apply(callback);
} finally {
    callback.close();
}
```

callback runtime 要同时维护 C closure/trampoline、signature、拥有它的 ZR state、owner thread
identity、closure value root、last error 和关闭状态。每次 native-to-ZR 回调大致经过：

```text
C ABI arguments
  -> validate callback still open and invoked on allowed context
  -> marshal into ZR values
  -> enter/save VM call stack state
  -> invoke closure
  -> verify stack/call-info integrity
  -> marshal return value or write zero storage on failure
  -> record error for caller/handle diagnostic
```

callback 失败不能把未初始化 C return storage 交给 native caller；当前实现会清零 call storage
并记录 native-call/marshal error。C 库若保存 callback 到调用返回之后，必须使用与其存储行为
匹配的 lifetime policy；`call`-scoped callback 不能被持久保存。关闭 callback 后再回调是错误，
不是“自动重新创建 closure”。

特别注意线程：callback 绑定到创建它的 VM state/owner context。跨线程、跨 isolated GC domain
调用需要明确的 dispatch/transport 策略；不要把 `CallbackHandle` 的 native code pointer 传到
任意线程并直接进入同一 VM。

## Pointer、pin 与 BufferHandle

```zr
let buffer = ffi.BufferHandle.allocate(256);
try {
    let pointer = buffer.pin();
    try {
        nativeWrite(pointer);
    } finally {
        pointer.close();
    }
} finally {
    buffer.close();
}
```

`PointerHandle`/`Ptr<T>` 是 ABI-aware wrapper，不是语言中的任意整数地址。它携带类型/owner
事实，并提供 `as`、`read`、index/meta access 与 `span` 等受控入口；每个操作仍需验证 handle
未关闭、pointer 可访问、目标 type 支持 lowering、边界/对齐足够。

`BufferHandle` 是 managed native byte buffer。`pin()` 产生 pointer view，避免 GC/移动/生命周期
不明时把对象内部地址传给 C。Pin 不授权无限期保存地址：native call 完成或 view close 后，
地址的有效性由 pointer/buffer contract 决定。`span()` 是明确的连续视图；若在 native callback
中保留 span/inline argument view，遇到 safepoint 或 stack relocation 后必须重新获取。

| 错误做法 | 为什么错误 | 正确替代 |
| --- | --- | --- |
| 将普通 ZR object 强转为 `Ptr<T>` | 没有 pin/layout/owner contract | 用 BufferHandle/正式 pointer API。 |
| 保存 `pin()` 返回地址到下一帧/线程 | view 生命周期可能结束或 domain 不同 | 复制数据或建立明确 owned native allocation。 |
| close buffer 后读 pointer | underlying allocation 已不可用 | 先完成所有 pointer 操作，再关闭 buffer。 |
| 用 `as()` 逃避 ABI type 检查 | reinterpret 不创建正确 layout/ownership | 仅对 contract 允许的 ABI-compatible target 使用。 |

## Native module/C API 对接

`zr.ffi` 自己通过 `ZrVmLibFfi_Register(global)` 注册 descriptor。宿主若嵌入 runtime：

```c
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_ffi/runtime.h"

if (!ZrVmLibFfi_Register(global)) {
    return ZR_FALSE;
}

if (!ZrVmLibFfi_ValidateNativeImportContract(contract, errorBuffer, sizeof(errorBuffer))) {
    /* errorBuffer is caller-owned; do not call through the contract. */
    return ZR_FALSE;
}
```

native module callback 处理 FFI value 时应使用 `ZrLibCallContext` typed accessors 和
`ZrLib_TempValueRoot_*`，不要自行解引用 `argumentValues`、私有 handle struct 或 raw pointer。
在一次 callback 中创建/保存 managed object 跨越潜在 GC 时，root 它；从 context 获得的 inline
argument span/view 是借用的，需要在 safepoint/stack growth 后重新获取。

详细 C 回调模板见[Native callback 配方](../05-interop/native-callback-recipes.md)，static native
extern ABI 的 contract 字段见[FFI 契约](../05-interop/ffi-contract.md)。

## 错误模型与排障

| 症状 | 可能层 | 首先检查 |
| --- | --- | --- |
| `loadLibrary` 失败 | path/权限/loader | 目标文件、平台 loader 依赖、handle error。 |
| `getSymbol` 失败 | symbol/signature | 名称导出、library open、signature ABI。 |
| contract symbol 被拒绝 | static contract | schema、hash、target ABI、library locator。 |
| callback 创建失败 | libffi/signature/policy | backend 是否启用、callback type/lifetime、thread policy。 |
| native callback 返回零值 | ZR invoke/marshal/VM integrity | callback last error、exception、stack preservation。 |
| pointer read/write 失败 | pin/owner/bounds/type | handle closed、buffer state、type lowering、index。 |
| reload 后 old handle 异常 | generation/library lifetime | 重新获取 contract/symbol，不复用旧 native entry。 |

在生产部署中，动态 FFI 错误不应靠捕获 message 字符串决定分支。优先使用稳定 status/contract
诊断，再把 message 用于日志和用户提示。
