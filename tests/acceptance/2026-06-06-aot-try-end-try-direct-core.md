# AOT TRY/END_TRY Direct Core

## Scope

- Generated AOT C `TRY` and `END_TRY` opcodes now emit direct exception-handler stack operations instead of calling `ZrLibrary_AotRuntime_Try` or `ZrLibrary_AotRuntime_EndTry`.
- `TRY` validates the generated frame and handler index, resolves the active `SZrCallInfo`, pushes the handler with `execution_push_exception_handler`, and refreshes the active generated frame.
- `END_TRY` validates the generated frame and handler index, finds the active handler with `execution_find_handler_state`, reads `frame.function->exceptionHandlerList[index]`, switches finally handlers to `ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY`, or pops non-finally handlers with `execution_pop_exception_handler`.
- `CATCH`, `THROW`, and `END_FINALLY` were handled in later `tests/acceptance/2026-06-06-aot-catch-direct-core.md`, `tests/acceptance/2026-06-06-aot-throw-direct-core.md`, and `tests/acceptance/2026-06-06-aot-end-finally-direct-core.md` slices; LLVM parity and real generated exception execution fixtures remain separate future work.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_emits_value_frame_cleanup_exit` required `zr_aot_try_direct` while the C backend still emitted the `TRY` / `END_TRY` runtime helpers.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering, source-contract, control shared-smoke, docs, and acceptance changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_control_shared_library_smoke.c`.
- Production scan: `backend_aot_c*.c` production files contain only the `zr_aot_try_direct` and `zr_aot_end_try_direct` marker strings for this slice and no `ZrLibrary_AotRuntime_Try` or `ZrLibrary_AotRuntime_EndTry` emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_control_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_control_shared_library_smoke_test"`
- MSVC command:
  `cmake --build build-msvc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_shared_library_smoke_test --config Debug -j 2; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_control_shared_library_smoke_test.exe`
- Whitespace command:
  `git diff --check`

## Results

- RED result: `test_aot_c_source_emits_value_frame_cleanup_exit` failed on missing `/* zr_aot_try_direct */`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_control_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- `git diff --check`: clean apart from existing LF-to-CRLF warnings reported by Git on this Windows checkout.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found only `zr_aot_try_direct` / `zr_aot_end_try_direct` marker emission for this slice and no `ZrLibrary_AotRuntime_Try` or `ZrLibrary_AotRuntime_EndTry` emission.
- At this slice's original acceptance time, `CATCH`, `THROW`, and `END_FINALLY` still remained helper-backed. They were removed by later acceptance slices; the checked AOT C backend now has no `TRY` / `END_TRY` / `THROW` / `CATCH` / `END_FINALLY` helper emission.

## Modularization Note

- `tests/parser/test_aot_c_shared_library_smoke.c` remains 1061 lines and was not grown further.
- This slice added a new focused `tests/parser/test_aot_c_control_shared_library_smoke.c` file, currently 165 lines, to keep control generated-C smokes isolated.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c` is 597 lines after this slice.

## Acceptance Decision

- Accepted for the AOT C `TRY` / `END_TRY` helper-removal slice.
- Generated C now owns the handler push and end-try handler-state transition path for the checked C backend.
- Remaining risks: the control smoke is compile-only and does not execute full source-level exception semantics; LLVM still uses helper-backed lowering for these exception-control paths; generated-function begin, fail, and explicit export publication still have runtime-backed boundaries.
