# Syntax-Dependent Plan System Rewrite Implementation Plan

> Status: completed on 2026-07-19. The checklist below is retained as the reproducible rewrite and audit procedure.
>
> Completion audit amendment: the first rewrite was structurally correct but too compressed. A second audit expanded every implementation plan with explicit inputs, deliverables, failure boundaries, existing test entry points and promotion evidence; all four indexes now map syntax designs 01-10, and concurrent AOT 08/10/11 completion records are linked from their owning plans.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the obsolete AOT, using, LSP, and debug plans in place with concise, dependency-ordered plans derived from the approved syntax and memory model, while adding the confirmed native/FFI/module/package contract.

**Architecture:** `docs/plans/syntax` is the only target-language authority. The four consumer plan sets project that authority into execution, lifetime, language-service, and debugging work without redefining syntax; historical execution logs leave plan bodies and remain discoverable through acceptance records and session notes.

**Tech Stack:** Markdown, ZR design examples, JSON `.zrp` examples, PowerShell validation, repository-local syntax and module documentation.

---

## File Responsibilities

- `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`: canonical native-library inventory, NativeExtern/FfiSignature lowering, ModuleSpecifier normalization, `.zrm`, `#alias`, `@package`, and `.zrp` v2 schema.
- `docs/plans/syntax/README.md`: authority order and dependency index including design 10.
- `docs/plans/syntax/2026-07-18-zr-syntax-and-memory-model-redesign.md`: total-design cross-reference and confirmed defaults.
- `docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md`: executable import/extern/package examples and negative fixture inventory.
- `docs/plans/aot/*.md`: execution and artifact projection of Canonical TypeRef, Place/SemIR/CFG, layout, ownership, reflection, FFI, and module contracts.
- `docs/plans/using/*.md`: deterministic lifetime plan in which owner Drop, Close/Dispose `using`, pattern matching, and plugin loading have separate responsibilities.
- `docs/plans/lsp/*.md`: shared semantic-query, diagnostics, migration, module-resolution, and incremental-snapshot plan.
- `docs/plans/debug/*.md`: DebugMap, Place/value inspection, ownership/ref/pool views, DAP/REPL, profiling, and acceptance plan.
- `.codex/sessions/20260719-0451-plan-system-rewrite.md`: live coordination only; remove after completion.

### Task 0: Extract Reusable Completion Evidence Before Rewriting

**Files:**
- Read: `docs/plans/aot/**/*.md`
- Read: `docs/plans/using/*.md`
- Read: `docs/plans/lsp/*.md`
- Read: `docs/plans/debug/*.md`
- Create: `docs/plans/aot/<plan-id>/<detail>.md`
- Create: `docs/plans/using/<plan-id>/<detail>.md`
- Create: `docs/plans/lsp/<plan-id>/<detail>.md`
- Create: `docs/plans/debug/<plan-id>/<detail>.md`

- [x] **Step 1: Classify old completion records.**

  A record is reusable only when it has a bounded capability, concrete implementation or artifact evidence, named tests or commands, and semantics that remain valid under `docs/plans/syntax`. Records based on old surface syntax may be retained only as migration/baseline evidence. Repeated micro-slice narration, transient failures, unverified broad completion claims, and concrete type-name/syntax special cases are removed.

- [x] **Step 2: Consolidate reusable evidence by stable capability.**

  Do not create one file per historical micro-case. Merge related slices into one detail record whose frontmatter contains `plan_id`, `record_id`, `status: completed`, `completed_at`, `source_plans`, and `evidence_scope`. The body must distinguish proven historical behavior from target-v2 work that remains open.

- [x] **Step 3: Use stable plan-id directories.**

  Use these directory identities:

  ```text
  aot/{00-baseline,02-type-layout,03-semir-execir,04-aot-lowering,05-ownership-gc,07-codegen,08-generics,09-memory,10-reflection,11-metadata,12-stripping}
  using/{01-ownership,02-resource-scope,03-metadata,04-unions,05-migration,06-semantics}
  lsp/{01-semantic-core,02-diagnostics,03-robustness,04-debug-repl,05-roadmap}
  debug/{01-core-hooks,02-introspection,03-traceback,04-zr-debug,05-dap,06-profiling,07-acceptance}
  ```

- [x] **Step 4: Link migrated records from their owning rewritten plan.**

  Each plan has a short `完成记录` section containing links only. Execution chronology and test transcripts must not return to the plan body.

