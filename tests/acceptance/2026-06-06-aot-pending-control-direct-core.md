# AOT Pending Control Direct Core

## Scope

- Generated AOT C pending-control opcodes now emit direct generated control blocks instead of calling `ZrLibrary_AotRuntime_SetPendingReturn`, `ZrLibrary_AotRuntime_SetPendingBreak`, or `ZrLibrary_AotRuntime_SetPendingContinue`.
- `SET_PENDING_RETURN`, `SET_PENDING_BREAK`, and `SET_PENDING_CONTINUE` share `backend_aot_write_c_pending_control_transfer()`.
- The generated block sets pending control, resumes outer finally blocks when needed, jumps to the target instruction otherwise, refreshes the active generated frame, recomputes the next instruction index from the active program counter, and dispatches.
- LLVM parity and broader exception helper removal remain separate future work.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_emits_value_frame_cleanup_exit` required `backend_aot_write_c_pending_control_transfer(FILE *file,` while the C backend still emitted the three `SetPending*` runtime helpers.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C lowering, source-contract, shared-smoke, docs, and acceptance changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C compile smoke: `tests/parser/test_aot_c_shared_library_smoke.c`.
- Production scan: `backend_aot_c*.c` production files contain only the new `zr_aot_pending_return`, `zr_aot_pending_break`, and `zr_aot_pending_continue` marker strings for this slice and no `ZrLibrary_AotRuntime_SetPending*` emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC source-contract command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC generated-C smoke command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test"`
- MSVC command:
  `cmake --build build-msvc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test --config Debug -j 2; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe`
- Whitespace command:
  `git diff --check`

## Results

- RED result: `test_aot_c_source_emits_value_frame_cleanup_exit` failed on missing `backend_aot_write_c_pending_control_transfer(FILE *file,`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_shared_library_smoke_test`: target built; 8 Unix-only runtime tests ignored.
- `git diff --check`: clean apart from existing LF-to-CRLF warnings reported by Git on this Windows checkout.
- Production scan over `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c*.c` found only the three new pending-control markers and no `ZrLibrary_AotRuntime_SetPendingReturn`, `ZrLibrary_AotRuntime_SetPendingBreak`, or `ZrLibrary_AotRuntime_SetPendingContinue` emission.

## Modularization Note

- `tests/parser/test_aot_c_shared_library_smoke.c` is now 1061 lines after adding the compile-only pending-control fixture.
- The clean follow-up boundary is extracting control/scaffolding shared-smoke helpers into a narrower module; this slice avoided mixing that structural change with the behavior change.

## Acceptance Decision

- Accepted for the AOT C pending-control helper-removal slice.
- Generated C now owns the pending-return, pending-break, and pending-continue control transfer path for the checked C backend.
- Remaining risks: the pending-control smoke is compile-only and does not execute full source-level control semantics; LLVM still uses helper-backed lowering for these control paths; later direct-core slices removed `TRY`, `END_TRY`, `THROW`, `CATCH`, and `END_FINALLY` helper emission from the checked C backend; generated-function begin, fail, and explicit export publication still have runtime-backed boundaries.
