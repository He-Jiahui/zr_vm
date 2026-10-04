---
related_code:
  - zr_vm_core/src/zr_vm_core/log.c
  - zr_vm_core/include/zr_vm_core/log.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/log.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
doc_type: module-reference
status: finite-native-compile-validated
---

# Log synchronization

`log.c` serializes the default output sink and the installed log callback with
a process-local recursive lock. A callback may write another log message on the
same thread. The public log API and callback payload are declared in `log.h`.

## Windows initialization

The Windows implementation uses a static `INIT_ONCE` and `CRITICAL_SECTION`.
`zr_log_lock` calls `InitOnceExecuteOnce` before entering the critical section;
`zr_log_unlock` leaves that same section. The initializer creates the critical
section once and returns `TRUE`.

The initializer has the SDK's `PINIT_ONCE_FN` signature:
`BOOL CALLBACK (PINIT_ONCE, PVOID, PVOID *)`. Its third argument is an optional
output pointer for the initialization context. This module does not publish an
initialization context, so the callback ignores all three arguments and callers
pass null context pointers. Using `PVOID` for the third argument is a different
function pointer type and is rejected by Clang when passed to
`InitOnceExecuteOnce`.

The POSIX implementation uses `pthread_once` and a recursive `pthread_mutex_t`.
Both paths keep initialization local to the logger.

## Finite validation

The actual source validation build found the Windows signature mismatch in
`source-direct-red-v35`. Configuration, including the real Clang executable ABI
probe, succeeded; the native build then failed at `log.c:41`. This is a compiler
failure before the source comparison tests could run, rather than a comparison
semantic RED. The original receipt and compiler log remain unchanged.

The repair changes only the third initializer parameter to `PVOID *`. A focused
compile of the actual file with the source target's Debug, UBSan and assertion
settings passed in `log-initonce-v41`. The single compiler job exited naturally
with code 0 and an empty Job, without cleanup actions. All 346 actual MD
dependencies matched their recorded input hashes; tool, resource and compile
context hashes were stable. The receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/log-initonce-v41/Root-receipt.json`
(86243 bytes, SHA256
`cc5df51c49f2d1a99184a8755a1a27c8a2610f07b699a60cca552976a6a27964`).
This gate establishes SDK callback type compatibility; it
does not establish multithreaded logger or callback reentry behavior.
