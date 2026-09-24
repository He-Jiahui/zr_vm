# Logical aggregate reconstruction recipes

## Requirement and boundary

Plan 01.04 batch 2 needs scalarized fields in the shared logical frame. Existing
SROA materialization maps describe field placement and identity facts, but do
not bind fields to checkpoint SSA values. The state-map consumer previously
preserved only a flat set of SSA IDs.

This stage adds function-owned aggregate identity/field recipes, deopt-scoped
field liveness, shared structural validation and transactional logical-state
preparation. Stable IDs describe aliases, forward references and cycles.
Explicit uninitialized fields do not create fake live values or roots. Type and
layout tokens stay logical; no host addresses enter the recipe.

This is a prerequisite for runtime object reconstruction. It does not yet
allocate heap objects, switch frames, synthesize recipes from SROA, reconstruct
inline call stacks or execute conditional cleanup. Those requirements remain
open in 01.04; this stage is not a completion claim for that plan.

## Reference evidence

- JDK `lua/jdk/src/hotspot/share/code/debugInfo.hpp`, `ObjectValue`: eliminated
  objects carry a distinct identity and field descriptors.
- JDK `lua/jdk/src/hotspot/share/runtime/deoptimization.cpp`, `realloc_objects`
  and `reassign_fields`: object allocation and field restoration are distinct
  steps. `TestRematerializeObjects.java` compares optimized deoptimization with
  interpreter results and checks restored field values.
- CPython `lua/cpython/Lib/copy.py`, `deepcopy` and `_deepcopy_list`: the identity
  memo is established before recursive field traversal.
  `lua/cpython/Lib/test/test_copy.py`, `test_deepcopy_memo`, requires repeated
  references to remain aliases. Zr uses stable recipe IDs instead of host IDs.
- Rust `lua/rust/compiler/rustc_mir_transform/src/elaborate_drops.rs`,
  `drop_style`: field initialization determines whether cleanup is dead,
  unconditional, conditional or open. Zr preserves uninitialized fields here;
  dynamic cleanup flags are a later consumer, not inferred from zero values.

## Test evidence

The initial ten tests compiled against the new schema with old production
behavior and all ten failed. Failures showed missing live fields/roots, missing
materialized and cloned recipes, accepted malformed metadata, and omitted
borrowed/unavailable field checks. This is the executable red baseline.

The expanded 21-case MSVC run passed all 19 core cases and failed the two
integration regressions: DCE removed a COPY used only by an aggregate field,
and the function hash did not change with that field's binding. Both consumers
now account for aggregate recipes. Independent specification review then found
that a field bound to the checkpoint's own result disappeared from BEFORE_EFFECT.
The added regression failed before the liveness fix; the complete 25-case core
suite subsequently passed on GCC.

Escape analysis also needed recovery uses in its last-use calculation. The
new three-scenario fixture checks recovery before, at and after suspension.
Both GCC and MSVC reproduced the missing last-use update before the fix.
The Windows negative run waited in the CRT assertion dialog; the owned test
process was stopped and its assertion output captured. The fixture now routes
assertions to stderr for unattended CTest execution.

Quality review also found that target storage could alias allocations indirectly
owned by the function. Three new regressions reproduced the unsafe acceptance
of frame slots, GC slots and a function-owned map pool when the request uses a
separate map: 25 passed, three failed on GCC before the fix. The overlap guard
now includes those objects and every owned pool before the commit can free any
old target storage.

Final validation on 2026-09-24 passed all 16 selected suites in each environment:

| Environment | Build directory | Result |
| --- | --- | --- |
| WSL GCC 11.4 Debug | `build/ssa-gcc-debug` | 16/16 passed |
| WSL Clang 14 Debug | `build/ssa-clang-debug` | 16/16 passed |
| Windows MSVC 19.44.35228 Debug | `build/ssa-msvc-debug` | 16/16 passed |
| WSL GCC ASan/UBSan | `build/ssa-gcc-asan-phase80` | 16/16 passed |

The aggregate suite contains 28 cases. Coverage includes both recipe allocation
failure ordinals (old values, roots and graph remain unchanged), repeated
preparation of a 257-object cycle with a single managed root, malformed/partial
initialization metadata, deep cloning, field-only DCE uses, hash invalidation,
phase availability and borrowed storage rejection. Escape tests additionally
exercise recovery before, at and after suspension. The sanitizer run enables
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`; no sanitizer findings occurred.

The build target set is `zr_vm_ssa_<name>_test` for the 15 `ssa_` suites below,
plus `zr_vm_container_specialization_test`. Build each target set with
`cmake --build <directory> --target <targets> -j 2`, then run:

```sh
ctest --test-dir <directory> --output-on-failure --no-tests=error --timeout 60 \
  -R '^(ssa_(deopt_aggregates|state_maps|state_map_liveness|state_map_ownership|deopt_validation|oracle_resume|oracle_projections|oracle_parallel_edges|core_model|effects_verifier|pass_manager_scalar|place_promotion|generated_fusion|escape_ownership|interprocedural_inlining)|container_specialization)$'
```

Windows uses the imported VsDevCmd environment, `--config Debug` for the build,
and `-C Debug` for CTest. Existing unrelated place-promotion initializer warnings
and MSVC warning-level override diagnostics remain; the final changed-source
build introduced no new compiler warnings. Specification review passed after
the phase-availability fix. Quality review passed after the storage-alias fix
and inspection of the three red/green regressions above.

No source syntax or artifact writer changes are included. The fixtures exercise
the actual core verifier, parser state-map builder, function cloning and logical
state materializer; no mock verifier is used. Runtime GC stress and Nth-object
allocation failure belong to the still-pending heap reconstruction stage.
