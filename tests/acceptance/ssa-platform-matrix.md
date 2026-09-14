---
related_code:
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
  - tests/core/test_ssa_platform_matrix.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
  - tests/cmake/ssa-platform-matrix.cmake
plan_sources:
  - docs/plans/ssa/10-jit-platforms/03-platform-matrix.md
  - docs/plans/ssa/10-jit-platforms/02-host-baseline-jit.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_platform_matrix.c
doc_type: testing-guide
---

# SSA Platform Matrix Acceptance

## Scope

This evidence record covers the platform capability, target ABI, artifact
compatibility, backend availability, runtime provenance, and semantic witness
contract for SSA plan 10.03.  It intentionally records real device/runtime
gaps instead of treating cross compilation as execution.

## Baseline

Before this slice, the repository had host-specific CMake platform branches and
host JIT/hotpatch contracts, but no shared value-only platform matrix verifier
or focused `test_ssa_platform_matrix.c`.  The worktree is concurrently dirty;
shared CMake, umbrella headers, and other SSA files were left untouched.

## Frozen Capability Matrix

The verifier stores one row per target profile; a missing row is not treated as
support.  The following baseline is the contract fixture, not a claim that all
rows have a runnable device in this host:

| Target/profile | ExecBC | AOT-C | AOT-LLVM | Host machine JIT | Threads / concurrent GC | PMU | Unwind / debug |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Desktop x86-64/AArch64 | declared per row | declared per row | declared per row | allowed only when declared | explicit bits | explicit bit | explicit bits |
| Android AArch64 | declared per row | declared per row | declared per row | forbidden | explicit bits | explicit bit | explicit bits |
| iOS AArch64 | declared per row | declared per row | declared per row | forbidden | explicit bits | explicit bit | explicit bits |
| WASM32/WASM64 | declared per row | declared per row | declared per row | forbidden | never inferred; absent means single-thread | forbidden by contract | explicit bits |

`compiled`, `executed`, and `semanticPassed` are independent observation
columns.  Only a `passed` row with all three true is runtime acceptance;
`cross-compile`, emulator, browser, and real-device provenance remain visible
in the runner column.

## Test Inventory

The focused executable exercises:

| Case | Expected result |
| --- | --- |
| Desktop ExecBC compiled, executed, semantic witness passed | `OK` |
| Runtime stage omitted | `RUNTIME_NOT_EXECUTED` |
| Pointer width 32-bit artifact against 64-bit target | `ARTIFACT_ABI_MISMATCH` |
| Numeric/layout contract hash drift | explicit numeric/layout mismatch |
| WASM threads/concurrent GC/PMU required but absent | `REQUIRED_FEATURE_UNSUPPORTED` |
| iOS/Android/WASM machine-code JIT | `MACHINE_CODE_JIT_FORBIDDEN` |
| WASM32 declared with 64-bit pointer witness | `ABI_INVALID` |
| Cross-compile Android observation without execution | `RUNTIME_UNAVAILABLE` |
| Restricted iOS patch with a new native import | `RESTRICTED_PATCH_NATIVE_IMPORT` |
| Host JIT x86-64 and AArch64 target profiles | `OK` when host feature is declared |
| Target-triple or callback ABI drift | explicit target/ABI mismatch |
| Switch versus computed-goto with matching semantic witnesses | `OK`; witness drift rejected |
| Unavailable AOT-LLVM backend | `BACKEND_UNSUPPORTED` |
| Passed row carrying `unsupportedFeatures` | `OBSERVATION_INVALID` (never a false pass) |

## Capability-driven CMake registration

`tests/cmake/ssa-platform-matrix.cmake` is the configure-time companion to the
value-only C contract.  It freezes ten desktop/mobile/WASM profile rows,
normalizes the current target without inferring runtime support, and exposes
`zr_vm_ssa_platform_matrix_register_test(...)`.  A missing executable, backend,
or required feature registers a visible CTest skip marker containing the
unavailable reason; it is never silently omitted or counted as a pass.

The declaration is self-checked independently of a configured project:

```text
cmake -P tests/cmake/ssa-platform-matrix.cmake
```

Expected output includes `SSA platform matrix self-check passed (1; 10
profiles)`.  This check validates profile identity, pointer-width policy,
mobile/WASM JIT prohibition, backend/feature vocabulary, and the
concurrent-GC/threads invariant.  It does not claim that Android, iOS, or WASM
rows have executed on a real device or browser.

## Tooling Evidence

Windows PowerShell, GCC 4.8.3:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
    -Wmissing-prototypes -Werror -Izr_vm_common/include \
    tests/core/test_ssa_platform_matrix.c \
    zr_vm_common/src/zr_vm_common/ssa_platform_contract.c \
    -o %TEMP%\\ssa_platform_matrix_green4.exe
