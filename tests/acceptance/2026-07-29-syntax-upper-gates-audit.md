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
last_verified: 2026-08-04
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

- The first bounded implementation initially admitted two Critical nested-layout
  downgrades: a root `GcFree` layout could hide a registry-resolved `GcMapped`
  child, and a raw-copy root could hide a move-only/lifecycle child.
- The committed recursive admission walk now validates every nested edge before
  pool creation. Direct regressions reject both downgrade forms; managed nested
  scan and nested custom Drop tests also prove positive traversal and
  exactly-once cleanup. The public contract states that retained VM state,
  registry backing, nested layouts/tables, and callback data must outlive the
  pool.
- A fresh isolated WSL GCC 11.4 replay on 2026-08-03 passed the canonical
  TypeLayout suite 14/14, including both former Critical cases. They are closed,
  not current Gate blockers.
- Follow-up production convergence removed the permanent
  `__zr_pool_values` mirror. Canonical inline delivery now fixes registry
  identity/layout id, external slab values participate in mark/minor/forwarding
  through a separate trace callback, and guard projections copy back only on
  writable close. Focused GCC evidence is native view 7/7, external GC layout
  2/2, canonical pool layout 14/14, and production provider 4/4.
- Writable `ref T` member chains now load nested inline structs, reverse-write
  the completed value through the property reference, and consume the temporary
  reference shell in ordinary variable/assignment/return contexts. Property
  ref 23/23, property lowering 22/22, type inference 122/122, and the source
  pool write/close/readback test pass.
- Source return/throw/break/continue/block cleanup and `out` view replacement
  release native guards in deterministic order. `CLOSE_SCOPE` now saves its next
  PC before nested native close and refreshes normal native-call state, closing
  the ASan stale-resume failure.
- Ordinary interpreter functions now lazily publish a stable, entry-owned
  prototype-layout registry. The source provider uses the same canonical
  non-boxing route without fabricated artifact registration, while attached
  artifact registries remain authoritative and fail closed without source-cache
  fallback. GCC, Clang, and MSVC each pass the same 13 executables and 271/271
  assertions; the focused MSVC ASan registry/lifetime subset passes 60/60.
- Design review confirms that stable native/ownership slabs are an explicit M3
  option. Full/minor compaction rewrites embedded managed children through the
  external visitor, while stateful canonical layouts fail concurrent admission
  until an isolation-safe per-operation state exists. M3 is therefore proven;
  the pause/allocation/scan-byte comparison is a distinct M5 requirement.

## 2026-08-04 final recount and review

- The fresh selector recursively scans Markdown files below the Syntax
  milestone subdirectories, excludes 19 `*-implementation-plan.md` files and
  the one task-support `m5-task4-property-import-bootstrap.md` record, and
  returns `TOTAL=55 COMPLETE=55 MISSING=0`. The exact marker distribution is
  20 Chinese plain completions, 16 English plain completions, 12 backticked
  English completions, three `completed_with_known_*` historical qualifiers,
  one backticked M4-qualified completion, and three Chinese promotion-qualified
  completions.
- A production-source literal scan returns zero occurrences for the removed
  `%module`, `%compileTime`, `%extern`, `%test`, `%owned`, `%import`, `%borrow`,
  `%loan`, `%unique`, `%shared`, and `%func` spellings. The 24-name migration
  table is used only by `report_removed_percent_syntax`: production parsing
  marks the error fatal, emits `legacy_syntax_removed`, and returns no AST.
  Remaining percent-token branches are modulo/modulo-assignment, rejection
  routing, parser recovery, or the internal intermediate closure delimiter.
- The final review found no unresolved Critical or Important defect in this
  bounded Gate 09 slice. The same 21 affected executables pass 530/530 Unity
  assertions under WSL GCC 11.4, WSL Clang 14.0, and MSVC 19.44 Debug; the
  MSVC ASan subset passes 120/120. These results do not promote the root gate.

## 2026-08-03 Gate 08 M1 provider-identity review

- Replaced the parser-owned 20-entry reflection type table with registered
  canonical TypeRoles owned by official `zr.builtin` and `zr.reflection`
  descriptors. Native plugin ABI is now 5.
- Core reflection import, module creation, and cache validation resolve the
  reflection provider by role; production code no longer compares the concrete
  module-name macro.
