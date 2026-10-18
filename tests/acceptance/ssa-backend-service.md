---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - tests/core/test_ssa_backend_service.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - tests/core/test_ssa_backend_admission.c
  - tests/cmake/ssa-backend-tests.cmake
  - tests/cmake/ssa-backend-admission/CMakeLists.txt
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
tests:
  - tests/core/test_ssa_backend_service.c
  - tests/acceptance/ssa-backend-service.md
  - tests/core/test_ssa_backend_admission.c
  - docs/testing-and-validation/ssa-backend-admission.md
plan_sources:
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - "user: 2026-09-14 SSA 10.01 backend service contract implementation"
  - "user: 2026-10-18 backend owner admission, module split and precise generation diagnostics"
doc_type: testing-guide
---

# SSA 10.01 Backend Service

## Scope

This isolated acceptance covers the core C ABI for backend registration,
immutable asynchronous compile jobs, generation-scoped invalidation,
interpreter resume, code/map ownership, leases, and shutdown.  It exercises a
C-only mock backend and does not claim that a concrete LLVM/JIT, ExecBC, or
AOT adapter is present. The commands and tool results below are historical records
from the original implementation revision, not a current runnable source list.
Current shared CMake routing and the 2026-10-18 finite admission evidence are
listed in the final section and the linked admission guide.

## Historical baseline (2026-09-14)

The first RED test compile intentionally preceded the new header and failed
with:

```text
fatal error: zr_vm_core/execution_backend.h: No such file or directory
```

After the header and implementation were added, an intermediate build exposed
missing code-handle definitions, an incomplete resume fixture, and strict
warning failures; those were fixed before the runs below.  No repository-wide
baseline is asserted here because sibling SSA tasks are concurrently changing
the shared worktree.  Existing unrelated failures, if any, remain the
responsibility of the parent integration acceptance.

## Test inventory

The focused executable runs these cases from
`tests/core/test_ssa_backend_service.c`:

- queueing proves `CompileAsync` does not call the compiler callback; pending
  and synchronous completion cover both worker outcomes;
- `Queued -> Compiling -> Ready -> Published` transitions and ticket/code
  separation;
- immutable-input rejection preserves source and instruction IDs;
- unsupported operation reports explicit ExecBC fallback;
- cancellation/reload invalidation disposes stale results exactly once;
- invalidation tombstones prevent a new compile for a reclaimed generation;
- duplicate code identity is rejected and unpublished code is unregistered
  before retirement;
- two requests sharing module and generation but using different domains:
  invalidating the first still permits acquisition of the second; this
  fixture does not independently vary module or backend registration identity;
- entry/map lookup and independent dependency-lease balancing;
- active execution leases delay map unregister/code retirement;
- shutdown cancels an in-flight job, waits for active leases, and destroys the
  backend only after final collection;
- interpreter-resume callback dispatch, a non-null logical request, and
  propagation of the callback's sourceId diagnostic sentinel; request field
  contents and actual interpreter execution are not checked.

The fixture passes null service/handle pointers to AcquireDependencyLease,
ReleaseDependencyLease and ReleaseCode. Other negative cases remove the
immutable-input flag, request an unsupported operation or target, submit a
cancelled completion, retry an invalidated generation, reuse a code identity,
and finalize shutdown while an execution lease remains. Repeated collection
occurs before and after lease release. These cases do not establish arbitrary
null-output coverage or repeated successful finalization behavior.  Concrete OOM injection is not applicable:
the service deliberately uses caller-owned fixed-capacity arrays and reports
`CAPACITY` before any hidden allocation.

## Historical tooling evidence (2026-09-14)

The following commands, including old `/tmp` and `$env:TEMP` output locations,
are retained verbatim as evidence of that revision. They omit the later
compile/facade modules and must not be copied as current build instructions.
Current build products belong under the configured E:/cargo-targets task root.


Tool versions observed:

- Windows MinGW GCC 4.8.3 (`gcc --version` from the configured toolchain);
- WSL Ubuntu GCC 11.4.0;
- WSL Ubuntu Clang 14.0.0.
- WSL Valgrind 3.18.1 (Memcheck and Helgrind).

Direct Windows warning-clean build and run:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -pedantic `
  -I zr_vm_core/include -I zr_vm_common/include `
  tests/core/test_ssa_backend_service.c `
  zr_vm_core/src/zr_vm_core/execution/execution_backend.c `
  zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c `
  -o $env:TEMP/ssa_backend_service.exe
& $env:TEMP/ssa_backend_service.exe
```

Observed result: compile succeeded and the executable exited `0` with no
assertion output.

WSL GCC AddressSanitizer/UndefinedBehaviorSanitizer run:

```bash
cd /mnt/e/Git/zr_vm
gcc -std=c11 -Wall -Wextra -Werror -pedantic -g -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/core/test_ssa_backend_service.c \
  zr_vm_core/src/zr_vm_core/execution/execution_backend.c \
  zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c \
  -o /tmp/ssa_backend_service_asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 /tmp/ssa_backend_service_asan
```

Observed result: exit `0`; no ASan, UBSan, or leak report.

WSL Clang warning-clean run:

