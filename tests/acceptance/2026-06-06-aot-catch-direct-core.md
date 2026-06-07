# AOT CATCH Direct Core

## Scope

- Generated AOT C `CATCH` now emits direct current-exception materialization instead of calling `ZrLibrary_AotRuntime_Catch`.
- The generated `zr_aot_catch_direct` block validates the destination slot against `frame.generatedFrameSlotCount`, resolves it through `ZrCore_Stack_GetValue(frame.slotBase + slot)`, copies `state->currentException` into the destination with `ZrCore_Value_Copy` and clears it with `ZrCore_Exception_ClearCurrent` when present, writes null with `ZrCore_Value_ResetAsNull` when absent, and clears pending control with `execution_clear_pending_control`.
- Generated C modules now include `zr_vm_core/exception.h` for the direct exception clear API.
- `THROW` and `END_FINALLY` were handled in later `tests/acceptance/2026-06-06-aot-throw-direct-core.md` and `tests/acceptance/2026-06-06-aot-end-finally-direct-core.md` slices; LLVM parity and real generated exception execution fixtures remain separate future work.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_emits_value_frame_cleanup_exit` required `zr_aot_catch_direct` while the C backend still emitted the `CATCH` runtime helper.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering, source-contract, control shared-smoke, docs, and acceptance changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_control_shared_library_smoke.c`.
- Production scan: `backend_aot_c*.c` production files contain only the `zr_aot_catch_direct` marker string for this slice and no `ZrLibrary_AotRuntime_Catch` emission.

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

- RED result: `test_aot_c_source_emits_value_frame_cleanup_exit` failed on missing `/* zr_aot_catch_direct */`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_control_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- `git diff --check`: clean apart from existing LF-to-CRLF warnings reported by Git on this Windows checkout.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found only `zr_aot_catch_direct` marker emission for this slice and no `ZrLibrary_AotRuntime_Catch` emission.
- At this slice's original acceptance time, `THROW` and `END_FINALLY` still remained helper-backed. They were removed by later acceptance slices; the checked AOT C backend now has no `TRY` / `END_TRY` / `THROW` / `CATCH` / `END_FINALLY` helper emission.

## Modularization Note

- `tests/parser/test_aot_c_shared_library_smoke.c` remains 1061 lines and was not grown further.
- `tests/parser/test_aot_c_control_shared_library_smoke.c` is now 179 lines and remains the focused control generated-C smoke boundary.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c` is 617 lines after this slice.

## Acceptance Decision

- Accepted for the AOT C `CATCH` helper-removal slice.
- Generated C now owns current-exception destination materialization and pending-control cleanup for the checked C backend.
- Remaining risks: the control smoke is compile-only and does not execute full source-level exception semantics; LLVM still uses helper-backed lowering for these exception-control paths; generated-function begin, fail, and explicit export publication still have runtime-backed boundaries.
