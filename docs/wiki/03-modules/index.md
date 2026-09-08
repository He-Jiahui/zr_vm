---
related_code:
  - CMakeLists.txt
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/library/test_project_import_resolver.c
doc_type: category-index
---

# 标准库与 Provider

ZrVm 的标准库不是由 parser 硬编码的一组名称，而是由 native module descriptor
注册到 `ZrLibrary_NativeRegistryState` 的 provider 集合。每个 provider 同时描述模块
身份、版本、运行阶段、公开函数/类型、参数 passing mode、协议位和 contract hash。
编译器、运行时、反射和 LSP 都消费同一份 descriptor projection。

## 签名记法

模块页表格中的 `name?: T` 表示该参数允许省略，`T?` 表示实现可能返回 `null`；这是
descriptor/hint 的接口记法，不等同于用户函数声明的语法。ZR 源码中的默认参数仍写成
`name: T = default`，而可空结果通常由推断类型和 `null`/`?.` guard 表达。表格中的
`null` 返回值表示操作完成后返回 ZR null，不表示 C API 的 `NULL` 指针。

## 模块导航

| 模块 | 页面 | 阶段 | 主要职责 |
| --- | --- | --- | --- |
| `zr.builtin` | [builtin](builtin.md) | Runtime/N0 | canonical 基础协议、Object/TypeInfo、primitive wrapper |
| `zr.system` | [system](system.md) | Runtime | 控制台、文件、环境、进程、VM、GC、异常 |
| `zr.container` | [container](container.md) | Runtime | Array/Map/Set/LinkedList、Span、Pool |
| `zr.math` | [math](math.md) | Runtime | 标量、向量、矩阵、复数、四元数、Tensor |
| `zr.network` | [network](network.md) | Runtime | TCP/UDP 根模块和句柄对象 |
| `zr.task` / `zr.thread` | [task-thread](task-thread.md) | Runtime | Job、Task、Scheduler、worker/isolate |
| `zr.iteration` | [iteration](iteration.md) | Runtime | Iterable/Enumerator/Iterator 协议 |
| `zr.ffi` | [ffi](ffi.md) | Runtime | `native extern`、指针、callback、动态库 |
| `zr.debug` / `zr.testing` | [debug-testing](debug-testing.md) | Debug/Test | 调试 hook、自省、断言和 TestManifest |
| `zr.reflection` / `zr.pooling` | [reflection-pooling](reflection-pooling.md) | Runtime | 类型反射、稳定槽池、借用视图 |
| `zr.compile` | [compile](compile.md) | CompileTool/N3 | build feature、编译期诊断、conditional call-elision |
| `zr.compile.declaration` | [compile-declaration](compile-declaration.md) | CompileTool/N3 | immutable declaration view、typed Patch 和生成字段 |

## 官方注册清单

以下 25 个名称来自 `native_binding_official_inventory.c`，是当前产品图中可被 resolver
识别的完整官方集合。聚合页面会在同一页覆盖其叶子 provider；例如 `zr.system.*` 详见
[system](system.md)，`zr.network.tcp/udp` 详见 [network](network.md)。

| 阶段 | 官方 module identity |
| --- | --- |
| Runtime/N0 | `zr.builtin` |
| Runtime/N1 | `zr.container`、`zr.iteration`、`zr.math`、`zr.task` |
| Runtime/N2 | `zr.debug`、`zr.ffi`、`zr.network`、`zr.network.tcp`、`zr.network.udp`、`zr.pooling`、`zr.reflection`、`zr.system`、`zr.system.assembly`、`zr.system.console`、`zr.system.env`、`zr.system.exception`、`zr.system.fs`、`zr.system.gc`、`zr.system.process`、`zr.system.vm`、`zr.thread` |
| CompileTool/N3 | `zr.compile`、`zr.compile.declaration` |
| Test/N3 | `zr.testing` |

## 注册约束

1. 宿主先创建 `SZrGlobalState`，再调用各模块的 `*_Register`。
2. 注册时验证 native plugin ABI、provider phase、module identity 和 contract hash。
3. 同一 canonical provider role 只能有一个官方 owner；重复或伪造 descriptor 失败。
4. `zr.testing` 只能附着到 Test host；CompileTool provider 不能进入运行时。
5. 共享库入口统一为 `ZrVm_GetNativeModule_v1()`，静态链接使用显式 `*_GetModuleDescriptor`。

模块页中的“导出”表示 descriptor 的公开 surface；隐藏字段、finalizer payload 和
宿主句柄不属于可移植的 ZR API。模块版本和协议 hash 变化时，必须同时更新 artifact
metadata、AOT registration 和 LSP cache generation。
