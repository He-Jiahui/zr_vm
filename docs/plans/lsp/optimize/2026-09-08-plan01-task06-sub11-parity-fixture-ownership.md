---
plan_id: optimize
task: plan01-task06-sub11
status: completed
related_code:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub11: Parity Fixture Ownership

## 状态与产出记录

- Started: 2026-09-08 00:29 +08:00
- Completed: 2026-09-08 00:39 +08:00
- Source: eacdee1f plus shared working-tree changes and the pending Sub10 fix.
- Status: test cleanup and public-linkage fix accepted.
- Outputs: existing parity fixture cleanup and testing documentation.
- Remaining gates: full LSP sanitizer/memory acceptance and unrelated project failures.

## Evidence and Change

After Sub10 releases the temporary binary IO graph, Clang parity reaches all
20 functional passes but exits 1 with 544 bytes in four allocations. Two
240-byte blocks are owned copies returned by SemanticQuery_TypeAt. The other
64 bytes are the source-hover result and its contents array. The fixture now
releases both inferred types and the hover result on the common cleanup paths.

MSVC also reports LNK2019 for the private Lsp_StringsEqual helper introduced
in the implementation-relation fixture and its included cross-snapshot cases.
All three calls now reject null URIs and use the public ZrCore_String_Equal API,
preserving the expected URI matches.
No production export or string-comparison behavior changes.

RED logs are plan01-task06-sub10-clang-parity.log and
plan01-task06-sub10-msvc-build.log under .codex/lsp-optimize-validation.
The existing 1,600-line test file only receives cleanup and a public API call;
its test grouping is unchanged, and no new test responsibility is appended.

## Validation and Review

All logs use .codex/lsp-optimize-validation/plan01-task06-sub11-.
GCC 11.4 Debug, Clang 14 ASan/UBSan/LSan and MSVC Debug each build the parity
target successfully and pass all 20 cases with exit 0. Clang reports no sanitizer
errors or retained allocations. The unchanged native query copies and hover
assertions still execute.

GCC Valgrind reports 1,295,828 allocations and frees, zero bytes at exit and
zero errors, exit 0. It uses --leak-check=full --show-leak-kinds=all
--errors-for-leak-kinds=all --error-exitcode=99.

```text
cmake --build <build> --target zr_vm_language_server_semantic_query_parity_test -j 10
<build>/bin/zr_vm_language_server_semantic_query_parity_test
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 <gcc-build>/bin/zr_vm_language_server_semantic_query_parity_test
```

Build directories are the same exclusive GCC, Clang and MSVC configurations
listed in Sub09. Windows builds use Invoke-VsDevCommand.ps1 and the executable
suffix .exe. The pending Sub10 source-release fix is present for these runs;
this test-only slice closes the remaining fixture leaks and shared-DLL link.
It does not independently close the lower-level compiler source leak.

Independent read-only review found no actionable issue in the deep-copy frees,
hover lifetime or public string-call preconditions. Stage only these two test
files, their module document/index and this subtask's plan/index entries.
