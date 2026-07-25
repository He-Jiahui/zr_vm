# Syntax 12 M4 Acceptance: AttachedDomain ThreadScheduler

## Scope

This record accepts the M4 `zr.thread.ThreadScheduler` AttachedDomain provider
on top of commit `9822600a89391f3803960864c7d8332c0c10616a` plus the M4 exact
source overlay. The validation snapshot was
`.codex/s12m4-dev-snapshot-r1`; its 13 source/test paths matched the working
tree by SHA-256 before the final replay.

The accepted source surface is:

- canonical ThreadScheduler/Send/Sync protocol metadata;
- caller-GcDomain mutator launch and AttachedDomain request queue;
- canonical prepared Job to caller Task provider bridge;
- protocol-id based Send/Sync generic constraints;
- resolved schedule-role ownership move facts; and
- focused ThreadScheduler runtime tests.

## Focused Regression Results

All three isolated toolchains built `zr_vm_thread_runtime_test` successfully.
The following M4 assertions passed in every run:

1. canonical ThreadScheduler descriptor contract;
2. worker state attachment to the caller GcDomain;
3. one Job consumed and completed through `ThreadScheduler(1)`;
4. two Jobs drained in submission order by one worker;
5. non-Send scheduler result rejected through the generic protocol constraint;
6. a consumed Job rejected on second submission.

| Toolchain | Build exit | Test process exit | M4 assertions | Target summary |
|---|---:|---:|---|---|
| GCC / WSL | 0 | 9 | 6/6 pass | 27 tests, 9 failures |
| Clang 14 / WSL | 0 | 9 | 6/6 pass | 27 tests, 9 failures |
| MSVC 19.44 / x64 Debug | 0 | 9 | 6/6 pass | 27 tests, 9 failures |

The exit code `9` is not treated as a pass. Each toolchain reports the same
pre-existing legacy failures: local/explicit TaskRunner inference and start,
manual coroutine pump, Channel/Transfer/Shared/WeakShared isolate round trips.
None exercises the M4 ThreadScheduler provider; the six M4 assertions above
all pass after those failures are reported.

## Lower-Layer Baseline

The provider bridge also rebuilt `zr_vm_task_runtime_test` under GCC. The M4
snapshot reports `54 Tests 6 Failures 0 Ignored`. Replaying the same target in
the committed M3 snapshot reports the identical six failure names and count:

- `test_zr_task_and_zr_coroutine_register_new_public_shapes`;
- `test_borrowed_value_used_before_await_still_compiles`;
- two legacy TaskRunner start/await cases;
- manual coroutine scheduler pump; and
- legacy default scheduler property assignment.

This comparison establishes that M4 did not expand the task-runtime baseline.

## Evidence

- `.codex/logs/s12m4-dev-gcc-thread-build.log`
- `.codex/logs/s12m4-dev-gcc-thread-test.log`
- `.codex/logs/s12m4-dev-clang-thread-build-final.log`
- `.codex/logs/s12m4-dev-clang-thread-test-final.log`
- `.codex/logs/s12m4-dev-msvc-thread-build.log`
- `.codex/logs/s12m4-dev-msvc-thread-test-final.log`
- `.codex/logs/s12m3-baseline-gcc-task-test.log`
- `.codex/logs/s12m4-dev-gcc-task-test.log`

## Outcome

M4 is accepted for its AttachedDomain ThreadScheduler scope. Cross-domain
transport, transfer envelopes, provider quota, and scheduler shutdown remain
outside this exact milestone as stated by its implementation plan.
