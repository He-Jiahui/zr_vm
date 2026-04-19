# 2026-04-19 W3 GC Region And Registry Hotspots

## Scope

This note closes the W3 GC hotspot slice for the current wave:

- remove the linear remembered-registry membership/removal cost
- remove the linear region-descriptor lookup in region release/reassign paths
- re-profile `gc_fragment_baseline` and `gc_fragment_stress`

This note is about the measured GC cost line. AOT remains compatibility-only and is not treated as a performance
target in this acceptance step.

## Implementation Summary

The accepted change turns remembered-registry and region-descriptor bookkeeping into cached-index maintenance instead of
repeated linear scans.

Key production updates:

- `zr_vm_common/include/zr_vm_common/zr_object_conf.h`
  - add `rememberedRegistryIndex`
  - add `regionDescriptorIndex`
- `zr_vm_core/include/zr_vm_core/raw_object.h`
  - initialize both indices to `ZR_MAX_SIZE`
- `zr_vm_core/src/zr_vm_core/gc/gc_object.c`
  - add cached region-descriptor lookup helpers
  - add cached allocate/release/reassign helpers
  - make remembered-registry membership/removal O(1)
- `zr_vm_core/src/zr_vm_core/gc/gc_mark.c`
  - stamp `rememberedRegistryIndex` when remembered objects are appended
- `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c`
  - restamp/clear remembered and region indices during prune, rewrite, and evacuation paths
- `zr_vm_core/src/zr_vm_core/gc/gc.c`
  - use cached region reassignment on pin/reassign paths
- `tests/gc/gc_tests.c`
  - add regression coverage for remembered-index and region-descriptor-index bookkeeping

The old allocate/release/reassign entry points remain as wrappers, but the hot work now flows through cached-index
helpers.

## Validation

### WSL gcc

Command:

```bash
cmake --build build-wsl-gcc --target zr_vm_gc_test --clean-first -j 8
./build-wsl-gcc/bin/zr_vm_gc_test
```

Result:

- `52 Tests 0 Failures 0 Ignored`

### WSL clang

Command:

```bash
cmake --build build-wsl-clang --target zr_vm_gc_test --clean-first -j 8
./build-wsl-clang/bin/zr_vm_gc_test
```

Result:

- `52 Tests 0 Failures 0 Ignored`

`--clean-first` is intentional here because stale ABI artifacts in the existing build directory previously produced a
bogus startup crash after the raw-object/header layout change.

## Benchmark Evidence

Baseline snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_w3_gc_baseline_20260419`

After snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_w3_gc_after_registry_index_20260419`

The after snapshot needed manual callgrind capture for the GC cases because the profile-tier representative capture does
not automatically emit `gc_fragment_*` callgrind artifacts.

Wall-ms comparison for `ZR interp`:

- baseline `gc_fragment_baseline = 474.053 ms`
- baseline `gc_fragment_stress = 1952.605 ms`
- after `gc_fragment_baseline = 476.112 ms`
- after `gc_fragment_stress = 1207.958 ms`

Derived comparison:

- stress/base ratio improved from `4.119` to `2.537`
- baseline case is effectively flat at `+2.059 ms`
- stress case improved by `744.647 ms`
- stress wall time improved by `38.14%`

Instruction-count comparison from the stress callgrind captures:

- baseline total Ir: `5,324,395,008`
- after total Ir: `2,687,736,329`
- net stress Ir change: `-49.52%`

## Hotspot Shift

Before, the stress case was dominated by the linear registry/region work:

- `garbage_collector_release_region_allocation = 1,541,092,102 Ir`
- `garbage_collector_forget_object_from_registries = 1,100,635,438 Ir`
- `garbage_collector_remembered_registry_contains = 45,301,853 Ir`
- `garbage_collector_allocate_region_id = 64,154,497 Ir`

After the cached-index change:

- `garbage_collector_release_region_allocation_cached = 21,869,448 Ir`
- `garbage_collector_forget_object_from_registries = 16,860,483 Ir`
- `garbage_collector_remembered_registry_contains = 1,057,748 Ir`
- `garbage_collector_allocate_region_id_cached = 69,372,565 Ir`

Relevant reductions:

- release path: `-98.58%`
- forget-object-from-registries: `-98.47%`
- remembered-contains: `-97.67%`

The top after-state stress hotspots are now led by different work:

- `zr_container_map_set_item_readonly_inline_no_result_fast = 345,157,161 Ir`
- `zr_container_map_get_item_readonly_inline_fast = 174,706,042 Ir`
- `_int_malloc = 146,479,158 Ir`
- `garbage_collector_mark_value = 135,509,626 Ir`
- `ZrCore_Execute = 122,092,410 Ir`
- `garbage_collector_ignore_registry_contains = 69,839,445 Ir`
- `garbage_collector_allocate_region_id_cached = 69,372,565 Ir`

That is the intended W3 outcome: the old linear remembered/region bookkeeping is no longer the dominant cost family.

## Acceptance Decision

Accepted as the W3 GC hotspot closeout for this wave.

This note claims:

- the remembered-registry and region-descriptor linear hotspots were materially removed
- both WSL toolchains pass the GC regression suite on the current worktree
- the stress benchmark improved substantially without an unexplained baseline-case regression

This note does not claim:

- that GC is now globally "done"

The next GC work, if continued, should move to the new after-state leaders:

- mark-side cost
- ignore-registry cost
- region allocation cost
- container/allocation support around the stress case