- Ordinary source ModuleId derivation rejects the reserved `zr.*` root. A
  corrected RED proved `zr/reflection.zr` could previously compile and spoof
  the canonical type name; GREEN rejects it before type registration.
- GCC, Clang, and MSVC each pass the same 12 executables and 395 Unity tests,
  including provider 9/9, project resolver 10/10, reflection surface 19/19,
  dynamic reflection 36/36, type inference 122/122, module 78/78, parser 74/74,
  and percent cutover 6/6.
- This proves M1 provider/canonical identity routing only. M2-M5 remain open.

## 2026-08-03 Gate 11/14 requirement review

- Gate 11 M1-M4 match their bounded first-version contracts. Gate 11 M5 remains
  open because the final versioned compile-tool executable section, actual
  transitive provider graph/cycle chain, and remaining artifact/reflection/LSP
  consumers are absent.
- Gate 14 manifest `SymbolId`/`TypeId` values are currently name hashes/XOR
  projections rather than canonical semantic identities; `moduleGraphHash`
  hashes only the module string.
- `AssertionFailure` does not retain the required source span/snapshot metadata,
  and `throws<E>` catches any exception without exact/subtype validation.
- LSP CodeLens still rebuilds roles from AST decorators instead of consuming
  TestRole facts/TestManifest. Debug has no canonical TestManifest consumer,
  async logical-stack projection, or case-parameter integration.
- Therefore the earlier Gate 14 M1/M2 "proven" labels are withdrawn. Runner
  behavior and syntax migration remain useful bounded evidence, not full Gate 14
  promotion.

## Gate ledger

