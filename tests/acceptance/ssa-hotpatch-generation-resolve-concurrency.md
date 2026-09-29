---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
tests:
  - tests/core/test_ssa_generation_publication.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
doc_type: acceptance
status: in-progress
---

# Hotpatch generation Resolve concurrency

## Scope

Protect `Generation_Resolve`'s multi-field version snapshot from concurrent
`Generation_Publish` state changes and `CollectRetired` record reuse. The public
const manager signature, record layout, handle ownership check, and lease model
remain intact. This is a manager-local metadata synchronization boundary; it
does not integrate generation handles with interpreter frames or install
executable code.

## Baseline

Source tracing found that `Publish` and `CollectRetired` mutate ordinary record
fields while holding `manager.lock`; `Resolve` checked foreign-manager
membership safely, then read generation, state, identity fields, profile, and
lease count without that lock. A valid lease prevents record reclamation but
does not prevent `Publish` from changing ACTIVE to RETIRED. Therefore a
concurrent Resolve/Publish is a C data race and can produce a mixed snapshot.

Before the production change, the new bounded lock-gate scheduling check built
in the existing D GCC cache and its direct binary returned exit code 1 with the
observed failure:

```text
Resolve snapshot lock check failed at line 204: !completedWhileLocked
```

The test holds the manager lock, signals a worker immediately before
`Generation_Resolve`, and waits up to one second of monotonic wall time for
completion. The old implementation returned during that observed interval.
The signal leaves a preemption window before the call, so this is a bounded
scheduling observation rather than a mathematical proof of mutual exclusion.
The concurrent Publish/Resolve/CollectRetired stress is complementary evidence.

## Test inventory

- Bounded lock-gate scheduling check: the worker signals immediately before
  Resolve while the manager lock is held. If scheduled into the call, it must
  remain blocked until release, then return a complete ACTIVE view with the
  expected generation, module, content, contract, profile, and lease count.
  The signal-to-call preemption window means this check alone is not a
  deterministic proof; the stress case supplies separate interleaving
  coverage.
- Concurrent publish/resolve stress: a reader holds a lease across a publish,
  verifies the old record resolves as RETIRED, then acknowledges that first
  Resolve/Release through an atomic flag before the writer starts the remaining
  loop. The registered fixture uses 100 publication rounds so it remains under
  the 15-second CTest timeout on Windows while retaining repeated reader/writer
  overlap. A standalone sanitizer build can set
  `ZR_GENERATION_STRESS_PUBLISH_COUNT=500` for a longer run. Both modes check
  generation/content identity pairing and bound retries.
- Existing generation publication and cross-manager-handle checks remain in
  the same direct test binary.

## Tooling evidence

The existing GCC build cache is
`D:/tmp/zr_vm/close-proxy-core-red` (Ninja, GCC 11.4.0); build outputs remain on
D:. Earlier RED and pre-ack GREEN attempts used this cache and recompiled
`hotpatch_generation.c` plus the generation publication test. The current Ninja
graph predates the added `Threads::Threads` CMake line and its link command
therefore had no explicit `-pthread`; it linked and ran in this glibc
environment. The final GCC evidence below comes from a standalone `-pthread`
harness, so the updated GCC CMake graph remains unverified. Root explicitly
regenerated the MSVC D cache with regeneration suppressed afterward; the
target rebuilt successfully with the thread-link declaration present.

## Results

- Baseline old implementation: observed bounded-scheduling lock-gate RED, direct
  process exit 1.
- A pre-ack GCC fixed-source run built and passed direct/CTest with 500 stress
  rounds, but it did not include the later Windows scheduling acknowledgement
  and is not evidence for the current fixture.
- The acknowledged 500-publication MSVC stress passed direct, but its registered
  CTest exceeded the 15-second timeout at 16.27 seconds. The diagnostic run
  before adding the acknowledgement had shown the reader completing with
  `resolves=0`, `retired=0`, and no worker failure because the writer finished
  before the reader was rescheduled.
- The current source keeps the first-Resolve acknowledgement and uses 100
  registered publication rounds, with a bounded attempt count of 20 attempts
  per round. MSVC direct completed all 100 rounds successfully with
  `published=100 attempts=100 capacity-retries=0 reader-resolves=7010
  retired-views=2 worker-failures=0`. Its registered CTest still timed out at
  15.67 seconds despite printing completed stress metrics
  (`reader-resolves=49326`, `retired-views=28`, no failures). The remaining
  delay was traced to treating 1000 `Sleep(1)` iterations as one second;
  Windows timer granularity made that lock-gate wait about 15.6 seconds. The
  wait now uses a `GetTickCount64` / `CLOCK_MONOTONIC` wall-clock deadline while
  retaining short sleeps as yields.
