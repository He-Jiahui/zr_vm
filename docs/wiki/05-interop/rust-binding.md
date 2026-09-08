---
related_code:
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/src
  - docs/cli-and-tooling/zr-vm-rust-binding.md
  - zr_vm_library/include/zr_vm_library/native_binding.h
implementation_files:
  - zr_vm_rust_binding/src
  - zr_vm_rust_binding/CMakeLists.txt
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/rust_binding
  - tests/cli/test_cli_import_basic_fixture.c
  - tests/cli/test_cli_project_incremental.c
doc_type: api-reference
---

# Rust Binding

Rust binding 将 C core/library API 包装成 opaque handles，避免 Rust 侧直接持有可移动 GC
对象地址。底层 `zr_vm_rust_binding_sys` 只暴露 ABI，安全层负责生命周期、错误转换和借用
范围。

## Runtime/project API

```c
ZrRustBinding_Runtime_NewBare(&options, &runtime);
ZrRustBinding_Runtime_NewStandard(&options, &runtime);
ZrRustBinding_Project_Open(path, &workspace);
ZrRustBinding_Project_Compile(runtime, workspace, &compileOptions, &result);
ZrRustBinding_Project_Run(runtime, workspace, &runOptions, &value);
ZrRustBinding_Project_CallModuleExport(runtime, workspace, &runOptions,
                                       module, export, args, argumentCount, &value);
ZrRustBinding_Runtime_Free(runtime);
```

workspace 可读取 project/root/manifest/entry，解析依赖和 artifact；`ManifestSnapshot_*`
提供版本、entry count、module/source hash、`.zro/.zri` 路径和 imports。compile/run 结果必须
显式 `*_Free`。

## Session、预算和回滚

`ZrRustBinding_ProjectSession_Start` 建立可持续调用 session；
`ZrRustBinding_ProjectSession_CallModuleExport` 支持
`ZrRustBinding_ProjectSession_CallModuleExportWithBudget` 版本。
`ZrRustBindingCallBudget` 可限制 instructions、deadline、heap、native calls、GC micros，
并通过 `ZrRustBinding_CancellationToken_New/Cancel/IsCancelled/Free` 取消。usage 返回
executedInstructions、elapsedMicros、peakHeapBytes、nativeCalls、gcMicros 和 termination。
`ZrRustBinding_ProjectSession_GcStep`、`ZrRustBinding_ProjectSession_Checkpoint`、
`ZrRustBinding_ProjectSession_Rollback`、`ZrRustBinding_ProjectSession_Free` 允许 REPL/编辑器增量工作；
rollback 后旧 value/session handle 不得继续使用。

## Value API

`ZrRustBinding_Value_NewNull`、`ZrRustBinding_Value_NewBool`、`ZrRustBinding_Value_NewInt`、
`ZrRustBinding_Value_NewFloat`、`ZrRustBinding_Value_NewString`、`ZrRustBinding_Value_NewArray`
和 `ZrRustBinding_Value_NewObject` 创建拥有值，`ZrRustBinding_Value_Free` 释放；
`ZrRustBinding_Value_GetKind` / `ZrRustBinding_Value_GetOwnershipKind` 读取类别，
`ZrRustBinding_Value_ReadBool`、`ZrRustBinding_Value_ReadInt`、`ZrRustBinding_Value_ReadFloat`
和 `ZrRustBinding_Value_ReadString` 复制 scalar/string，Array/Object 用
`ZrRustBinding_Value_Array_Length/Get/Push`、`ZrRustBinding_Value_Object_Get/Set` 访问。
返回的字符串写入调用方 buffer，先询问长度或
处理 `BUFFER_TOO_SMALL`。value 不可跨 runtime 线程共享。

## Native module builder

builder 按顺序调用 `ZrRustBinding_NativeModuleBuilder_New`、`SetDocumentation`、`SetModuleVersion`、
`SetTypeHintsJson`、`SetRuntimeRequirements`、`AddTypeHint`、`AddModuleLink`、`AddConstant`、
`AddFunction`、`AddType`、`Build`、`Free`（后续名称均带同一
`ZrRustBinding_NativeModuleBuilder_` 前缀）。callback 通过
`ZrRustBinding_NativeCallContext_GetModuleName/GetTypeName/GetCallableName/GetArgumentCount/`
`CheckArity/WithArgument/GetSelf` 获取上下文；argument view 支持
`ZrRustBinding_NativeArgumentView_GetKind/ReadBool/ReadInt/ReadFloat/ByteArrayLength/`
`ByteArrayGet/WithString`。回调返回新建
`ZrRustBindingValue`，错误返回 `ZrRustBindingStatus`，不会直接 longjmp。

## 状态码和线程安全

状态码包括 OK、INVALID_ARGUMENT、IO_ERROR、NOT_FOUND、ALREADY_EXISTS、BUFFER_TOO_SMALL、
COMPILE_ERROR、RUNTIME_ERROR、UNSUPPORTED、INTERNAL_ERROR、EXECUTION_TERMINATED。失败后
调用 `ZrRustBinding_GetLastErrorInfo`；错误 buffer 是线程局部快照。一个 runtime 只允许其
owner 线程进入 session，跨线程应在 Rust 层序列化调用或创建独立 runtime。