%TEMP%\\ssa_platform_matrix_green4.exe
```

Observed result: compile exit `0`, executable exit `0`.

The fixture also checks C++ inclusion, GCC/Clang extended-warning syntax, and
the host ABI probe.  Null initialization is a no-op, repeated initialization
is deterministic, and the contract owns no heap allocation or runtime handle;
therefore OOM/cancel/lease-balancing paths are not applicable to this
value-only verifier and are left to the platform adapter that produces a row.

WSL Ubuntu 22.04, GCC 11.4.0:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
    -Wmissing-prototypes -Werror -Izr_vm_common/include \
    tests/core/test_ssa_platform_matrix.c \
    zr_vm_common/src/zr_vm_common/ssa_platform_contract.c \
    -o /tmp/ssa_platform_matrix_gcc && /tmp/ssa_platform_matrix_gcc
```

Observed result: `gcc_exit=0`.

WSL Ubuntu 22.04, Clang 14.0.0:

```text
clang -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
    -Wmissing-prototypes -Werror -Izr_vm_common/include \
    tests/core/test_ssa_platform_matrix.c \
    zr_vm_common/src/zr_vm_common/ssa_platform_contract.c \
    -o /tmp/ssa_platform_matrix_clang && /tmp/ssa_platform_matrix_clang
```

Observed result: `clang_exit=0`.

RED evidence was collected before implementation: the same strict Windows
GCC command failed with `fatal error: zr_vm_common/ssa_platform_contract.h:
No such file or directory`.

Sanitizer commands (WSL, explicit no-PIE to keep the sanitizer startup path
deterministic in this environment):

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
    -Wmissing-prototypes -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -fno-pie -no-pie -g \
    -Izr_vm_common/include tests/core/test_ssa_platform_matrix.c \
    zr_vm_common/src/zr_vm_common/ssa_platform_contract.c \
    -o /tmp/ssa_platform_matrix_gcc_asan_ubsan && \
    /tmp/ssa_platform_matrix_gcc_asan_ubsan

clang -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
    -Wmissing-prototypes -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -fno-pie -no-pie -g \
    -Izr_vm_common/include tests/core/test_ssa_platform_matrix.c \
    zr_vm_common/src/zr_vm_common/ssa_platform_contract.c \
    -o /tmp/ssa_platform_matrix_clang_asan_ubsan && \
    /tmp/ssa_platform_matrix_clang_asan_ubsan
```

Observed result: both binaries compiled and ran three consecutive times with
`ASAN_OPTIONS=detect_leaks=1` and exit `0`; Valgrind with full leak checking
also exited `0`.  A Clang 14 PIE sanitizer binary had one transient WSL
startup failure during an earlier exploratory run; the final PIE binary ran
three times successfully, while the no-PIE command above is the reproducible
acceptance evidence.  The focused test does not allocate or dereference
managed pointers, so the sanitizers target array/string/diagnostic boundaries
rather than VM heap behavior.

MSVC (Visual Studio 2022 developer environment, `/W4 /WX`):

```text
cl /nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS \
   /I zr_vm_common\include tests\core\test_ssa_platform_matrix.c \
   zr_vm_common\src\zr_vm_common\ssa_platform_contract.c \
   /Fe:%TEMP%\ssa_platform_matrix_msvc_final.exe
%TEMP%\ssa_platform_matrix_msvc_final.exe
```

Observed result: compile exit `0`, executable exit `0`.

## Results

- Strict Windows GCC, WSL GCC, and WSL Clang focused binaries pass.
- Strict MSVC `/W4 /WX`, GCC/Clang extended-warning syntax checks, C++ header
  smoke, WSL ASan+UBSan (three runs per compiler), and Valgrind pass.
- The RED test failed for the intended missing-contract reason before code was
  added.
- A WSL `gcc -m32` probe is explicitly unavailable because the host lacks
  `/usr/include/bits/wordsize.h`; this is an environment gap, not a promoted
  32-bit result.
- No shared CMake, `tests/CMakeLists.txt`, `ssa-tests.cmake`, umbrella header,
  or unrelated host-JIT file was modified.
- Android, iOS, and WASM real-device/browser runtime smoke is unavailable in
  the current host and remains explicitly unavailable; cross compilation is
  not recorded as runtime acceptance.

## Acceptance Decision

The contract slice is focused-test accepted for its value-only verifier on
Windows GCC, WSL GCC, and WSL Clang.  The full 10.03 platform milestone is
blocked until the platform-specific rows have actual compiled/executed/
semantic-passed evidence from device, emulator, browser, or WASM runners.
