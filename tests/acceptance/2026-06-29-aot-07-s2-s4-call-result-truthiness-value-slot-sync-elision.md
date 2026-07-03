# AOT 07-S2/S4 Call-Result Truthiness Value-Slot Sync Elision

## Scope

This acceptance record covers one narrow 07-S2/S4 slice inside M1.5: a no-argument typed i64 call result that is consumed only by proven scalar-local generic truthiness no longer synchronizes back to a `SZrValue` stack slot.

Affected layers:

- AOT scalar-local live-value and consumer proof in `backend_aot_c_scalar_locals.c`.
- Source-level logical shared-library smoke assertions in `tests/parser/test_aot_c_logical_shared_library_smoke.c`.

The accepted generated shape for the source-level fixture is:

- `var inverted = !zero();` calls the typed i64 thunk into `zr_aot_s4`, then writes the bool local through `zr_aot_generic_logical_not_i64_scalar_local`.
- `if (zero())` reads the typed i64 call result through `zr_aot_generic_jump_if_i64_scalar_local`.
- `if (!one())` calls the typed i64 thunk into `zr_aot_s6`, then writes the bool local through the numeric logical-not local path.
- These call-result paths avoid `zr_aot_typed_destination`, `ZR_VALUE_FAST_SET(zr_aot_typed_destination, ...)`, and `zr_aot_static_i64_no_arg_direct_call_sync_stack_slot`.

## Baseline

Before this slice, the source-level call-result truthiness fixture had already kept `zero()` and `one()` results in i64 scalar locals for generic logical-not lowering. The remaining gap was the live-value proof used by the typed no-arg call writer: it still treated the following generic truthiness instructions as value-slot consumers, so generated C wrote the i64 call result back through a `SZrTypeValue *zr_aot_typed_destination` even when no later value-slot read existed.

Initial RED for this slice was the focused GCC logical shared-library smoke failing after the fixture started forbidding that marker:

```text
test_aot_c_generated_shared_library_executes_generic_truthiness_boundary_helpers:FAIL: Expected NULL
```

## Test Inventory

- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
  - Requires `zr_aot_s4 = zr_aot_typed_i64_fn_1();`.
  - Requires `zr_aot_s6 = zr_aot_typed_i64_fn_2();`.
  - Requires numeric scalar-local generic logical-not and jump-if markers for the truthiness fixture.
  - Forbids `zr_aot_typed_destination`, `ZR_VALUE_FAST_SET(zr_aot_typed_destination, ...)`, and `zr_aot_static_i64_no_arg_direct_call_sync_stack_slot`.
- Existing adjacent tests:
  - `zr_vm_aot_c_logical_contracts_test`
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`
  - `zr_vm_aot_c_call_shared_library_smoke_test`
  - `zr_vm_aot_c_call_contracts_test`

Boundary coverage:

- Source-level no-arg typed i64 zero call result consumed by generic `LOGICAL_NOT`.
- Source-level no-arg typed i64 zero call result consumed by generic `JUMP_IF`.
- Source-level no-arg typed i64 one call result consumed by generic `LOGICAL_NOT` and then bool branch.
- The scalar-local consumer proof also includes u64/f64 generic truthiness consumers, but this acceptance slice does not add a source-level no-arg u64/f64 call-result fixture.
- Dynamic, string, object, and unknown callable truthiness remain on runtime boundaries.

## Tooling Evidence

Focused RED/GREEN command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test'
```

GCC adjacent validation command:

```text
wsl -e bash -lc '
set -e
cmake --build build-wsl-gcc --target \
  zr_vm_aot_c_logical_contracts_test \
  zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test \
  zr_vm_aot_c_generic_jump_if_bool_local_smoke_test \
  zr_vm_aot_c_call_shared_library_smoke_test \
  zr_vm_aot_c_call_contracts_test -j 8
./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test
'
```

Clang validation used the same target and executable list under `build-wsl-clang`, plus the logical shared-library smoke.

Windows MSVC Debug validation:

```text
. 'C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1'
cmake --build build-msvc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test --config Debug --parallel 8
build-msvc/bin/Debug/zr_vm_aot_c_logical_contracts_test.exe
build-msvc/bin/Debug/zr_vm_aot_c_call_contracts_test.exe
build-msvc/bin/Debug/zr_vm_aot_c_logical_shared_library_smoke_test.exe
```

## Results

Focused RED:

- WSL GCC logical shared-library smoke failed 1/6 at line 307 with `Expected NULL` while the generated C still contained the typed call-result value-slot sync marker.

Focused GREEN:

- WSL GCC logical shared-library smoke: 6/0.
- Generated C contained `zr_aot_s4 = zr_aot_typed_i64_fn_1();`, `zr_aot_s6 = zr_aot_typed_i64_fn_2();`, `zr_aot_generic_logical_not_i64_scalar_local`, and `zr_aot_generic_jump_if_i64_scalar_local`.
- Generated C no longer contained `zr_aot_typed_destination`, `ZR_VALUE_FAST_SET(zr_aot_typed_destination, ...)`, or `zr_aot_static_i64_no_arg_direct_call_sync_stack_slot`.

Adjacent validation:

- WSL GCC: logical shared-library smoke 6/0, logical contracts 4/0, generic LOGICAL_NOT numeric local smoke 1/0, generic JUMP_IF bool/numeric/stack-copy local smoke 3/0, call shared-library smoke 5/0, call contracts 8/0.
- WSL Clang: logical shared-library smoke 6/0, logical contracts 4/0, generic LOGICAL_NOT numeric local smoke 1/0, generic JUMP_IF bool/numeric/stack-copy local smoke 3/0, call shared-library smoke 5/0, call contracts 8/0.
- Windows MSVC Debug: logical contracts 4/0, call contracts 8/0, logical shared-library smoke 0 failures / 6 ignored.

## Acceptance Decision

Accepted as a focused 07-S2/S4 support slice.

This does not complete 07-S2/S4 or M1.5. Remaining work includes dynamic/string/object truthiness, value-copy migration, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, full typed function-body zero-frame proof, and the later 08-12 plan stages.
