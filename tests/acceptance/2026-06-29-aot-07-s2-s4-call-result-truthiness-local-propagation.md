# AOT 07-S2/S4 Call-Result Truthiness Local Propagation

## Scope

This acceptance record covers one narrow 07-S2/S4 slice inside M1.5: generic truthiness consumers can reuse a no-argument typed numeric call result as a scalar local when the callee return kind is provable.

Affected layers:

- AOT scalar-local declaration and written-before proof in `backend_aot_c_scalar_locals.c`.
- Generic logical lowering in `backend_aot_c_lowering_generic_logical.c`.
- Source-level logical shared-library smoke and source-contract tests under `tests/parser`.

The accepted generated shape for the source-level fixture is:

- `var inverted = !zero();` calls the typed i64 thunk into `zr_aot_s4`, then writes `zr_aot_b3 = (TZrBool)(zr_aot_s4 == (TZrInt64)0);`.
- `if (!one())` calls the typed i64 thunk into `zr_aot_s6`, then writes `zr_aot_b5 = (TZrBool)(zr_aot_s6 == (TZrInt64)0);`.
- Both cases avoid `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 3, 4)`, `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 5, 6)`, and `SyncBoolLocal` for the call-result source slots.

## Baseline

Before this slice, call-result scalar-local inference could discover later `JUMP_IF` consumers and stack-copy consumers, but generic `LOGICAL_NOT` over a no-argument numeric call result did not recover the callee return kind. The focused source-level truthiness fixture still materialized the call result through the runtime generic logical-not helper and bool sync path.

Initial RED for this slice was the focused GCC logical shared-library smoke failing on the new generated-C assertion:

```text
test_aot_c_generated_shared_library_executes_generic_truthiness_boundary_helpers:FAIL: Expected Non-NULL
```

During adjacent validation, `test_aot_c_call_shared_library_smoke` also failed after its static numeric call case still expected the older `zr_aot_direct_stack_copy_sync_*_local_boundary` markers. Root-cause tracing showed the generated code had already moved to the previously accepted numeric scalar stack-copy shape:

```text
/* zr_aot_scalar_stack_copy_u64 dstSlot=4 srcSlot=5 */
zr_aot_u4 = zr_aot_u5;
/* zr_aot_scalar_stack_copy_f64 dstSlot=5 srcSlot=6 */
zr_aot_f5 = zr_aot_f6;
```

That upper-layer failure was a stale test expectation, not a call-result truthiness regression.

## Test Inventory

- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
  - Extends the generic truthiness source-level fixture with `var inverted = !zero();` and `if (!one())`.
  - Requires typed i64 direct calls for both numeric call-result slots.
  - Requires i64 scalar-local generic `LOGICAL_NOT` bool destinations.
  - Forbids generic logical-not runtime helper and bool sync for slots 4 and 6.
- `tests/parser/test_aot_c_logical_contracts.c`
  - Locks call-result callee-kind recovery helpers.
  - Locks no-arg typed i64 thunk recognition for call-result truthiness.
  - Locks the generic `LOGICAL_NOT` destination/source relation used by scalar-local inference.
- `tests/parser/test_aot_c_call_shared_library_smoke.c`
  - Updates the neighboring static numeric call smoke to assert accepted scalar stack-copy markers instead of stale direct stack-copy sync markers.
- Existing adjacent tests:
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`
  - `zr_vm_aot_c_generic_bool_equality_local_smoke_test`
  - control contracts, frame setup contracts, call contracts, and call shared-library smoke.

Boundary coverage:

- Numeric zero call result through `!zero()` returns true and is stored in a bool local.
- Numeric one call result through `!one()` returns false and drives a branch.
- `if (zero())` remains covered by the existing generic `JUMP_IF` i64 scalar-local path.
- The slice is intentionally limited to no-argument typed bool/i64/u64/f64 callee return proofs. Dynamic, string, object, and unknown callable truthiness remain on runtime boundaries.

## Tooling Evidence

Tool versions:

```text
gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0
Ubuntu clang version 14.0.0-1ubuntu1.1
cmake version 3.22.1
MSVC cl.exe 19.44.35228.0
Windows CMake version 3.23.0-rc2
```

Focused GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test'
```

GCC adjacent command:

```text
wsl -e bash -lc '
set -e
cmake --build build-wsl-gcc --target \
  zr_vm_aot_c_logical_contracts_test \
  zr_vm_aot_c_logical_shared_library_smoke_test \
  zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test \
  zr_vm_aot_c_generic_jump_if_bool_local_smoke_test \
  zr_vm_aot_c_generic_bool_equality_local_smoke_test \
  zr_vm_aot_c_control_contracts_test \
  zr_vm_aot_c_frame_setup_contracts_test \
  zr_vm_aot_c_call_shared_library_smoke_test \
  zr_vm_aot_c_call_contracts_test -j 8
./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_control_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test
./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test
'
```

Clang adjacent command used the same target and executable list under `build-wsl-clang`.

Windows MSVC Debug command:

```text
. 'C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1'
cmake --build build-msvc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_call_contracts_test --config Debug --parallel 8
```

## Results

- Focused WSL GCC logical shared-library smoke: 6 tests, 0 failures.
- WSL GCC adjacent group:
  - logical contracts 4/0
  - logical shared-library smoke 6/0
  - generic LOGICAL_NOT numeric local smoke 1/0
  - generic JUMP_IF bool/numeric/stack-copy local smoke 3/0
  - generic bool equality local smoke 1/0
  - control contracts 2/0
  - frame setup contracts 1/0
  - call shared-library smoke 5/0
  - call contracts 8/0
- WSL Clang adjacent group: same test counts passed. The build printed clock-skew warnings, but all listed tests completed with 0 failures.
- Windows MSVC Debug:
  - logical contracts 4/0
  - control contracts 2/0
  - frame setup contracts 1/0
  - call contracts 8/0
  - Unix-only shared-library smoke binaries reported 0 failures with expected ignored cases: logical shared 6 ignored, generic LOGICAL_NOT numeric 1 ignored, generic JUMP_IF 3 ignored, generic bool equality 1 ignored, call shared 5 ignored.
- The adjacent call shared-library failure was resolved by updating its static numeric call assertions to the already accepted scalar stack-copy local shape.

## Acceptance Decision

Accepted as a completed 07-S2/S4 sub-slice.

This does not complete 07-S2/S4 or 07/M1.5. Remaining work includes broader generic/dynamic/string/object truthiness, value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full typed function-body zero-frame proof.