- Final pre-NDEBUG-refactor MSVC target build exited 0. Direct exited 0 in 3.17s
  with `published=100 attempts=100 capacity-retries=0 reader-resolves=816
  retired-views=2 worker-failures=0`. Registered
  `ssa_generation_publication` CTest passed 1/1 in 1.17s (1.30s total under the
  15-second timeout). The prior 15.67-second timeout is retained above as the
  diagnosis that motivated the monotonic deadline. These results preceded the
  later test-only NDEBUG-safety refactor.
- Final-source MSVC target build exited 0 in 14.64s (2/2 build steps). Direct
  execution exited 0 in 2.03s with `published=100 attempts=100
  capacity-retries=0 reader-resolves=43770 retired-views=34
  worker-failures=0`. Registered `ssa_generation_publication` CTest passed 1/1
  (1.10s test, 1.19s total). This includes the NDEBUG-safe main and cleanup for
  an unexpectedly successful final stale-handle acquire. The verified commands
  were:

  ```text
  cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && set "TEMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp" && set "TMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp" && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_ssa_generation_publication_test -j 2'
  D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_ssa_generation_publication_test.exe
  ctest --test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc -R '^ssa_generation_publication$' --output-on-failure --no-tests=error
  ```
- A standalone ordinary GCC build with
  `ZR_GENERATION_STRESS_PUBLISH_COUNT=500` compiled and ran directly from the
  same D temporary harness directory, exit 0: `published=500 attempts=500
  capacity-retries=0 reader-resolves=559 retired-views=185 worker-failures=0`.
- Reproducible standalone GCC commands (the acceptance file is the saved
  command/output record; binaries were written under the temporary D directory and have been discarded):

  ```text
  wsl.exe --exec gcc -DZR_DEBUG -DZR_PLATFORM_UNIX -DZR_GENERATION_STRESS_PUBLISH_COUNT=500u -std=c11 -g -O1 -pthread -I/mnt/e/Git/zr_vm/zr_vm_core/include -I/mnt/e/Git/zr_vm/zr_vm_common/include /mnt/e/Git/zr_vm/tests/core/test_ssa_generation_publication.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c -o /mnt/d/tmp/zr_vm/hotpatch-generation-tsan/zr_vm_ssa_generation_publication_gcc500
  wsl.exe --exec /mnt/d/tmp/zr_vm/hotpatch-generation-tsan/zr_vm_ssa_generation_publication_gcc500
  ```

  Compile exit 0; run exit 0 with the metrics above.
- GCC ThreadSanitizer builds compiled, but both PIE and `-fno-pie -no-pie`
  executables exited 66 before the test reached `main`, with
  `FATAL: ThreadSanitizer: unexpected memory mapping`. This environment cannot
  provide a TSan race verdict; the ordinary 500-round GCC stress is not a
  substitute for that sanitizer verdict.
- The exact sanitizer run used
  `TSAN_OPTIONS=halt_on_error=1`; the PIE and no-PIE executables were
  `zr_vm_ssa_generation_publication_tsan` and
  `zr_vm_ssa_generation_publication_tsan_nopie` in
  `D:/tmp/zr_vm/hotpatch-generation-tsan`. Both failed during TSan startup with
  the mapping diagnostic; this acceptance file is the retained log.

  ```text
  wsl.exe --exec gcc -DZR_DEBUG -DZR_PLATFORM_UNIX -DZR_GENERATION_STRESS_PUBLISH_COUNT=500u -std=c11 -g -O1 -fsanitize=thread -fno-omit-frame-pointer -pthread -I/mnt/e/Git/zr_vm/zr_vm_core/include -I/mnt/e/Git/zr_vm/zr_vm_common/include /mnt/e/Git/zr_vm/tests/core/test_ssa_generation_publication.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c -o /mnt/d/tmp/zr_vm/hotpatch-generation-tsan/zr_vm_ssa_generation_publication_tsan
  wsl.exe --exec env TSAN_OPTIONS=halt_on_error=1 /mnt/d/tmp/zr_vm/hotpatch-generation-tsan/zr_vm_ssa_generation_publication_tsan
  wsl.exe --exec gcc -DZR_DEBUG -DZR_PLATFORM_UNIX -DZR_GENERATION_STRESS_PUBLISH_COUNT=500u -std=c11 -g -O1 -fsanitize=thread -fno-omit-frame-pointer -fno-pie -no-pie -pthread -I/mnt/e/Git/zr_vm/zr_vm_core/include -I/mnt/e/Git/zr_vm/zr_vm_common/include /mnt/e/Git/zr_vm/tests/core/test_ssa_generation_publication.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c -o /mnt/d/tmp/zr_vm/hotpatch-generation-tsan/zr_vm_ssa_generation_publication_tsan_nopie
  wsl.exe --exec env TSAN_OPTIONS=halt_on_error=1 /mnt/d/tmp/zr_vm/hotpatch-generation-tsan/zr_vm_ssa_generation_publication_tsan_nopie
  ```

## NDEBUG reliability

