# SSA 06.05 async frame budget acceptance

## Scope

This acceptance record covers the standalone frame-safe scheduling contract
from `docs/plans/ssa/06-gc-domain/05-async-frame-budget.md`:

- core async frame budget/pin/cancellation/teardown state;
- non-blocking waiter registration, recheck, wake/cancel/timeout arbitration,
  and exactly-once resume;
- immutable background compile snapshots, cancellation, warm-up metadata, and
  generation/contract stale-result rejection.
- running-job cancellation keeps the worker snapshot leased until `Complete`
  acknowledges it; `Release` is rejected while the job remains `RUNNING`.

The concrete VM task frame, existing execution budget, scheduler, and CMake
registration are integration owners outside this scoped change.

## Baseline

Before this change there was no public standalone async wait/compile queue
contract or focused `tests/task/test_ssa_async_frame_budget.c`.  The existing
`task_frame_runtime` and `execution_budget` APIs remain in the worktree and are
not replaced here.  The repository has unrelated concurrent edits and its
known full-test baseline; this record reports only the isolated target.

## Test inventory

`tests/task/test_ssa_async_frame_budget.c` covers:

1. budget exhaustion is pending away from a boundary and suspends only at a
   matching state-map boundary;
2. borrow/stack-alias rejection, pin/unpin balance, cancellation, and
   idempotent teardown;
3. wake in the registration/recheck window (no lost wakeup);
4. cancellation and timeout as single winners;
5. exactly-once continuation resume and release;
6. immutable snapshot copying and warm-up tagging;
7. stale generation disposal without publication;
8. malformed snapshot rejection.
9. running cancellation/acknowledgement lifecycle and strict request/queue
   identity validation.

Boundary/failure cases include zero/invalid identities, unsupported resource
guards, exhausted work at a non-boundary, cancelled jobs, stale generation,
contract hash mismatch, queue capacity, and repeated terminal operations.

## Tooling evidence

Commands were run from `E:\Git\zr_vm` (or `/mnt/e/Git/zr_vm` in WSL):

```text
# TDD RED (before implementation)
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/task/test_ssa_async_frame_budget.c
# observed: fatal error: zr_vm_core/async_frame_budget.h: No such file

# Windows GCC 4.8 standalone
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/task/test_ssa_async_frame_budget.c \
  zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c \
  zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c \
  -o %TEMP%\\ssa_async_frame_budget.exe
%TEMP%\\ssa_async_frame_budget.exe
# result: exit code 0

# WSL GCC 11.4 strict
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/task/test_ssa_async_frame_budget.c \
  zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c \
  zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c \
  -o /tmp/ssa_async_frame_budget && /tmp/ssa_async_frame_budget
# result: exit code 0

# WSL Clang 14 strict
clang -std=c11 -Wall -Wextra -Werror -pedantic \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/task/test_ssa_async_frame_budget.c \
  zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c \
  zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c \
  -o /tmp/ssa_async_frame_budget_clang && /tmp/ssa_async_frame_budget_clang
# result: exit code 0

# WSL GCC ASan/UBSan
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/task/test_ssa_async_frame_budget.c \
  zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c \
  zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c \
  -o /tmp/ssa_async_frame_budget_asan && \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 /tmp/ssa_async_frame_budget_asan
# result: exit code 0

# WSL Clang ASan/UBSan
clang -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/task/test_ssa_async_frame_budget.c \
  zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c \
  zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c \
  -o /tmp/ssa_async_frame_budget_clang_asan && \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 /tmp/ssa_async_frame_budget_clang_asan
# result: exit code 0
```

The Windows GCC toolchain does not recognize the combined sanitizer option;
sanitizer evidence therefore comes from both WSL toolchains.  No debugger was
needed because the isolated state-machine tests completed without a crash.

## Results

All focused test functions pass under Windows GCC, WSL GCC, WSL Clang,
and both WSL sanitizer builds.  The RED compile failed for the intended
missing-contract reason, then the GREEN runs passed.  No full CMake/CTest run
was claimed because the parent agent owns shared target registration and the
worktree contains unrelated in-progress changes.

## Acceptance decision

**Accepted for scoped handoff; integration pending.** The new contract and
focused test are self-contained and sanitizer-clean.  Parent integration must
register the two core sources and the focused test in the planned
`ssa_async_frame_budget` target, then run the repository's normal GCC/Clang and
MSVC matrices.  Concrete scheduler wiring, real state-map materialization, and
frame-thread performance/p99 measurements remain follow-up work under the
milestone plan.
