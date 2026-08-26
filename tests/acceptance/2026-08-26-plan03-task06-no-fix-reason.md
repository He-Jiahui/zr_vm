---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fixes.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_copy.c
tests:
  - tests/parser/test_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.1 Explicit No-Fix Reason

## Required Results

- A structured diagnostic can publish one defined no-fix reason.
- Typed fixes and a no-fix reason are mutually exclusive in either mutation
  order.
- Diagnostic deep-copy preserves the reason and rejects inconsistent input.
- Persistent semantic facts and the materialized borrowed query view retain
  the same reason after the producer object is freed.
- No consumer derives fixability from message, code, source spelling, or an
  empty fix array.

## Evidence

WSL GCC 11.4, WSL Clang 14, and MSVC 19.44 static builds directly execute
`zr_vm_semantic_query_diagnostics_test` at 2 Tests/0 Failures,
`zr_vm_semantic_query_contract_test` at 4/0, and
`zr_vm_compiler_semantic_query_diagnostics_test` at 46/0. Every test sequence
has process exit zero. GCC and Clang use one fixed ext4 snapshot of
`da9b86c654dc + 5 code/test/CMake overlays` with separate build directories.

## Acceptance Decision

Accepted for the parser-owned no-fix disposition and semantic-query copy
contract. Producer classification coverage, LSP protocol projection, and
compiler/LSP golden parity remain outside this acceptance record.

## 状态与产出记录

- 完成时间：2026-08-26 16:47 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：explicit no-fix reason、mutual exclusion、deep-copy/query
  preservation 和 fail-closed mutation tests。
