---
related_code:
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/internal.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/api.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/value.c
  - zr_vm_rust_binding/rust/Cargo.toml
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/native.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/native.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
implementation_files:
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/api.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/src/zr_vm_rust_binding/value.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/native.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/native.rs
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
plan_sources:
  - user: 2026-04-16 增加 zr_vm_rust_binding 模块，用于 Rust 绑定、Rust 启动和管理虚拟机与脚本工程
  - user: 2026-04-16 PLEASE IMPLEMENT THIS PLAN: zr_vm_rust_binding v1 Implementation Plan
tests:
  - tests/rust_binding/test_rust_binding_api.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs
  - tests/acceptance/2026-04-16-zr-vm-rust-binding-v1.md
doc_type: module-detail
---

# ZR VM Rust Binding

## Purpose

`zr_vm_rust_binding` 为 ZR VM 提供一个稳定的 host-facing C ABI，并在模块内附带一个局部 Rust workspace：

- `zr_vm_rust_binding_sys`
  只暴露 ABI 原始声明。
- `zr_vm_rust_binding`
  在 ABI 之上提供安全 RAII 包装、builder API、错误映射和值转换。

Rust 端不直接绑定 `SZrState`、`SZrTypeValue`、CLI 子进程或其他内部结构。长期契约是 `ZrRustBinding_*` 前缀的 ABI。

v1 覆盖：

- runtime / project scaffold / open / compile / run / module export call
- manifest snapshot 和 artifact 查询
- owned/live value mirror
- Rust 侧 native module / function / type 注册
- Cargo/CMake 联合验证

v1 明确不覆盖：

- `zr_vm_aot/` 归档中的历史 AOT host API
- packaging / publish workflow

## ABI Surface

### Opaque handles

公开 ABI 只暴露 opaque handle，不暴露运行时内部布局：

- `ZrRustBindingRuntime`
- `ZrRustBindingProjectWorkspace`
- `ZrRustBindingCompileResult`
- `ZrRustBindingManifestSnapshot`
- `ZrRustBindingValue`
- `ZrRustBindingNativeModuleBuilder`
- `ZrRustBindingNativeModule`
- `ZrRustBindingRuntimeNativeModuleRegistration`
- `ZrRustBindingNativeCallContext`

### Status and error model

- 所有 ABI 入口统一返回 `ZrRustBindingStatus`
- 最近一次错误通过 `ZrRustBinding_GetLastErrorInfo` 读取
- Rust safe 层把 `status + message` 包装成 `Error`

这让 Rust、未来 embedders 和现有 C 测试都共享一套可稳定演进的错误面。

## Shared Host Services

绑定模块没有复制 CLI 的 project/bootstrap 逻辑，而是把 CLI compile/run 所需的 project 服务抽成共享 host service：

- `.zrp` 解析和 project context 构建
- scaffold 目录和默认文件生成
- `src/` / `bin/` artifact 路径解析
- incremental manifest 加载与保存
- source / binary 执行入口准备
- `zr.system.process.arguments` 注入

`Project_Compile`、`Project_Run` 和 `Project_CallModuleExport` 都经由这条共享路径，因此 CLI 表面行为保持不变，而 Rust binding 复用同一份编译和运行语义。

## Project Workflow

### Scaffold and open

- `Project_Scaffold` 生成 `{name}.zrp`、`src/main.zr` 和 `bin/`
- 默认项目约定保持与 fixture 体系一致：
  - `source = "src"`
  - `binary = "bin"`
  - `entry = "main"`
- `Project_Open` 只加载 project context，不执行脚本

### Compile

- `Project_Compile` 返回 `compiled / skipped / removed`
- 支持显式 `.zro`
- `emitIntermediate = true` 时支持 `.zri`
- manifest snapshot 可读取：
  - version
  - module entry 列表
  - hash
  - artifact path
  - import 列表

### Run and export call

- `Project_Run` 支持 `Interp` 与 `Binary`
- `Project_CallModuleExport` 支持在 project runtime 环境中直接调用模块导出
- `RunOptions` 支持 module override 与 program args

`Runtime_NewBare` 仍保留为低层 runtime 配置入口，但 v1 project run / export call 依赖 standard profile。

## Runtime Profile

`Runtime_NewStandard` 对齐当前 CLI 标准栈：

- parser / compiler 注册
- `zr_vm_lib_math`
- `zr_vm_lib_system`
- `zr_vm_lib_network`
- `zr_vm_lib_container`
- `zr_vm_lib_ffi`
- task runtime builtin
- `zr_vm_lib_thread` 仅在当前 checkout 启用该模块时编入

## Value Mirror and Lifetimes

### Owned values

ABI 和 safe Rust 层都能直接构造：

- `null`
- `bool`
- `int`
- `float`
- `string`
- `array`
- `object`

owned array/object 在 binding 内部做深拷贝保存。真正调用 runtime 时再 materialize 成 VM value。

### Live values

run / export call / native callback 参数会返回 live handle：

- handle 背后保留 execution owner
- owner 维护 global 与已捕获 native module 引用
- VM-backed 复杂值通过 `TempValueRoot` 保活

这保证：

- live string / array / object / function 在 helper 分配期间不会被 GC 提前回收
- `Array_Get`、`Array_Push`、`Object_Get`、`Object_Set` 能操作真实运行时对象

### Ownership metadata

`OwnershipKind` 会从 VM value 映射到 binding 层并暴露给 Rust safe API。v1 safe 层已经把它作为显式查询元数据保留下来，而不是用“自动转换后丢语义”的方式隐藏。

## Native Module Bridge

### Why the bridge exists

