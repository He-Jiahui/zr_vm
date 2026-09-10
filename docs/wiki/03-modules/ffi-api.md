---
related_code:
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/module.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
implementation_files:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/ffi/ffi_fixture.c
doc_type: api-reference
---

# `zr.ffi` API 参考

FFI 分成两种入口：静态 `native extern` contract 和运行时 `zr.ffi` 动态句柄。两者都要求
显式类型、调用约定和 owner mode；系统不会从裸地址、C 头文件或参数数量猜测 ABI。没有
可用 libffi backend 时，callback 创建会报告 ABI mismatch，而不是生成一个看似可调用的
句柄。

## 静态 native extern

```zr
native extern("libsample") {
    fn add(left: i32, right: i32): i32;
    fn fill(buffer: ref u8, length: u64): void;
}
```

编译器把 declaration 投影为 `SZrNativeImportContract`，记录 library literal、symbol、参数
passing mode、返回类型、calling convention、指针/struct layout 和 source span。`ref/out`
参数必须绑定到合法 place；variadic、未声明的 union layout、隐式 string encoding 和任意
function pointer 都会被拒绝或要求显式 contract。

`native extern` 的 library 名称是编译期 literal；运行时变量应使用 `loadLibrary`，但仍需
提供 signature object。

## 模块函数

| 函数 | 签名 | 失败边界 |
| --- | --- | --- |
| `loadLibrary` | `(path: string): LibraryHandle` | 动态库不存在、权限或 ABI 加载失败。 |
| `callback` | `(signature: object, fn: function): CallbackHandle` | signature 无效、backend 不支持或线程策略不符。 |
| `sizeof` | `(type: object): int` | type 无有效 FFI lowering。 |
| `alignof` | `(type: object): int` | type 无有效 layout。 |
| `nullPointer` | `(type: object): Ptr<void>` | type 不是合法 pointer target。 |

## LibraryHandle 和 SymbolHandle

| 类型 | 方法 | 说明 |
| --- | --- | --- |
| `LibraryHandle` | `close(): null`、`isClosed(): bool` | close 幂等；关闭后 symbol lookup/call 失败。 |
| `LibraryHandle` | `getSymbol(name:string, signature:object): SymbolHandle` | 解析 symbol 并绑定 ABI signature。 |
| `LibraryHandle` | `getContractSymbol(contract:object): SymbolHandle` | 使用 retained native import contract。 |
| `LibraryHandle` | `getVersion(versionSymbol?: string): string` | 读取可选版本导出；不存在时按 provider 规则返回。 |
| `SymbolHandle` | `call(args:array): value` | 按 signature marshal、调用、反序列化结果。 |
| `SymbolHandle` | meta-call `symbol(...)` | positional 参数的受约束快捷形式。 |

```zr
let ffi = import("zr.ffi");
let lib = ffi.loadLibrary("libsample");
let sig = { return: "i32", parameters: ["i32", "i32"] };
let add = lib.getSymbol("add", sig);
let result = add.call([20, 22]);
lib.close();
```

`signature` object 的字段由 FFI contract schema 定义；未知字段、重复参数、长度不匹配和
不支持的 type 会在 lookup 前失败。SymbolHandle 持有 library 引用，必须先关闭 symbol/
callback，再关闭 library。

## PointerHandle、Ptr<T> 和 BufferHandle

| 类型 | 方法/字段 | owner 语义 |
| --- | --- | --- |
| `PointerHandle` | `length:int`、`as(type)`、`read(type)`、`span()`、索引读写、`close()` | 默认 borrowed；close 后不可读。 |
| `Ptr<T>` / `Ptr32<T>` / `Ptr64<T>` | `span(): Span<T>` | 只描述目标类型和宽度，不自动拥有内存。 |
| `BufferHandle` | `allocate(size)`、`close`、`pin`、`read`、`write`、`slice` | FFI owner mode `owned`；slice 受 parent 生命周期约束。 |
| `Char` / `WChar` | struct wrapper | 显式窄/宽字符，不隐式转换编码。 |

```zr
let buffer = ffi.BufferHandle.allocate(16);
buffer.write(0, [1, 2, 3]);
let pointer = buffer.pin();
let bytes = pointer.span();
// native call 使用 bytes 后再 close pointer/buffer
pointer.close();
buffer.close();
```

`pin()` 建立共享 pin loan；仍有 Span 使用时不能移动或 close backing buffer。`read`/`write`
检查 offset/length，write 数组元素必须为 0..255 整数。`nullPointer(type)` 生成可比较但不可
解引用的 null pointer；read 前必须检查非 null、未关闭和长度足够。

## callback

`callback(signature, fn)` 使用 libffi 或平台 backend 创建 foreign-callable trampoline。回调
期间 `ZrLibCallContext`、argument view 和 result slot 只在当前调用有效；foreign thread 调用
若不满足创建时线程策略会得到 `ZR_FFI_ERROR_CALLBACK_THREAD`。close callback 后 native
side 不能再调用 trampoline。

回调中的 ZR 异常会恢复 call-info、stack top、handler depth 和 pending control，再传播到
宿主边界。不要在 callback 中缓存 VM object、string buffer 或 pointer view；需要跨 safepoint
时用 root/pin/owned buffer。

## marshalling 事务

一次 SymbolHandle 调用按以下顺序运行：

1. 校验 library/symbol 未关闭，arity、type、calling convention 和 capability；
2. 为 string/array/struct 分配临时 native storage，建立 pin/root；
3. 转换 `in/ref/out` 参数并执行 foreign call；
4. 将返回值转换为 `SZrTypeValue`/脚本 object；
5. 解除 pin、释放临时 storage、更新 out place；
6. 任一步失败都先撤销 callback/temporary，再抛原始 FFI error。

原生函数部分写入后失败时，`out` 写回状态由 contract 标记；不能把未初始化 bytes 当成功
结果。C ABI 的 struct padding、signedness 和 alignment 必须由 type layout 明确给出。

## 错误分类

| 错误 | 触发 |
| --- | --- |
| `ZR_FFI_ERROR_LOAD` | 动态库加载失败。 |
| ABI/signature mismatch | 调用约定、参数/返回 layout 或 backend 不支持。 |
| closed handle | library/symbol/pointer/buffer/callback 已关闭。 |
| marshal/type error | value 与 contract 不兼容、数组元素越界。 |
| bounds/null error | pointer/buffer 越界或 null 解引用。 |
| callback thread error | foreign thread 不符合 callback 策略。 |

## C 注册和 contract 校验

```c
const ZrLibModuleDescriptor *descriptor = ZrVmLibFfi_GetModuleDescriptor();
if (!ZrVmLibFfi_Register(global)) {
    return ZR_FALSE;
}
if (!ZrVmLibFfi_ValidateNativeImportContract(contract, error, sizeof(error))) {
    /* do not call the symbol */
    return ZR_FALSE;
}
```

共享库使用 `ZrVm_GetNativeModule_v1()`。宿主必须让 library path、signature metadata、global
和 callback lifetime 对齐；关闭顺序建议为 callback -> symbol -> pointer/buffer -> library ->
state/global。更底层的 native descriptor/call context 见 [Native API](native-api.md)，静态
contract 的 C 结构见 [FFI Contract](../05-interop/ffi-contract.md)。
