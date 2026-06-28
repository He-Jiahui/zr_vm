# AOT 07-S2/S4 Generic JUMP_IF Numeric-Source Local Branch

## Scope

This acceptance record covers one narrow 07-S2/S4 slice: generic `JUMP_IF` may lower to a primitive numeric scalar-local branch when the condition slot is already proven to hold an i64, u64, or f64 scalar value.

The accepted generated C shape keeps existing `JUMP_IF` false-condition semantics:

- signed integer: `if (zr_aot_sS == (TZrInt64)0) { goto ...; }`
- unsigned integer: `if (zr_aot_uS == (TZrUInt64)0u) { goto ...; }`
- float: `if (zr_aot_fS == (TZrFloat64)0.0) { goto ...; }`

Dynamic, object, string, and otherwise unproven truthiness stays on `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy`.

## Baseline

Before this slice, generic `JUMP_IF` had a bool scalar-local fast path only. Proven numeric conditions still emitted a local `TZrBool zr_aot_truthy = ZR_FALSE;` and called `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, conditionSlot, &zr_aot_truthy)`.

## Test Inventory

- `tests/parser/test_aot_c_logical_contracts.c` locks numeric condition proof, marker strings, and frame-descriptor local-only proof.
- `tests/parser/test_aot_c_generic_jump_if_bool_local_smoke.c` now has a second Unix shared-library fixture covering i64, u64, f64, and zero-branch behavior.
- `tests/parser/test_aot_c_logical_shared_library_smoke.c` now expects source-level `if (zero())` to use the i64 local branch rather than requiring a truthiness helper for that fixture.
- `tests/parser/test_aot_c_control_contracts.c`, `tests/parser/test_aot_c_frame_setup_contracts.c`, and `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c` stayed in the adjacent regression set.

## Tooling Evidence

Initial RED:

```text
./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test
4 Tests 1 Failures 0 Ignored
missing required source contract text:
backend_aot_c_scalar_locals_i64_written_before(functionIr, conditionSlot, execInstructionIndex)
```

Focused WSL GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test'
```

Related WSL GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'
```

WSL Clang command:

```text
wsl -e bash -lc 'cmake --build build-wsl-clang --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 8'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_logical_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'
```

Windows MSVC Debug command:

```text
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build-msvc
cmake --build build-msvc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test --config Debug --parallel 8
```

Generated-C evidence:

- The numeric fixture contains `zr_aot_generic_jump_if_i64_scalar_local`.
- It contains `zr_aot_generic_jump_if_u64_scalar_local`.
- It contains `zr_aot_generic_jump_if_f64_scalar_local`.
- It branches with `if (zr_aot_s0 == (TZrInt64)0) {`.
- It branches with `if (zr_aot_u2 == (TZrUInt64)0u) {`.
- It branches with `if (zr_aot_f3 == (TZrFloat64)0.0) {`.
- It branches with `if (zr_aot_s4 == (TZrInt64)0) {` for the zero-condition success path.
- It does not call `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, 0/2/3/4`.
- It does not allocate `TZrBool zr_aot_truthy = ZR_FALSE;`.

## Results

- WSL GCC: logical contracts 4/0, generic JUMP_IF bool/numeric local smoke 2/0, control contracts 2/0, frame setup contracts 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- WSL Clang: logical contracts 4/0, generic JUMP_IF bool/numeric local smoke 2/0, control contracts 2/0, frame setup contracts 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- Windows MSVC Debug: logical contracts 4/0, control contracts 2/0, frame setup contracts 1/0, generic JUMP_IF bool/numeric local smoke 0 failures / 2 ignored, generic bool equality smoke 0 failures / 1 ignored, logical shared-library smoke 0 failures / 6 ignored because those smoke cases are Unix-only.
- `git diff --check` exits with only LF/CRLF warnings for the touched files.

## Acceptance Decision

Accepted as a completed 07-S2/S4 sub-slice. It does not complete 07-S2/S4 or the 07-12 plan.

Remaining work includes dynamic/object/string truthiness, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and the full typed function-body zero-frame proof.
