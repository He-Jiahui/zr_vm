---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
tests:
  - tests/core/test_ssa_generation_publication.c
  - tests/core/ssa_generation_discard_prepared.inc
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
doc_type: acceptance
status: scoped-accepted
---

# Prepared generation metadata cancellation

This slice adds an explicit `Generation_DiscardPrepared` operation over the
existing caller-owned metadata records. It implements the leaf plan's finite
pre-publication cancellation boundary: an abandoned PREPARED record can return
to FREE without replacing active or consuming another generation. It does not
complete 08.03, install/free executable code, change capability registrations,
or connect interpreter frames, callbacks, GC, or JIT code leases.

Baseline is main commit `00cc9123` (generation lease limit). The separate write
set is the generation C/header, the existing publication test's include/main
calls, the new independent fixture include, the module lifetime document, and
this acceptance. The 920-line original test gains only eight include/call lines.
No CMake or plan checkbox changes are required.

## Trigger and contract

Prepare scans FREE records. CollectRetired deliberately ignores PREPARED.
Previously a one-slot manager could Prepare a candidate, abandon it before
Publish, then receive CAPACITY indefinitely because no cancellation API could
return that slot. The fixture establishes CAPACITY and a zero collected count
before canceling and successfully preparing again.

Discard accepts only an unleased input handle whose record belongs to this
manager, matches its generation, is PREPARED, has full-width leaseCount zero,
and is distinct from active. All checks and clearing share the existing lock
with Publish and Acquire; ownership is checked before record dereference.
Numbered Acquire admits PREPARED, so an unleased prepared handle alone does not
prove that the candidate is unpinned.

Success clears the record identity, generation and lease count, marks it FREE,
and clears the supplied handle. Active, nextGeneration and count stay unchanged;
the success diagnostic reports the discarded generation in actualGeneration.
Slot reuse allocates a fresh generation. Generation exhaustion remains exhausted.

| Rejection | Status and diagnostic |
| --- | --- |
| Null manager/handle/record, or leased input | INVALID_ARGUMENT; zero diagnostic generation/count fields, matching Publish |
| Foreign record | NOT_PREPARED; expected supplied generation, actual zero |
| Generation mismatch, FREE, ACTIVE or RETIRED | NOT_PREPARED; expected supplied and actual current slot generation |
| Matching PREPARED with acquired leases | INVALID_STATE; expected/actual matching generation and lease count |
| Defensive PREPARED equal to active | INVALID_STATE; active stays unchanged |

Every rejection preserves the input handle and both managers' logical fields.
Busy detection compares a `uint_fast32_t` load with zero before narrowing; its
diagnostic saturates an exclusively seeded out-of-range count at UINT32_MAX.
The fixture restores fictional count/active state on every exit. Legitimate
leases are obtained and released with real API calls.

The public count field is deliberately left unresolved: Prepare/Rollback
increment it up to capacity, while collection and cancellation do not decrease
it. Allocation scans FREE states. This change neither defines live occupancy
nor removes the existing header TODO.

## Functional boundaries

The new include has always-active setup/checks in both Debug and NDEBUG:

| Fixture | Assertions |
| --- | --- |
| `test_discard_slot_reuse_and_aba` | CAPACITY before cancellation, cleared record/handle, optional null diagnostic, duplicate rejection, fresh generation reuse, old-copy Publish/Discard rejection, legitimate new Publish, 16 reuse cycles |
| `test_discard_guards_and_live_versions` | Same-number foreign manager, invalid/mismatched/active/retired guards, real PREPARED lease rejection, Resolve/Release then successful retry, retained retired and active versions preserved |
| `test_discard_rollback_and_generation_exhaustion` | Cancel rollback-created candidate without touching source/active, repeat rollback with fresh generation, cancel at UINT64_MAX without resetting allocator |
| `test_discard_full_width_busy_and_active_guard` | UINT32_MAX busy count, UINT32_MAX+1 where fast atomic is wider, defensive active guard, fictional state restoration |
| `test_discard_competing_operations(FALSE/TRUE)` | 16 copied-handle Publish/Discard and 16 Acquire/Discard competitions, only legal serialized outcomes, join before checks/cleanup, real lease release |

Competing threads have separate handle objects. No artificial-held lock,
unbounded start barrier, or short scheduling assertion is introduced. Each
successful creation is joined before checks and cleanup. A join error exits the
whole test without reclaiming a potentially live stack-backed worker fixture.
The validation drivers bound the whole process at 30 seconds; registered CTest
retains its existing timeout. Ordinary/ASan contention is scheduling coverage,
not a ThreadSanitizer race verdict. This slice does not run TSan.

