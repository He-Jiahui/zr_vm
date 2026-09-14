---
doc_type: acceptance-matrix
plan: docs/plans/ssa/11-tooling-acceptance/03-release-acceptance.md
manifest: tests/cmake/ssa-coverage-manifest.cmake
focused_test: tests/core/test_ssa_release_acceptance.c
status: open
---

# SSA requirement matrix

This is the release denominator, not a list of optimistic check marks.  Each
of the 47 leaf plans has one row, one expected CTest name, and three named
cases: a positive case, a boundary case, and a failure case.  A row is only
eligible for `accepted` when its evidence is tied to the same source revision,
dirty-tree digest, and environment as the manifest.  A focused test pass does
not promote a row or a milestone to `accepted`.

The names in this table are the stable identifiers used by
`test_ssa_release_acceptance.c`; the implementation and CTest registration
may be supplied by the owning leaf.  Keep this table and
`tests/cmake/ssa-coverage-manifest.cmake` in lock-step.  A missing test name,
missing case, duplicate ID, or unbound evidence is a gate failure.

| ID | Domain / owner | Expected CTest | Positive | Boundary | Failure | Current state |
| --- | --- | --- | --- | --- | --- | --- |
| 00.01 | measurement / `00-measurement-contracts/01-baseline-metrics.md` | `ssa_baseline_metrics` | baseline sample with checksum | missing PMU is unavailable | checksum mismatch rejected | open |
| 00.02 | measurement / `00-measurement-contracts/02-contract-freeze.md` | `ssa_contract_freeze` | versioned contract accepted | unknown field rejected | ABI version mismatch rejected | open |
| 00.03 | measurement / `00-measurement-contracts/03-differential-harness.md` | `ssa_differential_harness` | matching semantic events | zero CTest selection fails | drop-order mismatch rejected | open |
| 01.01 | ExecIR / `01-execir-ssa/01-core-model.md` | `ssa_core_model` | typed ExecIR module | empty block is explicit | malformed opcode rejected | open |
| 01.02 | ExecIR / `01-execir-ssa/02-ssa-construction.md` | `ssa_construction` | canonical SSA with phi | unreachable block retained for diagnostic | duplicate value definition rejected | open |
| 01.03 | ExecIR / `01-execir-ssa/03-effects-verifier.md` | `ssa_effects_verifier` | ordered effect tokens | empty effect range is checked | effect reordering rejected | open |
| 01.04 | ExecIR / `01-execir-ssa/04-state-maps.md` | `ssa_state_maps` | logical state materialized | partial state map is visible | state-slot alias rejected | open |
| 01.05 | ExecIR / `01-execir-ssa/05-oracle-projections.md` | `ssa_oracle_projections` | oracle/ExecBC/AOT projection | unsupported projection is reported | projection event mismatch rejected | open |
| 02.01 | optimization / `02-automatic-optimization/01-pass-manager-scalar.md` | `ssa_pass_manager_scalar` | ordered scalar passes | zero budget is a no-op | verifier failure rolls back | open |
| 02.02 | optimization / `02-automatic-optimization/02-gvn-range.md` | `ssa_gvn_range` | range guard elision | unknown range keeps check | stale generation rejected | open |
| 02.03 | optimization / `02-automatic-optimization/03-escape-ownership.md` | `ssa_escape_ownership` | nonescaping allocation plan | unknown escape is conservative | borrowed value escape rejected | open |
| 02.04 | optimization / `02-automatic-optimization/04-interprocedural-inlining.md` | `ssa_interprocedural_inlining` | guarded inline summary | recursive call not inlined | signature/effect mismatch rejected | open |
| 02.05 | optimization / `02-automatic-optimization/05-loops-specialization.md` | `ssa_loops_specialization` | bounded loop specialization | unknown trip count keeps generic loop | budget/guard failure falls back | open |
| 03.01 | binding / `03-interpreter-binding/01-dispatch-boundaries.md` | `ssa_dispatch_boundaries` | central execution boundary | cold boundary is explicit | invalid boundary context rejected | open |
| 03.02 | binding / `03-interpreter-binding/02-static-binding-facts.md` | `ssa_static_binding_facts` | token/signature/layout fact | missing fact stays dynamic | ambiguous binding rejected | open |
| 03.03 | binding / `03-interpreter-binding/03-guarded-caches.md` | `ssa_guarded_caches` | generation-guarded cache | cache miss enters cold lane | stale generation not reused | open |
| 03.04 | binding / `03-interpreter-binding/04-generated-fusion.md` | `ssa_generated_fusion` | generated fusion pattern | partial pattern keeps scalar path | unknown generated opcode rejected | open |
| 04.01 | frame/native / `04-frame-native/01-frame-layout.md` | `ssa_frame_layout` | packed frame layout | alignment padding accounted | overlapping slot rejected | open |
| 04.02 | frame/native / `04-frame-native/02-call-return-tail.md` | `ssa_call_return_tail` | return and tail transfer | zero return values explicit | invalid tail target rejected | open |
| 04.03 | frame/native / `04-frame-native/03-native-abi.md` | `ssa_native_abi` | typed native ABI | copy/marshal on layout mismatch | callback/signature mismatch rejected | open |
| 04.04 | frame/native / `04-frame-native/04-roots-observation.md` | `ssa_roots_observation` | precise frame roots | empty root set recorded | unmapped live root rejected | open |
| 05.01 | layout / `05-data-layout/01-objects-layout-maps.md` | `ssa_objects_layout_maps` | object layout map | private unknown shape stays generic | public layout change rejected | open |
| 05.02 | layout / `05-data-layout/02-arrays-slices.md` | `ssa_arrays_slices` | checked contiguous view | empty slice is valid | overflow/bounds violation rejected | open |
| 05.03 | layout / `05-data-layout/03-maps-strings.md` | `ssa_maps_strings` | map/string storage contract | missing hash is conservative | invalid storage kind rejected | open |
| 05.04 | layout / `05-data-layout/04-aggregate-soa.md` | `ssa_aggregate_soa` | aggregate SROA/SoA plan | unknown alias keeps AoS | public/FFI layout rewrite rejected | open |
| 06.01 | GC/domain / `06-gc-domain/01-young-allocation.md` | `ssa_young_allocation` | young TLAB allocation | TLAB exhaustion refills or safepoints | invalid remembered-set entry rejected | open |
| 06.02 | GC/domain / `06-gc-domain/02-major-budget.md` | `ssa_major_budget` | budgeted major slice | budget hit preserves cursor | overflow/unbounded pause rejected | open |
| 06.03 | GC/domain / `06-gc-domain/03-domain-sharing.md` | `ssa_domain_sharing` | Send/Sync shared value | immutable value crosses worker | borrowed/affine handle rejected | open |
| 06.04 | GC/domain / `06-gc-domain/04-cross-domain-clone.md` | `ssa_cross_domain_clone` | transactional cross-domain clone | cycle and alias preserved | quota/generation mismatch aborts | open |
| 06.05 | GC/domain / `06-gc-domain/05-async-frame-budget.md` | `ssa_async_frame_budget` | async frame checkpoint | cancelled checkpoint unwinds | borrowed value across await rejected | open |
| 07.01 | AOT / `07-aot-backends/01-aotir-contract.md` | `ssa_aotir_contract` | shared AOTIR contract | unsupported operation explicit | pointer/contract drift rejected | open |
| 07.02 | AOT / `07-aot-backends/02-c-llvm-lowering.md` | `ssa_c_llvm_lowering` | C/LLVM lowering parity | unsupported lowering keeps fallback | event/exception mismatch rejected | open |
| 07.03 | AOT / `07-aot-backends/03-generics-lto-pgo.md` | `ssa_generics_lto_pgo` | generic release policy | stale profile ignored | capability/ABI mismatch rejected | open |
| 07.04 | AOT / `07-aot-backends/04-aot-runner-coverage.md` | `ssa_aot_runner_coverage` | actual backend coverage | zero denominator unavailable | fallback cannot be labelled AOT | open |
| 08.01 | artifact / `08-artifact-hotpatch/01-schema-relocation.md` | `ssa_schema_relocation` | pointer-free artifact | unknown optional section skipped | raw process address rejected | open |
| 08.02 | artifact / `08-artifact-hotpatch/02-capability-validation.md` | `ssa_capability_validation` | capability intersection | empty capability set valid | capability escalation rejected | open |
| 08.03 | artifact / `08-artifact-hotpatch/03-generation-publication.md` | `ssa_generation_publication` | atomic generation publish | old frame keeps old generation | stale binding returns link error | open |
| 08.04 | artifact / `08-artifact-hotpatch/04-rollback-restricted.md` | `ssa_rollback_restricted` | restricted rollback | repeated rollback idempotent | new import/layout change rejected | open |
| 09.01 | SIMD / `09-language-simd/01-inferred-protocols.md` | `ssa_inferred_protocols` | inferred optimization protocol | unknown fact conservative | invalid protocol summary rejected | open |
| 09.02 | SIMD / `09-language-simd/02-numeric-vector-ir.md` | `ssa_numeric_vector_ir` | strict numeric vector IR | fast math requires project permission | semantic numeric drift rejected | open |
| 09.03 | SIMD / `09-language-simd/03-batch-vectorization.md` | `ssa_batch_vectorization` | ordered batch/vector | scalar fallback visible | unordered reduction without permission rejected | open |
| 10.01 | platform / `10-jit-platforms/01-backend-service.md` | `ssa_backend_service` | asynchronous backend service | pending compile is not ready | active-lease shutdown rejected | open |
| 10.02 | platform / `10-jit-platforms/02-host-baseline-jit.md` | `ssa_host_baseline_jit` | host baseline JIT lifecycle | unsupported architecture falls back | JIT code persistence rejected | open |
| 10.03 | platform / `10-jit-platforms/03-platform-matrix.md` | `ssa_platform_matrix` | platform capability matrix | cross-compile is not execution | mobile/WASM JIT rejected | open |
| 11.01 | tooling / `11-tooling-acceptance/01-build-profiles.md` | `ssa_build_profiles` | reproducible profile/cache | cache miss rebuilds | conflicting profile flags rejected | open |
| 11.02 | tooling / `11-tooling-acceptance/02-optimization-remarks.md` | `ssa_optimization_remarks` | source-located optimization remark | missing profile is estimated | stale document remark discarded | open |
| 11.03 | tooling / `11-tooling-acceptance/03-release-acceptance.md` | `ssa_release_acceptance` | current-revision release manifest | focused-only keeps gate open | legacy consumer/missing backend blocks | open |

