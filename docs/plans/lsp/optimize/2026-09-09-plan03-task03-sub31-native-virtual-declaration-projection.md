---
plan_id: optimize
task: plan03-task03-sub31
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
tests:
  - tests/language_server/test_lsp_virtual_declaration_projection_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/lsp_query_result_cleanup.h
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.31: Native Virtual Declaration Projection

## Failure And Evidence

Definitions for builtin native functions, constants and module literals return
the correct virtual URI but an empty `(0,0)-(0,0)` range. The initial three new
GCC cases fail with exit 1. Expected selections in `zr-decompiled:/zr.math.zr`
are `sqrt` at `(12,8)+4`, `PI` at `(1,14)+2`, and `zr.math` at `(0,15)+7`.

There are two causes. The native provider publishes a module-entry placeholder
for every member. After replacing that placeholder with rendered coordinates,
the shared document range converter still discards the range when the virtual
document has never been opened. GDB 12.1 stops at that converter with
`start=(13,9,387)`, `end=(13,13,391)`, and `fileVersion=(nil)`. Its stack passes
through the definition location projector into the new regression. The
metadata range is already correct before this second failure.

Evidence is under `.codex/lsp-optimize-validation/`:

- `plan03-task03-sub31-parity-gcc-red.log`
- `plan03-task03-sub31-virtual-range.gdb`
- `plan03-task03-sub31-virtual-range-gdb.log`

## Change And Contracts

Native declaration rendering now belongs to
`metadata/lsp_native_declaration_projection.c`. Every rendered declaration
record carries the exact descriptor-row address and declaration kind. `Find`
requires one matching row and copies its rendered range. Identical spellings
do not merge rows; missing, wrong-kind or ambiguous identities fail closed and
leave an empty output range. The address is an ephemeral projection key, not a
cross-snapshot semantic identity or a replacement for canonical metadata tokens.

The metadata provider uses this projection for builtin virtual module entries
and module-link, function, constant and type members. The shared document range
converter obtains generated native text when no document snapshot is available
and applies the existing content-aware UTF-16 conversion. It does not open or
analyze the generated document. Open documents retain their snapshot contents.

`Build` returns GC-owned text and a caller-owned record array. Descriptor
pointers are borrowed only while that descriptor is alive. `Find` frees its
temporary array before returning a copied range. No descriptor pointer is
stored across provider reload, and no request-time AST inference is added.

`lsp_virtual_documents.c` decreases from 1015 to 371 physical lines; the new
projection implementation is 733 lines. The existing oversized provider and
interface-support files receive only calls from their current projection and
conversion responsibilities. The new regression is a separate 181-line header.

The module-link interface test now checks the rendered `printLine` identifier
for definitions and declaration-inclusive references. A pre-existing private
string-helper call prevented MSVC linking; explicit null guards plus the public
Core string equality API repair that call. The existing parity result cleanup
is shared through a 44-line header and used for this test's location/highlight
entries, removing six test-owned leaks totaling 136 bytes.

## Reference Evidence

- Roslyn `lua/roslyn/src/Features/Core/Portable/MetadataAsSource/DecompilationMetadataAsSourceFileProvider.cs`
  creates a SymbolKey and resolves its location in generated source.
  `MetadataAsSourceHelpers.cs` resolves that key in the generated compilation.
  ZR deliberately rejects unavailable projection identity instead of adopting
  the helper's final empty-location fallback.
- Roslyn `lua/roslyn/src/EditorFeatures/Test/MetadataAsSource/MetadataAsSourceTests.CSharp.cs:399`
  checks the selected generated declaration in `TestExtendedPartialMethod1`.
- rust-analyzer `lua/rust/src/tools/rust-analyzer/crates/ide/src/goto_definition.rs:954`
  tests exact navigation for same-named symbols in different modules.

## Verification

Source is `7af81698` plus this milestone's changes and the existing shared
worktree overlay. The final cleanup changes only test ownership; production
source is identical across the project, snapshot, relation and stdio runs.

| Toolchain | Configuration | Build Directory |
| --- | --- | --- |
| GCC 11 | Debug, static | `/home/hejiahui/.codex-builds/l8-callable-value-gcc` |
| Clang 14 | Debug, shared, ASan/UBSan/LSan | `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang` |
| MSVC 19.44 | Debug, shared | `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current` |

