# AOT 07-S2/S4 Generic JUMP_IF Bool-Source Local Branch

## Scope

This acceptance record covers one narrow 07-S2/S4 slice: generic `JUMP_IF` may lower to a bool scalar-local branch when the condition slot is already proven to hold a bool scalar value.

The accepted generated C shape is `if (!zr_aot_bS) { goto ...; }`, preserving the existing false-condition jump semantics for `JUMP_IF`. Dynamic, integer, object, string, and otherwise unproven truthiness stays on `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy`.

Affected layers:

- AOT C logical/control lowering
- AOT scalar-local declaration and written-before proof
- AOT generated-frame descriptor proof
- Parser test harness and shared-library smoke coverage

## Baseline

Before this slice, generic `JUMP_IF` always emitted a local `TZrBool zr_aot_truthy = ZR_FALSE;` and called `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, conditionSlot, &zr_aot_truthy)` even when the condition slot had already been written as a bool scalar local.

The first GREEN attempt generated the local branch body, but the hand-built fixture's bool constant condition did not declare `zr_aot_b0`. Root cause: scalar-local declaration and later-consumer scans did not classify generic `JUMP_IF` as a bool condition consumer.

## Test Inventory

- `tests/parser/test_aot_c_logical_contracts.c` locks the new helper, marker, prototype, function-body plumbing, scalar-local proof hooks, and frame-descriptor case.
- `tests/parser/test_aot_c_generic_jump_if_bool_local_smoke.c` builds a focused IR fixture with a bool constant condition, generic `JUMP_IF`, and two return paths.
- `tests/parser/test_aot_c_control_contracts.c` stays in the related group to guard existing branch/safepoint contracts.
- `tests/parser/test_aot_c_frame_setup_contracts.c` stays in the related group to guard generated-frame proof plumbing.
- `tests/parser/test_aot_c_logical_shared_library_smoke.c` remains the broader logical regression set.
- `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c` stays in the adjacent regression set because it shares strict bool-source scalar-local proof and branch consumption.

Boundary coverage:

- Proven bool condition: local branch generated and executed.
- False condition semantics: `JUMP_IF` jumps on false, returning the target-path value `17`.
- Fallback preservation: the local smoke forbids truthiness fallback only for the proven bool fixture; source-level mixed truthiness remains covered by the broader logical smoke.
- Platform boundary: Unix shared-library execution validates generated C execution; Windows builds the same target and reports the Unix-only branch as ignored.

## Tooling Evidence

Initial RED:

```text
./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test
4 Tests 1 Failures 0 Ignored
missing required source contract text: backend_aot_c_write_generic_jump_if_scalar_local(

./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test
1 Tests 1 Failures 0 Ignored
Expected Non-NULL (`zr_aot_generic_jump_if_bool_scalar_local` / `if (!zr_aot_b0)`)
```

Focused WSL GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test'
```

Related WSL GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'
```

WSL Clang command:

```text
wsl -e bash -lc 'cmake --build build-wsl-clang --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 8'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_logical_contracts_test'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_control_contracts_test'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_logical_shared_library_smoke_test'
wsl -e bash -lc './build-wsl-clang/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'
```

Windows MSVC Debug command:

```text
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build-msvc
cmake --build build-msvc --config Debug --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test --parallel 8
```

Generated-C evidence:

- The hand-built bool condition project contains `zr_aot_generic_jump_if`.
- It contains `zr_aot_generic_jump_if_bool_scalar_local`.
- It branches with `if (!zr_aot_b0) {` and `goto zr_aot_fn_0_ins_4;`.
- It does not call `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, 0`.
- It does not allocate `TZrBool zr_aot_truthy = ZR_FALSE;`.

## Results

- WSL GCC: logical contracts 4/0, control contracts 2/0, frame setup contracts 1/0, generic JUMP_IF bool local smoke 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- WSL Clang: logical contracts 4/0, control contracts 2/0, frame setup contracts 1/0, generic JUMP_IF bool local smoke 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- Windows MSVC Debug: logical contracts 4/0, control contracts 2/0, frame setup contracts 1/0, generic JUMP_IF bool local smoke 0 failures / 1 ignored, generic bool equality smoke 0 failures / 1 ignored, logical shared-library smoke 0 failures / 6 ignored because those smoke cases are Unix-only.

No relevant failures remain for this sub-slice.

## Acceptance Decision

Accepted as a completed 07-S2/S4 sub-slice. It does not complete 07-S2/S4 or the 07-12 plan.

Remaining work includes broader generic/dynamic/string truthiness boundaries, value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and the full typed function-body zero-frame proof.
