---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
implementation_files: []
plan_sources:
  - user: 2026-09-05 review plans and implementation, prioritize functional fixes, delegate implementation, validate performance, and commit each completed item
  - docs/plans/syntax/README.md
  - docs/plans/aot/index.md
  - docs/plans/lsp/index.md
  - docs/plans/debug/index.md
  - docs/plans/using/index.md
  - docs/plans/benchmark/optimize/index.md
tests:
  - tests/CMakeLists.txt
doc_type: category-index
---

# Astra Review And Optimization Implementation Plan

> For agentic workers: use subagent-driven-development and evidence-driven-wsl-validation. Follow the existing main-branch policy. The user has authorized planning followed immediately by delegated implementation and individual commits.

**Goal:** Audit the six plan families against current code and executable evidence, repair demonstrated functional and safety defects first, and meet the existing performance gates without weakening them.

**Architecture:** Compiler semantic facts remain authoritative. Runtime, LSP, debugger, artifact readers and AOT consume those contracts. Reviews are split by existing plan directory; shared source files have one implementation owner. Independent functionality continues while builds, benchmarks and reviews run.

**Tech Stack:** C11, CMake/Ninja, Unity/CTest, WSL GCC/Clang, Windows MSVC, sanitizers, Callgrind, structured benchmark reports.

## Baseline And Preservation

- Repository: `E:/Git/zr_vm`; the request's escaped `zr\_vm` spelling refers to this checkout.
- Initial HEAD: `56837dbd`, branch `main`, 14 commits ahead of `origin/main`.
- Existing tracked edits: `execution_dispatch.c`, `frame_slot_layout_initialization_tests.inc`, two core-runtime documents and two benchmark plans. Existing untracked acceptance: `tests/acceptance/2026-09-01-packed-signed-scalar-frame-base.md`.
- Six third-party submodules are dirty. Preserve these and all preceding changes; stage explicit task paths only.
- Old build paths in documentation do not exist in this checkout. Establish fresh build provenance before citing any historical pass count.
- Coordination notes last updated September 1 are historical context, outside the four-hour active window. The coordination helper fails on an empty result's `Count`; the initial freshness inventory was obtained directly with PowerShell.

## Execution Order

- [x] Read directory indexes, current-state records, benchmark gates and recent coordination state.
- [x] Publish this optimization and review plan before delegating implementation.
- [ ] Inventory each family's detailed plans, implementation entry points, tests and evidence gaps.
- [ ] Rebuild a current WSL GCC baseline and classify every failure by responsible lower layer.
- [ ] Repair functional, lifecycle, malformed-input and contract defects with failing regressions first.
- [ ] Validate changed modules with WSL GCC and Clang; add sanitizers for memory/lifetime changes and Windows MSVC compatibility checks.
- [ ] Repair benchmark execution/reporting gaps; collect valid comparable samples and optimize measured hotspots.
- [ ] Review each patch for contract compliance and code quality, update module/acceptance documents, then commit it independently.
- [ ] Replay integrated functional and performance gates and reconcile all directory status records.

## Directory Work Packages

Each package writes its detailed findings and actionable repair plan before changing production code. Findings must identify severity, trigger, source path and line, observed versus intended behavior, regression test, repair ownership and acceptance gate. Historical claims are labeled as such until replayed.

| Package | Plan input | Output | First executable work |
|---|---|---|---|
| Compiler foundations | Syntax 01-07 | `syntax/foundations-review.md` | Inspect canonical types, borrow/ownership, properties, artifact/fixture contracts; reproduce the highest-priority concrete defect. |
| Libraries and advanced syntax | Syntax 08-14 | `syntax/advanced-review.md` | Inspect reflection, pooling, FFI, generated metadata, Task/Iterator/TestManifest boundary and cleanup tests. |
| Ownership and using | Using 00-07 | `using/review.md` | Check Close/Drop separation and abrupt cleanup, confirm surfacePending boundaries, repair demonstrated lifecycle defects. |
| LSP | LSP 00-05 and optimize ledger | `lsp/review.md` | Reproduce outstanding semantic analyzer/reference/ownership regressions after canonical fact migration; repair producers before consumers. |
| Debug | Debug 01-07 | `debug/review.md` | Check generation safety, DAP input handling, pause/resume/frame references and disabled-hook overhead. |
| AOT | AOT 00-12 and traceability | `aot/review.md` | Verify archived/current build ownership and four-backend claims; repair usable execution or artifact safety gaps. |
| Performance | Benchmark optimize 00-05 | `benchmark/review.md` | Verify persistent runner and statistics contracts; establish baseline and executable performance targets. |

## Functional Gates

1. Reproduce each defect before fixing it, preferably through an existing leaf test target. Do not update expected output to accept wrong behavior.
2. Record exact configure/build/test commands, compiler version, source revision, exit status and failure count in `tests/acceptance/2026-09-05-astra-<topic>.md`.
3. Validate lower-layer tests, affected integration tests and malformed/boundary/lifecycle cases. Full integrated CTest must not conceal failures behind skipped tests or disabled capabilities.
4. Keep capability gaps explicit: absent AOT backends, unfrozen using syntax and missing compiler facts do not become supported through fallback inference.

## Performance Gates

- Use the original benchmark scope and algorithm/checksum contracts, with environment identity and raw structured reports.
- Valid steady-state samples require CV below 5%; cold-start, bytecode loading and native execution remain separate implementations/scopes.
- Accept an optimization only with at least 3% benefit and a 95% confidence interval excluding zero; preserve original representative-case and memory limits.
- Intermediate interpreter gate: numeric/control/call representative geomean `ZR/Lua <= 3.0`.
- Final interpreter gate: full representative geomean `ZR/Lua <= 2.0`, each case `<= 5.0`, and no unexplained RSS growth above 5%.
- Native/AOT gate: native coverage above 90%, geomean `ZR/Lua <= 1.25`, `ZR/QuickJS <= 1.25`, `ZR/.NET <= 1.5`, and the original per-case limits.
- Callgrind instruction reductions are useful hotspot evidence and cannot substitute for a failed or missing wall-clock gate.
- Serialize controlled performance sampling with other CPU-heavy work when measurements require it. Continue independent source review, documentation and functional fixes while samples run.

## Ownership And Commits

- Root owns this index, integration builds and Git index/commit operations. Workers report exact changed paths; they do not stage or commit concurrently.
- Before touching a shared producer/runtime/header, workers report the proposed file ownership. Root resolves overlap without waiting for another user approval.
- An independent domain may implement after recording its plan and confirming ownership. Root reviews its diff and evidence before committing.
- Commit messages identify the actual defect or optimization. Every code commit includes its focused regression and applicable module/acceptance documentation.
- Task state progresses through `reviewing`, `planned`, `reproduced`, `implemented`, `verified`, `committed`. A domain or the overall goal remains open while required gates remain unmet.

## Review Ledger

Detailed records will replace review work-package entries with evidence-backed findings as each directory audit completes. The overall status is **in progress**; no historical test or performance completion is promoted by this initial plan.