| Toolchain | Relations | Parity | Snapshot CTest | Source Contracts | Full Stdio |
| --- | --- | --- | --- | --- | --- |
| GCC | 29/29, exit 0 | 26/26, exit 0 | 1/1, exit 0 | exit 0 | exit 0, 36.67 MiB |
| Clang | 29/29, exit 0 | 26/26, exit 0 | 1/1, exit 0 | exit 0 | exit 0, 700.08 MiB |
| MSVC | 29/29, exit 0 | 26/26, exit 0 | 1/1, exit 0 | exit 0 | exit 0, 45.23 MiB |

The four new definition cases detach the request-time AST and require exactly
one location selecting the module/function/constant/type identifier. The fifth
case performs 64 projections of two same-named descriptor rows, requires their
distinct lines and correct non-BMP UTF-16 columns in an unopened virtual document,
and rejects a copied descriptor row, wrong kind and missing identity. Clang's
focused relation/parity/snapshot/contracts/stdio runs have no sanitizer report.

All three complete interface runners have 114 PASS / 2 FAIL, exit 1:

- `LSP Class Member Navigation And Completion`: classes fixture diagnostics.
- `LSP Hover And Completion Surface Explicit Exact Type Failures`: missing
  expected exact-failure hover text.

The updated module-link case passes all three compilers. Clang interface LSan
decreases from this milestone's initial 20280 bytes / 428 allocations to 20144
bytes / 422 allocations after test cleanup; the touched test no longer appears
in a leak stack. This is not the older September 7 baseline of 18528 bytes / 384
allocations. The whole interface memory gate remains unaccepted.

All three complete project runners have 55 PASS / 7 FAIL, exit 1, with exactly
the seven failure names recorded in Sub30. Clang project LSan remains 19160
bytes / 481 allocations. Neither aggregate is described as passing.

Build with `cmake --build <build> --target <targets> --parallel 16`; MSVC adds
`--config Debug` and uses the `using-vsdevcmd` wrapper. Targets are:

```text
zr_vm_semantic_query_relations_test
zr_vm_language_server_semantic_query_parity_test
zr_vm_language_server_lsp_interface_test
zr_vm_language_server_lsp_project_features_test
zr_vm_language_server_lsp_semantic_snapshot_test
zr_vm_language_server_lsp_source_contracts_test
zr_vm_language_server_stdio
```

Run the corresponding `<build>/bin/` executables. Snapshot validation uses
`ctest --test-dir <build> --output-on-failure -R '^language_server_lsp_semantic_snapshot$'`;
MSVC adds `-C Debug`. Stdio uses
`node tests/language_server/stdio_smoke.js <stdio-executable> <cli-executable>`.
Clang uses `ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1` and the
established sanitizer-only stdio budget
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824`. GCC/MSVC retain 512 MiB.
Peak bytes are GCC 38453248, Clang 734085120 and MSVC 47427584.

Final logs use prefix `plan03-task03-sub31-`, suffix `{gcc,clang,msvc}.log`, and
groups `relations`, `project`, `snapshot`, `contracts`, `stdio`,
`cleanup-parity` and `cleanup-interface`. An initial overlapping GCC execution
failed with permission denied while linking; it was discarded and rerun only
after the build completed. The initial MSVC private-symbol link failure was
fixed before the final build and executions. All sessions have ended.

## 状态与产出记录

- Started: 2026-09-09 11:04 +08:00.
- Completed: 2026-09-09 11:39 +08:00.
- Status: builtin native virtual declaration projection and its focused
  three-toolchain regression accepted.
- Outputs: projection module, provider and UTF-16 conversion integration,
  descriptor-identity regressions, shared test cleanup, module contract and this
  record; source version and reproducible commands are recorded above.
- Remaining: project-scoped binary/plugin virtual URIs and actual parser
  external-origin producers, multi-definition relation matrix, aggregate
  interface/project failures and memory reports, Plan 03 Tasks 3/7/8 and the
  complete native/browser/desktop-Web acceptance gates.
