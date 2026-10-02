---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - tests/core/test_ssa_roots_observation.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/core/test_ssa_roots_observation.c
doc_type: testing-guide
---

# Core observation: reused scalar storage

Date: 2026-10-02 UTC (2026-10-03 Asia/Shanghai).

## Scope and reproduced failure

The real core layout validator accepts non-overlapping live ranges with the
same physical slot, scalar type, size and alignment. Observation has no current
instruction or active logical occupant. Previously its write loop accepted
conflicting logical payloads, wrote 111 then 222 into the same backing bytes,
and returned success. Earlier logical writeback could then disagree with the
final frame storage.

The new regression finalizes a legitimate three-slot layout, with independent
storage between the two reused entries. Inputs 111/333/222 must be rejected
before any frame, writeback, invalidation-array or invalidation-count output
changes. The regression compares snapshots of every output and repeats the
failure twice. It asserts `SLOT_OVERLAP`, index 2, related index 0.

Test-first RED used the unchanged production source plus this regression.
GCC's real three-unit compile succeeded and the executable exited 1 at the
new `!ZrCore_Execution_ObserveFrame` assertion (line 482). MSVC repeated that
same failure with the immutable old-source snapshot, all three compile exits
and link exit 0, fixture exit 1, `inputs_changed=[]`.

## Repair and controls

The existing preflight pass now compares actual overlapping scalar write
bytes before writing any output. It uses the native byte representation and
the declared byte size; unrelated bytes in the 64-bit input do not conflict.
No allocation, layout ABI change or active-occupant inference is introduced.

The final executable invokes eight test functions: six existing root and
observation tests and two new functions with these additional checks:

- conflicting reused payloads reject twice and preserve all four outputs;
- identical reused four-byte payloads succeed twice and invalidate each
  physical identifier once;
- differing unused bytes of a 64-bit input still succeed for a four-byte slot;
- read-only reused aliases succeed without changing frame bytes;
- independent scalar storage materializes correctly;
- distinct physical identifiers sharing the same byte interval are first
  accepted by the real layout finalizer, then obey conflict rejection and
  identical-payload acceptance.

## Actual-source validation products

All products are under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/core-observation` (called `ROOT` below).
The actual units are the test file, `execution_frame_roots.c`, and
`execution_frame_observation.c`. Input snapshots copy source text only and
resolve 39 repository source/header/include files; compiled products are never
copied from another drive or a borrowed cache.

The shared local driver is
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/actual_tu_driver.py`.
Full compiler/linker commands, PID, timings, exits and source hashes are in
each run's `receipt.json` and `inputs-before.json`/`inputs-after.json`.

| Run | Build and execution result | Evidence under ROOT |
| --- | --- | --- |
| GCC original-source RED | build 0, expected fixture 1 | `red-build.log`, `red-run.log`, `red-gcc` |
| MSVC old-source RED | three compiles/link 0, expected fixture 1, stable inputs | `msvc/red-msvc-cache-v3/receipt.json` |
| GCC ASan/UBSan final | three compiles/link/fixture 0, stable inputs | `gcc/green-v1/receipt.json` |
| MSVC final | three compiles/link/fixture 0, stable inputs | `msvc/green-msvc-cache-v1/receipt.json` |
| GCC executable repeat | exit 0, empty stdout/stderr | `green-v1-fixture-repeat.json` |
| MSVC executable repeat | exit 0, empty stdout/stderr | `green-msvc-cache-v1-fixture.exe-repeat.json` |
| Fresh registered MSVC CTest | configure/build/CTest 0; one matching test passed | `formal-msvc/cfg-receipt.json`, `build-receipt.json`, `ctest-receipt.json` |
| Clang ASan/UBSan with native LLD | actual three Clang objects linked 0; fixture 0 | sibling `validation-control/core-clang-link-smoke-3/link-receipt.json`; `core-clang-link-smoke-3-fixture-repeat.json` |

