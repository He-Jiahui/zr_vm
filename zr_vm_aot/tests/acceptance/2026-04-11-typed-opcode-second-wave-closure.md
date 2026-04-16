# Typed Opcode Second-Wave Closure

## Scope
- Close the strong-typed opcode second wave across parser emission, quickening, interpreter dispatch, intermediate writer, AOT C, and AOT LLVM.
- Revalidate the newly added typed arithmetic/equality coverage and add explicit `KNOWN_NATIVE_CALL` regression coverage from a source fixture that actually emits native-call provenance.
- Record adjacent fixes that were required to get the milestone trustworthy:
  - speculative parser branches must not leak diagnostics
  - repeated ExecBC/AOT roundtrip runs must not reuse stale static caches across `SZrGlobalState` instances
  - stale incremental objects after `SZrGlobalState` layout changes must be treated as a rebuild hazard, not as a source-level logic regression
  - MSVC must be able to link `zr_vm_execbc_aot_pipeline_test` after the AOT helper split

## Baseline
- Before this milestone, strong-typed source could still fall back to generic arithmetic/equality opcodes or generic call dispatch even when type and callee provenance were statically knowable.
- Validation also exposed four separate trust blockers:
  - speculative parser branches in `parser_expressions.c`, `parser_expression_primary.c`, and `parser_statements.c` could write stderr / callback noise for branches that were later discarded
  - repeated WSL clang/gcc runs of `zr_vm_execbc_aot_pipeline_test` could flake because static caches inside super-array paths were keyed only by address-stable globals instead of per-runtime identity
  - after `SZrGlobalState` layout changes, stale incremental objects could misread fields such as `parserModuleInitState`; this looked like a runtime crash but was actually an ABI-mismatch rebuild hazard
  - Windows MSVC link of `zr_vm_execbc_aot_pipeline_test.exe` failed on unresolved `backend_aot_c_step_flags_for_instruction`
- Repository-wide full-suite baselines were not re-run for this acceptance. This document covers the targeted parser/AOT/CLI matrix that is directly relevant to the typed-opcode second-wave closure.

## Test Inventory
- Unit / focused subsystem:
  - `zr_vm_literal_surface_test`
  - `zr_vm_compiler_integration_test`
  - `zr_vm_execbc_aot_pipeline_test`
- Integration / project:
  - `zr_vm_cli tests/fixtures/projects/hello_world/hello_world.zrp`
- Boundary and negative cases covered inside the targeted suites:
  - typed signed/unsigned arithmetic and bool/string/float equality emission
  - typed const and plain-destination quickening
  - `KNOWN_NATIVE_CALL` emission from decorator helper provenance
  - invalid literal fixtures, const reassignment fixtures, construct-target misuse fixtures, and ownership misuse fixtures already embedded in `zr_vm_literal_surface_test`
- Tool-assisted and repeated validation:
  - WSL gcc Debug targeted build/run
  - WSL clang Debug targeted build/run
  - Windows MSVC Debug targeted build/run
  - earlier repeated ExecBC reruns after the cache fix:
    - WSL clang Debug `zr_vm_execbc_aot_pipeline_test` x10: pass
    - WSL gcc Debug `zr_vm_execbc_aot_pipeline_test` x3: pass

## Tooling Evidence
- Toolchains:
  - WSL gcc: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
  - WSL clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
  - Windows MSVC: `cl.exe` `19.44.35224.0`
- Why these tools were used:
  - WSL gcc/clang are the primary parser/runtime/AOT evidence environments for `zr_vm`
  - Windows MSVC Debug confirms the AOT helper export fix and keeps the milestone portable on the user-facing platform
- Exact commands used in this closeout:

