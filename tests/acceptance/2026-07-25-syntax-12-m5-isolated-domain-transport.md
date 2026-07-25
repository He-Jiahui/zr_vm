---
related_code:
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
plan_sources:
  - docs/plans/syntax/12-async-task-job-scheduler/m5-isolated-domain-transport-implementation-plan.md
  - docs/plans/syntax/12-async-task-job-scheduler/m5-isolated-domain-transport.md
tests:
  - tests/thread/test_thread_runtime.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
doc_type: acceptance-record
---

# Syntax 12 M5 Acceptance: IsolatedDomain Transport

## Scope

This record accepts the M5 `zr.thread.ThreadScheduler` IsolatedDomain
transport on `4cecdf7943ad68e123737167a2cf521c9bda14ca` plus the M5 exact
overlay. Validation used `.codex/s12m5-isolated-snapshot`; it contains only
the M5 source/test overlay and does not consume the shared worktree's unrelated
Syntax or LSP changes.

The accepted scope is host-only execution policy selection, caller-side
bounded provider FIFO, worker `GcDomain` publication before request visibility,
independent request/capture/result ownership envelopes, artifact reload,
caller-side completion commit, quota/fault/shutdown closure, and no
worker-domain object or native pointer retained by the caller queue.

## ThreadScheduler Results

Every toolchain built `zr_vm_thread_runtime_test` successfully. The ten M5
assertions passed in each final run: zero capture, scalar capture, structured
clone capture/result, multi-request completion, shared FIFO, quota fault,
forbidden resource fault, later shutdown, and queued shutdown.

| Toolchain | Build exit | Test process exit | M5 assertions | Target summary |
|---|---:|---:|---|---|
| GCC / WSL | 0 | 9 | 10/10 pass | 37 tests, 9 failures |
| Clang 14 / WSL | 0 | 9 | 10/10 pass | 37 tests, 9 failures |
| MSVC 19.44 / x64 Debug | 0 | 9 | 10/10 pass | 37 tests, 9 failures |

Exit `9` is not treated as a full target pass. Every final toolchain run has
the same pre-existing failures: local/explicit TaskRunner inference and
start, Channel/Transfer round trip, and Shared/WeakShared isolate round trips.
They predate this M5 overlay and do not exercise the M5 provider assertions.

## Canonical Transfer Results

The thread provider delegates ownership lifecycle semantics to the canonical
transfer implementation. The same snapshot verifies both lower layers on all
three toolchains.

| Target | GCC | Clang | MSVC |
|---|---|---|---|
| `zr_vm_resource_cross_domain_transfer_test` | 24/24, exit 0 | 24/24, exit 0 | 24/24, exit 0 |
| `zr_vm_resource_cross_domain_transfer_race_test` | 5/5, exit 0 | 5/5, exit 0 | 5/5, exit 0 |

These tests cover value-copy, structured clone, immutable/resource provider
operations, prepare/commit/decode/allocation failure terminal cleanup, and
queued/claimed claim-abort races. At the scheduler boundary, no layout provider
metadata is available through `SZrTypeValue`; ResourceMove and ImmutableHandle
therefore become a canonical `FORBIDDEN` contract and fault the consumed Job's
Task instead of guessing from a type name, value category, or pointer.

## Evidence

- `.codex/logs/s12m5-refactor-gcc-r2-build.log`
- `.codex/logs/s12m5-refactor-gcc-r2-test.log`
- `.codex/logs/s12m5-refactor-clang-r2-build.log`
- `.codex/logs/s12m5-refactor-clang-r2-test.log`
- `.codex/logs/s12m5-refactor-msvc-r2-build.log`
- `.codex/logs/s12m5-refactor-msvc-r4-test.log`
- `.codex/logs/s12m5-refactor-*-r2-transfer-test.log`
- `.codex/logs/s12m5-refactor-*-r2-race-test.log`

The first post-refactor MSVC test attempt exited `0xC0000005` and was rejected
as evidence. Two subsequent serial executions (`r3` and `r4`) both produced
the final `37 tests / 9 baseline failures` result above; `r4` is the accepted
MSVC thread evidence.

## Outcome

M5 is accepted for its IsolatedDomain transport scope. The source contract is
unchanged: `ThreadScheduler.schedule(Job<T>): Task<T>` remains the only public
surface. Future scheduler support that projects canonical type-layout provider
metadata into a transport call boundary is separate from this milestone; it
must retain the same envelope state machine and may not introduce value-class
or text-based fallbacks.