`zr_vm_library/native_binding.h` 偏静态 C 描述符模型。为了让 Rust 能安全注册 native extension，binding 增加了一层 descriptor bridge：

- Rust 构建 builder / descriptor
- safe 层把 Rust 字符串、数组、generic metadata 和闭包序列化成 ABI 描述符
- C 层复制 descriptor 所需内存，并接管 callback user-data 生命周期

这样 Rust callback 不需要触碰 VM 内部结构，也不需要依赖 CLI 子进程。

### C ABI native registration

稳定 ABI 当前提供：

- `NativeModuleBuilder_*`
- `Runtime_RegisterNativeModule`
- `RuntimeNativeModuleRegistration_Free`
- `NativeCallContext_*`

支持的描述符类别包括：

- module version / runtime requirement / type hints / module links
- constants
- functions
- types
- fields
- methods
- meta methods
- enum members
- generic parameters
- FFI type metadata

### Callback lifetime model

Rust safe 层把闭包装箱后交给 C 层：

- callback 调用走 Rust trampoline
- C 模块持有 user-data pointer
- destroy hook 在 module/builder teardown 时释放闭包
- runtime registration 只负责 runtime registry 引用
- module handle 与 live execution owner 都能继续持有 native module，直到最后一个引用释放

这一点很重要。native callback 参数创建出的 live `Value` handle 会额外保留 native module 引用，防止 callback 结束后模块描述符过早释放。

### Callback context

`NativeCallContext` safe API 当前支持：

- `module_name`
- `type_name`
- `callable_name`
- `argument_count`
- `check_arity`
- `argument(index)`
- `self_value()`

这让 Rust callback 可以只面对 binding `Value`，不需要接触 `ZrLibCallContext`。

## Rust API Layers

### `zr_vm_rust_binding_sys`

raw crate 现在声明了：

- runtime / project / manifest / value ABI
- native builder / module / registration ABI
- native callback typedef
- constant / function / type / method / meta-method / generic descriptor ABI

### `zr_vm_rust_binding`

safe crate 当前提供两层 API：

基础 host API：

- `RuntimeBuilder`
- `Runtime`
- `ProjectWorkspace`
- `CompileOptions`
- `RunOptions`
- `CompileResult`
- `Manifest`
- `ArtifactPaths`
- `Value`
- `OwnershipKind`

native extension API：

- `ModuleBuilder`
- `FunctionBuilder`
- `TypeBuilder`
- `MetaMethodBuilder`
- `ConstantDescriptor`
- `FieldDescriptor`
- `ParameterDescriptor`
- `GenericParameterDescriptor`
- `EnumMemberDescriptor`
- `TypeHintDescriptor`
- `ModuleLinkDescriptor`
- `FfiTypeMetadata`
- `PrototypeType`
- `MetaMethodType`
- `NativeCallContext`
- `Runtime::register_native_module`

Rust safe 层通过 `Drop` 自动释放：

- runtime
- workspace
- manifest/result/value handle
- native module
- runtime registration

## Cargo and CMake Integration

模块内 Rust workspace 维持局部 `Cargo.toml`，没有把仓库根目录改成 Cargo workspace。

`zr_vm_rust_binding/CMakeLists.txt` 负责把 Rust 校验接回当前 CMake：

- `zr_vm_rust_binding_rust_check`
- `zr_vm_rust_binding_rust_test`

执行时会注入：

- `ZR_VM_RUST_BINDING_LIB_DIR`
- 平台对应的 runtime library path

因此 CMake 和 Cargo 都会针对当前构建出的 binding 动态库做一致性验证。

## Testing Coverage

### C tests

`tests/rust_binding/test_rust_binding_api.c` 当前覆盖：

- scaffold / open / compile / run roundtrip
- manifest/artifact 查询
- scalar value kind / ownership metadata
- owned array / object accessor
- module export call
- native module registration roundtrip
  - module constant
  - native function callback
  - native type static method callback
  - callback context 字段
  - destroy hook 生命周期
- invalid native function descriptor rejection

### Rust tests

`zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs` 覆盖：

- scaffold / compile / run roundtrip
- manifest load
- scalar `ValueKind` / `OwnershipKind` mirror
- owned array / object API
- module export call

`zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs` 覆盖：

- safe `ModuleBuilder` / `FunctionBuilder` / `TypeBuilder` roundtrip
- Rust closure trampoline
- native module registration + binary execution
- destroy hook 生命周期
- invalid function descriptor error surface

### CLI regression evidence

现有 `tests/cmake/run_cli_suite.cmake` 继续作为 shared host service 抽取后的 CLI 表面回归层，当前至少覆盖：

- `--compile --run` 默认 binary 路径
- `--project -m` project module 执行
- interp / binary 切换
- process argument passthrough

本轮收口验证重新执行了 `cli_integration`，确认 CLI compile/run 主路径仍与 binding 共用的 project/bootstrap 语义保持一致。

## Current Limits

- bare runtime 仍不直接支持 project run / export call
- `zr_vm_aot/` 归档中的历史 AOT、packaging、publish 仍在 v1 之外
- safe Rust layer 已经覆盖常用 native builder 路径，但更高层 ergonomic conversion 仍可以继续扩展

## Maintenance Notes

- 修改 ABI 时，同时更新 `zr_vm_rust_binding.h`、`zr_vm_rust_binding_sys`、`zr_vm_rust_binding` 和对应测试
- 修改 project/bootstrap 逻辑时，优先确认 CLI shared host service 和 binding 调用路径是否仍保持语义一致
- 修改 native callback 生命周期时，必须同时验证：
  - callback 参数 `Value` 释放
  - runtime registration 释放
  - native module 释放
  - Rust destroy hook 是否在最后一个引用释放后触发
