# AOT M1.5 07-S5 Iterator Boundary Helpers

## Scope

- Iterator C lowering now emits `ZrLibrary_AotRuntime_IterInit()`, `ZrLibrary_AotRuntime_IterMoveNext()`, and `ZrLibrary_AotRuntime_IterCurrent()` helper guards.
- `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE` now emits `ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse()` plus a generated `goto` only when the helper reports the branch should be taken.
- Generated C keeps iterator markers but no longer expands `SZrTypeValue` slot lookup, cached iterator fast paths, direct `ZrCore_Object_Iter*` calls, or iterator-specific generated failure blocks.

## Baseline

- RED: after flipping `zr_vm_aot_c_iterator_contracts_test` to require helper-only iterator lowering, the test failed because `ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse()` did not exist and the C lowering still emitted direct generated iterator core code.
- Existing runtime API already exposed `IterInit`, `IterMoveNext`, and `IterCurrent`; this slice adds the branch-specific helper needed to keep bool result validation out of generated C.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_iterator_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_iterator_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_iterator_smoke.c` must contain all iterator runtime helper calls and must not contain the old generated direct iterator fast/core templates.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_iterator_contracts_test zr_vm_aot_c_iterator_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_iterator_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_iterator_shared_library_smoke_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_iterator_contracts_test zr_vm_aot_c_iterator_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_iterator_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_iterator_shared_library_smoke_test
  ```
- Broader focused build and run:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_iterator_contracts_test zr_vm_aot_c_iterator_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_control_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_iterator_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_iterator_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_iterator' --output-on-failure
  ```

## Results

- RED observed: `Missing source contract text: ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(`.
- GREEN focused results: iterator contracts 1/0 and iterator shared-library smoke 1/0.
- Generated check passed: `aot_c_iterator_smoke.c` contains `IterInit`, `IterMoveNext`, `IterCurrent`, and `IterMoveNextJumpIfFalse` helper calls plus the expected generated branch `goto`. It does not contain the old generated `ZrCore_Object_IterInit`, cached iterator fast paths, `ZrCore_Object_IterMoveNext`, `ZrCore_Object_IterCurrent`, or iterator-specific unsupported-path templates.
- Broader GREEN results: source contracts 19/0, iterator contracts 1/0, iterator shared-library smoke 1/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- `ctest -R 'aot_c_iterator'` exited 0 but reported no registered tests in this build; the focused iterator binaries were run directly.

## Acceptance Decision

- Accepted for the iterator boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
