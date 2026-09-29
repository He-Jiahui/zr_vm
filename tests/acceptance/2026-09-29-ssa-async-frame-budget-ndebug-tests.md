# SSA Async Frame Budget Test Checks Under NDEBUG

## Scope

This acceptance covers the standalone test harness
`tests/task/test_ssa_async_frame_budget.c`. It ensures frame-budget, wait
registry, and compile-queue state operations and their expectations remain
active when the C compiler defines `NDEBUG`. No production runtime behavior or
the wider 06.05 milestone is changed here.

## Baseline

The test used the C `assert` macro for both checks and state-changing calls such
as `AsyncWaitRegistry_Init`, `CompileQueue_Init`, `QueueCompilation`, and
`ClaimNext`, while calling `Deinit` unconditionally afterward. With `NDEBUG`,
the compiler removes the entire assert expression, so setup and transitions
vanish but cleanup remains. Running the original test in this configuration
could deinitialize an uninitialized stack queue; that unsafe run was not
attempted.

A safe pre-change canary modeled initialization with a counter and guarded its
cleanup counter with `if (initialized)`. It used no queue and never called
`Deinit` on an uninitialized object. GCC Debug printed
`init_calls=1 initialized=1 deinit_calls=1`; GCC `-DNDEBUG` printed
`init_calls=0 initialized=0 deinit_calls=0`.

Before editing the test, the existing focused test compiled and ran in strict
GCC Debug mode with exit code 0.

## Test Inventory

The focused executable runs 11 existing test functions covering:

1. frame budget suspension only at a matching state-map boundary;
2. borrow rejection, pin balance, cancellation, and idempotent teardown;
3. wait suspension, resume, and completion gates;
4. cancellation requested before frame start;
5. wake between wait registration and recheck, followed by one resume;
6. cancel and timeout as single terminal winners;
7. rejection of a wait that crosses a stack alias;
8. immutable compile snapshot copying and stale-generation disposal;
9. malformed snapshot rejection and queued cancellation;
10. running-job cancellation with the worker snapshot retained until
    `Complete` acknowledges it;
11. queue and output-handle identity rejection.

`TEST_CHECK` always evaluates its condition, records a failure, and jumps to the
test cleanup path. Initialization results are saved before being checked.
Wait handles, queue handles, and the claimed worker snapshot lease have
explicit state flags. Cleanup releases active handles and only calls registry
or queue `Deinit` after successful initialization and after handle/worker
ownership has ended.

## Tooling Evidence

Tool: Windows GCC 4.8.3 (`D:\Tools\development\c\bin\gcc.exe`). Every
temporary source and binary was placed in
`D:\tmp\zr_vm\async-frame-ndebug-check`; no CMake cache or repository build
directory was used. After the outputs were recorded below, each temporary
file was removed individually and the empty scratch directory was removed.

Safe pre-change canary, compiled and run before the test edit:

```text
cmd.exe /d /c "D:\Tools\development\c\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -pedantic D:\tmp\zr_vm\async-frame-ndebug-check\assert-side-effect-canary.c -o D:\tmp\zr_vm\async-frame-ndebug-check\assert-side-effect-canary-debug.exe"
cmd.exe /d /c D:\tmp\zr_vm\async-frame-ndebug-check\assert-side-effect-canary-debug.exe
# output: init_calls=1 initialized=1 deinit_calls=1; exit 0

cmd.exe /d /c "D:\Tools\development\c\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -pedantic -DNDEBUG D:\tmp\zr_vm\async-frame-ndebug-check\assert-side-effect-canary.c -o D:\tmp\zr_vm\async-frame-ndebug-check\assert-side-effect-canary-ndebug.exe"
cmd.exe /d /c D:\tmp\zr_vm\async-frame-ndebug-check\assert-side-effect-canary-ndebug.exe
# output: init_calls=0 initialized=0 deinit_calls=0; exit 0
```

The original source's strict Debug build and direct run both exited 0. The
final source was then compiled directly with the two runtime implementation
files and executed in both configurations:

