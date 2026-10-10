---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c
  - zr_vm_parser/src/zr_vm_parser/place_queries.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.h
  - tests/cmake/ssa-retained-builder-cfg.cmake
  - tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/semantic_value_facts.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
implementation_files:
  - tests/parser/test_ssa_retained_builder_cfg.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.c
  - tests/parser/support/ssa_retained_builder_cfg_fixture.h
  - tests/cmake/ssa-retained-builder-cfg.cmake
  - tests/cmake/ssa-retained-builder-direct-validation/CMakeLists.txt
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - "user: 2026-10-10 finite retained builder CFG gate; execution reserved for Root review"
tests:
  - tests/parser/test_ssa_retained_builder_cfg.c
  - docs/acceptance/ssa-retained-builder-cfg.md
doc_type: testing-guide
status: finite-green
---

# Retained builder CFG validation

## Current status

Finite green. Root's actual runtime r2 configured a fresh build route, compiled
all 31 C sources, linked and passed the exact eight cases with UBSan and no
warnings or sanitizer diagnostics. A separate MSVC gate compiled those 31
sources with zero warnings; it did not link or run them. The
[acceptance record](../acceptance/ssa-retained-builder-cfg.md) identifies the
actual receipts and preserves the rejected first attempt.

## Target and finite production closure

The dedicated target is `zr_vm_ssa_retained_builder_cfg_test`; its CTest name is
`ssa_retained_builder_cfg`. The new manifest explicitly lists the existing
28 builder-support production TUs plus actual `exec_ir_owned_module.c`.
The runner and SemIR fixture add two test TUs: 31 C sources total.

All files are compiled from the actual current checkout. There are no broader
parser/core archives, imported module targets, provider redirects, fake
definitions, forced allocator headers or source copies. The target uses the
project's standard Windows `_CRT_SECURE_NO_WARNINGS` definition, matching the
top-level and existing builder test configuration. No `/wd` or `-Wno` category
options were added. `/W4`, `/UNDEBUG`, real headers and UBSan remain enabled.

The source closure consists of builder/compare/control-edge/CFG/normalization,
place eligibility and queries, scalar-scratch facts, SSA/promotion, linear and
CFG/loop effects, state-map production; Core model/binding rows, structural,
constant, SSA and effect/backedge verification, ownership/liveness, state-map
storage/deopt/materialization support, execution contracts and the opaque owner.
The materialization support TU is linked to supply actual map validation and
lookup; the test does not call concrete materialization.

## Exact eight cases

| Case | Gate |
| --- | --- |
| 1 promoted_diamond | Independent diamond phi, promotion, constants and retained empty state map |
| 2 promoted_loop | Independent loop-carried phi, ADD dataflow, constants and retained empty state map |
| 3 parameter_diamond | Genuine parameter storage, heap/effect merge and six map phases |
| 4 parameter_loop | Genuine parameter storage, carried heap/effect dependencies and six map phases |
| 5 two_functions | Distinct published tokens7001/7002 and signatures0x1234567887654321/0x2234567887654321; generation1 and local ValueId1 independently in both functions; shared module constant indexes0–3 |
| 6 phi_predecessor_refusal | Clone-only diamond predecessor2→3; exact diagnostic, restoration and original unchanged |
| 7 stale_memory_refusal | Clone-only header LOAD5 consumes entry-store output instead of header heap phi; exact diagnostic, restoration and original unchanged |
| 8 map_source_refusal | Clone-only STORE4 map source4→999; exact diagnostic, restoration and original unchanged |

Negative cases compare all nine stable diagnostic fields. Phi refusal expects
block4/instruction10/source10, expectedVersion2/actualVersion3, hashes0.
Memory refusal expects block2/instruction5/source5, expected header heap token,
actual entry-store heap token, hashes0. Map refusal expects
block2/instruction4/source999, versions and hashes4/999. Every diagnostic names
published token7001. No negative test mutates borrowed owner storage.

