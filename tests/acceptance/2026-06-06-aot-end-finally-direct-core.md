# AOT END_FINALLY Direct Core

## Scope

- Generated AOT C `END_FINALLY` now emits direct pending-control and finally-resume logic instead of calling `ZrLibrary_AotRuntime_EndFinally`.
- Affected layers: AOT C control lowering, focused C source contracts, generated-C control compile smoke, CMake test registration, semantic docs, plan/session records.
- The generated `zr_aot_end_finally_direct` block validates the active generated call frame, pops the matching handler, switches on `state->pendingControl.kind`, resumes pending exceptions through `execution_unwind_exception_to_handler`, resumes pending return/break/continue through `execution_resume_pending_via_outer_finally` or `execution_jump_to_instruction_offset`, copies pending return values with `ZrCore_Value_Copy`, clears pending control for direct jumps, refreshes the generated frame, and dispatches through the generated switch.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed on missing `/* zr_aot_end_finally_direct */` while the C backend still emitted `ZrLibrary_AotRuntime_EndFinally`.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering, contracts, smoke, CMake, docs, and acceptance changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Focused control source contract: `tests/parser/test_aot_c_control_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_control_shared_library_smoke.c`.
- Production scan: `backend_aot_c*.c` production files contain direct exception-control markers and no `ZrLibrary_AotRuntime_Try`, `ZrLibrary_AotRuntime_EndTry`, `ZrLibrary_AotRuntime_Throw`, `ZrLibrary_AotRuntime_Catch`, or `ZrLibrary_AotRuntime_EndFinally` emission.
- Boundary coverage: pending none, pending exception, pending return, pending break, pending continue, default pending cleanup, and compile-only generated control fixture shape.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_control_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_control_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_control_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_control_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_control_shared_library_smoke_test"`
- MSVC command:
  `cmake --build build-msvc --config Debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_control_shared_library_smoke_test --parallel 2; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_control_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_control_shared_library_smoke_test.exe`
- Whitespace command:
  `git diff --check`

## Results

- RED result: source contract failed on missing `/* zr_aot_end_finally_direct */`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_control_contracts_test`: 1 test, 0 failures.
- GCC `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_control_contracts_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_control_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_control_contracts_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_control_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- `git diff --check`: 0 whitespace errors; Git reported existing LF-to-CRLF checkout warnings on this Windows worktree.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found only direct exception-control markers and no checked exception-control runtime helper emission.

## Modularization Note

- `tests/parser/test_aot_c_control_contracts.c` is 188 lines and keeps exception-control shape assertions out of the already oversized aggregate source-contract file.
- `tests/parser/test_aot_c_source_contracts.c` is 1440 lines after this slice.
- `tests/parser/test_aot_c_control_shared_library_smoke.c` is 225 lines after this slice.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c` is 802 lines after this slice.

## Acceptance Decision

- Accepted for the AOT C `END_FINALLY` helper-removal slice.
- Generated C now owns pending-control/finally resume for the checked C backend.
- Remaining risks: the control smoke is compile-only and does not execute full source-level exception semantics; LLVM still uses helper-backed lowering for these exception-control paths; generated-function begin, fail, and explicit export publication still have runtime-backed boundaries.
