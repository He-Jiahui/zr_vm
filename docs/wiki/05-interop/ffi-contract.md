---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_support.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler
implementation_files:
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_support.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/ffi/test_native_extern_contract.c
  - tests/ffi/test_ffi_module.c
  - tests/ffi/test_ffi_native_call_pin_contract.c
  - tests/ffi/ffi_fixture.c
doc_type: api-reference
---

# FFI Contract 参考

FFI contract 把 ZR 类型和外部 ABI 之间的可验证事实固定下来。它既服务 native extern
编译，也服务 zr.ffi 动态 lookup。没有 contract 的裸地址、隐式编码、变长参数和未知
struct layout 都应被拒绝。

## contract 组成

一个 native import contract 至少包含：

| 部分 | 内容 |
| --- | --- |
| library | 编译期 string literal 或受信 runtime library path。 |
| symbol | 导出名称或 retained contract symbol。 |
| calling convention | 平台 ABI/cdecl 等，由 backend 识别。 |
| return type | canonical TypeId、FFI lowering 和 optional/void 标志。 |
| parameters | 顺序、名称、TypeId、VALUE/IN/OUT/REF mode、nullable 和 ownership。 |
| layout | size、alignment、field offsets、padding、scan/Pin policy。 |
| pointer policy | Ptr32/Ptr64、target type、borrowed/owned、length/bounds。 |
| callback policy | trampoline lifetime、创建线程和 foreign-thread 规则。 |
| version/hash | library version symbol、module/package hash 和 contract digest。 |

编译器把 source range 和 module identity 一起写入 contract；resolver 不能只按 symbol name
复用另一个 library 的 signature。

## native extern 语法和验证

~~~zr
native extern("libsample") {
    fn add(left: i32, right: i32): i32;
    fn fill(buffer: ref u8, length: u64): void;
}
~~~

校验顺序为：

1. library literal 非空，module/provider 可在当前 phase 使用；
2. symbol/name 和 return/parameter type 可解析；
3. passing mode 与 source place/ownership 相容；
4. struct/array/pointer layout、alignment 和 calling convention 可降低；
5. capability、library version、public contract 和 artifact hash 匹配；
6. 生成 call-binding row，并把 source span 写入诊断。

ref 和 out 不等价：ref 要求已有可写 place，out 要求正常返回前写入；两者都不能把 borrow
逃出 call。string 默认不是 C char*，必须在 contract 中声明 encoding/termination 或
使用 BufferHandle。

## type lowering

| ZR 形状 | 常见 lowering | 注意 |
| --- | --- | --- |
| bool/int/float | scalar register/stack | signedness 和宽度必须明确。 |
| struct | inline bytes 或 pointer | field offset/padding 由 TypeLayout 提供。 |
| array/span | pointer + length | 需 pin/borrow contract，不能传 managed header。 |
| string | UTF-8 buffer + length 或 nul-terminated | 由 contract 指定，不能猜。 |
| Ptr32/Ptr64 | 固定宽度 pointer | 与 host address width 不符时拒绝。 |
| callback | backend trampoline pointer | lifetime/线程策略由 CallbackHandle 管理。 |
| resource/object | opaque pointer/handle | 默认 borrowed；owner/release hook 必须显式。 |

sizeof(type) 和 alignof(type) 只对有有效 FFI lowering 的 type 成功。nullPointer(type)
只创建 null token，不提供解引用权限。

## 动态 zr.ffi schema

动态 API 的 signature object 至少描述 return 和 parameters；可选字段包括 calling convention、
struct fields、pointer target/length、string encoding、ownership、version symbol 和 callback
thread policy。LibraryHandle.getSymbol(name, signature) 在返回 SymbolHandle 前完成
parse/lookup/contract validation；`getContractSymbol(index:int)` 用编译器生成的整数索引
从活动调用帧查找同库的保留 contract，不接收任意 contract 对象。

