---
date: 2026-10-01
status: focused-passed
plan_sources:
  - docs/plans/ssa/06-gc-domain/03-domain-sharing.md
  - docs/plans/ssa/06-gc-domain/04-cross-domain-clone.md
---

# Local Throw GC Scope Recovery

## Failure Evidence

The real protected target field-pair OOM probe first reported MEMORY_ERROR,
mutation depth 0 to 1 and target root count 4 to 8. The original log is
`D:/tmp/zr_vm/ssa-control/clone-real-red-ctest.log`.

The independent pre-fix depth target passed normal return and failed seven of
eight cases: empty entry retained two recursive mutation levels, and outer
execution/native depths were reset to zero. The full pre-fix target also
detected a blocked second mutator within its bounded wait and exited before
unsafe cleanup. Logs are `exception-scopes-depths-red.log` and
`exception-scopes-lock-red.log` under the same control directory.

## Current Native Results

MSVC 19.44 C11 Debug passed all ten new cases: empty entry, outer execution,
GC-aware/detached/critical native modes, outer mutation, normal return, nested
TryRun, retained outer lock and another mutator acquiring the released lock.
`exception-scopes-final-direct.log` preserves the entry/exit depths and all
ten passing Unity results.

The five registered suites passed 5/5 in 27.35 seconds: `exception_gc_scopes`,
`gc_nested_mutation`, `aot_gc_root_frame`, `ssa_cross_domain_clone` and
`close_meta_exception`. The direct concurrent-major suite passed 14/14.
Logs are `exception-scopes-fixed-ctest.log` and
`exception-scopes-major-direct.log` under `D:/tmp/zr_vm/ssa-control`.

The first native build compiled changed sources but reported LNK1236 for the
exception object while linking. Dumpbin successfully read that object's COFF
headers. An unchanged incremental relink completed all remaining links, and
functional tests then passed. The failed build, object headers and successful
retry are retained as `exception-scopes-fixed-build.log`,
`exception-object-headers-final.log` and `exception-scopes-link-retry.log`.
This transient link error is not counted as a functional RED.

GCC 11.4 and Clang 14 each completed all 44 build steps, passed the five
registered suites (25.14 and 9.26 seconds total), and passed all 14 direct
concurrent-major cases. The new scope target passed ten cases on both.
Preserved logs are
`{gcc,clang}-exception-scopes-final-{build,ctest,last-test,major-direct}.log`
under the same control directory. No sanitizer or forced-C++ result is claimed.

## Remaining Clone Gate

After scope restoration, the protected clone OOM passes its mutation-depth
check and fails the remaining target root balance check, expected 4 and
observed 8. The log is `clone-roots-after-scope-red.log`. Clone transaction
cleanup and retry acceptance remain a separate subtask; this scope change does
not claim clone failure atomicity or full SSA 06 acceptance.

All reusable caches, compiler temporaries and logs are under `D:/tmp/zr_vm`.
No build artifact is moved between volumes.