Local reference review supported explicit pre-publication cleanup, not a copied
generation algorithm: Lua `src/ldo.c:996-1016` releases parser scaffolding after
protected parsing, and `src/lstate.c:269-284` separates partially built-state
cleanup. QuickJS `quickjs.c:35927-35931` frees a failed module value, with its own
unresolved-dependencies TODO limiting that analogy. CPython
`Lib/importlib/_bootstrap.py:841-874` removes a failed module and clears its
initializing flag; `Lib/test/test_importlib/test_spec.py:277-296` asserts its
absence. These references do not determine zr_vm count/lease ownership.

## Environment and artifact ownership

All generated objects, temporary files, logs, scripts and binaries stay under
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\generation-discard-prepared`
(`/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-discard-prepared`). Repository
sources remain on E and are read in place. WSL uses Ubuntu-22.04, explicit `-e`,
fixed Linux PATH and `/` bootstrap cwd, then the driver enters its own D output
directory before invoking a compiler. No source copy, ext4 build, large CMake
configure, staging, commit, or foreign cleanup is performed by this agent.

Tools: GCC 11.4.0 (`11.4.0-1ubuntu1~22.04.3`), Ubuntu Clang 14.0.0
(`14.0.0-1ubuntu1.1`), installed MSVC toolset `14.44.35207` through the existing
`E:\Visual Studio\Common7\Tools\VsDevCmd.bat`. Native default-toolset metadata
matches the preceding lease acceptance. Windows FileVersionInfo probing failed
and does not establish a more precise cl.exe patch version.

`frozen-source.sha256` records the four code/test inputs. They stayed unchanged
from the first GREEN compile through final checks. Prior completed lease
artifacts were safely reduced by 16 substituted/temporary files, 13,008,956
bytes; `lease-cleanup-receipt.tsv` retains SHA256 and byte evidence. All prior
lease logs/scripts/GDB receipts and representative GCC/Clang/MSVC binaries were
preserved.

## RED and discarded validation attempts

The real first fixture was written before the API implementation. A declared
fail-closed scaffold returned NOT_PREPARED and changed nothing. The command
`cmd.exe /d /c <D-root>\msvc.cmd debug red` built successfully (0), then failed
(1) at the first valid DiscardPrepared expected-OK assertion, original include
line 135. `red-msvc-debug/build.log` and `run.log`, plus its binary/PDB, preserve
that behavioral RED. The final source contains no scaffold or test-only API.

Failures and invalid attempts remain recorded; none counts as a pass:

| Attempt | Actual result and limit |
| --- | --- |
| Initial GCC RED | Compile timeout 124 at 600s, driver 1; new include helpers plus an earlier-read main produced unused-function diagnostics while the fixture was being extended; not a behavioral RED |
| First GCC GREEN sanitizer Debug | Compile timeout 124 at 600s, empty build log, driver 1; no run |
| First Clang no-PIE sanitizer Debug | Overall driver compile/link timeout 124 at 600s, driver 1; four complete instrumented objects remained and were later independently relinked |
| Mutable-driver GCC final Debug | Reported build 0, then run 127 because generation-test did not exist; readelf also confirmed no file, while the ELF loader existed; script was edited during this attempt, so its command integrity is unsuitable for acceptance |

The initial GCC run was inspected as cc1 in D-state with an empty temporary
assembly file. This identifies an I/O wait observation, not a source failure or
a definitive kernel/filesystem root cause. Editing fixtures/drivers while those
attempts were pending was a validation mistake. Final GCC uses a distinct
immutable `validate-pipe-frozen.sh`; final Clang NDEBUG started after the last
driver change. `-pipe` reduces temporary assembly file I/O without removing
instrumentation. Native final runs use a separate frozen process watchdog.

## Commands and final gates

All focused builds use these four translation units:

```text
tests/core/test_ssa_generation_publication.c
zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c
zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c
```

Linux compile flags are `-std=c11 -g -O1 -pipe -pthread -Wall -Wextra -Werror
-fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined`, with
`-DNDEBUG` for NDEBUG, both core/common include roots, and `-fuse-ld=gold` for
Clang. Compiler invocation timeout is 600s. Runs use timeout 30s,
`ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1`.

```powershell
# <D-root> is the absolute artifact directory above; no C: or E: outputs.
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-discard-prepared/validate-pipe-frozen.sh gcc debug frozen
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-discard-prepared/validate-pipe-frozen.sh gcc ndebug frozen
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-discard-prepared/validate.sh clang ndebug pipe
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/bin:/bin /bin/bash /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/generation-discard-prepared/relink-clang-debug.sh
```

The Debug Clang relink first checked zero matching writer processes, nonzero
ELF objects, hashes, and `nm` ASan/UBSan markers. Its new output directory has
`objects.sha256`, `objects-elf.log`, `objects-symbols.log`, verbose build log,
and run log. It linked the frozen Debug objects with
`clang -v -pthread -fsanitize=address,undefined -no-pie -fuse-ld=gold`, preserving
instrumentation. Both Clang links report 131 ld.gold local ASan export warnings
(`__asan_check_*`, `__asan_extra_spill_area`); compiler diagnostics have no other
warnings. These linker warnings are retained, not presented as clean output.
No default-PIE stability claim or fresh ASLR/GDB sampling is made here.

Native flags are `/utf-8 /std:c11 /experimental:c11atomics /W3 /Zi /Od
/DZR_PLATFORM_WIN /DZR_PLATFORM_WIN_USE_MSVC /D_CRT_SECURE_NO_WARNINGS`, the two
include roots, and `/DNDEBUG` for NDEBUG. TEMP, TMP and cwd are D-only before
VsDevCmd. `msvc.cmd` produces UTF-8 builds; final compiler child is bounded at
600s and the binary child at 30s. Watchdog receipts name the exact process
handles it started; no foreign process selection/kill occurs.

```powershell
& "$PSHOME\pwsh.exe" -NoProfile -ExecutionPolicy Bypass -File 'D:\tmp\zr_vm\ssa-20261002-01a0fc3b\generation-discard-prepared\validate-msvc.ps1' -Mode debug
& "$PSHOME\pwsh.exe" -NoProfile -ExecutionPolicy Bypass -File 'D:\tmp\zr_vm\ssa-20261002-01a0fc3b\generation-discard-prepared\validate-msvc.ps1' -Mode ndebug
```

| Final artifact | Build/link exit | Run exit |
| --- | --- | --- |
| frozen-gcc-debug | 0 | 0 |
| frozen-gcc-ndebug | 0 | 0 |
| relink-clang-debug | 0 (relink) | 0 |
| pipe-clang-ndebug | 0 | 0 |
| final2-msvc-debug | 0 | 0 |
| final2-msvc-ndebug | 0 | 0 |

Earlier native GREEN Debug and NDEBUG also built/ran 0/0; the bounded final2
runs replace them as representative evidence. All six focused final
build/link and run gates passed. Focused direct runs
do not establish full code lifetime or the complete leaf-plan acceptance.

## Root review and independent gates

The root inspected the six-file change and complete new fixture. An independent
read-only review found no actionable findings, checked membership before
dereference and the lock-protected state/lease checks, and confirmed that
worker joins precede assertions and cleanup. The four current code-input
hashes match the frozen manifest.

The root rebuilt the registered generation and GVN targets in the reusable
native matrix, exit 0 (`control/gvn-owned-discard-root-build.log`). The native
generation fixture reports C4389 at its snapshot enum comparison and C4127 at
the platform-width condition; these warnings remain visible in the log and
this build is not claimed to pass `/WX`.

The initial two-test `-j 1` CTest invocation passed GVN but timed out the
generation test at 42.32 seconds, command exit 8. It is retained in
`control/gvn-owned-discard-root-ctest.log`. With no source change or timeout
change, the root ran the original registration alone:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^ssa_generation_publication$ --output-on-failure --no-tests=error -j 1
```

That invocation returned 0, 1/1 PASS in 7.96 seconds; its evidence is
`control/discard-prepared-alone-ctest.log`. The earlier timeout is not counted
as a pass and its underlying cause is not asserted here.

The root independently reran `frozen-gcc-debug/generation-test` using fixed
Linux PATH, ASan leak detection and UBSan halt-on-error. It returned 0 in
9.244 seconds without sanitizer diagnostics; the existing stress case reports
100 publications, 7013 reader resolves and zero worker failures. The exact
launcher and result are in `control/discard-prepared-root-gcc-sanitizer.log`.
All root logs, temporary files and artifacts remain below the owned D-drive
root. These gates accept only this metadata-cancellation slice.
