# WSL Clang Performance Report Rebuild Validation

## Status

- Accepted for the WSL clang `performance_report` recovery slice.
- The original `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:1752` assertion is no longer reproducible on the rebuilt Release chain.

## Root Cause

- The failing clang path was not a current-source VM logic regression.
- The failure came from stale or inconsistent clang build artifacts that mixed different instruction enum layouts between parser-side and runtime-side objects.
- That mismatch caused the parser to encode `FUNCTION_RETURN` with a raw instruction word that the runtime later interpreted as a different opcode.

## Evidence

- Reusable debugger script:
  - `tests/core/gdb_clang_execution_dispatch_assert.gdb`
- Before rebuilding the affected clang targets:
  - `create_instruction_2(FUNCTION_RETURN, 1, 0, 0)` produced `0x10051`
  - runtime dispatch later interpreted opcode `81` as `BITWISE_XOR`
  - the process aborted in `execution_dispatch.c`
- Control path on gcc:
  - the same instruction creation path produced `0x10056`
- After rebuilding the relevant clang benchmark targets:
  - clang also produced `0x10056`
  - `hello_world` executed normally
  - the benchmark chain stopped reproducing the assert

## Resolution

- No VM runtime source workaround was added for this slice.
- The actual fix was to rebuild the affected clang benchmark targets so parser/runtime objects were regenerated from a consistent instruction layout.
- This preserves the assert as a valid guard instead of masking the underlying inconsistency.

## Release Validation

### Build Mode

- `build/codex-wsl-current-clang-release-make`
  - `CMAKE_BUILD_TYPE=Release`
  - `CMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG`
- `build/codex-wsl-current-gcc-release-make`
  - `CMAKE_BUILD_TYPE=Release`
  - `CMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG`

### Commands

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-current-clang-release-make -R '^benchmark_registry$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_JAVA_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/java.exe ZR_VM_JAVAC_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/javac.exe ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-clang-release-make -R '^performance_report$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-current-gcc-release-make -R '^benchmark_registry$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_JAVA_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/java.exe ZR_VM_JAVAC_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/javac.exe ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-gcc-release-make -R '^performance_report$' --output-on-failure"
```

### Results

- WSL clang Release `benchmark_registry`: PASS
- WSL clang Release `performance_report(profile)`: PASS
- WSL gcc Release `benchmark_registry`: PASS
- WSL gcc Release `performance_report(profile)`: PASS

## Current Non-Blocking Debts

- `ZR aot_llvm` still reports the expected opaque-pointer prepare failure and stays `SKIP`.
- `string_build / ZR aot_c` still reports a correctness `Segmentation fault`.
- Full-tree release builds remain blocked by the unrelated `semantic_type_from_ast` type conflict in `zr_vm_language_server`.

## Acceptance Decision

- Accepted.
- Reason:
  - the previously failing clang benchmark chain is green again
  - the recovered chain is verified on Release builds, not only Debug
  - the remaining failures are isolated to known non-blocking benchmark debts and an unrelated full-build blocker