GCC 11.4.0 uses C11, O0/debug, ASan+UBSan, frame pointers and non-PIE. Execution uses
`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. MSVC uses C11, `/MTd`,
`/Od`, `/Z7`, `/Gy`, `/Gw` and `/OPT:REF`, with MSVC 19.44.35228.0 from the installed 14.44.35207
x64 tools. The driver reads only the existing toolchain-environment metadata
to initialize MSVC; it does not borrow objects or binaries.

Final snapshot configuration:
`ROOT/snapshots/green-v1-1790973124254876300/snapshot-config.json`.
Its adjacent `snapshot-manifest.json` contains original and copied input
hashes. A native `--verify-snapshot` pass exited 0 and reported all 39 input
files unchanged against the actual repository after execution.

Reproduce the final direct gates with:

```powershell
# MSVC cached-toolchain config, with a fresh label in a copied config for rebuild:
C:\Users\HeJiahui\AppData\Local\Python\bin\python.exe D:\tmp\zr_vm\ssa-20261003-01a0fe2b\validation-control\actual_tu_driver.py D:\tmp\zr_vm\ssa-20261003-01a0fe2b\core-observation\green-msvc-cache-v1.json --toolchain msvc --verify-snapshot
wsl -e /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/core-observation/repeat_fixture.py /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/core-observation/gcc/green-v1/fixture /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/core-observation
wsl -e /usr/bin/env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /mnt/d/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/core-clang-link-smoke-3/fixture
```

## Failed attempts and limits

The initial MSVC driver had a batch-command quoting error. A second attempt
timed out importing VsDevCmd after 180 seconds. Neither was a test result;
their directories `msvc/red-current` and `msvc/red-v2` remain preserved.
The cached environment path subsequently produced the valid RED and GREEN
results above.

Clang's initial original-source RED attempt compiled all three units, but
linking timed out at 180 seconds. Its later header-hash sweep observed the
intentional production repair after the compiler/linker workers had ended;
that original-source `clang/red-current` run is not an accepted gate.
The immutable final-source Clang 14.0.0 `clang/green-v1` attempt also compiled all three
units with ASan/UBSan, but linking timed out at 180 seconds. Its final receipt
reports stable inputs and exit -15. That timeout is preserved and is not a
Clang runtime pass.

A separate GCC link of the already compiled Clang objects also timed out at
180 seconds (exit -15); `clang-gcc-link/receipt.json` preserves the object
hashes and command. This attempt never executed a fixture and adds no runtime
coverage.

The validation collaborator's fresh native LLD link used this task's exact
three final Clang objects in place. Its actual Clang 14 `-###` link plan retains
the static Clang ASan/UBSan runtime, non-PIE and section flags. The local native
linker uses canonical system-library paths through the WSL filesystem view and
a path-only libc linker script. It preserves the original objects and reads
the system runtime inputs without copying any compiled product or borrowing an
old build cache. The link receipt reports exit 0, no timeout, and unchanged
object/system-library hashes. The executable is under the validation
collaborator's current-task D directory listed above. Its actual sanitizer
execution exited 0 in 3.083 seconds with empty stdout/stderr.
Native `llvm-readobj` subsequently exited 0 when reading ELF headers, program
headers, dynamic entries and symbols from the same D-local file. The inspection
confirmed ELF magic, `__asan_init`, UBSan handler symbols and the Linux dynamic
interpreter; `clang/native-link-recovery/readelf-receipt.json` records the
actual LLVM command and the binary hash
`31d3e49ed5d1e6245f27072944e8daee9b65b3d7c30f184b24ce24f2696d254b`.
The root independently inspected the same final three-object ELF64 x86_64
program and executed it in WSL with leak detection and UBSan halt-on-error
enabled. It exited 0 with empty stdout/stderr. The validation collaborator's
additional `validation-control/core-clang-link-smoke-3/runtime-proof/receipt.json`
also records fixture exit 0, `readelf`/`nm` exits 0 and successful ELF,
interpreter and sanitizer-symbol checks against that same binary hash.

The author's separate request to regenerate that Clang link plan timed out
after 90 seconds before entering LLD; `clang/native-link-recovery/query-failure.json`
records the captured terminal exception. It is a failed environment attempt.
The successful collaborator link already contains the actual final objects,
so the subsequent ELF checks and execution use that verified program directly.
An earlier WSL `readelf` metadata inspection timed out at 60 seconds;
`clang/native-link-recovery/readelf-failure.json` records that failed inspection.
The native LLVM inspection above succeeded without changing the executable.

The registered CTest gate uses a fresh minimal CMake project under
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/formal-core-observation`.
It reproduces the existing `zr_vm_ssa_core_roots_observation_test` declaration
and `ssa_core_roots_observation` registration from
`tests/cmake/ssa-tests.cmake`: the same three units, core/common includes and
CRT definition, with explicit platform/debug definitions. The target built
4/4 Ninja edges. `ctest --test-dir ROOT/formal-msvc -R
'^ssa_core_roots_observation$' --output-on-failure --no-tests=error` matched and
passed one test. The foreign parent CMake file was read without modification.

These gates do not claim a full parent CMake build, an existing-cache
CTest pass, performance benefit or completion of milestone 04.04. The observer
still lacks active-occupant selection; conflict rejection is the bounded
correct behavior until an explicit selector contract exists. No foreign dirty
source, shared parser documentation, job or cache was modified.