- [x] **Step 5: Validate the evidence migration.**

  Run:

  ```powershell
  Get-ChildItem docs/plans/aot,docs/plans/using,docs/plans/lsp,docs/plans/debug -Directory -Recurse |
    Get-ChildItem -File -Filter *.md |
    Select-String -Pattern '^plan_id:|^record_id:|^status: completed$'
  ```

  Expected: every migrated detail file exposes all three stable metadata fields; no rewritten plan body contains rolling completion logs.

### Task 1: Freeze Shared Authority And Module/FFI Contracts

**Files:**
- Create: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`
- Modify: `docs/plans/syntax/README.md`
- Modify: `docs/plans/syntax/2026-07-18-zr-syntax-and-memory-model-redesign.md`
- Modify: `docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md`

- [x] **Step 1: Record the current and target `zr.*` native namespace inventory.**

  Include current descriptor evidence, normalize the bare `debug` module to `zr.debug`, reserve `zr.reflection`, `zr.reflection.declaration`, and `zr.pooling`, and prohibit compiler branches keyed by concrete module/type names.

- [x] **Step 2: Define typed NativeExtern lowering.**

  Specify `native extern(...)` source declarations, Canonical CallableContract input, persistent FfiSignature output, ABI/calling convention, symbol, direction, string encoding, pointer/nullability, ownership, layout, callback, pinning, error, and platform constraints. Dynamic `zr.ffi` remains the explicit runtime-discovery API.

- [x] **Step 3: Define ModuleSpecifier normalization and `.zrp` v2.**

  Lock these equivalences and identities:

  ```text
  core.math.quaternion == core/math/quaternion
  .math.quaternion     == ./math/quaternion
  ..math.quaternion    == ../math/quaternion
  #lib/tool            == aliases["#lib"] + /tool
  @math.matrix         == @math/matrix
  ```

  Package names are exactly one `@identifier` segment. An explicit `.zrm` locator imports its manifest entry; package submodules resolve through the package export map. Logical imports never become unrestricted host filesystem paths.

- [x] **Step 4: Extend the comprehensive syntax reference.**

  Add positive and negative examples for builtin, absolute logical, relative dotted/slash, `#alias`, `@package`, `.zrm`, duplicate spellings, root escape, unknown alias/package, non-exported package module, ABI mismatch, and stale artifact cases.

- [x] **Step 5: Validate task 1.**

  Run:

  ```powershell
  $files = Get-ChildItem docs/plans/syntax -File -Filter *.md
  $files | Select-String -Pattern 'TBD|TODO|fill in|implement later'
  ```

  Expected: no unresolved placeholder in the new or modified target-design prose.

### Task 2: Rewrite The AOT Plan Set In Place

**Files:**
- Modify: `docs/plans/aot/index.md`
- Modify: `docs/plans/aot/00-current-state.md`
- Modify: `docs/plans/aot/01-design-principles.md`
- Modify: `docs/plans/aot/02-typed-value-and-layout.md`
- Modify: `docs/plans/aot/03-instruction-set-refactor.md`
- Modify: `docs/plans/aot/04-semir-and-c-backend.md`
- Modify: `docs/plans/aot/05-ownership-gc-and-bridge.md`
- Modify: `docs/plans/aot/06-implementation-blueprint.md`
- Modify: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
- Modify: `docs/plans/aot/08-generic-sharing.md`
- Modify: `docs/plans/aot/09-memory-management.md`
- Modify: `docs/plans/aot/10-reflection.md`
- Modify: `docs/plans/aot/11-metadata.md`
- Modify: `docs/plans/aot/12-code-stripping.md`
- Modify: `docs/plans/aot/07-12-codegen/2026-07-19-08-s6v-10-s4z43-11-s5a.md`

- [x] **Step 1: Replace historical state narration with a bounded baseline.**

  Separate implemented evidence, compatibility debt, target state, and non-claims. Declare AST/bytecode/type-name inference invalid as AOT semantic sources.

- [x] **Step 2: Rebuild the lower-to-upper execution chain.**

  Use Canonical Type graph -> Bound/Semantic IR -> Place/CFG facts -> validated public contracts -> ExecIR/ExecBC -> VM/AOT C/LLVM. Define destination-first value construction and typed call/return ABI.

- [x] **Step 3: Project layout, ownership, reflection, FFI, and module contracts.**

  Cover struct/ref struct/Span, copy/move/drop glue, GC maps/barriers, pooling NoScan proof, generic sharing, `zr.reflection`, FfiSignature stubs, ModuleId/AssemblyRef, `.zrm`, and trimming roots.