## Evidence contract

The focused validator requires every row to carry:

- `revision`, a non-empty source revision identifier;
- `dirtyDigest`, a digest of the complete dirty-tree snapshot (not only
  tracked files); and
- `environment`, including compiler/toolchain and relevant configuration.

The three case names are a denominator.  They must be executed or explicitly
recorded as unavailable; an omitted case is not equivalent to a pass.  The
validator also requires unique IDs and unique milestone/coverage rows.

The current focused run only proves the validator's state transitions.  It is
not evidence that all rows above have run.  At the time this record was
written, the shared checkout was concurrently changing; the observed source
revision was `b2b726eed491f5fe358914b8ca9382c33ccf2586`.  The parent
integration must refresh that value and the dirty digest after the final
commit before using this matrix as a release record.

## Required coverage dimensions

The manifest keeps these dimensions independent:

| Dimension | Required variants | Gate rule |
| --- | --- | --- |
| Semantic | parser call forms; ExecIR CFG/phi/effects; runtime GC/ownership/async/native; artifact/hotpatch; tooling/LSP; differential event order | all expected cases execute and pass |
| Backend | ExecBC; AOT-C; AOT-LLVM; host JIT | actual backend is recorded; fallback is visible |
| Platform | WSL GCC; WSL Clang; Windows MSVC; Android AOT/ExecBC; iOS AOT/restricted patch; WASM ExecBC/AOT | cross-compilation is not execution |
| Sanitizer | ASan; UBSan; LSan; TSan; Valgrind; Helgrind | any failed or unavailable row keeps the gate open |
| Artifact | schema roundtrip; no process addresses; capability intersection; generation/rollback | all required checks execute |
| Performance | 15 representative workloads from the manifest | samples need matching environment/checksum; no sample means no claim |
| Legacy | SemIR projection and AOT opcode decoder consumer scans | removal claim requires zero production consumers |
| Documentation | module API; ExecIR/artifact schema; CLI/LSP reference; acceptance records | all artifacts are linked and current |

No performance number is asserted by this file.  A candidate may only claim a
relative improvement after paired samples share the environment and checksum,
have a non-zero sample count, and pass the independent variance/quality gate.
