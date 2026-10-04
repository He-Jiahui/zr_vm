---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - tests/core/test_ssa_backend_service.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
tests:
  - tests/core/test_ssa_backend_service.c
  - tests/acceptance/ssa-backend-service.md
plan_sources:
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - "user: 2026-09-14 SSA 10.01 backend service contract implementation"
doc_type: testing-guide
---

# SSA 10.01 Backend Service

## Scope

This isolated acceptance covers the core C ABI for backend registration,
immutable asynchronous compile jobs, generation-scoped invalidation,
interpreter resume, code/map ownership, leases, and shutdown.  It exercises a
C-only mock backend and does not claim that a concrete LLVM/JIT, ExecBC, or
AOT adapter is present.  The current CMake registration compiles `execution_backend.c` and
`execution_code_handle.c` into the isolated `zr_vm_ssa_backend_service_test`
target and registers CTest name `ssa_backend_service`.

## Baseline

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

## Tooling evidence

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

## Results

All focused assertions pass on the direct Windows GCC, WSL GCC+ASan/UBSan,
and WSL Clang runs.  The implementation now keeps callback invocation outside
the service lock, rejects generation/contract/hash mismatches, disposes stale
and duplicate results, and preserves registrations while active retired code
leases remain.  No failures remain in the isolated test.

## Acceptance decision

Accepted for the isolated 10.01 core contract with the stated scope. The
current CMake configuration registers the focused target and CTest entry.
Before the milestone can be called complete, build
`zr_vm_ssa_backend_service_test`, run the named CTest test, and exercise
concrete AOT/ExecBC/JIT adapters plus the full repository matrix.