- [x] **Step 4: Replace micro-slice logs with milestone gates.**

  Every milestone lists goal, full in-scope behavior inventory, dependencies, parser/SemIR/core/AOT/artifact/project tests, performance guardrails, and exact exit evidence. Reusable portions of the dated codegen record move to the owning `08-generics`, `10-reflection`, and `11-metadata` detail records; the old cross-stage path is removed after link validation.

- [x] **Step 5: Validate task 2.**

  Run:

  ```powershell
  Select-String -Path docs/plans/aot/*.md -Pattern '最新阶段|full repository GREEN is not claimed'
  Select-String -Path docs/plans/aot/*.md -Pattern 'Place IR|Semantic IR|Canonical TypeRef'
  ```

  Expected: no rolling execution-log markers; every architectural plan contains an applicable canonical semantic reference.

### Task 3: Rewrite The Using/Lifetime Plan Set In Place

**Files:**
- Modify: `docs/plans/using/index.md`
- Modify: `docs/plans/using/00-current-state.md`
- Modify: `docs/plans/using/01-ownership-as-generics.md`
- Modify: `docs/plans/using/02-using-scopes-and-plugin-guards.md`
- Modify: `docs/plans/using/03-metadata-and-token-model.md`
- Modify: `docs/plans/using/04-union-types.md`
- Modify: `docs/plans/using/05-migration-and-phasing.md`
- Modify: `docs/plans/using/06-syntax-and-semantic-checks.md`
- Modify: `docs/plans/using/07-implementation-blueprint.md`

- [x] **Step 1: Split the four previously conflated responsibilities.**

  Owner lifetime uses `Unique<T>`/`Shared<T>`/`Weak<T>`, automatic Drop, and `drop(value)`; Close/Dispose alone uses `using`; patterns use `if let`/`switch`; plugins use `loadPlugin` plus result/union handling.

- [x] **Step 2: Keep Close/Dispose grammar explicitly pending.**

  Define the cleanup and effect contract without inventing syntax while syntax design 07 marks the statement surface `surfacePending`. Require identical normal/return/throw/break/continue cleanup plans and reject suspension unless the protocol explicitly supports async cleanup.

- [x] **Step 3: Replace ownership string/generic special cases with canonical contracts.**

  Project owner/ref/read-only types, Place availability/borrowing facts, DropContract, bridge roots, artifact signatures, and diagnostics from syntax designs 01-04 and 09.

- [x] **Step 4: Define migration and complete acceptance matrices.**

  Include `%using` role classification, `%borrow/%loan/%unique/%shared/%weak` migration, field-scope removal, plugin/pattern manual-review edits, cleanup CFG goldens, and VM/AOT parity.

- [x] **Step 5: Validate task 3.**

  Run:

  ```powershell
  Select-String -Path docs/plans/using/*.md -Pattern '%import|%using|%borrow|%loan' | Where-Object Line -NotMatch 'legacy|migration|旧|负例'
  ```

  Expected: no old spelling presented as current syntax.

### Task 4: Rewrite The LSP Plan Set In Place

**Files:**
- Modify: `docs/plans/lsp/index.md`
- Modify: `docs/plans/lsp/00-current-state.md`
- Modify: `docs/plans/lsp/01-semantic-inference-core.md`
- Modify: `docs/plans/lsp/02-diagnostics-and-errors.md`
- Modify: `docs/plans/lsp/03-lsp-robustness-and-position.md`
- Modify: `docs/plans/lsp/04-debug-and-repl.md`
- Modify: `docs/plans/lsp/05-implementation-blueprint.md`

- [x] **Step 1: Replace expression-shape inference with shared query architecture.**

  Document position resolves to revision-scoped SyntaxId/SymbolId/TypeId/PlaceId/BlockId and causal facts. LSP never recomputes borrow, range, ownership, construction, or module semantics.

- [x] **Step 2: Define target diagnostics and language intelligence.**

  Cover `fn/ref/in/out/scoped/readonly`, moved/uninitialized/borrow conflicts, ref-like escape, resource Drop, property/ref return, `init/new/own/@call/createInstance`, module/package/FFI resolution, hover, signature help, completion, semantic tokens, inlay hints, navigation, rename, and references.

- [x] **Step 3: Define migration and workspace edits.**

  Reuse syntax design 06 reports and causal facts; require idempotent, revision-checked, non-overlapping edits and explicit manual-review results for control-flow-changing rewrites.

