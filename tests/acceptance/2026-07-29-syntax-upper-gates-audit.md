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
last_verified: 2026-07-30
---

# Syntax upper gates requirement-to-evidence ledger

This ledger is intentionally stricter than a source-file or focused-smoke inventory. A
requirement is `proven` only when its production path and direct acceptance test have been
rerun in the named environment. `indirect` means relevant implementation or tests exist but
do not prove the whole clause. `contradicted` means the current tree contains direct evidence
that the published requirement is not implemented. `missing` means no direct implementation
or test was found.

## 2026-07-30 isolated validation snapshot

- A WSL Ubuntu 22.04 GCC 11.4 Debug source snapshot completed a cold full build
  (`3176/3176` Ninja steps). After the final source synchronization, CMake
  reconfiguration and the complete incremental build also exited zero.
- The same isolated build passed all 123 registered CTest tests in one serial run
  (`123/123`, 573.17 seconds). This includes `language_pipeline`, projects,
  language-server stdio, CLI/AOT, generic, descriptor, debug, container, and
  migration targets. The standalone post-fix `language_pipeline` replay passed
  `1/1` in 216.34 seconds.
- Gate 11 focused suites pass 52/52: compile-time execution 37/37, attribute
  contract 3/3, comptime descriptor 2/2, comptime runtime 7/7, and declaration
  transform contract 3/3.
- The production parser tree contains zero occurrences of the listed removed
  percent-keyword literals. `percent_syntax_cutover`, `cli_syntax_migration`,
  and `legacy_migration` pass 3/3, while ordinary modulo remains valid.
- The repository migration scan is closed at `machineApplicable=0`,
  `requiresReview=610`, `blocked=3`, `unknown=0`, with 878 scanned and 412
  explicitly excluded files. The three blocked findings are deliberate negative
  coverage (`%future`, `%mutex`, `%atomic`), not accepted production syntax.
- The 55 historical status records remain 55/55 complete in their declared leaf
  scope. These results do not promote the root plan: the missing and partial
  rows below remain the authoritative upper-gate blockers.

## Gate ledger

