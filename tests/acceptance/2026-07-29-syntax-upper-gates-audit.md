---
scope:
  - Syntax 06B
  - Syntax 07B
  - Syntax 08
  - Syntax 09
  - Syntax 10F
  - Syntax 10C
  - Syntax 11
  - Syntax 14
status: in-progress
last_verified: 2026-08-03
---

# Syntax upper gates requirement-to-evidence ledger

This ledger separates three claims that must not be conflated:

1. all 55 historical leaf status records are complete in their declared scope;
2. the production parser has completed the one-time breaking syntax cutover;
3. the root Syntax redesign and every upper promotion gate are complete.

The first two claims are proven below. The third is false while 07B and the
remaining owner gates are open.

## 2026-08-01 validation snapshot

- The canonical milestone selection is unchanged: 55 records, 55 completion
  markers, 0 missing. It excludes implementation plans and the completed
  task-level `m5-task4-property-import-bootstrap.md` support record. Raw status
  spellings are 20 Chinese `已完成`, 16 plain
  `completed`, 12 backticked `completed`, three historical
  `completed_with_known_*` qualifiers, one backticked M4-qualified
  `completed`, and three Chinese M2/M3/M4 promotion-qualified completions.
- The production parser recognizes 24 removed percent names only to emit the
  structured `legacy_syntax_removed` diagnostic and return no AST. This is
  rejection-only recognition, not dual syntax. Ordinary remainder `%` and
  remainder assignment `%=` remain operators. One `%` separator belongs to
  the internal intermediate declaration text format, not a source keyword.
- Non-percent compatibility forms in the cutover matrix, including `func`,
  keywordless functions, old arrows, old constructors, old ownership members,
  split properties, and comptime decorator types, are rejected.
- The syntax-reference manifest has 29 features: 15 `current`, 1 `negative`,
  and 13 `design-pending`. Those 13 entries keep 07B open.
- The repository migration inventory is deterministic at 908 scanned files,
  435 structured exclusions, 649 findings, and 6 allowlisted negative
  findings. Classification is `machineApplicable=0`, `maybeIncorrect=0`,
  `blocked=0`, `targetNotPromoted=0`, `requiresReview=649`, and
  `unknown=0`. The review findings are retained source/embedded examples that
  require semantic classification; they are not accepted parser rules.
- The inventory protocol is 9/9 on the clean intended snapshot. Its six exact
  allowlist entries are deletion/unknown-syntax negative fixtures; three cover
  the LSP formatter's fail-closed behavior and none enables production parsing.
- Focused GCC evidence includes the eight registered cutover/reference/provider
  CTests, type inference 122/122, official provider convergence 4/4, artifact
  TestManifest roundtrip/corruption 1/1, SemIR 10/10, and the LSP CompileTool
  projection assertion. The final clean multi-toolchain result is recorded in
  `2026-08-01-syntax-status-and-breaking-cutover.md`.
- The follow-up Gate 09/11 matrix is green under GCC, Clang, and MSVC: 11
  executables and 144 Unity tests per toolchain. It includes the strict cutover,
  callable source Pool path, native out/ref property projection, GC/artifact
  pool contracts, ref-like identity, reflection, compile-time Patch/cache, and
  project-manifest phase separation. The first MSVC replay exposed missing DLL
  exports for internal acceptance APIs; adding `ZR_PARSER_API` to the existing
  declarations closed the link failure and all 144 tests then passed.

## 2026-08-02 strict-cutover final replay

- The 55-record recount remains `TOTAL=55 COMPLETE=55 MISSING=0`; this is a
  leaf-scope claim only.
- A fresh isolated WSL GCC Debug all-target build completed. The focused matrix
  is parser 74/74, percent cutover 6/6, reflection 18/18, module 78/78,
  manifest 10/10, comptime 14/14, CLI cache 12/12, LSP advanced 0 failures,
  and compiler integration 127/127 with no custom false-green failure output.
- A fresh MSVC 19.44 Debug shared-library replay rebuilt and linked the
  comptime runtime target and passed 14/14. Snapshot acceptance now calls only
  public Export/Import/Free APIs, so it no longer depends on unexported parser
  internals.
