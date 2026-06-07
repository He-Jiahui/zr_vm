# AOT THROW Direct Core

## Scope

- Generated AOT C `THROW` now emits direct exception normalization and in-frame unwind dispatch instead of calling `ZrLibrary_AotRuntime_Throw`.
- The generated `zr_aot_throw_direct` block validates the payload slot, clears pending control, snapshots the source `SZrTypeValue`, normalizes it with `ZrCore_Exception_NormalizeThrownValue` and `ZrCore_Exception_NormalizeStatus`, unwinds through `execution_unwind_exception_to_handler`, refreshes the generated frame when the handler remains in the same generated function, computes `zr_aot_next_instruction` from the active program counter, and dispatches through the generated switch.
- If no in-frame generated handler exists, generated C calls `ZrCore_Exception_Throw(state, state->currentExceptionStatus)` and exits through `ZR_AOT_C_FAIL()` rather than falling back to an instruction helper.
- `END_FINALLY` was handled in the later `tests/acceptance/2026-06-06-aot-end-finally-direct-core.md` slice; LLVM parity and real generated exception execution fixtures remain separate future work.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_emits_value_frame_cleanup_exit` required `zr_aot_throw_direct` while the C backend still emitted the `THROW` runtime helper.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering, source-contract, control shared-smoke, docs, and acceptance changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_control_shared_library_smoke.c`.
- Production scan: `backend_aot_c*.c` production files contain only the `zr_aot_throw_direct` marker string for this slice and no `ZrLibrary_AotRuntime_Throw` emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC source-contract command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC generated-C smoke command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_control_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_control_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_control_shared_library_smoke_test"`
- MSVC command:
  `cmake --build build-msvc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_shared_library_smoke_test --config Debug -j 2; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_control_shared_library_smoke_test.exe`
- Whitespace command:
  `git diff --check`

## Results

- RED result: `test_aot_c_source_emits_value_frame_cleanup_exit` failed on missing `/* zr_aot_throw_direct */`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_control_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- `git diff --check`: clean apart from existing LF-to-CRLF warnings reported by Git on this Windows checkout.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found only `zr_aot_throw_direct` marker emission for this slice and no `ZrLibrary_AotRuntime_Throw` emission.
- Remaining `ZrLibrary_AotRuntime_` emissions in the C backend are the known generated-function begin/fail/export-return boundaries; the following END_FINALLY slice removed the last checked exception-control helper emission.

## Modularization Note

- `tests/parser/test_aot_c_shared_library_smoke.c` remains 1061 lines and was not grown further.
- `tests/parser/test_aot_c_control_shared_library_smoke.c` is now 185 lines and remains the focused control generated-C smoke boundary.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c` is 666 lines after this slice.

## Acceptance Decision

- Accepted for the AOT C `THROW` helper-removal slice.
- Generated C now owns exception payload normalization and in-frame unwind dispatch for the checked C backend.
- Remaining risks: the control smoke is compile-only and does not execute full source-level exception semantics; LLVM still uses helper-backed lowering for these exception-control paths; generated-function begin, fail, and explicit export publication still have runtime-backed boundaries.