- [x] **Step 4: Define artifacts, incremental invalidation, and performance budgets.**

  Specify `.zrs/.zri/.zro/.zrm` consumption, ModuleId dependency invalidation, package aliases, partial documents, cancellation, bounded caches, and p50/p95/p99 latency/memory gates.

- [x] **Step 5: Validate task 4.**

  Run:

  ```powershell
  Select-String -Path docs/plans/lsp/*.md -Pattern 'Canonical TypeRef|PlaceId|ModuleSpecifier|revision'
  Select-String -Path docs/plans/lsp/*.md -Pattern 'zero-minus|bitwise-not direct leaf'
  ```

  Expected: shared identities and revision semantics are present; old expression-microcase plan prose is absent.

### Task 5: Rewrite The Debug Plan Set In Place

**Files:**
- Modify: `docs/plans/debug/index.md`
- Modify: `docs/plans/debug/01-core-hook-fixes.md`
- Modify: `docs/plans/debug/02-introspection-api.md`
- Modify: `docs/plans/debug/03-traceback-and-errors.md`
- Modify: `docs/plans/debug/04-script-debug-library.md`
- Modify: `docs/plans/debug/05-dap-agent-enhancements.md`
- Modify: `docs/plans/debug/06-profiling-and-tooling.md`
- Modify: `docs/plans/debug/07-testing-and-acceptance.md`

- [x] **Step 1: Define DebugMap as the shared source/runtime contract.**

  Map instruction/native-PC ranges to module, function, source, inline chain, scope, SymbolId, PlaceId/value location, and availability; preserve stable identity through AOT relocation and metadata trimming.

- [x] **Step 2: Define safe inspection for new value categories.**

  Cover inline struct/ref struct/Span, ref/readonly views, moved/uninitialized places, Unique/Shared/Weak, PoolHandle/PoolRef generations, property access without implicit getter execution, GC objects, resource classes, and optimized-out values.

- [x] **Step 3: Define control, exception, async/thread, and cleanup behavior.**

  Include stepping across inlining/tail calls, exception and Drop cleanup, async logical stacks, native callbacks, thread stops, safepoints, reentrancy, and fail-closed behavior.

- [x] **Step 4: Define `zr.debug`, DAP/REPL, profiling, security, and acceptance.**

  Normalize the current bare `debug` module to `zr.debug`, separate trusted/sandboxed capabilities, consume shared semantic facts, and test VM/AOT/source/binary/trimmed matrices under load.

- [x] **Step 5: Validate task 5.**

  Run:

  ```powershell
  Select-String -Path docs/plans/debug/*.md -Pattern 'DebugMap|PlaceId|PoolRef|zr.debug'
  ```

  Expected: each cross-cutting contract appears in its owning plan and the index links all plans.

### Task 6: Cross-Plan Consistency And Completion Audit

**Files:**
- Modify: `docs/plans/2026-07-19-syntax-dependent-plan-system-rewrite.md`
- Modify then remove on completion: `.codex/sessions/20260719-0451-plan-system-rewrite.md`

- [x] **Step 1: Validate Markdown links and anchors.**

  Parse every local Markdown link under the five plan directories, resolve relative paths, and report every missing target.

- [x] **Step 2: Validate terminology and forbidden current spellings.**

  Scan current-design sections for `%xxx`, `$Type(...)`, bare `debug`, ownership type-name dispatch, AST-as-artifact identity, and newline/ASI termination claims. Allow matches only in explicit baseline, migration, negative, or superseded contexts.

- [x] **Step 3: Validate milestone completeness.**

  Ensure every implementation milestone declares goal, scope inventory, lower-layer dependencies, required tests, failure/boundary cases, and exit evidence. Ensure no upper plan claims syntax support before its syntax dependency gate.

- [x] **Step 4: Validate code fences and embedded JSON.**

  Check balanced fences and parse every fenced `json` block. Confirm ZR examples use semicolons for simple statements and do not rely on newline termination.

- [x] **Step 5: Check the final worktree without staging.**

  Run:

  ```powershell
  git diff --check -- docs/plans
  git status --short -- docs/plans .codex/sessions
  ```

  Expected: no whitespace errors; only intended plan files and the temporary coordination note are changed. Do not stage or commit in the mixed worktree.

- [x] **Step 6: Retire coordination state.**

  Remove `.codex/sessions/20260719-0451-plan-system-rewrite.md` after all checks pass, then report rewritten files, deliberate pending surfaces, and verification evidence.