The pre-refactor main routine placed manager initialization and the remaining
setup API calls inside `assert`; source inspection shows that `NDEBUG` would
remove those calls and leave the manager uninitialized before `Deinit`. A
separate safe mechanism canary—not a dynamic run of the old test main—compiled
under Debug and returned 0; the same canary compiled with `-DNDEBUG` returned
1 with `NDEBUG canary: assert skipped setup; exited before Deinit`. The canary
puts a setup-counter assignment inside `assert` and checks the counter before
any teardown, demonstrating expression elision without executing the old
test's undefined-behavior path. The final main routine uses
always-active `CHECK_GENERATION_STATUS` and `CHECK_GENERATION_VALUE` checks and
guards cleanup with explicit initialization/lease flags. Final Debug and
NDEBUG standalone GCC harnesses both compiled and ran with exit 0. Their
stress metrics were respectively `published=100 attempts=100 capacity-retries=0
reader-resolves=7884 retired-views=41 worker-failures=0` and
`published=100 attempts=100 capacity-retries=0 reader-resolves=2780
retired-views=5 worker-failures=0`. These runs include the cleanup guard for an
unexpectedly successful final stale-handle acquire.

The final test harness compile/run commands used D for `TMPDIR`, executables,
and logs:

```text
wsl.exe --exec env TMPDIR=/mnt/d/tmp/zr_vm/generation-ndebug-check gcc -DZR_DEBUG -DZR_PLATFORM_UNIX -std=c11 -g -O0 -pthread -I/mnt/e/Git/zr_vm/zr_vm_core/include -I/mnt/e/Git/zr_vm/zr_vm_common/include /mnt/e/Git/zr_vm/tests/core/test_ssa_generation_publication.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c -o /mnt/d/tmp/zr_vm/generation-ndebug-check/generation_publication_debug
wsl.exe --exec /mnt/d/tmp/zr_vm/generation-ndebug-check/generation_publication_debug
wsl.exe --exec env TMPDIR=/mnt/d/tmp/zr_vm/generation-ndebug-check gcc -DNDEBUG -DZR_PLATFORM_UNIX -std=c11 -g -O0 -pthread -I/mnt/e/Git/zr_vm/zr_vm_core/include -I/mnt/e/Git/zr_vm/zr_vm_common/include /mnt/e/Git/zr_vm/tests/core/test_ssa_generation_publication.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c -o /mnt/d/tmp/zr_vm/generation-ndebug-check/generation_publication_ndebug
wsl.exe --exec /mnt/d/tmp/zr_vm/generation-ndebug-check/generation_publication_ndebug
```

The final logs were written as `debug-compile-final.log`, `debug-run-final.log`,
`ndebug-compile-final.log`, and `ndebug-run-final.log` in
`D:/tmp/zr_vm/generation-ndebug-check`. Both profile compiles and direct runs
exited 0.

The canary source and binaries were written to the temporary D directory. Commands and
observed results:

```text
wsl.exe --exec env TMPDIR=/mnt/d/tmp/zr_vm/generation-ndebug-check gcc -std=c11 -g -O0 /mnt/d/tmp/zr_vm/generation-ndebug-check/ndebug_assert_canary.c -o /mnt/d/tmp/zr_vm/generation-ndebug-check/ndebug_assert_canary_debug
wsl.exe --exec /mnt/d/tmp/zr_vm/generation-ndebug-check/ndebug_assert_canary_debug
wsl.exe --exec env TMPDIR=/mnt/d/tmp/zr_vm/generation-ndebug-check gcc -DNDEBUG -std=c11 -g -O0 /mnt/d/tmp/zr_vm/generation-ndebug-check/ndebug_assert_canary.c -o /mnt/d/tmp/zr_vm/generation-ndebug-check/ndebug_assert_canary_ndebug
wsl.exe --exec /mnt/d/tmp/zr_vm/generation-ndebug-check/ndebug_assert_canary_ndebug
```

Both compiles exited 0. The Debug run exited 0. The `-DNDEBUG` run exited 1
with `NDEBUG canary: assert skipped setup; exited before Deinit`. The canary
logs were named
`canary-debug-compile.log`, `canary-debug-run.log`,
`canary-ndebug-compile.log`, and `canary-ndebug-run.log` in the same D folder.
After verification, root removed all 17 files (190,785 bytes) and deleted the
empty `D:/tmp/zr_vm/generation-ndebug-check` directory. The commands, exit
statuses, and metrics are retained in this acceptance record.

## Acceptance decision

The race and bounded-scheduling old-code RED are recorded, and the same-lock
snapshot repair passes the current-source Debug/NDEBUG standalone GCC harnesses,
the 500-round GCC harness, and final-source MSVC target/direct/CTest. The GCC
CMake cache was not rerun; the regenerated MSVC CMake target and registered
CTest passed. TSan cannot run in this WSL environment due to its runtime
memory-map failure, so no sanitizer race verdict is claimed.
