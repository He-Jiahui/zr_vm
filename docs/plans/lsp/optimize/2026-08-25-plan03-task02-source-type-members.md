---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2j: Source Type Member Visibility Facts

## Goal

Publish source struct, class, and interface fields and methods as canonical
receiver-member candidates for `VisibleSymbols`. The consumer must use only
compiler-published symbol identity and scope facts, never member spelling,
AST pairing, or a language-server table fallback.

## Implementation

- The shared type-member registration path now registers fields as canonical
  `ZR_SEMANTIC_SYMBOL_KIND_FIELD` records from their exact member declaration
  node, in addition to the existing function-member registration.
- Struct, class, and interface compilers register those symbols before adding
  their member metadata. Existing later loops leave an already-valid symbol
  intact.
- Source scope publication projects resolved field and function members into
  their owning type scope with canonical owner identity, declaration range,
  access modifier, declaration order, and static/receiver flags.
- Static method scope state flows to nested block scopes. The existing query
  filter therefore excludes instance receiver members in a static method
  without inspecting a member name or source form.

## Contract

For `class Meter { var reading: int; static var total: int; ... }`, both
fields have exact canonical field `SymbolId` records. `VisibleSymbols` exposes
them only when `includeReceiverMembers` is requested. At an instance method
position it exposes `reading`, `total`, instance methods, and static methods;
at a static method position it exposes only static receiver members. Struct
and interface fields follow the same exact identity path. Missing member
symbols publish no candidate and fail closed.

## Verification

- RED: source class fields had no semantic field symbol, so the exact-member
  assertion failed before receiver scope publication could occur.
- MSVC static: symbols 15/15, semantic query 29/29, query contract 3/3,
  compiler diagnostics 46/46, and compiler integration 127/127 passed with
  zero failures and real process exit zero.
- Windows GCC 4.8.3 static: a fresh focused build stopped while compiling
  unchanged core code because the toolchain does not support `_Thread_local`.
  No GCC executable result is claimed.
- WSL GCC: the dedicated cache configuration remained in WSL 9P I/O wait and
  was terminated before it could compile or run a target. No WSL GCC result is
  claimed.
- Clang executable verification is not claimed in this child; the earlier
  static cache remains blocked by the pre-existing C11 inline link ABI issue.

## 状态与产出记录

- 完成时间：2026-08-25 15:55:56 +08:00
- 状态：MSVC 子里程碑完成；Windows GCC 4.8.3 与 WSL GCC 环境未形成可用执行
  证据，Clang executable gate 仍受既有 static-link ABI 问题阻断，均未计入通过。
- 完成项目：source struct/class/interface field SymbolId、receiver-member
  scope facts、method/field static-context projection、source member visibility
  RED/GREEN、MSVC query/diagnostic/compiler-integration 回归、模块文档与验收记录。
- 后续项目：imports/aliases、binary/native producer parity、Task 2 LSP consumer
  迁移，以及恢复可用 GCC/Clang 环境后的跨工具链 executable gate。
