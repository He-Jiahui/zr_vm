# AOT 07-S2/S4 Generic LOGICAL_NOT Bool Source Local Branch

## Scope

This acceptance record covers one narrow 07-S2/S4 slice: generic `LOGICAL_NOT` may lower to bool scalar locals only when its source slot is already proven to hold a bool value and its destination is immediately consumed by `JUMP_IF_BOOL_FALSE`.

The accepted generated C shape is `zr_aot_bD = (TZrBool)(!zr_aot_bS);` followed by the bool-local branch. Generic int truthiness remains a runtime boundary.

## Baseline

Before this slice, the hand-built bool-source pipeline still emitted `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)` and kept the result on the frame path.

An attempted broad optimization exposed a semantic trap: `SyncBoolLocal` only syncs a slot that already contains a bool value; it does not convert an int slot through truthiness. Therefore `!one()` where `one()` returns int `1` must keep the generic runtime helper and `GenericPrimitiveIsTruthy`.

## Test Inventory

- `tests/parser/test_aot_c_logical_contracts.c` locks the new helper names, markers, strict bool-source proof, and function-body call shape.
- `tests/parser/test_aot_c_frame_setup_contracts.c` stays in the related group to guard frame-elision proof plumbing.
- `tests/parser/test_aot_c_logical_shared_library_smoke.c` adds a hand-built bool constant source followed by generic `LOGICAL_NOT` and `JUMP_IF_BOOL_FALSE`, requiring `zr_aot_generic_logical_not_scalar_local`, `zr_aot_b1 = (TZrBool)(!zr_aot_b0);`, and `if (!zr_aot_b1)`.
- The same smoke keeps `!zero()` and `!one()` int truthiness on `GenericPrimitiveLogicalNot` / `GenericPrimitiveIsTruthy` and forbids the stale int bool-local assignment.

## Tooling Evidence

WSL GCC:

```text
cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test -j4
./bin/zr_vm_aot_c_logical_contracts_test
./bin/zr_vm_aot_c_frame_setup_contracts_test
./bin/zr_vm_aot_c_logical_shared_library_smoke_test
```

WSL Clang:

```text
cmake --build build-wsl-clang --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test -j4
./bin/zr_vm_aot_c_logical_contracts_test
./bin/zr_vm_aot_c_frame_setup_contracts_test
./bin/zr_vm_aot_c_logical_shared_library_smoke_test
```

Windows MSVC Debug:

```text
cmake --build build-msvc --config Debug --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test
.\build-msvc\bin\Debug\zr_vm_aot_c_logical_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_frame_setup_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_logical_shared_library_smoke_test.exe
```

Generated-C grep evidence:

- `generic_logical_not_bool_source_project` contains `zr_aot_generic_logical_not_scalar_local`, `zr_aot_b1 = (TZrBool)(!zr_aot_b0);`, and `zr_aot_jump_if_bool_false_scalar_local`.
- That bool-source project does not contain `GenericPrimitiveLogicalNot(state, &frame, 1, 0)` or `SyncBoolLocal(state, &frame, 1)`.
- `generic_truthiness_project` still contains `GenericPrimitiveLogicalNot(state, &frame, 3, 4)` and `GenericPrimitiveLogicalNot(state, &frame, 5, 6)`, and does not contain `zr_aot_b5 = (TZrBool)(!zr_aot_b6);`.

## Results

- WSL GCC: logical contracts 4/0, frame setup contracts 1/0, logical shared-library smoke 6/0.
- WSL Clang: logical contracts 4/0, frame setup contracts 1/0, logical shared-library smoke 6/0.
- Windows MSVC Debug: logical contracts 4/0, frame setup contracts 1/0, logical shared-library smoke 0 failures / 6 ignored because those smoke cases are Unix-only.

## Acceptance Decision

Accepted as a completed 07-S2/S4 sub-slice. It does not complete 07-S2/S4 or the 07-12 plan.

Remaining work includes broader generic/dynamic/string logical boundaries, value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and the full typed function-body zero-frame proof.
