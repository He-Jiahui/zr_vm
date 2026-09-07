---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
related_module_docs:
  - docs/parser-and-semantics/lsp-typecheck-canonical-binding.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_semantic_query_parity.c
doc_type: milestone-record
---

# Plan 03 Task 7.70: Canonical Local Binding Identity

## Failure and Contract

The structured local query fixture declared `var result = seed + 1`. Symbol
collection published the local symbol selection at `result`, but the typecheck
pass looked it up with the whole declaration node, whose range starts at `var`.
The lookup missed the canonical symbol and fell back to
`RegisterVariable`, creating a second parser symbol. Reads then carried the
duplicate `SymbolId` and used their own range as the declaration range. LSP
query returned a canonical type but could not bridge it to the LSP symbol, and
local hover reported the read position as the declaration.

The typecheck lookup now uses an identifier variable's pattern range. Successful
lookups register the inferred binding with the existing `SymbolId`, `TypeId`, and
selection range. Runtime-only or otherwise unresolved bindings retain the
existing fallback behavior.

## TDD And Implementation

Before the fix, the new analyzer identity fixture failed on the first inferred
local. The fixture was restored to the old implementation for a direct RED
replay, then the range normalization was applied. It covers inferred locals,
explicitly typed locals, and nested same-name locals. Each case checks declaration
and read roles, shared identity, exact declaration range, declaration node, and
the absence of duplicate `result` records.

## Verification

- GCC parser reference facts: `0` failures; semantic symbols: `30/30`.
- GCC, Clang ASan/UBSan, and MSVC analyzer identity fixture: PASS.
- GCC, Clang ASan/UBSan, and MSVC LSP interface: structured local query and
  local reference hover PASS; the frozen complete-interface failure set moved
  from eight to six plus the existing container-matrix failure (six total).
- GCC, Clang ASan/UBSan, and MSVC source-contract target: PASS.
- GCC, Clang ASan/UBSan, and MSVC semantic-query parity: source snapshot and
  detached-analyzer source hover PASS. The existing local write/reference
  projection failure remains.

The Clang semantic-analyzer executable still reports previously recorded
expression/cleanup/generic failures, an unrelated call-fact UAF in its broad
runner, and sanitizer leaks. The full interface retains the existing closed
generic, exact-type, extern metadata, and container-matrix failures; this slice
does not claim Plan 03 Task 7 or Task 8 completion.

Commands and logs are recorded under `.codex/task770-*` in the local validation
workspace. The build configurations were GCC Debug, Clang ASan/UBSan Debug, and
MSVC static Debug, with network and thread libraries enabled where configured.

## 状态与产出记录

- 开始时间：2026-09-07 08:18:21 +08:00。
- 实际完成时间：2026-09-07 08:30:00 +08:00。
- 状态：Task 7.70 canonical local binding identity 修复、回归和三工具链窄验证完成；
  Plan 03 Task 3、Task 7、Task 8 及完整 sanitizer/interface 门禁继续进行中。
- 完成项目：修正普通变量声明的 identifier pattern lookup range；新增 inferred、explicit
  typed、nested shadowing identity 回归；更新 parser/LSP module boundary 与计划索引。
- 源码版本：基于 `59901684` 的共享工作树；并发 core/parser/AOT/call-binding、stdio、
  benchmark 和其它未提交 overlay 不属于本子项。
- 产出路径：本记录、`docs/parser-and-semantics/lsp-typecheck-canonical-binding.md`、
  `tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h`、
  `tests/language_server/test_semantic_analyzer.c` 与
  `semantic_analyzer_typecheck.c`。
- 剩余门槛：local write/reference projection、closed generic/exact-type/extern/container
  producer failures、Clang LSan 与 broad analyzer UAF，以及 Task 3 sourceless/provider
  generation、Task 7 完整 consumer 矩阵、Task 8 的 16-target、stdio/CLI/WASM/editor/
  performance 验收。
