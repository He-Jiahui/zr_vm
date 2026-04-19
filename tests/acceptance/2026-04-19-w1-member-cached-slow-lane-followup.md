# 2026-04-19 W1 Member Cached Slow-Lane Follow-Up

## Scope

This note records the follow-up slice after the earlier exact-receiver-pair landing in
`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`.

The accepted change here is narrower:

- keep the outer single-slot exact-receiver pair hot path as-is
- trim the remaining cached slow lane inside `execution_member_get_cached`
- trim the remaining cached slow lane inside `execution_member_set_cached`
- avoid repeating the same exact-receiver object work before falling back to prototype-based cached resolution

This is still W1 runtime work. It is not another W2 quickening hit-rate pass.

## Implementation

Primary production file:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Accepted runtime changes:

1. resolve `receiverObject` once at the `execution_member_try_cached_get/set` entry
2. keep single-slot exact-receiver fast handling isolated in dedicated helpers
3. merge the multi-slot exact-object lane and prototype lane into one slot walk instead of:
   - first scanning for exact-object hits
   - then scanning the same slots again for prototype hits
4. preserve the existing fast own-field, callable, descriptor, and version-mismatch behavior while removing duplicated work

The hot-path intent is simple: reduce runtime work inside `dispatch_loops` without pretending this is another opcode-mix improvement.

## Validation

### WSL gcc

Commands:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_execution_member_access_fast_paths_test zr_vm_instructions_test zr_vm_execution_dispatch_callable_metadata_test -j 8"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test && ./build-wsl-gcc/bin/zr_vm_instructions_test && ./build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test"
```

Result:

- all focused targets passed

### WSL clang

Commands:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_execution_member_access_fast_paths_test zr_vm_instructions_test zr_vm_execution_dispatch_callable_metadata_test -j 8"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test && ./build-wsl-clang/bin/zr_vm_instructions_test && ./build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test"
```

Result:

- all focused targets passed
- the temporary unused-helper warning introduced during refactor was removed before acceptance

### Windows MSVC CLI smoke

Command:

```powershell
.\build-msvc-cli-smoke\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

Result:

- passed
- output: `hello world`

## Fresh Tracked Snapshot

Before snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_exact_pair_20260419`
- report timestamp: `2026-04-19T12:07:54Z`

After snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_cached_slow_lane_20260419`
- report timestamp: `2026-04-19T14:06:08Z`

Tracked-case command:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm/build/benchmark-gcc-release && export ZR_VM_TEST_TIER=profile && export ZR_VM_PERF_CALLGRIND_COUNTING=1 && export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp && export ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop && ctest -R '^performance_report$' --output-on-failure"
```

All eight tracked non-GC `ZR interp` rows report `PASS`.

## Before / After

### Dispatch hotspot

`dispatch_loops` remains the hottest tracked non-GC interpreter case.

Callgrind totals:

- total Ir: `368,618,055 -> 368,501,763` (`-116,292`, `-0.0315%`)

The important function-local change is inside the cached member execution bodies, not in opcode mix:

- `execution_member_get_cached`: `53,630 -> 52,932` (`-698`, `-1.30%`)
- `execution_member_set_cached`: `28,289 -> 27,593` (`-696`, `-2.46%`)

The next hotter runtime bodies did not move:

- `value_to_int64 = 2,764,800 -> 2,764,800`
- `execution_try_builtin_mul = 2,534,400 -> 2,534,400`

That is exactly the expected shape for this slice: less cost inside cached member execution, with the remaining first-order cost still sitting in shared numeric/runtime helpers.

### Tracked opcode / helper / slowpath mix

Tracked aggregate opcode totals are unchanged:

- `GET_MEMBER = 2`
- `GET_MEMBER_SLOT = 383,727`
- `SET_MEMBER_SLOT = 174,093`
- `FUNCTION_CALL = 327`
- `KNOWN_NATIVE_CALL = 4,167`
- `KNOWN_VM_CALL = 18,428`
- `TO_OBJECT = 24`

Tracked aggregate slowpaths are unchanged:

- `meta_fallback = 2,064`
- `protect_e = 3,981`
- `callsite_cache_lookup = 640`

Tracked aggregate helpers are effectively unchanged:

- `value_copy = 5,210,463 -> 5,210,465`
- `stack_get_value = 738,812 -> 738,812`
- `get_member = 384,622 -> 384,622`
- `set_member = 174,068 -> 174,068`
- `precall = 178,850 -> 178,850`

The only visible suite-level counter drift is `mixed_service_loop` adding `+2` to `value_copy` and `value_reset_null`, with identical opcode and slowpath counts. That is not a hotspot reorder and does not change the acceptance conclusion.

## Measurement Boundary

The profile-tier markdown wall-ms table from the after snapshot is not used as the acceptance signal for this slice.

Reason:

- this rerun was taken in `profile` tier with callgrind instruction counting enabled
- the report is a single measured iteration
- the wall-ms inflation affected the whole suite together, while opcode totals, slowpaths, helper counts, and callgrind Ir stayed effectively flat

So the trustworthy signal here is:

1. focused WSL gcc/clang runtime validation stayed green
2. Windows CLI smoke stayed green
3. tracked non-GC opcode mix did not regress
4. `dispatch_loops` callgrind Ir dropped inside `execution_member_get_cached/set_cached`

That is enough to accept the slice honestly without pretending the noisy wall-ms numbers are a real throughput regression.

## Acceptance Decision

Accepted for W1 cached-member slow-lane follow-up.

This note claims:

- the cached slow lane inside `execution_member_get_cached/set_cached` is cheaper on the tracked line
- the improvement comes from runtime execution cost reduction, not from another specialization / quickening hit-rate change
- there is no tracked non-GC opcode-mix or slowpath regression explaining the remaining hotspot line
- the next priority still belongs to:
  - `value_copy`
  - `stack_get_value`
  - `value_to_int64`
  - `execution_try_builtin_mul`

This note does not claim:

- that W2 residual generic-call debt disappeared
- that profile-tier wall-ms under callgrind counting is a stable throughput acceptance metric

The bounded W2 residue remains the same:

- `call_chain_polymorphic` generic `FUNCTION_CALL`
- `callsite_cache_lookup`
