---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_core/include/zr_vm_core/conf.h
  - zr_vm_library/include/zr_vm_library/conf.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_api_conf.h
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
  - docs/library-and-builtins/index.md
tests:
  - tests/ffi/test_native_extern_contract.c
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_official_provider_convergence.c
doc_type: api-reference
---

# C API 通用约定

## 导出和类型

公共函数使用模块导出宏：`ZR_CORE_API`、`ZR_PARSER_API`、`ZR_LIBRARY_API`、
`ZR_LANGUAGE_SERVER_API` 或 provider 自己的 `ZR_*_API`。这些宏在静态构建为空，在共享
构建映射到 `__declspec(dllexport/dllimport)` 或 ELF visibility；宿主不得自行重定义 ABI。

`TZrInt8/16/32/64`、`TZrUInt*`、`TZrFloat32/64`、`TZrSize`、`TZrPtr`、`TZrBool` 是跨编译器
的基础类型。结构体必须通过头文件初始化，不能依赖未声明的 padding 或 C++ name mangling；
C++ 调用方要包在 `extern "C"` 中。

## 所有权标记

| 标记/形状 | 默认含义 |
| --- | --- |
| 返回 `const T *` | 借用，属于 global/registry/cache，不由调用方 free |
| 返回 `T *` + `Free/Close` | 调用方拥有，必须在同一 state/global 释放 |
| `const T *` 参数 | 调用期间借用；函数不会接管 |
| `ZR_OUT T *` | 调用方提供存储，函数写入；成功前内容未定义 |
| `SZrTypeValue *result` | native binding 结果槽，函数必须按 descriptor 约定写入或抛异常 |
| `TZrStackValuePointer`/`ZrLibCallContext` | 只在当前 VM 调用窗口有效，不能跨 GC/safepoint 保存 |

GC 对象值不能用 `memcpy` 复制来延长生命周期。使用 `ZrCore_Value_Copy`、临时 root、
`ZrCore_Gc_NativeCallPinObject`、`ZrCore_Gc_NativeCallPinValue` 或对应 ownership API。native callback 若分配/调用可能触发 GC，
先把局部 `SZrTypeValue` 放入 root，再访问任何可能移动的对象地址。

## 错误模型

Core/library 函数通常返回 `TZrBool` 或枚举状态。`false` 只表示操作失败，详细信息读取
global module diagnostic、state current exception 或输出参数；不要把 `false` 当作 null 值
写回脚本。Parser 使用 `SZrStructuredDiagnostic`，包含 code、cause、suggestion、source span
和 fix。Rust binding 使用 `ZrRustBindingStatus` 和线程局部 last-error info。

## Native descriptor contract

`ZrLibModuleDescriptor` 的 function/method/type rows 必须同时给出参数数量、参数类型、
passing mode、generic constraints、return type、dispatch flags 和 contract role。注册器会
验证指针/计数一致性、ABI/runtime 版本、module identity、provider phase、canonical role 和
contract hash。`ZrLibCallContext` 提供 state、descriptor、self、argumentValues、stack anchors、
inline frame 信息；callback 返回前必须恢复 stack/call-info，不能泄漏临时 root。

## 线程和 reentrancy

一个 `SZrState` 只能由其所属 mutator 使用；跨线程应创建 secondary state 并通过 provider
定义的 Send/Sync/transfer contract 传值。native callback 可重入，但必须保存并恢复
`callInfoList`、`stackTop`、exception handler depth 和 pending control。阻塞 callback 应在
descriptor 设置 `ZR_LIB_NATIVE_DISPATCH_FLAG_BLOCKING_DETACHED`，否则 GC safepoint 可能被阻塞。