- The complete registered CTest replay is 121/126. `language_server` passes.
  The five remaining upper-level failures are the pre-existing AOT Span
  artifact-equivalence smoke inside `language_pipeline` and four independently
  changing Debug suites. They are retained as root-promotion blockers and are
  not attributed to the strict parser cutover.

## 2026-08-03 Gate 10F canonical-contract review

- Review reopened 10F because schema v3 aliased the language callable identity to the ABI
  signature hash and used a concrete managed-type name blacklist.
- Schema v4 now persists independently hashed canonical passing/escape/initialization/call-site
  facts, validates their mapping to FFI directions, and round-trips them through `.zro`, C AOT,
  and LLVM AOT.
- Aggregate/union hashes are derived from validated canonical `SZrTypeLayout`. A local blittable
  type named `Span` is accepted, proving admission no longer dispatches on that concrete name.
- Fresh WSL GCC 11.4 and Clang 14 focused evidence is native extern 29/29, AOT C stripping 37/37,
  and strict percent cutover 6/6. MSVC 19.44 has the same counts, with one explicitly Unix-only
  LLVM runtime-loading case ignored. The 55-record selector remains
  `TOTAL=55 COMPLETE=55 MISSING=0`.

## 2026-08-03 Gate 09 canonical-layout review

- The native generational pool now accepts canonical `SZrTypeLayout` and derives
  element size/alignment, copy, Drop, and scan behavior from it. Production
  erased-value `Pool<T>` no longer owns handwritten value copy/drop callbacks.
- Admission fails closed for move-only or missing copy paths, dangling nested
  registries, managed layouts without a GC visitor, and value/value-slot layouts
  without a VM state. Copy errors roll back without publishing a handle.
- Focused WSL GCC evidence is canonical TypeLayout 14/14, pool 13/13, GC stress
  3/3, and artifact 3/3. Exactly-once deferred Drop and canonical mapped visitor
  routing are direct tests. The canonical target is also 14/14 under Clang 14 and
  MSVC 19.44 with no warning attributed to the new pool sources.
- This is a bounded M3 slice. The production provider's hidden value array is
  still the actual GC root owner; compact-safe closed-`T` moving slabs and
  language-level early-exit cleanup remain open.

## Gate ledger

