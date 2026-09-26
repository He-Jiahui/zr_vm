---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_symbol_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_property_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_symbol_projection.c
plan_sources:
  - docs/plans/astra/index.md
  - docs/plans/lsp/00-current-state.md
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/plans/lsp/05-implementation-blueprint.md
  - docs/plans/lsp/optimize/index.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_symbol_projection_cases.h
  - tests/language_server/test_lsp_source_contract_no_local_reference_collection_cases.h
  - tests/language_server/test_semantic_analyzer.c
  - tests/acceptance/2026-09-05-astra-lsp-symbol-projection.md
doc_type: milestone-detail
---

# LSP Plan And Implementation Review

Review baseline: `56837dbdb30fb2dbb50b598df44a1a25a38b172d` production,
with master plan `c95e5387`, on the existing `main` checkout. Source line
references below describe that baseline unless explicitly marked otherwise.
Historical pass counts are not current acceptance evidence.

## Severity-Ranked Findings

### P1: Public symbol lookup revives unavailable canonical identity

- Evidence: `interface/lsp_interface_support.c:5272` first attempts canonical
  property and symbol queries, but `:5296-5328` then enumerates all scopes,
  declaration ranges and retained `SZrSymbol.references`. Its range helper at
  `:5249` also treats a missing source on either side as a wildcard.
- Lower-layer contract: `semantic/semantic_analyzer.c:1939` binds query source,
  requires `ZrParser_SemanticQuery_SymbolAt`, and projects only the returned
  stable `SymbolId` through `symbol_table.c:1171`.
- Trigger: a valid declaration remains in the presentation table while its
  semantic context is unavailable, or while its projected `semanticId` no
  longer matches the canonical declaration. The upper helper can still return
  that declaration after the canonical helper correctly returns null.
- Impact: callers including public hover at `interface/lsp_interface.c:1757`
  and metadata declaration projection can consume a symbol for which semantic
  identity validation has failed. This defeats Task 7.62's explicit fail-closed
  boundary and keeps local range-based identity inference alive.
- Repair owner: LSP. Add failing runtime parity cases, preserve property lookup
  through `PropertyAt`, and delegate ordinary lookup exclusively to canonical
  `GetSymbolAt`. Extract the small projection boundary from the 5k-line support
  file while deleting its now-unused range matcher.
- Acceptance: both negative cases fail before the production edit, then pass;
  positive declaration/use/property identity remains covered; parser query,
  analyzer, parity, source-contract and interface suites run on current code.

### P2: Identity-only resolve handlers remain advertised

- Evidence: `stdio/stdio_initialize_capabilities.c:63-75` publishes
  document-link and code-lens `resolveProvider=true`; their handlers at
  `stdio/stdio_editor_features.c:135-137` and `:174-176` only deep-copy incoming
  JSON. This is the exact contract mismatch called out by optimize plan 00.
- Trigger and impact: a client requests deferred resolution or supplies stale
  data and receives an unchanged payload without enrichment or revalidation.
- Repair task: inventory all resolve capabilities across native/WASM; add
  protocol assertions, then stop advertising identity-only resolution unless
  a snapshot-checked resolver is implemented. Keep this separate from semantic
  projection repair because it changes protocol capability negotiation.
- Acceptance: initialize/dispatch/negative/stale-data corpus matches a single
  capability registry; real code-action revalidation remains available.

### P2: Old plan status and broad failure summaries need reconciliation

- Evidence: optimize `index.md` still cites the deleted
  `semantic_analyzer_references.c`, missing lifecycle state machine and
  text-only diagnostic result IDs. Current `stdio_lifecycle.c:8-63` has explicit
  states, `snapshot/lsp_semantic_snapshot.c:342-344` records project/provider/
  semantic generations, and `diagnostics/lsp_diagnostic_store.c:208-211` includes
  those generations and the dependency fingerprint in result IDs.
- Conversely, the latest Plan 03 records explicitly leave the complete
  16-target matrix, native/Web/provider parity and stdio/CLI gates open.
  Task 7.60 fixed twelve source-less test queries; Task 7.61 records complete
  analyzer success; Task 7.62 does not justify resurrecting the older fourteen
  analyzer failures as a current baseline.
- Impact: stale checkboxes and historical blanket failures can redirect repair
  work toward already fixed code or hide a new regression.
- Repair task: replay the current targeted and integrated matrices and retain
  a named failure ledger, source revision and exact command for each remaining
  problem. Link historical claims to this current crosswalk.

## Plan-Versus-Code Matrix

