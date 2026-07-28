---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-28-e1b2a-receiver-canonical-carrier
status: completed
completed_at: 2026-07-28 19:39 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_metadata.c
  - tests/debug/test_debug_introspection.c
  - docs/core-runtime/debug-canonical-local-bindings.md
doc_type: milestone-record
---

# LSP 04 E1b2a Receiver Canonical Carrier

##状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-28 19:39 +08:00 | 已完成 | E1 context reconstruction 的 receiver carrier：编译期 instance receiver 以结构化 role 投影到 typed-local artifact row；暂停帧查询按该 row 输出精确 receiver binding 和当前帧值，二进制 reader/runtime projection完整保留。 | GCC、Clang、fresh MSVC shared 均为 `zr_vm_debug_metadata_test` 6 Tests/0 Failures、`zr_vm_debug_introspection_test` 2 Tests/0 Failures，真实 exit 0。 |

## 已完成计划项

- `SZrFunctionTypedLocalBinding.roleFlags` 在 `.zro` patch 38 后写入、读取并复制到 runtime；旧 artifact 的 role 为零并保持 unavailable。
- `ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER` 只由当前 function declaration 的 receiver effect 和注入 receiver 的 stack slot 结构化确定，未使用 local name。
- `ZrCore_Debug_EvaluationContext_GetReceiver` 复验 paused activation、generation、PC 和 local liveness；只有唯一、active 且 canonical identity 完整的 receiver row 可用。
- free/static frame 返回 `NO_RECEIVER`；缺失、重复、inactive 或 identity 不完整的 row 返回 `METADATA_UNAVAILABLE`，不从 member name、display type、AST 或文本重建。
- binary roundtrip 通过 `SZrCompiledPrototypeInfo` / `SZrCompiledMemberInfo.functionConstantIndex` 定位 instance function constant，比较 source 与 runtime-loaded function 的 role、SymbolId、TypeId、PlaceId 与 declaration range。

## Deferred Plan Items

- E1b2b structured generic type/value substitution snapshot；当前 generic context 仍只有 availability flags。
- E2 formal parser/binder/Place query reuse，及 E3-E5 effect policy、result transport 和 REPL generation。

## Related Documentation

- [Debug canonical local bindings](../../../core-runtime/debug-canonical-local-bindings.md)