| Gate | Current evidence | State | Remaining proof/work |
|---|---|---|---|
| 08 M1-M5 | reflection behavior is routed through the canonical capability registry; nullable `typeof` is rejected until narrowed; TypeId fields are authenticated against the per-generation registry; callable by-ref modes survive runtime projection; focused reflection surface 18/18 | indirect | add real reflection artifact/trimming/corruption, full VM/AOT execution, remaining LSP and stress/perf evidence |
| 09 M1 | source-callable `Pool<T>` identity/recycle plus C state-machine, million-handle, ABA, exhaustion, alignment and concurrency evidence; pool 13/13 | proven | preserve scalar handle identity and ABI v4 descriptor contract |
| 09 M2 | source-callable `tryRead`/`tryBorrow`, native `out` writeback, readonly/writable ref-property metadata, ref-like identity, storage/escape rejection and no-repeat-validation counter | indirect | complete the full local/return/container/closure/suspension matrix and view replacement/early-exit ordering |
| 09 M3 | deferred reclaim, partial-init rollback, GcFree/GcMapped/GcBarriered accounting and cards are covered by pool 13/13 plus GC stress 3/3; canonical TypeLayout admission/scan, direct/nested lifecycle consistency, copy-error rollback, state/concurrency fail-closed checks, and exactly-once deferred Drop are covered by TypeLayout 14/14 | indirect | replace the production erased-value mirror with compact-safe closed-`T` moving slabs and prove language early-exit cleanup |
| 09 M4 | native/binary/reflection contract hash parity and corrupt/missing/unknown rejection are covered by artifact 3/3; runtime-only/readonly/property-reference facts cross native import | indirect | finish dedicated LSP facts and full reflection non-boxing/lifetime evidence |
| 09 M5 | million-handle and churn/hot-access counters are separated | indirect | add final pause/allocation/scan-byte promotion matrix after M2-M4 close |
| 10F M3 | schema v4 persists independent canonical callable and ABI vectors; TypeLayout/capability-driven admission; `.zro`, C AOT, and LLVM AOT consumers; native extern 29/29 and AOT stripping 37/37 focused evidence | proven | preserve in final matrix |
| 10C | frozen 25-module N0-N3 inventory; phase-typed owners; distinct official provider descriptors rejected; LSP CompileTool phase/hash convergence | indirect | prove artifact/reflection/debug identity and every owner provider before global promotion |
| 11 M1-M2 | build facts, typed diagnostics/effects, deterministic limits/cache | proven | preserve |
| 11 M3 | typed AttributeUsage/AttributeData, Conditional elision, static decorator shape coverage, runtime decorator executor/helper removal | proven | preserve retained-data consumers |
| 11 M4 | first-version public contract is GeneratedField-only; typed diagnostics, interfaceAdds, attributeAdds, normal rebind/layout, provenance, `.zri` generated source maps, artifact/reflection retention, and atomic cross-kind Patch commit with allocator-failure rollback are covered across GCC/Clang/MSVC/MSVC-ASan | proven | preserve; GeneratedType/Method/Property remain unpublished unless separately admitted through the reference gate |
| 11 M5 | runtime decorator deleted; artifact/reflection and LSP CompileTool projection present; v2 buildDependencies are phase-separated in canonical manifest/lock output; project-owned lock admission is strict/atomic and feeds the compiler resolver without a parallel caller-owned lock graph; the resolver validates version range and CompileTool lock/ZRM package from one owned byte snapshot, checks actual package/entry SHA-256, hashes the canonical CompileTool lock section, and preserves runtime isolation; ordinary import now activates a materialized compiler-owned `.zrs` provider, keeps private helpers provider-local, exports only `pub`/`pro` transforms, executes a public typed Patch, and keeps the dependency out of the runtime graph; provider-to-provider build-dependency imports fail closed before recursion until the transitive phase-cycle graph is promoted; comptime cache v5 compares the full canonical 32-byte digest, includes current-module source identity, and has a versioned, deterministic, fixed-endian, whole-snapshot-authenticated format with atomic fail-closed import; the project CLI atomically persists `.zr_comptime_cache` and proves miss/hit/same-length semantic-edit miss/corrupt repair; formatter uses the migration plan as a fail-closed output gate, preserves canonical CompileTool syntax plus spaced/adjacent `%` and `%=` operators, and emits no edit for removed syntax | indirect | define and consume the final versioned compile-tool executable section, validate the actual transitive provider graph, and complete remaining artifact/reflection/LSP consumer acceptance |
| 14 M1 | ordinary function test/case/skip roles, typed TestManifest, production typecheck-and-trim | proven | preserve |
| 14 M2 | official Test-phase `zr.testing` provider with assert/equal/throws and bounded structured failure | proven | preserve |
| 14 M3 | deterministic discovery/filter/list/run, process isolation, jobs, timeout, output and exit codes | proven | preserve sync/async reference matrix |
| 14 M4 | percent/draft migration and role-driven LSP CodeLens/projection | indirect | canonical Debug TestManifest consumer and final idempotent cross-consumer acceptance |
| 06B parser cutover | removed syntax has rejection-only recognition and no old AST/lowering/runtime decorator semantics | proven | keep migration diagnostics and operator/internal-IR allowlist distinct |
| 06B repository promotion | inventory has no machine/block/unknown/not-promoted findings, but 645 review findings and owner-gated reference slots remain | indirect | classify/migrate remaining current inputs as owner gates close |
| 07B | coverage still has 13 owner-gated `design-pending` entries | contradicted | promote only after each owner gate has independent evidence |

## Current conclusion

The strict production parser cutover is complete. The 55 historical leaf
records are confirmed in their own scope. The root Syntax redesign is not
complete: 07B is explicitly open, and 08/09/10C/11/14 still have the partial
rows listed above. No acceptance document may translate leaf completion into a
root promotion.