| Plan | Current source evidence | Status and remaining gate |
|---|---|---|
| 00 current state | Canonical query owns symbol binding; local AST use-site collector is deleted in `56837dbd`. | Structural migration is partial; upper projection fallback remains P1. |
| 01 semantic snapshot/query | `GetSymbolAt` uses canonical IDs; dedicated property, reference, call/type hierarchy and external-reference adapters exist. | Implemented slices; complete source/binary/native, unavailable/stale and nonzero provider-generation matrix still open. |
| 02 diagnostics/safe fixes | Persistent compiler diagnostic projection, typed fixes and the shared diagnostic store exist; Task 6 closure records compiler/LSP parity. | Replay analyzer/ownership and apply-edit-rebind cases on current build. Do not substitute message matching for missing producer facts. |
| 03 position/snapshot | Document identity includes semantic dependencies; snapshot revalidation and historical/cache infrastructure exist. | Old L6 acceptance is historical. Replay UTF-16/edit/cancel/close tests and original latency/memory gates after functional fixes. |
| 04 debug/REPL | Plan specifies generation-checked formal expression facts and capability-scoped evaluation. | Debug worker owns implementation review; LSP must preserve the shared fact/context contract. This review does not promote debug acceptance. |
| 05 implementation blueprint | L1-L8 contracts have many completed leaves; L8 expressly requires fallback deletion and complete protocol/provider matrix. | Partial. Removing one fallback is a bounded repair, not L8 or Task 8 completion. |
| optimize 00 baseline/capability | Current explicit lifecycle and diagnostic-store code supersede some initial findings; identity-only resolve remains. | Refresh baseline and capability inventory before final release claims. |
| optimize 01 transport/lifecycle | Dedicated lifecycle, JSON-RPC, frame-reader and request-registry modules exist. | Current negative and deterministic teardown evidence required; historical unchecked lists are not absence-of-code evidence. |
| optimize 02 snapshots/workspaces | Shared snapshot identities and generation-bearing diagnostic result IDs exist. | Replay dependency edits, disk/overlay transitions and provider reload; verify Web uses equivalent contracts. |
| optimize 03 canonical semantics | Task 7.60/7.61 claim source-aware queries and generic member returns fixed; Task 7.62 removed local reference collection. | Current focused baseline pending; P1 repair selected first. Full consumer and 16-target gates remain open. |
| optimize 04 editor correctness | Canonical relation adapters exist; public helper still has range fallback; identity-only resolve still advertised. | Complete one canonical projection defect now; protocol/syntax editor backlog remains explicit. |
| optimize 05 native/Web | Worker still has hand-written provider/version declarations; native advertises additional providers. | Shared capability/error/semantic golden matrix remains required. |
| optimize 06 performance/acceptance | Source contains several multi-thousand-line mixed-responsibility modules; original latency/cache/peak-RSS budgets documented. | Extract the touched projection boundary; collect current measurements after functional correctness. No budget relaxation. |

## Functional Repair Sequence

1. **Planned:** add declaration projection regressions for missing semantic
   context and mismatched presentation identity. Establish canonical positive
   controls before deliberately invalidating state and restore state before
   fixture destruction.
2. **Pending RED:** run current GCC semantic-query parity executable with the
   two new tests. Root coordinates build configuration and shared target builds.
3. **Pending implementation:** add `interface/lsp_symbol_projection.c`, bind
   request source once, retain canonical property lookup, then return canonical
   `SemanticAnalyzer_GetSymbolAt` directly. Remove the range fallback and helper
   from `lsp_interface_support.c`; prevent reintroduction via source contracts.
4. **Pending verification:** parser semantic query, symbol table, reference
   tracker, analyzer, local query, parity, source-contract, interface and
   ownership regressions. Extend to project/stdio only after lower layers pass.
   Record failures by exact test name and producer/projection ownership.
5. **Pending integration:** GCC/Clang evidence, MSVC compatibility, appropriate
   memory-tool validation, module documentation and acceptance record. Root
   reviews and creates the individual commit; workers do not stage paths.

## Performance Suggestions And Unchanged Gates

- `SymbolTable_FindBySemanticId` currently scans every scope/symbol. Measure
  repeated hover/reference queries on 1k/10k-file and 100k-symbol workloads;
  if lookup is material, add a semantic-ID index owned by the same analyzer
  snapshot and invalidate it with symbol-table rebuild. Do not use a process-
  global ID map because IDs and backing allocations belong to contexts.
- Track parser query count, cache hits, semantic rebuild/full-parse ratio and
  result size alongside latency; this distinguishes identity/projection work
  from compilation and provider I/O.
- Preserve original warm hover p95 <= 50 ms, completion/signature p95 <= 100 ms,
  single-document diagnostics p95 <= 250 ms, 100-file edit diagnostics p95
  <= 500 ms, cancellation observation <= 50 ms, exact semantic-cache budget
  256 MiB and native process peak-memory limit 512 MiB. Preserve current plus
  two historical text/semantic snapshots. Later suggested budgets do not
  silently replace these accepted gates.
- Bind reports to commit/compiler/build type/machine, retain raw samples and
  p50/p95/p99, and apply the master plan's CV < 5%, >= 3% improvement and 95%
  confidence-interval rules before accepting an optimization claim. Functional
  repair continues independently while controlled performance sampling waits.

## Current Evidence

Source review and repair plan are complete. Runtime RED/GREEN evidence and
remaining failure names will be recorded here and in the acceptance document
after root provides a fresh configured WSL build. No current full-green or
performance acceptance claim is made by this review.
