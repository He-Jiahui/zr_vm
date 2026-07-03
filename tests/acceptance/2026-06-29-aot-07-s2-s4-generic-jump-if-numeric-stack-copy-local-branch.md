# AOT 07-S2/S4 Generic JUMP_IF Numeric Stack-Copy Local Branch

## Scope

This acceptance record covers one narrow 07-S2/S4 slice: a primitive numeric condition that reaches generic `JUMP_IF` through a stack-copy instruction keeps its numeric scalar-local kind.

The accepted shape for the focused fixture is:

- `GET_CONSTANT` writes the source as `zr_aot_s0`.
- `SET_STACK dst=5 src=0` lowers to `zr_aot_scalar_stack_copy_i64 dstSlot=5 srcSlot=0`.
- Generic `JUMP_IF slot=5` lowers to `zr_aot_generic_jump_if_i64_scalar_local`.
- The branch predicate is `if (zr_aot_s5 == (TZrInt64)0) { goto ...; }`.

`JUMP_IF_BOOL_FALSE` remains a bool-only branch consumer. Unknown or unproven generic truthiness may still fall back to `ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy`.

## Baseline

Before this slice, scalar-local kind inference treated generic `JUMP_IF` after a stack copy as a bool consumer. In the focused fixture, slot 5 was declared as `zr_aot_b5`, the constant and copy materialized through `frame.slotBase`, and the branch used `GenericPrimitiveIsTruthy`.

## Test Inventory

- `tests/parser/test_aot_c_generic_jump_if_bool_local_smoke.c` adds a third Unix shared-library smoke case for `GET_CONSTANT i64 -> SET_STACK -> JUMP_IF`.
- `tests/parser/test_aot_c_logical_contracts.c` locks the shared `backend_aot_c_scalar_locals_truthiness_consumer_kind()` helper and its stack-copy/call-result proof shape.
- Existing adjacent tests remain: control contracts, frame setup contracts, logical shared-library smoke, and generic bool equality local smoke.

## Tooling Evidence

Initial RED:

```text
./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test
3 Tests 1 Failures 0 Ignored
test_aot_c_generated_shared_library_executes_generic_jump_if_numeric_stack_copy_branch:FAIL: Expected Non-NULL
```

Generated C before the fix contained only:

```text
TZrBool zr_aot_b5 = ZR_FALSE;
ZrLibrary_AotRuntime_CopyStack(state, &frame, 5, 0)
ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, 5, &zr_aot_truthy)
```

Generated C after the fix contains:

```text
TZrInt64 zr_aot_s5 = (TZrInt64)0;
/* zr_aot_scalar_stack_copy_i64 dstSlot=5 srcSlot=0 */
zr_aot_s5 = zr_aot_s0;
/* zr_aot_generic_jump_if_i64_scalar_local */
if (zr_aot_s5 == (TZrInt64)0) {
```

Focused WSL GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test'
```

Full focused/adjacent WSL GCC command:

```text
wsl -e bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'
```

WSL Clang command:

```text
wsl -e bash -lc 'cmake --build build-wsl-clang --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 8 && ./build-wsl-clang/bin/zr_vm_aot_c_logical_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_jump_if_bool_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'
```

Windows MSVC Debug command:

```text
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build-msvc
cmake --build build-msvc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_jump_if_bool_local_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_generic_bool_equality_local_smoke_test --config Debug --parallel 8
```

## Results

- WSL GCC: logical contracts 4/0, generic JUMP_IF bool/numeric/stack-copy local smoke 3/0, control contracts 2/0, frame setup contracts 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- WSL Clang: same target set passed with counts 4/0, 3/0, 2/0, 1/0, 1/0, 6/0.
- Windows MSVC Debug: logical contracts 4/0, control contracts 2/0, frame setup contracts 1/0, generic JUMP_IF smoke 0 failures / 3 ignored, generic bool equality smoke 0 failures / 1 ignored, logical shared-library smoke 0 failures / 6 ignored because those smoke cases are Unix-only.
- `git diff --check` exits with only LF/CRLF warnings for the touched files.

## Acceptance Decision

Accepted as a completed 07-S2/S4 sub-slice. It does not complete 07-S2/S4 or the 07-12 plan.

Remaining work includes call-result truthiness propagation, dynamic/object/string truthiness, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and the full typed function-body zero-frame proof.