```bash
cd /mnt/e/Git/zr_vm
cmake --build build/codex-wsl-gcc-debug --target zr_vm_literal_surface_test zr_vm_compiler_integration_test zr_vm_execbc_aot_pipeline_test zr_vm_cli_executable -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_literal_surface_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

```bash
cd /mnt/e/Git/zr_vm
cmake --build build/codex-wsl-clang-debug --target zr_vm_literal_surface_test zr_vm_compiler_integration_test zr_vm_execbc_aot_pipeline_test zr_vm_cli_executable -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_literal_surface_test
./build/codex-wsl-clang-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build E:\Git\zr_vm\build\codex-msvc-debug-typed2 --config Debug --target zr_vm_literal_surface_test zr_vm_compiler_integration_test zr_vm_execbc_aot_pipeline_test zr_vm_cli_executable --parallel 8
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_literal_surface_test.exe
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_compiler_integration_test.exe
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_execbc_aot_pipeline_test.exe
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_cli.exe E:\Git\zr_vm\tests\fixtures\projects\hello_world\hello_world.zrp
```

- Earlier targeted commands retained as milestone evidence:

```bash
cd /mnt/e/Git/zr_vm
./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test
./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test
# repeated to 10 passes while validating the cacheIdentity fix
```

```bash
cd /mnt/e/Git/zr_vm
./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test
# repeated to 3 passes while validating the cacheIdentity fix
```

## Results
- Current closeout matrix:
  - WSL gcc Debug:
    - `zr_vm_literal_surface_test`: `60 Tests 0 Failures 0 Ignored`
    - `zr_vm_compiler_integration_test`: `78 Tests 0 Failures 0 Ignored`
    - `zr_vm_execbc_aot_pipeline_test`: `89 Tests 0 Failures 0 Ignored`
    - `zr_vm_cli hello_world`: `hello world`
    - new regression lines observed:
      - `Known Native Calls Quicken To Dedicated Call Family: PASS`
      - `ExecBC True AOT Lowers Known Native Call Family: PASS`
  - WSL clang Debug:
    - `zr_vm_literal_surface_test`: `60 Tests 0 Failures 0 Ignored`
    - `zr_vm_compiler_integration_test`: `78 Tests 0 Failures 0 Ignored`
    - `zr_vm_execbc_aot_pipeline_test`: `89 Tests 0 Failures 0 Ignored`
    - `zr_vm_cli hello_world`: `hello world`
    - new regression lines observed:
      - `Known Native Calls Quicken To Dedicated Call Family: PASS`
      - `ExecBC True AOT Lowers Known Native Call Family: PASS`
  - Windows MSVC Debug:
    - `zr_vm_literal_surface_test`: `60 Tests 0 Failures 0 Ignored`
    - `zr_vm_compiler_integration_test`: `78 Tests 0 Failures 0 Ignored`
    - `zr_vm_execbc_aot_pipeline_test`: `89 Tests 0 Failures 0 Ignored`
    - `zr_vm_cli hello_world`: `hello world`
    - new regression lines observed:
      - `Known Native Calls Quicken To Dedicated Call Family: PASS`
      - `ExecBC True AOT Lowers Known Native Call Family: PASS`
- Fixes proven by earlier milestone validation and still present:
  - speculative parser branches no longer emit discarded diagnostics
  - `cacheIdentity` isolates static native/super-array caches per global runtime, eliminating the repeated ExecBC flake
  - clean-first rebuild resolves the stale ABI mismatch after `SZrGlobalState` layout changes; the crash signature was not reproduced after clean rebuilds
  - exporting `backend_aot_c_step_flags_for_instruction` from `backend_aot.c` / `backend_aot_internal.h` fixes the Windows MSVC link failure

## Acceptance Decision
- Accepted.
- Exact reason:
  - strong-typed arithmetic/equality emission remains covered
  - source-emitted `KNOWN_NATIVE_CALL` now has explicit regression coverage in compiler integration and ExecBC/AOT pipeline tests
  - WSL gcc, WSL clang, and Windows MSVC Debug all pass the relevant targeted suites after the new coverage was added
  - the parser speculative-diagnostic fix, runtime cache fix, rebuild hazard diagnosis, and MSVC export fix are all documented with matching evidence instead of being left as tribal knowledge
- Remaining risks:
  - this acceptance revalidated source-emitted `KNOWN_NATIVE_CALL`; it did not add a repo-backed source fixture that naturally emits `SUPER_KNOWN_NATIVE_CALL_NO_ARGS`
  - inference from code inspection: `compiler_quickening.c`, `execution_dispatch.c`, `backend_aot.c`, and `writer_intermediate.c` still carry the zero-arg native opcode wiring, but this document does not claim a separate source-level reproducer for that shape
