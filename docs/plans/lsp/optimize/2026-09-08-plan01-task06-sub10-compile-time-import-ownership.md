---
plan_id: optimize
task: plan01-task06-sub10
status: completed
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - tests/parser/test_compile_time_import_ownership.c
  - tests/cmake/zr_vm_compile_time_import_ownership_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub10: Compile-Time Import Ownership

## 状态与产出记录

- Started: 2026-09-08 00:12 +08:00
- Completed: 2026-09-08 00:40 +08:00
- Status: decoded-source ownership fix accepted.
- Source: eacdee1f plus shared working-tree changes.
- Outputs: compile-time import source cleanup, ownership tests and module docs.
- Remaining gates: full LSP sanitizer/peak-memory acceptance and unrelated project failures.

## Evidence and Scope

Sub08's Clang type-inference test reaches all 124 functional passes, then leaks
5,187 bytes in 44 allocations. The direct allocation is an SZrIoSource created by
compile_statement_load_imported_compile_time_module. Its binary path closes the
reader but never releases the decoded graph, on either success or failure.

Projection helpers allocate their own native arrays and preserve GC-managed
strings/values. Tests will require all IO allocations and exact sizes to balance
before compilation returns, and inspect projected defaults after graph release.
A rejected compile-time projection covers the collection-failure cleanup path.

The intended production change only adds missing source releases to existing
exits. compile_statement.c is already oversized; no new helper responsibility is
being appended. Extracting its shared source/binary projection and cleanup code
would broaden this ownership fix, so that structural work remains a separate
compile-time import module boundary.

## RED and Change

GCC's new ownership executable fails all three cases before the fix: ordinary
binary import allocates 90 IO blocks but frees only 45; successful and rejected
compile-time projections each allocate 26 but free only 13. Every input reader
is closed, isolating the missing decoded-graph ownership rather than stream
ownership. Log: plan01-task06-sub10-gcc-red-tests.log in the validation directory.

Four calls to ZrCore_Io_ReadSourceFree now cover the invalid graph, module
allocation failure, projection failure and successful publication exits.
Projection and cache logic are unchanged.

## Validation

All logs use .codex/lsp-optimize-validation/plan01-task06-sub10-; the final
parity results use Sub11's prefix after its independent fixture cleanup.

| Check | GCC 11.4 Debug | Clang 14 ASan/UBSan/LSan | MSVC Debug |
| --- | --- | --- | --- |
| Ownership regression | 4/4, exit 0 | 4/4, exit 0 | 4/4, exit 0 |
| Related CTest | 3/3, exit 0 | 3/3, exit 0 | 3/3, exit 0 |
| Existing type inference | 124/124, exit 0 | 124/124, exit 0 | 124/124, exit 0 |
| Extended query parity with Sub11 | 20/20, exit 0 | 20/20, exit 0 | 20/20, exit 0 |

The CTest group contains the four ownership cases, three binary metadata source
cases and four LSP cast-operand cases. The fourth ownership case rejects the
second function after successfully projecting the first, checking the partial
cleanup order under sanitizers. GCC Valgrind reports 4,767 allocations and frees,
zero bytes at exit and zero errors, exit 0.

```text
cmake --build <build> --target zr_vm_compile_time_import_ownership_test zr_vm_binary_metadata_source_test zr_vm_language_server_cast_operand_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_query_parity_test -j 10
ctest --test-dir <build> -R "^(compile_time_import_ownership|binary_metadata_source|language_server_cast_operand_facts)$" --output-on-failure
<build>/bin/zr_vm_type_inference_test
<build>/bin/zr_vm_language_server_semantic_query_parity_test
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 <gcc-build>/bin/zr_vm_compile_time_import_ownership_test
```

Build paths and toolchain options are unchanged from Sub09. Windows uses
Invoke-VsDevCommand.ps1 and .exe suffixes. Clang's original 5,187-byte,
44-allocation type-inference leak is gone without disabling leak detection.
Parity initially retains 544 test-owned bytes after this production fix;
Sub11, committed as 507a5b4c, releases those query results and fixes MSVC linkage.

## Review and Commit Boundary

Independent read-only review confirms that function arrays, inferred element
types and variable path bindings are separately allocated, while retained names
and constant objects are GC-managed. No dangling IO-native reference was found.
The added partial-projection failure case addresses the review's cleanup-path
coverage observation. Object defaults and variable path-binding combinations
remain outside this focused four-case test; the full parent matrix stays open.

Stage the four-call compiler fix, new ownership test/CMake fragment, only its
include in tests/CMakeLists.txt, module document/index and this task's plan/index
entries. Shared call-binding, checkpoint, provider-generation and other changes
are not part of this commit.
