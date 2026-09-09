---
plan_id: optimize
task: plan03-task03-sub30
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
tests:
  - tests/language_server/test_lsp_multi_project_provider_generation_cases.h
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_semantic_snapshot.c
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.30: Multi-Project Provider Acquisition

## Failure And Cause

The new binary/native matrix opens two projects with the same module/member
names and initially identical signatures. Replacing only project A's provider
advances the context-wide generation. Project A is eagerly reanalyzed, while
project B's analyzer is invalidated when next looked up.

Both cases initially fail at reload round 0, project B's local hover. The GCC
runner increases from seven to nine failures and exits 1. GDB 12.1 stops at the
local expression query with `provider=2`, `analyzer=2`, a non-null AST and
`semanticContext=(nil)`. The stack is local expression query, GetHover,
`provider_matrix_check_project`, and the multi-project test. Analyzer lookup
correctly hides the old facts, but the acquisition fast path considers the
retained AST sufficient and never rebuilds them.

A second RED sets the existing cache storage limit to zero halfway through the
matrix. Both cases fail at round 4, project A: the query leaves cache storage
above the configured limit. The acquisition boundary must enforce the limit
even when watched reload has already rebuilt that project's analyzer.

Local evidence is under `.codex/lsp-optimize-validation/`:

- `plan03-task03-sub30-project-gcc-red.log`
- `plan03-task03-sub30-provider-generation.gdb`
- `plan03-task03-sub30-provider-generation-gdb.log`
- `plan03-task03-sub30-project-gcc-budget-red.log`

## Change And Lifetime

`LspSemanticQuery_TryGetAnalyzerForUri` now reanalyzes a retained AST when its
semantic context is unavailable. It uses the existing project-aware analysis
entry point, which installs the correct project's loader and native descriptor
provider before compiler publication. Failed analysis returns failure. The
existing LRU enforcement runs before the acquired analyzer is returned.

Hover acquires the analyzer before borrowing symbol-table entries. Local
expression/reference queries use the same acquisition boundary. Request
snapshot acquisition completes analysis before capturing its identity and
dependency set. Canonical fact queries remain read-only once acquisition has
completed; no type/name reconstruction or public export is added.

The analyzer continues owning its compiler context. Generation invalidation
retains the AST and hides stale borrowed facts; successful analysis rebuilds
facts with the current nonzero generation. LRU eviction releases cache storage
without discarding canonical facts. Query structs are freed before provider
replacement, and old snapshots are used only for validation and release.
The tests do not dereference a borrowed query result across reload.

The new matrix lives in a separate test header with only an include and two
registrations added to the large runner. Production edits change existing
acquisition call sites and one fast path, with no new responsibility or helper
family. The existing oversized interface/query files remain orchestration
boundaries for this bounded fix; their separate Plan 06 modularization gate
remains open.

## Reference Evidence

- Roslyn `lua/roslyn/src/Workspaces/Core/Portable/Workspace/Solution/SolutionState.cs:910`
  updates metadata references through the selected ProjectId and forks that
  project state. `lua/roslyn/src/Workspaces/CoreTest/SolutionTests/SolutionTests.cs:2134`
  tests that project-specific property and rejects an unknown project id.
- rust-analyzer `lua/rust/src/tools/rust-analyzer/crates/base-db/src/change.rs:48`
  applies changed files through their source roots and updates crate graph
  inputs explicitly. This supports keeping provider selection project-scoped.
- ZR retains its conservative context-wide provider generation. An unrelated
  project can require fresh analysis, but its type, provider URI and reference
  set must still come from its own project. No upstream language semantics are
  introduced by this cache/acquisition fix.

## Regression Contract

Each binary/native test performs eight provider replacements on project A,
alternating `int` and `float`; project B always retains `int`. Both projects
must publish current canonical primitive types and complete external member
identities. Definitions must resolve to the selected project's own artifact;
references must contain exactly its member range and exclude the other project.

The first request after reload rotates between hover, snapshot acquisition,
local expression facts, member resolution and local reference facts. Admitted
snapshots must stay valid through the current-generation queries. A snapshot
captured before provider replacement must fail validation afterward. Rounds
4-7 run with a zero cache storage budget and still require canonical results.

