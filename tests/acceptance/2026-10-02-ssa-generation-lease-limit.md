---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
tests:
  - tests/core/test_ssa_generation_publication.c
  - tests/acceptance/2026-10-02-ssa-generation-lease-limit.md
doc_type: acceptance-record
---

# SSA 08.03 generation lease limit

## Scope

Bound each generation record's lease count at `UINT32_MAX`. `AcquireActive`
and numbered `Acquire` check the full-width atomic value under the existing
manager lock before incrementing it. At the limit they return the existing
`GENERATION_OVERFLOW` status, leave the output handle clear, and report the
record generation plus `leaseCount == UINT32_MAX`. No enum value or record
layout changes. `Generation_StatusName(OVERFLOW)` now returns `overflow`.

This is the metadata generation manager slice. It does not install executable
code, connect frame entry/exit, cancel prepared staging, or complete 08.03.

## Baseline and RED

The previous helper incremented `atomic_uint_fast32_t` without a bound, while
Release and the public diagnostic/view counts use `TZrUInt32`. On a platform
with a wider fast type, accepting lease number `2^32` makes those public reads
truncate to zero. With a 32-bit fast type, the atomic itself can wrap to zero,
which would satisfy retired collection despite outstanding handles.

The new fixture was compiled against the unchanged implementation first. GCC
RED compilation exited 0; its run exited 1 at the active-entry overflow check
(`status == GENERATION_OVERFLOW`), because the old helper returned OK.

The retained RED executable was inspected with GDB using the D-local
`red.gdb` script. GDB exited 0 and showed:

```text
sizeof(records[0].leaseCount) = 8
records[0].leaseCount = 4294967296
status = ZR_HOT_PATCH_GENERATION_OK
diagnostic.leaseCount = 0
```

GDB warned that the source was newer than the executable: it intentionally
inspected the pre-fix RED binary after the fix was written. The breakpoint
is the unchanged overflow assertion location in the fixture.

## Test inventory

`test_lease_limit` runs separately for the active and numbered entry paths.
It uses exclusive fixture-side atomic seeding instead of billions of calls:

- `UINT32_MAX - 1` accepts the last legal lease, reports `UINT32_MAX`, and
  produces a resolvable handle.
- An additional Acquire rejects with OVERFLOW, the expected generation/count,
  a clear handle, and the `overflow` status name.
- Failed acquisition preserves the atomic count, all exposed active record
  identity fields, the other slot's FREE state, active pointer, manager count,
  and next-generation counter.
- Release reduces the limit by one; either entry can acquire again.
- A retained RETIRED record at the limit rejects numbered Acquire and cannot
  be collected. After fictional leases are reset to the one actual holder,
  Release reaches zero and collection succeeds without changing the active
  replacement record.

Checks and API calls remain active under `NDEBUG`. The fixture is
single-threaded during seeding and resets fictional counts before Deinit,
including on its failure path. The same executable also runs the existing
manager-ownership, bounded Resolve-lock observation, 100-publication concurrent
Resolve/Publish stress, and ordinary lease/collection cases.

## Tooling and commands

