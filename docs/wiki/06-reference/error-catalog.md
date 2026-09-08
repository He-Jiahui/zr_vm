---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_contract_validation.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/core-runtime/index.md
tests:
  - tests/parser/test_parser_recovery_ownership.c
  - tests/core/test_runtime_crash_recovery.c
  - tests/library/test_native_registry_descriptor_invalidation.c
  - tests/ffi/test_native_extern_contract.c
doc_type: reference
---

# 错误目录

## Parser/semantic

常见 code 包括 `unexpected_token`、`legacy_syntax_removed`、`missing_semicolon`、
`unresolved_name`、`type_mismatch`、`invalid_cast`、`definite_assignment`、`borrow_conflict`、
`reference_escape`、`non_exhaustive_switch`、`invalid_await_context` 和
`invalid_native_extern`。结构化诊断总是包含 source range；若有修复，`fixes` 给出 edit 和
applicability。恢复 parser 只能继续分析，不代表 AST 可生产编译。

## Runtime

`NullReferenceError` 表示对 null/absent 目标使用普通 `.`；`TypeError` 表示动态类型或 ABI
转换失败；`IOException` 表示文件/网络/动态库 I/O；`MemoryError` 表示 allocator/heap limit；
`RuntimeError` 表示执行/模块/调度故障。异常对象保留 stack frames，native callback 必须先
清理再抛出。

## Call binding/registry

call binding status 包括 missing contract/target/signature/module/layout、stale generation、
invalid relocation、ambiguous overload 和 unsupported operation。registry error 包括 ABI/
version/capability/phase mismatch、reserved official module、duplicate provider/contract、
invalid canonical role。遇到 stale generation 应重新 resolve；不能强制使用旧 target。

## AOT/FFI/Rust/CLI

AOT 拒绝 ABI version、input hash、layout hash、native import 或 call-binding row 不匹配；FFI
错误分为 load、signature、marshal、native call、callback thread/lifetime、closed handle。
Rust binding 将这些映射到 `COMPILE_ERROR`、`RUNTIME_ERROR`、`UNSUPPORTED`、
`EXECUTION_TERMINATED` 等状态，并通过 `GetLastErrorInfo` 提供消息。CLI 退出码 2 是命令误用，
3 是 runner/VM 基础设施失败；不要把用户程序的 1 与 crash 的 3 混合。