| Gate | Published requirement | Current evidence | State | Remaining proof/work |
|---|---|---|---|---|
| 08 M1 | exact/static/erased type identity, every declaration category, `typeid`/`typeof`/`resolve`, no name dispatch | `reflection.h`, `reflection_descriptor_native.c`, `test_reflection_type_surface.c` | indirect | rerun category/identity vectors and audit concrete-name branches |
| 08 M2 | fields/properties/methods, access/order/ambiguity, generic/overload query, by-ref boundary | `reflection_member_query.c`, `test_reflection_type_surface.c` | indirect | map every query/access/by-ref clause to a direct passing case |
| 08 M3 | source/native/binary metadata and contract-hash parity; corrupt/stripped negatives | no dedicated reflection artifact roundtrip target found | missing | add artifact roundtrip, corruption and trimming tests |
| 08 M4 | direct/spread construction, rejection/ambiguity/throw, generation invalidation, compact, VM/AOT parity | reflection surface/stress tests and call-spread acceptance exist | indirect | prove all constructor vectors through VM and both AOT paths |
| 08 M5 | LSP projection, migration, 100k/deep/cache/GC/throw stress | `test_reflection_type_stress.c`; no dedicated LSP reflection-fact target found | indirect | add LSP facts and complete the published stress matrix |
| 09 M1 | generational identity, stale/ABA rejection, deliver/recycle | fresh WSL GCC `zr_vm_generational_pool_test` state-transition and million-handle vectors | proven | preserve through final multi-toolchain rerun |
| 09 M2 | `PoolRef`/`PoolReadRef`, `bool + out`, conflicts, hot access, escape rejection | descriptor passing modes plus imported canonical `REF_LIKE` storage/array/closure/await/yield rejection and conflict tests | indirect | prove language-level out-view default/definite-assignment, replacement ordering and derived-loan cleanup |
| 09 M3 | deferred reclaim, slabs, GC scan/barrier modes, compact and exactly-once drop | `zr_vm_generational_pool_gc_stress_test` proves partial rollback, distinct scan classes/cards, direct hot path and churn; base suite proves deferred drop/alignment/concurrency | indirect | prove language early-exit guard cleanup and managed moving-slab base+offset compaction |
| 09 M4 | source/native/binary contract parity, reflection projection, corrupt record rejection | `zr_vm_generational_pool_artifact_test` executes source hash and proves native/binary/reflection parity plus zero/unknown rejection | indirect | add direct PoolRef non-boxing reflection and LSP canonical-capability evidence |
| 09 M5 | LSP facts and million-handle/churn/scan-byte/thread-local stress | fresh base and GC/stress targets prove million handles, 100k churn, distinct scan bytes, concurrent boundary and zero-revalidation direct loop | indirect | add LSP facts and allocation/GC-pause comparison evidence |
| 10F M3 | canonical `NativeExtern`/`FfiSignature` across scalar, aggregate, directions, callback, ownership, ABI and errors | `2026-07-29-syntax-10f-native-extern-ffi-contract.md` accepts focused M3 | proven | rerun focused vectors after upper-gate changes; this does not prove 10C |
| 10C | freeze N0-N3 providers and converge descriptor/artifact/LSP/reflection/debug identity and phase | no complete owner-provider inventory/convergence acceptance found | missing | implement after 08/09/11/14 owner gates are proven |
| 11 M1 | `zr.compile`, declared build facts and `comptime if` pruning | `2026-07-29-syntax-11-m1-build-facts-comptime-if.md` | proven | preserve while replacing legacy compile-time paths |
| 11 M2 | typed effects, deterministic limits/cache and `feature/assert/error/warning` APIs | fresh WSL GCC `comptime_contract` 2/2 and `comptime_runtime_contract` 7/7 prove typed effects, fuel/depth budgets, diagnostics and deterministic caching | proven | preserve through final multi-toolchain rerun |
| 11 M3 | typed AttributeUsage/AttributeData and direct-void Conditional call elision | fresh WSL GCC `attribute_contract` 3/3 and compile-time cases prove role/schema/target/repeatability checks plus disabled-call argument elision; legacy decorator execution paths still exist | indirect | delete legacy runtime decorator projection and prove retained AttributeData consumers |
| 11 M4 | immutable declaration views, append-only Patch, one expansion round and ordinary rebind | fresh WSL GCC declaration contract 3/3 and compile-time 37/37 prove typed views/Patch validation, one round, collision/budget checks and `GeneratedField` layout rebind | indirect | implement non-empty `interfaceAdds`, `attributeAdds`, `diagnostics`, all generated declaration kinds, source maps and transactional application/rollback |
| 11 M5 | artifact/reflection/LSP/formatter/build dependency/migration consumers; no runtime decorator | legacy decorator path remains and consumers are absent | contradicted | converge consumers, then delete runtime decorator/name-dispatch paths |
| 14 M1 | bind test/case/skip roles on ordinary functions, typed TestManifest, production trimming | compiler test lowering is an explicit removed stub; documentation says no manifest is generated | contradicted | implement typed binding/artifact contract after Gate 11 metadata roles |
| 14 M2 | N3 `zr.testing` descriptor with assert/equal/throws and bounded structured failures | no `zr_vm_lib_testing` provider found | missing | add the official Test-phase provider and direct contract tests |
| 14 M3 | deterministic discovery/filter/list/run, sync/async, isolation/jobs/timeout/output/exit codes | no typed CLI runner found | missing | add manifest-driven runner and failure/isolation/stress tests |
| 14 M4 | LSP/debug facts and idempotent migration; no source-text discovery or hidden main | no manifest consumer found | missing | integrate LSP/debug/migration and add absence tests |
| 06B M4 | zero blocked/unknown/targetNotPromoted, structured repository edit, current docs/artifacts/LSP | production percent parser cutover is committed, but reference coverage still contains `design-pending` owner gates | contradicted | finish owner gates, rerun inventory, migrate all current fixtures/artifacts |
| 06B M5 | remove legacy AST/helpers/lowering/runtime/editor semantics under a structured allowlist | percent production parser paths were removed; compile-time decorator/runtime compatibility remains | indirect | delete remaining superseded semantic paths and prove allowlist-only legacy text |
| 07B | promote every current/negative reference case across parser/SemIR/VM/AOT/artifact/CLI/LSP/debug/migration/perf | `coverage.json` still contains owner-gated `design-pending` entries | contradicted | promote only after every owner gate and 06B are independently proven |

## Promotion clauses that remain globally open

- Shared paths must dispatch by canonical role/capability and never by concrete source type name.
- VM, AOT C, AOT LLVM, artifacts, LSP, reflection and debug consumers must use the same
  canonical identity and contract facts.
- Old syntax or behavior may remain only in migration input, explicit negative coverage,
  strings/comments, or superseded historical documents.
- Every positive surface needs boundary and failure cases with stable diagnostics; a missing
  owner implementation cannot be represented as a passing negative.
- Full WSL GCC/Clang, Windows MSVC, migration inventory, reference project, artifact and
  stress evidence must be fresh after the final cutover.

## Current execution order

1. Complete Gate 11 M3-M5 (M2 is proven; M3/M4 remain partial).
2. Complete Gate 14 M1-M4.
3. Close missing direct evidence in 08/09 and rerun 10F.
4. Complete 10C provider convergence.
5. Execute 06B repository-wide cutover and legacy deletion.
6. Promote 07B and run the final multi-toolchain acceptance matrix.