| Gate | Current evidence | State | Remaining proof/work |
|---|---|---|---|
| 08 M1 | official provider roles and canonical TypeRoles replace parser/core name dispatch; contract-only reflection cannot materialize; host loaders compose; projections and parent graphs validate; `zr.*` source spoofing is rejected; provider 9/9, resolver 10/10, reflection surface 19/19 and dynamic reflection 36/36 pass across GCC/Clang/MSVC | proven | preserve registered identity and fail-closed provider resolution |
| 08 M2-M5 | nullable `typeof`, authenticated TypeId fields, member queries/construction and callable by-ref projection have focused behavior, but the complete artifact/AOT/LSP/stress promotion matrix is absent | indirect | add real reflection artifact/trimming/corruption, full VM/AOT execution, remaining LSP and stress/perf evidence |
| 09 M1 | source-callable `Pool<T>` identity/recycle plus C state-machine, million-handle, ABA, exhaustion, alignment and concurrency evidence; pool 13/13 | proven | preserve scalar handle identity and ABI v4 descriptor contract |
| 09 M2 | source-callable `tryRead`/`tryBorrow`, native `out` writeback, readonly/writable ref-property metadata, ref-like identity, local/field/global/array/box/native-ABI/closure/await/yield matrix, no-repeat-validation counter, and source return/throw/break/continue/replacement cleanup; the relevant six-target matrix passes 187/187 across GCC/Clang/MSVC | proven | preserve capability-driven ref-like and cleanup contracts |
| 09 M3 | recursive canonical admission rejects nested lifecycle downgrades; production canonical inline delivery has no permanent mirror, fixes registry identity/layout id, traces and rewrites external managed values across full/minor GC, performs temporary guard projection with writable copyback, resumes after native close without replaying a closed view, gives ordinary interpreter entry functions a stable canonical registry without weakening artifact validation, uses the design-permitted stable native slab path, and rejects state-dependent concurrent admission; the registry slice passes 271/271 across GCC/Clang/MSVC and 60/60 under focused MSVC ASan | proven | preserve compact-safe external tracing and fail-closed concurrency capability boundaries |
| 09 M4 | native/binary/reflection contract hash parity and corrupt/missing/unknown rejection are covered by artifact 3/3; runtime-only/readonly/property-reference facts cross native import | indirect | finish dedicated LSP facts and full reflection non-boxing/lifetime evidence |
| 09 M5 | million-handle and churn/hot-access counters are separated | indirect | add final pause/allocation/scan-byte promotion matrix after M2-M4 close |
| 10F M3 | schema v4 persists independent canonical callable and ABI vectors; TypeLayout/capability-driven admission; `.zro`, C AOT, and LLVM AOT consumers; native extern 29/29 and AOT stripping 37/37 focused evidence | proven | preserve in final matrix |
| 10C | frozen 25-module N0-N3 inventory; phase-typed owners; distinct official provider descriptors rejected; LSP CompileTool phase/hash convergence | indirect | prove artifact/reflection/debug identity and every owner provider before global promotion |
| 11 M1-M2 | build facts, typed diagnostics/effects, deterministic limits/cache | proven | preserve |
| 11 M3 | typed AttributeUsage/AttributeData, Conditional elision, static decorator shape coverage, runtime decorator executor/helper removal | proven | preserve retained-data consumers |
| 11 M4 | first-version public contract is GeneratedField-only; typed diagnostics, interfaceAdds, attributeAdds, normal rebind/layout, provenance, `.zri` generated source maps, artifact/reflection retention, and atomic cross-kind Patch commit with allocator-failure rollback are covered across GCC/Clang/MSVC/MSVC-ASan | proven | preserve; GeneratedType/Method/Property remain unpublished unless separately admitted through the reference gate |
| 11 M5 | runtime decorator deleted; artifact/reflection and LSP CompileTool projection present; v2 buildDependencies are phase-separated in canonical manifest/lock output; project-owned lock admission is strict/atomic and feeds the compiler resolver without a parallel caller-owned lock graph; the resolver validates version range and CompileTool lock/ZRM package from one owned byte snapshot, checks actual package/entry SHA-256, hashes the canonical CompileTool lock section, and preserves runtime isolation; ordinary import now activates a materialized compiler-owned `.zrs` provider, keeps private helpers provider-local, exports only `pub`/`pro` transforms, executes a public typed Patch, and keeps the dependency out of the runtime graph; provider-to-provider build-dependency imports fail closed before recursion until the transitive phase-cycle graph is promoted; comptime cache v5 compares the full canonical 32-byte digest, includes current-module source identity, and has a versioned, deterministic, fixed-endian, whole-snapshot-authenticated format with atomic fail-closed import; the project CLI atomically persists `.zr_comptime_cache` and proves miss/hit/same-length semantic-edit miss/corrupt repair; formatter uses the migration plan as a fail-closed output gate, preserves canonical CompileTool syntax plus spaced/adjacent `%` and `%=` operators, and emits no edit for removed syntax | indirect | define and consume the final versioned compile-tool executable section, validate the actual transitive provider graph, and complete remaining artifact/reflection/LSP consumer acceptance |
| 14 M1 | ordinary function role binding and production trim work, but manifest SymbolId/TypeId/moduleGraphHash are fabricated name projections rather than canonical semantic identities | contradicted | serialize canonical semantic SymbolId/TypeId and real module-graph identity with collision/cross-module/overload tests |
| 14 M2 | official Test-phase provider exists, but AssertionFailure lacks required span/snapshot metadata and `throws<E>` does not validate E/subtypes | contradicted | implement structured bounded snapshots/source span and typed exception matching |
| 14 M3 | deterministic discovery/filter/list/run, process isolation, jobs, timeout, output and exit codes | proven | preserve sync/async reference matrix |
| 14 M4 | percent/draft migration is covered, but LSP CodeLens reconstructs AST decorator roles rather than consuming facts/manifest; Debug consumer is absent | contradicted | migrate LSP and Debug to canonical TestRoleFact/TestManifest, including async logical stack and case parameters |
| 06B parser cutover | removed syntax has rejection-only recognition and no old AST/lowering/runtime decorator semantics | proven | keep migration diagnostics and operator/internal-IR allowlist distinct |
| 06B repository promotion | inventory has no machine/block/unknown/not-promoted findings, but 645 review findings and owner-gated reference slots remain | indirect | classify/migrate remaining current inputs as owner gates close |
| 07B | coverage still has 13 owner-gated `design-pending` entries | contradicted | promote only after each owner gate has independent evidence |

## Current conclusion

The strict production parser cutover is complete. The 55 historical leaf
records are confirmed in their own scope; the current directory additionally
contains one completed task-level support record outside that selector. The root
Syntax redesign is not complete: 07B is explicitly open; Gate 09 still lacks
M4 consumer acceptance and M5 performance promotion work; Gate 14 has
reopened contract defects; and 08 M2-M5, 10C, and 11 M5 remain open.
No acceptance document may translate leaf completion into a root promotion.