Each case registers its owner and mutable clone before checks. Failed checks
record numerical line/expected/actual observations and reach cleanup. Producer
failure observations include stage, allocations/releases, built functions,
transient release and owner publication. Reporting occurs after all registered
resources are released; no aborting CHECK/Unity longjmp path exists.

## Dedicated route and evidence

The new `ssa-retained-builder-direct-validation/CMakeLists.txt` restricts the
finite route to actual E:/Git/zr_vm and a separate build child under
E:/cargo-targets/zr_vm/build. It requires Windows, Ninja, Debug and an MSVC
compiler frontend. UBSan defaults on and requires Clang with no recovery.
For clang-cl it checks that the reviewed toolchain link rule passes both
sanitizer driver flags before /link. The ordinary executable compiler ABI probe
is retained.

The accepted configuration emitted:

- `compile_commands.json`
- `retained-builder-target-sources.tsv`: absolute source paths and SHA256
- `retained-builder-settings.tsv`: compiler, linker, link rule and target settings
- `retained-builder-cfg.map` after actual linking

Root reviewed the actual object/link/import closure before executing
`ctest -R '^ssa_retained_builder_cfg$' --output-on-failure --no-tests=error`
in the reviewed build directory. The six runtime phases were configure, build,
latest dependencies, all defined symbols, all undefined symbols and CTest;
each exited naturally with 0. The fresh build contained 31 compilation steps
and one link. CTest reported the eight names above in the same order, all PASS,
and `ssa_retained_builder_cfg cases=8 failed=0`.

The actual latest dependency records cover all 31 source edges. Defined-symbol
and import inventories each cover all 31 objects. The linker map and symbol
records identify sole actual providers for `BuildModule`, `CloneOwnedModule`,
`VerifyModule` and `ValidateStateMap`. External dependencies use real
header-inline array/fact helpers and `/MD` CRT allocation/memory/qsort/assert/
stdio operations, plus compiler/runtime and sanitizer imports. No Source,
VM bootstrap, execution oracle, native callback, security,
network or FFI behavior is called. STORE and THROW properties are IR metadata;
they do not perform stores into runtime objects or throw language exceptions.

## Historical attempt and compiler evidence

Runtime r1 compiled all 31 sources and linked with exit0, but its build log
contained 23 warnings: 20 Windows CRT `strcpy` deprecation warnings admitted
through actual headers and three aggregate-brace warnings in the builder.
The controller rejected that attempt before CTest. Its false receipt is retained.
Root aligned the target with standard Windows CRT configuration and changed
three zero initializers on two builder lines to `{{0u}, 0u}`. Runtime r2 used
those exact current files; no unrelated production warnings were reclassified.

The MSVC compile-only gate used actual C11, `/experimental:c11atomics`, `/MD`,
`/W4`, `/UNDEBUG` and no forced include for all 31 sources. Each raw output
contained its source basename only. Actual `/sourceDependencies` JSON Version
1.2 records identify Source and Includes; ProvidedModule is present and empty,
while ImportedModules and ImportedHeaderUnits are absent. No link, runtime or
fixed ABI byte-layout claim follows from that compiler receipt.

At Root's runtime review, before this documentation update, all 294 selected
inputs, seven metadata files, 33 artifacts and six raw logs matched their pins.
The selected inputs include the three original guides: their receipt pins are
historical review metadata once the guides are updated. Current guide hashes
are frozen separately for Root's Git transaction; this update does not claim
that all original 334 input/metadata/artifact pins remain current or that runtime
was rerun. The 291 other selected inputs and the compiler/tool/header evidence
retain their identities. Root also verified 240 MSVC selected inputs and 93
raw-output/object/dependency artifacts before this update.

## Scope limits

OOM injection, leak instrumentation, complete module-copy coverage, runtime
Function ownership, exceptions/cleanup/suspend, source-level loop semantics and
pass differential execution have separate denominators. This gate does not
claim them. Validation allocates; it supplies no zero-allocation evidence.
Shared test registration is left to Root's separately leased integration work.
All47, Source, M1, full Linux, full MSVC runtime and normal/default project
configuration/runtime acceptance remain incomplete.
