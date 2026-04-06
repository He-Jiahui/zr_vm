# Crash Recovery Validation

## Scope

- Harness-level crash interruption for Unity tests.
- When a VM panic/crash happens while a `SZrState` is still active, print the current VM exception stack and continue with later tests in the same executable.

## Touched Files

- `tests/core/test_runtime_crash_recovery.c`
- `tests/harness/runtime_support.c`
- `tests/harness/runtime_support.h`
- `tests/harness/unity_crash_guard.c`
- `tests/harness/unity_crash_guard.h`
- `tests/unity_config.h`
- `tests/CMakeLists.txt`
- `tests/third_party/zr_unity/CMakeLists.txt`

## Validation Commands

### WSL gcc

```powershell
wsl bash -lc "cmake --build /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-gcc-debug --target zr_vm_runtime_crash_recovery_test zr_vm_task_runtime_test -j 8"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-gcc-debug && ctest --output-on-failure -R '^core_runtime$'"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-gcc-debug && ./bin/zr_vm_task_runtime_test"
wsl bash -lc "/mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-gcc-debug/bin/zr_vm_runtime_crash_recovery_test"
```

Observed:

- `core_runtime` passed.
- `zr_vm_task_runtime_test` passed.
- `zr_vm_runtime_crash_recovery_test` passed and printed the recovered crash plus VM exception payload.

### WSL clang

```powershell
wsl bash -lc "cmake --build /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug --target zr_vm_runtime_crash_recovery_test zr_vm_task_runtime_test -j 8"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug && ctest --output-on-failure -R '^core_runtime$'"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug && ./bin/zr_vm_task_runtime_test"
```

Observed:

- `core_runtime` passed.
- `zr_vm_task_runtime_test` passed.
- No new clang-only regression was introduced by the harness changes.

### Windows MSVC

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_runtime_crash_recovery_test zr_vm_task_runtime_test --parallel 8
ctest --test-dir build\codex-msvc-debug -C Debug --output-on-failure -R "^core_runtime$"
.\build\codex-msvc-debug\bin\Debug\zr_vm_task_runtime_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_runtime_crash_recovery_test.exe
```

Observed:

- Build succeeded.
- `core_runtime` passed.
- `zr_vm_task_runtime_test` passed.
- `zr_vm_runtime_crash_recovery_test` passed and printed the recovered crash plus VM exception payload on Windows as well.

## Notes

- The current recovery path first hooks VM panic dispatch, so VM-triggered aborts are interrupted before `ZR_ABORT()` terminates the process.
- Signal handlers remain as a fallback for fatal signals, but the validated path for `ZrCore_Exception_Throw()` is the panic hook path.
- Existing unrelated compiler warnings in `zr_vm_library` and other modules remain baseline and were not changed by this work.
