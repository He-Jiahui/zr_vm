---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/benchmarks/aot_runner/aot_runner.c
  - tests/performance/perf_report.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/benchmarks/aot_runner/aot_runner.c
  - tests/performance/perf_report.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/07-aot-backends/04-aot-runner-coverage.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
doc_type: acceptance-record
status: accepted-focused
---

# ExecIR clone and AOT runner contract checks, 2026-10-02

This record covers three focused commits on `main`: `a22b5479` rejects source
side-array counts beyond their owned capacities; `579758b9` reports sampled
interpreter work inside a compiled AOT entry as mixed fallback; `f91b163b`
initializes each temporary module function slot before including it in rollback.
The complete 01.01, 07.04, and M1 exit gates remain open.

The environment was WSL2 kernel `6.18.33.2-microsoft-standard-WSL2`, Ubuntu GCC
11.4.0 and Clang 14.0.0, plus Visual Studio environment 17.14.40 for MSVC.
Every new binary, compiler temporary, and verification log was placed below
`D:/tmp/zr_vm`; no build directory was moved between drives.

## Clone rejection and rollback

The new `test_clone_rejects_side_array_count_beyond_capacity` checks a regular
value array, nested GC-map entries, and frame-layout slots. Clone failure must
return `INVALID_ARGUMENT`, retain the function token and invalid instruction/block
IDs, and preserve the already published destination.

`test_module_clone_initializes_early_rejected_function_slot` adds a second source
function with `valueCount > valueCapacity`. The first source function is copied;
the second must fail before copying its arrays, and the existing destination must
retain its original function token. Before `f91b163b`, GCC ASan reproducibly
aborted inside `FreeModule`: the failed function slot still held ASan's
`0xbebebebe...` allocation poison and rollback tried to free it. The repair
initializes the slot before incrementing `temporary.functionCount`, so early
validation failure and later partial-copy failure both have valid cleanup state.

| Check | Result | Evidence below `D:/tmp/zr_vm` |
| --- | --- | --- |
| Initial malformed-count regression | RED: clone accepted count beyond capacity | `execir-core-model/ssa_core_model.exe` was the initial fixture binary |
| GCC and Clang direct focused model test after shape guard | PASS | `ssa-control/ssa-core-model-wsl-gcc.log`, `ssa-core-model-wsl-clang.log` |
| MSVC focused model test after shape guard | PASS | `ssa-control/ssa-core-msvc-build.log`, `ssa-core-msvc-run.log` |
| GCC ASan/UBSan module early-rejection regression before slot initialization | RED: invalid free and process abort | `ssa-core-early-reject/gcc-red-asan-run.log` |
| GCC ASan/UBSan with leak detection after slot initialization | PASS, repeated serial run PASS | `ssa-core-early-reject/gcc-green-asan-run.log`, `gcc-green-asan-rerun.log` |
| Clang ordinary focused build and run | PASS | `ssa-core-early-reject/clang-green-plain-build.log`, `clang-green-plain-run.log` |
| Clang ASan/UBSan serial run with leak detection disabled | PASS | `ssa-core-early-reject/clang-green-asan-rerun.log` |
| MSVC rebuild and direct execution after slot initialization | PASS | `ssa-control/core-clone-fix-msvc-build.log`, `core-clone-fix-msvc-run.log` |

The first Clang sanitizer execution, launched alongside GCC, exited with a
segmentation fault before writing a sanitizer report. Its serial retry passed
with `detect_leaks=0` and recorded interceptor warnings. This is a validation
limitation: this record claims GCC leak detection and Clang plain/sanitized
execution, not a passing Clang leak-sanitizer run. GCC leak detection passed
both the initial GREEN and serial repeat; the focused Windows and Linux tests
also exercised the repaired branch successfully.

The direct compilation uses `test_ssa_core_model.c`, `execution_contract.c`, and
the existing core `exec_ir` sources listed by the model-test CMake target.
The retained local driver is `D:/tmp/zr_vm/ssa-control/run_core_clone_shape.sh`:

```powershell
wsl.exe --exec /bin/bash --noprofile --norc /mnt/d/tmp/zr_vm/ssa-control/run_core_clone_shape.sh gcc green asan
wsl.exe --exec /bin/bash --noprofile --norc /mnt/d/tmp/zr_vm/ssa-control/run_core_clone_shape.sh clang green plain
```

It sets `TMPDIR`, `TMP`, and `TEMP` to its D-drive output directory and enables
`-fsanitize=address,undefined -fno-omit-frame-pointer` for the `asan` mode.
The abandoned RED binary was deleted after its report had been retained.

## Mixed compiled AOT fallback

`test_runner_marks_compiled_entry_with_fallback_as_mixed` registers a compiled C
entry whose sampled semantic work contains interpreter sites. The result must
be `FALLBACK`, keep `actualBackend == C`, set `mixedExecution`, retain its
checksum, and validate. Native-helper-only work stays `RAN`. Without a sampling
rate, coverage stays unavailable and the runner does not infer fallback.
The phase-report validator accepts the same-backend fallback only when
available coverage includes interpreter work.

The regression failed before implementation with the diagnostic that compiled
AOT fallback work was not marked as mixed execution. Windows GCC, WSL GCC,
WSL Clang, and MSVC all subsequently printed `ssa aot runner coverage PASS`.
Logs are `ssa-control/aot-runner-coverage-green-v2.log`,
`aot-runner-coverage-wsl-gcc.log`, `aot-runner-coverage-wsl-clang.log`,
`aot-runner-msvc-build.log`, and `aot-runner-msvc-run.log`.

The standalone Linux command compiles `test_ssa_aot_runner_coverage.c`,
`aot_coverage.c`, `aot_runner.c`, `perf_report.c`, and `perf_statistics.c`, with
the benchmark, performance, and common include paths and `-lm`. The MSVC
target is `zr_vm_ssa_aot_runner_coverage_test`.

## Adjacent 01.02 construction smoke

While selecting the next plan slice, the existing `zr_vm_ssa_construction_test`
was rebuilt from its GCC and Clang WSL trees and run through the registered
`ssa_construction` CTest. Both ran one CTest successfully. The same target was
rebuilt with MSVC and ran 11 Unity cases with zero failures. The direct MSVC
result is `ssa-control/ssa-construction-msvc-run.log`; WSL logs are
`ssa-control/ssa-construction-wsl-gcc-ctest.log` and
`ssa-control/ssa-construction-wsl-clang-ctest.log`.

This is focused existing-target validation, not acceptance of all 01.02 gates:
the full source-language optional-chain, exception/cleanup, and differential
matrix remains unclaimed. The GCC/Clang CMake caches remain in their original
WSL paths under D and were invoked through WSL; no cross-drive cache reuse was
attempted.

`git diff --check` passed for all committed paths. Shared-workspace changes in
session-checkpoint code, SSA test registration, semantic-query documentation,
and code-review coverage were excluded from these commits. No performance
gain, complete native workload coverage, or complete plan gate is claimed.