```bash
cd /mnt/e/Git/zr_vm
clang -std=c11 -Wall -Wextra -Werror -pedantic \
  -I zr_vm_core/include -I zr_vm_common/include \
  tests/core/test_ssa_backend_service.c \
  zr_vm_core/src/zr_vm_core/execution/execution_backend.c \
  zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c \
  -o /tmp/ssa_backend_service_clang
/tmp/ssa_backend_service_clang
```

Observed result: Ubuntu Clang 14.0.0 compiled and the executable exited `0`.

WSL ownership and lock checks:

```bash
cd /mnt/e/Git/zr_vm
valgrind --tool=memcheck --leak-check=full --error-exitcode=99 \
  /tmp/ssa_backend_service_valgrind_20260914
valgrind --tool=helgrind --error-exitcode=99 \
  /tmp/ssa_backend_service_helgrind_20260914
```

Observed results: Memcheck reported `0 bytes in 0 blocks` at exit and
`ERROR SUMMARY: 0 errors`; Helgrind reported `ERROR SUMMARY: 0 errors`.
The fixture is single-threaded, so these runs verify lock instrumentation and
cleanup paths but are not a substitute for a multi-worker stress test.

The CMake target/CTest commands from the plan were not run in this subtask;
editing `tests/cmake/ssa-tests.cmake` and any umbrella/index is explicitly
reserved for the parent integration task.

## Historical results

At that revision, all focused assertions passed on the direct Windows GCC, WSL GCC+ASan/UBSan,
and WSL Clang runs.  That implementation kept callback invocation outside
the service lock, rejects generation/contract/hash mismatches, disposes stale
and duplicate results, and preserves registrations while active retired code
leases remain.  No failures remained in that isolated test run; this is not a current-source acceptance claim.

## Historical acceptance decision

Accepted for the isolated 10.01 core contract with the stated scope. The
then-recorded CMake configuration registered the focused target and CTest entry.
Before the milestone can be called complete, build
`zr_vm_ssa_backend_service_test`, run the named CTest test, and exercise
concrete AOT/ExecBC/JIT adapters plus the full repository matrix.


## Current source route and finite admission evidence (2026-10-18)

`tests/cmake/ssa-backend-tests.cmake` now owns one shared explicit source list.
Each service/admission target links its own test plus these four production TUs:

- `execution_backend.c`: lifecycle, registration, cancellation, invalidation/status;
- `execution_backend_compile.c`: request/candidate/queue/ProcessNext/Complete/Publish/QueryTicket;
- `execution_backend_facade.c`: borrowed default pointer and short facade;
- `execution_code_handle.c`: code/maps, leases and retirement.

The normal test aggregation and standalone
`tests/cmake/ssa-backend-admission/CMakeLists.txt` use this same function.
Service has five compile roles; `/UNDEBUG` or `-UNDEBUG` must occur after any
NDEBUG definition because many real API calls are inside assert. Admission has
574 lines, 33 cases and always-active CHECK. Its current owner policy admits
execution schema6/logical1, nonzero ABI matched to the selected registration
(positive ABI23, not a global ABI17 gate), and zero descriptor/target/request/result
reserved fields. The generic equality checker stays separate.

The first actual10-case RED had2PASS/8FAIL from missing rejections. Expanded
actual33-case RED had32PASS/1FAIL from independent contract.generation diagnostic
1/1 instead of1/2, with clean disposal and service7 PASS. A post-run helper regex
failure left that original receipt false; offline reconciliation accepted the
native RED without overwriting it. The minimal generation diagnostic correction
leaves status, locks, cleanup, counters and the test oracle intact.

Root's current actual GREEN returned0/0/0/0, both CTests Passed and admission33
cases passed;50 inputs stayed unchanged, service assert activation and process/
handle closure were observed. It reused the prior10-role build and recompiled
only two compile.c roles plus two links. Independent GREEN review accepted
33 admission cases, seven service scenarios, exact cache reuse and the offline
header overlay (SHA256 221A4D88BF309C2F11566A992B054E98B849D5F7229D651AC5E6ABDBE93E483D).
This grants no full-repository or concurrent-provider credit. Full raw/receipt provenance and exact test coverage are in the
[current admission guide](../../docs/testing-and-validation/ssa-backend-admission.md).

The existing target-query/worker ordering, Complete disposal handoff, borrowed
default pointer lifetime and Resume/Finalize concurrency TODOs remain open.
Historical single-threaded Memcheck/Helgrind and current serial mocks do not
close them.

## Current MSVC syntax-only observation

Six unique C sources (two fixtures and four production TUs) passed /Zs /W4 /WX /UNDEBUG /showIncludes with natural exit0,50 inputs unchanged and no output files. Independent syntax review 394EB90F40205BDE63D56C9AC1061B71317B7E6D36D12B65A5EEE7D2E8D0C00D accepted this finite result. The actual local dependency union is38 headers/def; eight additional headers are protected by the earlier sealed Source41 baseline and a matching-current offline overlay, making58 protected inputs without changing the historical50 runtime guards. This is syntax compatibility only, not MSVC linking/runtime, Linux or a full matrix.
