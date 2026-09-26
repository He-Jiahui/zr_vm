---
related_code:
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/module.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_support.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
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
doc_type: module-detail
---

# `zr.ffi` 与 `native extern`

**状态：`current`（功能依赖平台动态库/FFI backend）；Runtime provider，descriptor 版本
`1.0.0`。LibraryHandle、CallbackHandle、PointerHandle 和 BufferHandle 提供 `close()`；
SymbolHandle 没有公开 `close()`，由 GC finalizer 回收。**

FFI provider 将静态 ABI contract 和运行时动态库句柄分开。源代码声明 native symbol，
编译器保留参数/返回类型、passing mode、调用约定和 library contract；运行时只在
contract 校验通过后执行 marshalling。裸地址、未声明的 variadic 形状和隐式类型猜测均
被拒绝。

## 源语法

```zr
module app.native;

native extern("libsample") {
    fn add(left: i32, right: i32): i32;
    fn fill(buffer: ref u8, length: u64): void;
}
```

`native extern` 声明必须位于允许的 module/type scope，并显式给出返回类型。参数可以是
scalar、canonical struct、`Ptr<T>`、`ref`/`out` view 或 callback；编译器把声明投影为
`SZrNativeImportContract`，并把 source range 保存到诊断和 artifact。

## 动态句柄类型

模块函数：`loadLibrary(path): LibraryHandle`、`callback(signature, fn): CallbackHandle`、
`sizeof(type)`、`alignof(type)`、`nullPointer(type)`。

### 导出签名

| 对象 | 成员签名 | 返回 |
| --- | --- | --- |
| 模块 | `loadLibrary(path: string)`；`callback(signature: object, fn: function)`；`sizeof(type: object)`；`alignof(type: object)`；`nullPointer(type: object)` | `LibraryHandle`、`CallbackHandle`、`int`、`int`、`Ptr<void>` |
| `LibraryHandle` | `close()`；`isClosed()`；`getSymbol(name: string, signature: object)`；`getContractSymbol(index: int)`；`getVersion(name?: string)` | `null`、`bool`、`SymbolHandle`、`SymbolHandle`、`string` 或 `null` |
| `SymbolHandle` | `call(args: array)`；直接 callable meta-call `symbol(...)` | `value` |
| `CallbackHandle` | `close()` | `null` |
| `PointerHandle` | `as(type: object)`；`read(type: object)`；`span()`；索引读写；`close()` | `Ptr<void>`、`value`、`Span<u8>`、`u8`/`null`、`null` |
| `Ptr<T>` | `span()` | `Span<T>` |
| `BufferHandle` | `allocate(size: int)`；`close()`；`pin()`；`read(offset: int, length: int)`；`write(offset: int, bytes: array)`；`slice(offset: int, length: int)` | `BufferHandle`、`null`、`Ptr<u8>`、`array`、写入字节数 `int`、复制出的 `BufferHandle` |

对象方法：

- `LibraryHandle.close/isClosed/getSymbol/getContractSymbol/getVersion`
- `SymbolHandle.call(args)` 和受约束的 meta-call
- `CallbackHandle.close()`
- `PointerHandle.as/read/close`，`Ptr<T>.span/getItem/setItem`
- `BufferHandle.allocate/close/pin/read/write/slice`

句柄的宿主 payload 放在 `SZrRawObject::finalizerData`，finalizer 清理 context 后释放
payload，重复 finalizer 不会二次访问。`BufferHandle.pin()` 产生共享 pin loan；由它创建
的 pointer 视图在关闭前维持底层字节存活。`BufferHandle.close()` 可以先标记逻辑关闭，
实际存储在最后一个 pin 释放后回收；`BufferHandle.slice()` 则复制为独立的 owned 存储。
`getContractSymbol` 读取编译器发出的整数索引，只在当前活动调用帧保留的同库 contract
中解析，不接收任意 contract 对象。

## 调用事务

符号调用依次校验句柄与 ABI、编组参数并准备临时存储、调用 native 函数、解码返回值，
然后回收该调用持有的临时资源。受检的分配和编组失败会返回错误；不能假定所有构造与
解码失败都能回滚：**BUG:** 隐藏 owner/callback 字段写入失败仍可能发布句柄，聚合返回
字段写入失败仍可能交付缺字段对象，`BufferHandle.read()` 的数组追加失败也可能返回短数组。
这些路径需要故障注入核验。native callback 中的 ZR 异常会恢复保存的 call-info、stack top、
handler depth 和 pending control，不能污染外层执行。

## 安全与线程边界

默认 pointer 是 borrowed；只有明确 owner/pin contract 的句柄可跨 native 调用保存。callback
受 active lifetime 和创建线程策略约束，foreign thread 调用会得到
`ZR_FFI_ERROR_CALLBACK_THREAD`。`ZrLibCallContext`、argument view 和返回 slot 只在当前
callback 期间有效。

## C 入口

```c
const ZrLibModuleDescriptor *d = ZrVmLibFfi_GetModuleDescriptor();
TZrBool ok = ZrVmLibFfi_Register(global);
TZrBool valid = ZrVmLibFfi_ValidateNativeImportContract(
    contract, errorBuffer, errorBufferSize);
```

具体函数指针见 `zr_vm_lib_ffi/runtime.h`；宿主必须保持 library path、signature metadata
和 global 生命周期一致。销毁 global 前应停止 native 调用，关闭仍在使用的 callback、
pointer、buffer 和 library 句柄，并释放 SymbolHandle 的 VM 引用；SymbolHandle 没有
公开 `close()`，其 native 状态由 finalizer 回收。`LibraryHandle.close()` 会阻止新符号调用，
已有 SymbolHandle 尚未最终化时会延迟卸载动态库。
