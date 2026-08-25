---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-source-type-members.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2j Source Type Member Visibility Facts

## Scope

Validate canonical `VisibleSymbols` facts for source struct, class, and
interface fields and methods. A receiver candidate must originate from the
compiler-registered declaration symbol, not a member name, a source range
guess, or an LSP-side reconstruction.

## Test Inventory

`test_visible_symbols_projects_source_type_members` compiles `class Meter`
with instance/static fields and instance/static methods. It verifies exact
field symbols, opt-in receiver visibility, owner identity, and static-method
exclusion of instance members.

`test_visible_symbols_projects_source_struct_and_interface_members` compiles
`struct Point` and `interface Readable`. It verifies their field symbols and
that the receiver query returns those same exact identities from the owning
method/signature scopes.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 15/15, 29/29, 3/3, 46/46, 127/127; zero failures; process exit 0 |
| Windows GCC 4.8.3 static | fresh focused symbols build | blocked before parser compilation by unchanged core `_Thread_local` incompatibility |
| WSL GCC | isolated cache configure | stopped in 9P I/O wait before any compile/test result |
| Clang | executable target | not claimed; existing static-link ABI blocker remains outside this child |

## Acceptance Decision

Accepted for the MSVC source type-member producer submilestone. Cross-toolchain
executable acceptance remains open and is explicitly not substituted with an
LSP fallback. Imports/aliases, binary/native source parity, and Task 2 LSP
consumer migration remain open.

## 状态与产出记录

- 完成时间：2026-08-25 15:55:56 +08:00
- 状态：MSVC acceptance passed；GCC/Clang executable gates 未完成，原因与
  证据如上，未做通过声明。
- 完成项目：canonical source type member identity、receiver/static visibility
  filtering、exact owner/range assertions、focused query and compiler
  integration evidence。
- 后续项目：恢复跨工具链验证，继续 imports/aliases、binary/native producers 与
  LSP consumer migration。