All generated binaries, compiler temporary files, scripts, and logs are in
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\generation-leases`, corresponding to
`/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases`. Sources were compiled
directly from the existing checkout; no source snapshot or cross-disk copy was
created. WSL used `-e`, a fixed Linux PATH, D-only working directory, and D-only
TMPDIR. No repository CMake registration or shared build cache was changed.

Tools: GCC 11.4.0 (`11.4.0-1ubuntu1~22.04.3`), Ubuntu Clang 14.0.0
(`14.0.0-1ubuntu1.1`), and the installed MSVC toolset `14.44.35207` via
`E:\Visual Studio\Common7\Tools\VsDevCmd.bat`. GDB was Ubuntu 12.1
(`12.1-0ubuntu1~22.04.2`); the installed alternate linker was GNU gold 1.16
(Ubuntu Binutils 2.38).

RED and GDB commands from PowerShell:

```powershell
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/red.sh
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash -c '/usr/bin/gdb -q -batch -x red.gdb >red-gdb.log 2>&1; result=$?; cat red-gdb.log; exit "$result"'
```

`red.sh` compiled the generation test plus `hotpatch_generation.c`,
`hotpatch_publish.c`, and `hotpatch_retire.c`, with both core/common include
directories and `gcc -std=c11 -g -O0 -pthread`.

Final Linux commands:

```powershell
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/validate.sh gcc debug
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/validate.sh gcc ndebug
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/validate.sh clang debug
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/validate.sh clang ndebug
```

The validation script compiles the same four sources with
`-std=c11 -g -O1 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer
-Wall -Wextra -Werror`, adding `-DNDEBUG` in the corresponding mode.
It runs with `ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1`.
Compiler outputs and test outputs are saved as `<compiler>-<mode>-build.log`
and `<compiler>-<mode>-run.log`.

The default Clang GNU ld links spent more than ten minutes in D state with
`wchan=p9_client_rpc`, waiting on D-drive filesystem access. No source was
moved and no process was killed. While they waited, the already compiled
Clang objects were linked with the installed gold linker:

```powershell
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/clang-gold.sh debug
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/clang-gold.sh ndebug
```

`clang-gold.sh` runs `clang -pthread -fsanitize=address,undefined -fuse-ld=gold`
on the four existing object files for the selected mode, then executes with
the same ASan/UBSan options. The mode-specific object names were taken from
the waiting GNU ld command lines: the NDEBUG driver explicitly included
`-DNDEBUG` during compilation. Gold did not recompile or alter these objects.
Its logs contain warnings about exporting local `__asan_check_*` assembly
helpers and `__asan_extra_spill_area` from Clang's static sanitizer runtime
export list. Those warnings are retained in the gold build logs; links and
instrumented runs exited 0. The original GNU ld Debug subsequently completed
with build/run exit 0 and an empty build log.

The GNU ld NDEBUG first run exited 139 with an empty run log. Three bounded
direct rechecks of that executable returned 139, 0, 139. A GDB run with its
default disabled ASLR and `detect_leaks=0` reached main and exited normally;
the GDB script itself returned 1 because its final `bt` was requested after
normal inferior exit, when no stack remained. This debugger exit is not a test
assertion failure.

An ASLR-enabled GDB script then stopped at either main or the startup signal.
Five already-started samples were retained; the first two stopped before main
with SIGSEGV and this stack, while the remaining three reached the main
breakpoint. All five debugger invocations exited 0; reaching the breakpoint
alone is not counted as a completed test run.

```text
__sanitizer::internal_mmap
__sanitizer::MmapNamed
__sanitizer::ReservedAddressRange::Init
__sanitizer::SizeClassAllocator64<__asan::AP64<...>>::Init
__asan::Allocator::InitLinkerInitialized
__asan::AsanInitInternal
_dl_init
_dl_start_user
```

This locates the observed SIGSEGV in sanitizer startup, before the lease
fixture can run. It does not establish that the default PIE sanitizer
configuration is reliable. The GDB scripts and per-attempt logs are retained
under the same D directory. No further ASLR sampling was performed.

The original Clang drivers automatically removed their temporary objects
after linking, so a further object-only no-PIE link was unavailable. Root
authorized one focused NDEBUG recompile with the source frozen, retaining
ASan/UBSan and adding `-fno-pie -no-pie -fuse-ld=gold`:

```powershell
wsl.exe --cd /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/nopie -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/nopie/validate.sh
```

This script uses the same four sources, a D-local TMPDIR, `timeout 600s` for
compilation/linking, and two bounded instrumented runs (`timeout 30s` each).
It disables core dumps for these diagnostic/recheck processes. The no-PIE
build exited 0 and both instrumented runs exited 0, each reporting 100
publications and zero worker failures. This is a bounded compatibility gate;
it does not change production configuration or claim stable default PIE
sanitizer startup. Gold's sanitizer-runtime export warnings remain recorded.

Final Windows commands:

```powershell
cmd.exe /d /c D:\tmp\zr_vm\ssa-20261002-01a0fc3b\generation-leases\msvc.cmd debug
cmd.exe /d /c D:\tmp\zr_vm\ssa-20261002-01a0fc3b\generation-leases\msvc.cmd ndebug
```

The script calls VsDevCmd for x64 and keeps TEMP/TMP, object/PDB/link outputs,
and logs in its D-local mode directory. Flags are `/utf-8 /std:c11
/experimental:c11atomics /W3 /Zi /Od /DZR_PLATFORM_WIN
/DZR_PLATFORM_WIN_USE_MSVC /D_CRT_SECURE_NO_WARNINGS`, with `/DNDEBUG` for that
mode. Initial successful runs without `/utf-8` emitted C4819 code-page warnings;
their logs were retained as `build-pre-utf8.log`, then both modes were rebuilt
and rerun with explicit UTF-8 source decoding.

## Results and acceptance

| Gate | Build exit | Run exit |
| --- | --- | --- |
| GCC RED, unchanged implementation | 0 | 1, expected overflow assertion |
| GDB, retained RED binary | n/a | 0, full-width overflow observed |
| GCC ASan/UBSan Debug | 0 | 0 |
| GCC ASan/UBSan NDEBUG | 0 | 0 |
| Clang ASan/UBSan Debug, GNU ld | 0 | 0 |
| Clang ASan/UBSan Debug, same objects / gold | 0 | 0 |
| Clang ASan/UBSan NDEBUG, same objects / gold | 0 | 0 |
| Clang ASan/UBSan NDEBUG, GNU ld, first run | 0 | 139, pre-main sanitizer startup |
| GNU ld NDEBUG direct rechecks | n/a | 139 / 0 / 139 |
| ASLR-enabled GDB startup samples | n/a | five debugger exits 0; two pre-main SIGSEGV, three main breakpoints |
| Clang ASan/UBSan NDEBUG, no-PIE / gold | 0 | 0 / 0 |
| MSVC UTF-8 Debug | 0 | 0 |
| MSVC UTF-8 NDEBUG | 0 | 0 |

The lease-limit slice passes its focused GCC/Clang sanitizer and MSVC gates,
including the bounded Clang NDEBUG no-PIE compatibility gate. The original
GNU ld NDEBUG executable has an observed pre-main sanitizer startup failure
under ASLR. Its successful recheck and the successful gold run are retained
as individual results, not a claim of stable default PIE startup. Root's
independent validation is recorded below. The completed direct runs report
zero worker failures. No full repository matrix is claimed here. There is no new
compatibility path, public test hook, or performance claim. Prepared staging
cleanup, executable lifetime integration, and the broader 08.03 gates remain
outside this change.

## Root independent validation

The root reviewed the exact five-file change and independently built the formal
`zr_vm_ssa_generation_publication_test` MSVC Debug target in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc` (build exit zero;
`control/independent-targets-build-v2.log`). The first five-test CTest run with
`-j 2` timed out in this test after 46.57 seconds while its other four tests
passed. This failed run remains recorded in
`control/independent-targets-ctest.log`; it is not counted as a passing suite.

The same binary then passed its original registered CTest alone in 5.51
seconds, without changing the test timeout or source:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/generation-serial-ctest.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^ssa_generation_publication$ --output-on-failure -j 1
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_linux_artifact.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/generation-root-clang-nopie-sanitizer.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-leases/nopie/clang-ndebug-nopie
```

Both root commands exited zero. The independent Clang no-PIE ASan/UBSan/LSan
run reported 100 publications, zero worker failures, and no sanitizer
diagnostic. It confirms the scoped compatibility configuration, without
reclassifying the recorded default-PIE startup failures. All root outputs and
temporary files remained under the D-drive artifact root.
