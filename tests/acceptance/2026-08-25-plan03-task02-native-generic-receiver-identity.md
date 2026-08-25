---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - tests/parser/test_semantic_query_symbols.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-native-generic-receiver-identity.md
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.3b Native Generic Receiver Identity

## Scope

Accept only the parser producer for resolved native generic receiver calls.
Affected layers are type inference, semantic call/reference facts, public
`SymbolAt`/`CallAt` query projection, and parser regression tests.

## Baseline

The RED fixture compiled successfully but its native generic `CALL` fact was
unresolved because the member-symbol producer rejected an external member with
generic parameters. That left `SymbolAt` without a valid target identity.

## Test Inventory

- Inferred and explicit native generic receiver calls share one `SymbolId`.
- Their closed callable `TypeId` values differ.
- `SymbolAt` and `CallAt` project the same target identity.
- Canonical call display preserves the declaration generic clause and the
  respective closed argument/return types.
- External declarations retain zero ranges.
- Existing query, query-contract, canonical-consumer, and compiler diagnostic
  suites guard nearby semantic facts and query behavior.

## Tooling Evidence

MSVC static validation used the isolated
`.codex/build-lsp-plan03-native-generic-msvc-r2` cache. Every listed test was
run as its executable, with the process returning zero:

| Target | Result |
| --- | --- |
| `zr_vm_semantic_query_symbols_test` | 19 tests, 0 failures |
| `zr_vm_semantic_query_test` | 29 tests, 0 failures |
| `zr_vm_semantic_query_contract_test` | 3 tests, 0 failures |
| `zr_vm_canonical_consumers_test` | 19 tests, 0 failures |
| `zr_vm_compiler_semantic_query_diagnostics_test` | 46 tests, 0 failures |

The attempted compiler-integration run overlapped a detached prior invocation.
Its evidence is excluded. Windows GCC 4.8.3 remains blocked before parser
compilation by the shared `_Thread_local` issue; no GCC test result is claimed.
No local Clang executable or WSL distribution is available.

## Results

The first generic call now registers an external declaration identity without
capturing a closed `TypeId`. Later inferred and explicit instantiations reuse
that identity but retain their own closed call types. The test proves no source
range, name lookup, AST pairing, or language-server fallback is used.

## Acceptance Decision

Accepted for the MSVC focused native-generic receiver producer submilestone.
Task 2 is not complete: binary generic artifact projection, broader native
visible facts, external origins/relations, LSP consumer migration, and
cross-toolchain executable gates remain open.

## 状态与产出记录

- 完成时间：2026-08-25 21:28 +08:00。
- 状态：已完成并精确提交。MSVC focused acceptance 已完成；不声明 Task 2、
  binary parity 或跨工具链完成。
- 完成项目：resolved native generic receiver identity、closed callable
  projection、canonical display、negative baseline与focused回归证据。
- 后续项目：`.zro` generic metadata producer parity、native/binary visible
  facts、relation/external-origin query以及LSP consumer迁移。
