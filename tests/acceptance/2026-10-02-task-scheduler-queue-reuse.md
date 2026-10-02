---
related_code:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
plan_sources:
  - docs/plans/ssa/06-gc-domain/05-async-frame-budget.md
tests:
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
doc_type: acceptance-record
status: scoped-accepted-msvc
---

# Cooperative scheduler queue reuse

The fix detaches the exhausted private queue while retaining its old cursor,
then checks the cursor reset before publishing a replacement queue. Existing
queue/head pairs use the boolean core setter and readback. Scheduler allocation
and attach run under a native pin with protected exception recovery and unpin.
The approved change is limited to these two runtime files, these two test files,
`docs/library-and-builtins/zr-task-job-scheduler.md`, this acceptance file,
and the two root-owned CMake registration files described below.

## Reproduction and evidence

The first source function schedules a cold Job returning 7, reads its result,
and captures the completed Task with an explicit host GC root. A second source
function schedules a Job returning 9 in the same VM. The host inspects the old
Task status; a third source function reads the saved Task result again. This
uses the source compiler, local scheduler, and actual Task.result ABI.

The verified v3 RED linked the immutable pre-fix library: test build and link
exited 0, run exited 1, and the old Task status was FAULTED (5), expected
COMPLETED (4). The changed runtime v3 GREEN built/linked/ran with exit 0 and
passed all six tests then present. The earlier multi-Job single-function probe
reported an unrelated object/native-member failure; the direct returned-Task
v2 probe had a C access violation and is excluded as acceptance evidence.
Their logs are retained without treating crashes as the required RED.

Final verification uses the frozen six-file source manifest, the same nine
immutable libraries and harness objects, and the root-captured MSVC environment.
The task-local driver writes all response files, objects, PDBs, executables,
receipts and logs below:

`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/task-scheduler-queue-reuse`

Commands from `E:/Git/zr_vm`:

```powershell
python -Xutf8 D:/tmp/zr_vm/ssa-20261002-01a0fc3b/task-scheduler-queue-reuse/run-final.py
```

`run-final.py` freezes SHA-256 for the six approved files and linked inputs,
runs `run-msvc.py red-final-v12` and `run-msvc.py green-final-v12`, and verifies
that those hashes remained unchanged. MSVC 19.44 uses `/std:c11 /W4 /WX /Od
/Zi /MDd`, with the repository's Debug/platform/Unity definitions and includes.
The RED links the captured pre-fix `zr_vm_library.lib`; the GREEN additionally
links the freshly compiled `task_runtime.c` object. The cache receipt records
the SHA-256 of all nine captured libraries; current root-build artifacts are
not read during the focused gate.

The final RED builds and links successfully and runs ten tests with two
failures (run exit 2): both completed-result reuse and cooperative reuse see
the saved completed Job change to FAULTED (5) rather than COMPLETED (4).
The final GREEN builds, links and runs
successfully: **10 tests, 0 failures, 0 ignored**. Detailed exit codes and
commands are in `red-final-v12-msvc-receipt.json` and
`green-final-v12-msvc-receipt.json`; `final-verification.json` records source
and input hashes plus driver exit codes.

## Coverage and limits

| Case | Executed assertion |
| --- | --- |
| Completed Job queue reuse | 7 remains completed and readable after a new Job returns 9 |
| Reentrant scheduling | Outer source Job returns 13; native host submits a real captured cold Job through canonical ScheduleJob; inner Task is QUEUED during the outer call and later returns 19 |
| Cooperative queue reuse | Real source yieldNow and ABI delay(3) complete; previously completed Job remains completed |
| Checked field writes | Real read-only descriptors reject cursor reset, exhaustion detach and replacement attach; fields and member versions show only accepted writes |
| Successful replacement | Cursor becomes zero, queue attaches, both member mutations advance version; scheduler pin flags and ignored registration return to entry values |
| Queue allocation OOM | The actual queue object request and GC retry are rejected, producing MEMORY_ERROR; scheduler pin is released, queue remains null, and host recovery permits retry |

The field/OOM cases compile the exact private queue include into the test
translation unit and exercise real core setters/allocator/GC. They do not
export a product test hook. In the RED executable those private-helper checks
use the new helper, while the source regressions execute the old linked runtime;
only the latter are the pre-fix bug reproduction.

All new cases collect observations, release their explicitly acquired roots,
and destroy the VM before Unity assertions. OOM injection rejects exactly the
initial queue object and its retry, allowing Error normalization to allocate.
The host clears the current exception and restores FINE before testing retry.
An earlier persistent-allocation rejection also blocked Error normalization
and caused stack overflow; its log is retained as a discarded fixture, not a
runtime acceptance result.

The rejected nested `Task<Task<int>>` source variant failed compiler lowering;
a single-function variant later hit an invalid cached native-member callable.
Both are retained as scope limits; the accepted reentrant case uses scalar
Jobs and separate source entries with explicit host roots. No unrelated
compiler/runtime repair or legacy fallback was added. The worker gate above is focused
MSVC Debug with captured dependencies. Root additionally rebuilt the current
registered target below. Neither gate establishes WSL/GCC/Clang/Release or
sanitizer acceptance, or completion of SSA 06.05. The worker did not write
shared Ninja state.

## Formal registration, root verification and cleanup

The existing scheduler target had no CTest registration. Root executed
`ctest --test-dir <matrix/msvc> -R '^task_job_scheduler$' --output-on-failure
--no-tests=error -j 1`, which exited 8 with no tests found. It then added one
include in `tests/CMakeLists.txt` and a small
`tests/cmake/task-scheduler-queue-tests.cmake` module. The module registers
`task_job_scheduler` only when its existing executable target exists, with
labels `ssa;task` and a 120-second timeout. The final commit scope is eight
files; the six-file worker RED/GREEN remains the immutable earlier evidence.

Root built the current `zr_vm_task_job_scheduler_test` target successfully
(exit 0), then ran the registered CTest: 1/1 passed in 13.71 seconds, exit 0.
The formal output reports all ten Unity tests passing. Exact argv and outputs
are retained under the owned root's `control/` directory in
`task-job-scheduler-registration-red.log`, `task-job-scheduler-formal-build.log`
and `task-job-scheduler-formal-ctest.log`. Independent review verified the six
source hashes against final v12 and found no introduced actionable finding;
its report is `review/scheduler-v2/review.md` below the same task root.

The setter review applies to the canonical Scheduler's existing private
fields. Generic host-installed PROPERTY descriptors can invoke callbacks in
Core; no such descriptors exist on these canonical private fields. The
normalized scalar/null/object paths reviewed here preserve member versions,
cache updates and the actual GC barrier.

After final verification, 113 obsolete task-owned probe artifacts were removed,
releasing 352,389,009 logical bytes. Resolved D: boundaries, no-reparse checks,
exclusive file hashes and active executable checks were recorded in
`task-scheduler-queue-reuse/cleanup-discarded-receipt.json`. Final v12 binaries,
objects, immutable support caches and all logs/receipts/scripts remain.

The queue-reuse repair is scoped-accepted for current formal MSVC Debug.
Whole frame-budget/background-compilation integration and SSA 06.05 remain open.
