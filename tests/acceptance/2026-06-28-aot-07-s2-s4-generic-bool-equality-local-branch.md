# AOT 07-S2/S4 Generic Bool Equality Local Branch

## Scope

This acceptance record covers one narrow 07-S2/S4 slice: generic `LOGICAL_EQUAL` and `LOGICAL_NOT_EQUAL` may lower to bool scalar locals only when both operands are already proven bool scalar values and the result can stay local for a bool consumer.

The accepted generated C shape is `zr_aot_bD = (TZrBool)((zr_aot_bL ==|!= zr_aot_bR) != 0u);` followed by a bool-local branch. Mixed or dynamic generic equality remains a runtime boundary.

## Baseline

Before this slice, hand-built generic bool equality IR still emitted `ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual` / `GenericPrimitiveLogicalNotEqual` and kept the branch condition on the frame path.

During GREEN debugging, the first local compare was generated but the second branch still read `frame.slotBase[3]`. Root cause: the compare lived in a successor basic block, and bool written-before proof did not use block-entry bool source facts when proving the compare's destination write for the immediately following branch.

## Test Inventory

- `tests/parser/test_aot_c_logical_contracts.c` locks the new helper, marker, prototype, function-body plumbing, scalar-local proof hooks, and frame-descriptor cases.
- `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c` builds a focused IR fixture with bool constants, generic `LOGICAL_EQUAL`, generic `LOGICAL_NOT_EQUAL`, and `JUMP_IF_BOOL_FALSE`.
- `tests/parser/test_aot_c_logical_shared_library_smoke.c` remains the source-level regression that keeps mixed/dynamic generic equality on runtime fallback.
- `tests/parser/test_aot_c_frame_setup_contracts.c` stays in the related group to guard frame proof plumbing.

## Tooling Evidence

Initial RED:

```text
./bin/zr_vm_aot_c_logical_contracts_test
4 Tests 1 Failures 0 Ignored
missing required source contract text: backend_aot_c_write_generic_bool_compare_scalar_local(

./bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test
1 Tests 1 Failures 0 Ignored
Expected Non-NULL (`zr_aot_generic_bool_compare_scalar_local`)
```

WSL GCC:

```text
cmake --build build-wsl-gcc --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test -j4
./bin/zr_vm_aot_c_logical_contracts_test
./bin/zr_vm_aot_c_frame_setup_contracts_test
./bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test
./bin/zr_vm_aot_c_logical_shared_library_smoke_test
```

WSL Clang:

```text
cmake -S . -B build-wsl-clang
cmake --build build-wsl-clang --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test -j4
./bin/zr_vm_aot_c_logical_contracts_test
./bin/zr_vm_aot_c_frame_setup_contracts_test
./bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test
./bin/zr_vm_aot_c_logical_shared_library_smoke_test
```

Windows MSVC Debug:

```text
cmake -S . -B build-msvc
cmake --build build-msvc --config Debug --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_generic_bool_equality_local_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test
.\build-msvc\bin\Debug\zr_vm_aot_c_logical_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_frame_setup_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_generic_bool_equality_local_smoke_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_logical_shared_library_smoke_test.exe
```

Generated-C evidence:

- The hand-built bool equality project contains `zr_aot_generic_bool_compare_scalar_local`.
- It contains `zr_aot_b2 = (TZrBool)((zr_aot_b0 == zr_aot_b4) != 0u);` and `zr_aot_b3 = (TZrBool)((zr_aot_b0 != zr_aot_b1) != 0u);`.
- It branches with `if (!zr_aot_b2)` and `if (!zr_aot_b3)`.
- It does not call `ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(state, &frame, 2, 0, 4)`, `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(state, &frame, 3, 0, 1)`, or `SyncBoolLocal` for slots 2/3.
- The source-level mixed/dynamic equality smoke still passes through the runtime helper path.

## Results

- WSL GCC: logical contracts 4/0, frame setup contracts 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- WSL Clang: logical contracts 4/0, frame setup contracts 1/0, generic bool equality local smoke 1/0, logical shared-library smoke 6/0.
- Windows MSVC Debug: logical contracts 4/0, frame setup contracts 1/0, generic bool equality local smoke 0 failures / 1 ignored, logical shared-library smoke 0 failures / 6 ignored because those smoke cases are Unix-only.

## Acceptance Decision

Accepted as a completed 07-S2/S4 sub-slice. It does not complete 07-S2/S4 or the 07-12 plan.

Remaining work includes broader generic/dynamic/string logical boundaries, value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and the full typed function-body zero-frame proof.
