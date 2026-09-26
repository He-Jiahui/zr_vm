---
related_code:
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/module.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_support.c
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
implementation_files:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_support.c
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
| `LibraryHandle` | `close(): null`、`isClosed(): bool` | close 幂等；关闭后 symbol lookup/call 失败，已有 symbol 延迟动态库卸载。 |
| `LibraryHandle` | `getSymbol(name:string, signature:object): SymbolHandle` | 解析 symbol 并绑定 ABI signature。 |
| `LibraryHandle` | `getContractSymbol(index:int): SymbolHandle` | 按编译器生成的整数索引，从活动调用帧解析 retained native import contract。 |
| `LibraryHandle` | `getVersion(versionSymbol?: string): string\|null` | 读取可选版本导出；未找到时返回 `null`。 |
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
不支持的 type 会在 lookup 前失败。正常创建的 SymbolHandle 通过隐藏 owner 引用保持
LibraryHandle 可达，但没有公开 `close()`；释放其 VM 引用后由 finalizer 减少 symbol
计数。可以先调用 `lib.close()`，它会立即阻止新 lookup 和 symbol 调用，直到剩余
SymbolHandle 最终化后才卸载动态库。**BUG:** 若已有 SymbolHandle 使库仍保持加载，
`lib.close()` 后 `getVersion()` 只检查物理库句柄，仍可能调用版本导出。

## PointerHandle、Ptr<T> 和 BufferHandle

| 类型 | 方法/字段 | owner 语义 |
| --- | --- | --- |
| `PointerHandle` | `length:int`、`as(type)`、`read(type)`、`span()`、索引读写、`close()` | 默认 borrowed；close 后不可读，但当前 `as(type)` 仍可创建空地址别名，见下文 BUG。 |
| `Ptr<T>` / `Ptr32<T>` / `Ptr64<T>` | `span(): Span<T>` | 只描述目标类型和宽度，不自动拥有内存。 |
| `BufferHandle` | `allocate(size)`、`close`、`pin`、`read`、`write`、`slice` | FFI owner mode `owned`；slice 复制为独立存储，不借用 parent 生命周期。 |
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

`pin()` 建立共享 pin loan；`buffer.close()` 可先逻辑关闭，最后一个 pointer pin 释放后
才回收字节。`read`、`write`、`slice` 检查 offset/length；调用方应只传 0..255 的整数
字节。**BUG:** 当前 `write` 经数值读取后直接转为 `unsigned char`，例如 `[256]` 会成功
写入 `0`，浮点值也可能被接受，未执行所需的严格类型和值域校验。`nullPointer(type)`
生成可比较但不可解引用的 null pointer；读取前必须检查非 null 和有效长度。

**BUG:** `PointerHandle.close()` 后仍可调用 `as(type)` 创建空地址别名，且若隐藏 owner
是 BufferHandle，还可能重新增加 pin 计数。BufferHandle 关闭后拒绝新 `pin()`；但若
已有 pin 保持字节存活，`read()`、`write()`、`slice()` 未检查关闭标志，仍能操作已关闭
句柄。这些是当前实现的关闭契约缺陷，不应将关闭后的操作当作受支持的用法。

## callback

`callback(signature, fn)` 使用 libffi 或平台 backend 创建 foreign-callable trampoline。回调
期间 `ZrLibCallContext`、argument view 和 result slot 只在当前调用有效；foreign thread 调用
若不满足创建时线程策略会得到 `ZR_FFI_ERROR_CALLBACK_THREAD`。close callback 后 native
side 不能再调用 trampoline。

回调中的 ZR 异常会恢复 call-info、stack top、handler depth 和 pending control，再传播到
宿主边界。不要在 callback 中缓存 VM object、string buffer 或 pointer view；需要跨 safepoint
时用 root/pin/owned buffer。

## marshalling 事务

一次 SymbolHandle 调用的主要阶段如下：

1. 校验 library/symbol 未关闭，arity、type、calling convention 和 capability；
2. 为 string/array/struct 分配临时 native storage，建立 pin/root；
3. 转换 `in/ref/out` 参数并执行 foreign call；
4. 按参数顺序写回 `ref/out` place，再检查 callback 错误；
5. 将返回值转换为 `SZrTypeValue`/脚本 object；
6. 统一恢复 callback 状态、解除 pin、释放临时 storage，再报告暂存错误。

**BUG:** 句柄创建时隐藏 owner/callback 字段写入失败可被忽略，仍交付缺少依赖引用的
对象；库可能在符号地址仍被使用时提前被 GC 卸载。聚合结果的字段写入失败也可能返回
缺字段对象，`BufferHandle.read()` 追加数组元素失败可返回短数组。不能把这些失败路径
描述为已保证完整回滚；实际内存后果仍需故障注入验证。

**TODO:** `ref/out` 当前逐项写回，后一个转换或 Store 失败时，前面的 place 可能已经
改变；需确认公开契约是否允许部分提交。不能把未初始化 bytes 当成功结果。C ABI 的
struct padding、signedness 和 alignment 必须由 type layout 明确给出。

## 错误分类

| 错误 | 触发 |
| --- | --- |
| `ZR_FFI_ERROR_LOAD` | 动态库加载失败。 |
| ABI/signature mismatch | 调用约定、参数/返回 layout 或 backend 不支持。 |
| closed handle | library/pointer/buffer/callback 已关闭，或 symbol 所属 library 已关闭；当前 `getVersion`、`PointerHandle.as` 和部分 buffer 操作存在上述例外 BUG。 |
| marshal/type error | value 与 contract 不兼容、缓冲区范围越界或数组元素不能转换；当前 `BufferHandle.write` 未拒绝超出字节范围的数值。 |
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
和 callback lifetime 对齐；销毁 state/global 前应停止 native 调用并关闭 callback、
pointer/buffer 和 library。SymbolHandle 没有公开 `close()`；释放其 VM 引用后由 GC
finalizer 归还 native 状态，不能假定引用释放会立即卸载动态库。更底层的 native descriptor/call
context 见 [Native API](native-api.md)，静态
contract 的 C 结构见 [FFI Contract](../05-interop/ffi-contract.md)。