~~~zr
let ffi = import("zr.ffi");
let lib = ffi.loadLibrary("libsample");
let signature = {
    return: "i32",
    parameters: ["i32", "i32"],
    callingConvention: "default"
};
let add = lib.getSymbol("add", signature);
let answer = add.call([20, 22]);
lib.close();
~~~

signature object 不是普通 map：字段名、数组元素类型和 optional/default 规则由 FFI schema
读取；未知字段或重复参数应在 lookup 时报错。

## pointer、buffer 和 pin

PointerHandle 默认 borrowed，BufferHandle 的 owner mode 是 owned。`read`、`write`、
`slice` 检查 offset/length，`slice` 复制为独立 owned buffer。调用方应只向 `write`
传入 0..255 的整数；**BUG:** 当前实现使用宽松数值转换并直接收窄为字节，`[256]`
会成功写入 `0`，浮点值也可能被接受。`pin` 建立共享 pin loan；buffer 可先逻辑关闭，
底层字节待最后一个 pointer pin 释放后回收。**BUG:** 有存活 pin 时，已关闭 buffer 的
`read`、`write`、`slice` 仍可操作字节；已关闭 pointer 的 `as` 仍可创建空地址别名。

~~~zr
let buffer = ffi.BufferHandle.allocate(8);
buffer.write(0, [72, 105, 0]);
let ptr = buffer.pin();
let bytes = ptr.span();
ptr.close();
buffer.close();
~~~

若 native call 保存指针，必须把 owner mode 声明为 owned 或让宿主保证 BufferHandle 长于
foreign call；不能把 borrowed pointer 放入全局缓存或传到 isolated scheduler。

## callback contract

callback(signature, fn) 在 backend 创建 trampoline。创建阶段固定 signature、返回值
marshalling、线程策略和 close hook；foreign thread 调用若不满足策略报告
ZR_FFI_ERROR_CALLBACK_THREAD。callback 只在 active lifetime 内可调用，关闭后再次调用是
错误，不会自动重建。

callback 中的 VM 异常要通过 core/native bridge 恢复 call-info、stack top、handler depth
和 pending control；不能从 foreign thread 直接操作任意 state。需要回调到另一个 state 时，
建立明确的 queue/transfer 协议。

## C 校验和调用入口

~~~c
TZrBool ZrVmLibFfi_ValidateNativeImportContract(
    const SZrNativeImportContract *contract,
    TZrChar *errorBuffer,
    TZrSize errorBufferSize);

TZrBool ZrVmLibFfi_Register(SZrGlobalState *global);
~~~

函数名和 contract 结构以当前 zr_vm_lib_ffi/runtime.h、zr_ffi_contract.h 为准。验证失败时
不要调用 library/symbol；errorBuffer 属于调用方，成功/失败都要保证容量和终止符。

## 调用事务和错误

foreign call 的事务顺序：

~~~text
validate -> allocate/marshal -> pin/root -> invoke
  -> write back ref/out -> check callback -> unmarshal result -> unpin/free
~~~

受检的失败路径应撤销本次调用取得的临时资源并报告错误；**BUG:** 句柄隐藏 owner/callback
字段或聚合结果字段写入失败可被忽略而交付不完整对象，`BufferHandle.read()` 数组追加
失败可能返回短数组，因此当前不能承诺任意阶段失败都会完整回滚。常见
错误：ZR_FFI_ERROR_LOAD、ABI/signature mismatch、closed handle、marshal/type error、
bounds/null error、callback thread error。**TODO:** `ref/out` 逐项写回，后续失败不会回滚
先前 place；需核实公开契约是否允许部分提交。不得把未初始化 memory 当成功值。

## 版本兼容

库版本、calling convention、struct layout、pointer width、public contract 或参数 mode
改变都可能破坏 ABI。contract digest 应和 package/artifact entry hash 一起验证；不匹配时
resolver 失败关闭。升级流程是递增 module/package version -> 重新生成 .zri/.zro call-binding
rows -> 运行 FFI pin/ABI tests -> 更新文档和示例。