```text
cmd.exe /d /c "D:\Tools\development\c\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g -I E:\Git\zr_vm\zr_vm_core\include -I E:\Git\zr_vm\zr_vm_common\include E:\Git\zr_vm\tests\task\test_ssa_async_frame_budget.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c -o D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-debug.exe"
cmd.exe /d /c D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-debug.exe
# build exit 0; direct run exit 0

cmd.exe /d /c "D:\Tools\development\c\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -pedantic -O2 -DNDEBUG -I E:\Git\zr_vm\zr_vm_core\include -I E:\Git\zr_vm\zr_vm_common\include E:\Git\zr_vm\tests\task\test_ssa_async_frame_budget.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c -o D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-ndebug.exe"
cmd.exe /d /c D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-ndebug.exe
# build exit 0; direct run exit 0
```

Two temporary source mutations exercised the failure cleanup paths with
`NDEBUG` still defined. The first sets the stale-snapshot test's queue capacity
to zero; the second changes the copied-snapshot expectation from byte 1 to byte
2 while the simulated worker holds the snapshot. Both mutated executables
compiled successfully, then returned the expected exit code 1 after exactly
one failed `TEST_CHECK`. The init probe reported only `queueInitialized`, with
no call through the init-guarded `Deinit`. The worker probe reported only the
snapshot check; cleanup acknowledged the worker and released the handle
without an additional cleanup failure.

```text
cmd.exe /d /c "D:\Tools\development\c\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -pedantic -O2 -DNDEBUG -I E:\Git\zr_vm\zr_vm_core\include -I E:\Git\zr_vm\zr_vm_common\include D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-init-failure.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c -o D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-init-failure.exe"
cmd.exe /d /c D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-init-failure.exe
# build exit 0; run exit 1; only failure: queueInitialized

cmd.exe /d /c "D:\Tools\development\c\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -pedantic -O2 -DNDEBUG -I E:\Git\zr_vm\zr_vm_core\include -I E:\Git\zr_vm\zr_vm_common\include D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-worker-failure.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_async_wait.c E:\Git\zr_vm\zr_vm_core\src\zr_vm_core\execution\execution_compile_queue.c -o D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-worker-failure.exe"
cmd.exe /d /c D:\tmp\zr_vm\async-frame-ndebug-check\ssa-async-frame-worker-failure.exe
# build exit 0; run exit 1; only failure: snapshot[0] == 2u
```

The `-DNDEBUG` build left `NDEBUG` defined. These are direct standalone
compiler runs, not a full CMake Release build. After source freeze, the
root-owned MSVC Debug focused target and registered CTest also passed:

```text
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && set TEMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp && set TMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_ssa_async_frame_budget_test -j 2'
# build exit 0; 4/4 build steps
D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_ssa_async_frame_budget_test.exe
# direct run exit 0; main invoked 11 source-level test functions; no stdout
ctest --test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc -R "^ssa_async_frame_budget$" --output-on-failure --no-tests=error
# 1/1 registered test passed; exit 0
```

## Results

The canary reproduced standard `assert` expression removal without touching an
uninitialized runtime object. The changed test replaces `assert` with an
always-evaluated check and guards partial setup and cleanup ownership. Strict
GCC Debug and `-DNDEBUG` builds both ran the final 11-test executable with exit
code 0. The final MSVC Debug executable also returned 0 for its 11 source-level
test functions, and the registered focused CTest passed 1/1. NDEBUG probes
confirmed failed queue initialization skips `Deinit` and a failed check during
an active worker path runs cleanup without secondary failures. The original
`NDEBUG` binary was deliberately not run.

## Acceptance Decision

**Accepted for this test-only reliability change.** Strict GCC Debug and
`-DNDEBUG` standalone runs passed, as did the MSVC Debug direct run and
registered focused CTest (1/1). No full CMake Release, Clang, sanitizer, or
production-runtime matrix is claimed.