## Verification

Source is `7e99539d` plus the four production edits, the new regression and the
existing shared worktree changes. These builds use the same source directory:

| Toolchain | Configuration | Build Directory |
| --- | --- | --- |
| GCC | Debug, static | `/home/hejiahui/.codex-builds/l8-callable-value-gcc` |
| Clang | Debug, shared, ASan/UBSan/LSan | `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang` |
| MSVC | Debug, shared | `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current` |

Final verification includes cache-limit enforcement and the zero-budget cases:

| Toolchain | Project Runner | Parity | Snapshot CTest | Full Stdio Smoke |
| --- | --- | --- | --- | --- |
| GCC | 55 PASS / 7 FAIL, exit 1 | 21/21, exit 0 | 1/1, exit 0 | exit 0, 36.59 MiB |
| Clang | 55 PASS / 7 FAIL, exit 1 | 21/21, exit 0 | 1/1, exit 0 | exit 0, 713.79 MiB |
| MSVC | 55 PASS / 7 FAIL, exit 1 | 21/21, exit 0 | 1/1, exit 0 | exit 0, 45.08 MiB |

Both new cases pass on every compiler, including all eight replacements and
the four zero-budget rounds. Each full project failure-name set exactly matches
the seven-failure Sub28 GCC baseline. Clang parity, snapshot and stdio have no
sanitizer report. Full stdio smoke exercises its protocol, latency, memory,
shutdown, exit-status and empty-stderr assertions with the final implementation.
Peak bytes are GCC `38363136`, Clang `748466176` and MSVC `47267840`.

Final logs are `.codex/lsp-optimize-validation/plan03-task03-sub30-`
`{project,parity,snapshot,stdio}-{gcc,clang,msvc}.log`. Build commands and
executables all complete; no test timeout is counted as passing.

Build targets with `cmake --build <build> --target ... --parallel 16`:

```text
zr_vm_language_server_lsp_project_features_test
zr_vm_language_server_semantic_query_parity_test
zr_vm_language_server_lsp_semantic_snapshot_test
zr_vm_language_server_stdio
zr_vm_cli_executable
```

Run project/parity executables from `<build>/bin/`; snapshot uses
`ctest --test-dir <build> --output-on-failure -R '^language_server_lsp_semantic_snapshot$'`.
Run `node tests/language_server/stdio_smoke.js <stdio-executable> <cli-executable>`.
Clang uses `ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1`, and
the established sanitizer-only stdio limit
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824`. GCC/MSVC retain the default
512 MiB production limit. Windows build/CTest use the `using-vsdevcmd` wrapper;
CTest arguments are passed as a `-Command` array so `-C` cannot bind to the
PowerShell wrapper's parameter. The initial abbreviated MSVC CLI target was
corrected to `zr_vm_cli_executable` before running its smoke.

Complete project runners retain the same seven historical failures:

- LSP Auto Discovers Project From Source File
- LSP Imported Constructor And Meta Call Infer Through Module Type
- LSP Relative And Alias Import Literal Navigation And Hover
- LSP Network Native Members Semantic Tokens Cover Chain And Receivers
- LSP Semantic Tokens Cover External Metadata Members
- LSP Semantic Tokens Cover Native Value Constructor Members
- LSP Pooling Hover Completion And Projection Expose Guard Contract

Clang's full project runner retains the known `19160 bytes / 481 allocations`
LSan report, matching the
[earlier baseline](2026-09-07-plan01-task06-sub07-rename-canonical-type-assertions.md).
The complete project and parent memory gates are not accepted by this slice.

## Status And Outputs

- Started: 2026-09-09, continuing after Sub29.
- Completed: 2026-09-09 11:00 +08:00.
- Status: multi-project acquisition and cache-budget regression accepted across
  GCC, Clang and MSVC; complete project/memory gates remain open.
- Outputs: multi-project regression, shared acquisition fix, module contract
  and this evidence record.
- Remaining: full project runner failures/LSan, sourceless virtual declaration
  URI and multi-definition relation coverage, Plan 03 Tasks 3/7/8 and the full
  native/Web acceptance matrix.
