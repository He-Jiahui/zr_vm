# 2026-04-20 W2 Call Chain Dispatch Callable-Parameter Provenance

## Scope

This note closes the bounded deferred W2 residue inside
`call_chain_polymorphic` where the child `dispatch(callable, value, delta)`
function still left its hot tail call on the cached dynamic/meta path instead
of the known-VM call family.

Primary production file:

- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`

Focused regression coverage:

- `tests/parser/test_compiler_regressions.c`
- `tests/parser/test_compiler_call_lowering_focus_main.c`

## Root Cause

Two compiler-side proof gaps overlapped on the real benchmark project path:

1. typed callable provenance recovery could resolve runtime prototypes only
   through the global type library, so locally compiled classes like
   `PolyAdder`, `PolyMultiply`, and `PolyXor` could miss their owner-local
   `@call` prototype metadata
2. known-call quickening only closed over the older generic callable proof
   paths, so the child `dispatch(...)` parameter tail site could stay on
   `SUPER_DYN_TAIL_CALL_CACHED` even when every owner callsite passed a VM
   callable of the same local class family

The result was a bounded but real residual pocket:

- outer benchmark callsites still kept a few generic `FUNCTION_CALL`s
- the child `dispatch` tail site stayed on cached dynamic/meta machinery
- `callsite_cache_lookup` remained at `640` in the focused profile

## Implementation

`compiler_quickening.c` now keeps callable provenance on the real project path
instead of relying on the global type library alone.

Accepted production changes:

1. add owner-local runtime prototype recovery:
   - `compiler_quickening_find_owner_runtime_prototype_by_name(...)`
   - `compiler_quickening_resolve_type_ref_runtime_prototype(...)`
   now falls back from `ZrLib_Type_FindPrototype(...)` to the current
   prototype owner's `prototypeInstances`, materializing them from
   `prototypeData` when needed
2. reuse typed slot type-ref recovery for callable provenance:
   - `compiler_quickening_resolve_typed_slot_callable_provenance_before_instruction(...)`
3. recover child parameter callable provenance from owner callsites:
   - `compiler_quickening_resolve_child_parameter_callable_provenance_from_owner_calls(...)`
4. allow known-call quickening to rewrite resolved dynamic/meta call families
   into `KNOWN_VM_CALL` / `KNOWN_VM_TAIL_CALL` and
   `KNOWN_NATIVE_CALL` / `KNOWN_NATIVE_TAIL_CALL` once provenance is proven

## Validation

### WSL gcc focused parser coverage

Command:

```bash
cd /mnt/e/Git/zr_vm/build-wsl-gcc
ninja -j8 zr_vm_compiler_call_lowering_focus_test zr_vm_parser_test
./bin/zr_vm_compiler_call_lowering_focus_test
./bin/zr_vm_parser_test
```

Result:

- `zr_vm_compiler_call_lowering_focus_test`: `7 Tests 0 Failures 0 Ignored`
- `zr_vm_parser_test`: `64 Tests 0 Failures 0 Ignored`

## Focused Benchmark Evidence

Pre snapshot on the live runtime worktree before this compiler fix:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_add_mixed_numeric_exact_fast_20260420-105913`

Focused after snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_call_chain_polymorphic_after_dispatch_callable_parameter_known_vm_tail_20260420-114236`

Command:

```bash
cd /mnt/e/Git/zr_vm/build/benchmark-gcc-release
export ZR_VM_TEST_TIER=profile
export ZR_VM_PERF_CALLGRIND_COUNTING=1
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp
export ZR_VM_PERF_ONLY_CASES=call_chain_polymorphic
ctest -R '^performance_report$' --output-on-failure
```

Focused `call_chain_polymorphic` profile changes:

- `FUNCTION_CALL = 322 -> 319`
- `KNOWN_VM_CALL = 960 -> 963`
- `SUPER_DYN_TAIL_CALL_CACHED = 320 -> 0`
- `KNOWN_VM_TAIL_CALL = 0 -> 320`
- `callsite_cache_lookup = 640 -> 0`
- `meta_call_prepare = 0 -> 320`

Wall-ms movement in the same focused profile/callgrind configuration:

- `107.018 ms -> 101.698 ms`

Interpretation:

- the outer benchmark callsites recover three more direct known-VM calls
- the child `dispatch(...)` tail site no longer pays the cached dynamic tail
  lane
- the old `callsite_cache_lookup` pocket is removed on this benchmark path
- the remaining residual is no longer "cached dyn tail lookup"; it has shrunk
  to the still-explicit remaining generic calls plus bounded meta-call
  preparation work

## Acceptance Decision

Accepted as the bounded W2 follow-up for callable-parameter provenance inside
`call_chain_polymorphic`.

This note claims:

- owner-local runtime prototypes are now recoverable for typed callable
  provenance
- child `dispatch(callable, ...)` tail-call lowering now reaches the known-VM
  family on the real benchmark project path
- the old benchmark-local `callsite_cache_lookup` residue on this path is
  closed

This note does not claim:

- that every remaining generic call in the tracked suite is gone

That stronger claim would still be false. The accepted conclusion is narrower:
the deferred `call_chain_polymorphic` dispatch-callable residue is now closed as
a bounded W2 follow-up instead of remaining an open quickening miss.
