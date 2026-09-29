---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
implementation_files:
  - tests/library/test_ssa_rollback_restricted.c
tests:
  - tests/library/test_ssa_rollback_restricted.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/04-rollback-restricted.md
doc_type: acceptance
status: verified
---

# Rollback restricted test under NDEBUG

## Scope

This change fixes test execution reliability only. The test no longer places
manager initialization, Apply, Rollback, or restricted-profile validation
inside `assert`. Each API is called directly, its status is checked with an
always-active `TEST_CHECK`, and failed initialization returns before manager
deinitialization. No production source or CMake registration changed.

## Test-first evidence

The original test called `GenerationManager_Init`, `ApplyValidated`, Rollback,
and profile validation from `assert` expressions. With `NDEBUG`, the assert
macro removes each expression, including the calls, while the final manager
deinitialization remains. Relying on the resulting uninitialized atomic state
to crash would be nondeterministic.

A temporary canary counted evaluation of the initialization assert and
returned before deinitialization if the call was omitted. The test translation
unit plus its five linked implementation sources was compiled with GCC
`-DNDEBUG`, with output in `D:/tmp/zr_vm/rollback-ndebug-red`. The binary exited
1 with:

```text
NDEBUG removed generation manager initialization
```

The RED compiler command was:

```text
gcc -std=c11 -DNDEBUG \
  -I /mnt/e/Git/zr_vm/zr_vm_core/include \
  -I /mnt/e/Git/zr_vm/zr_vm_common/include \
  /mnt/e/Git/zr_vm/tests/library/test_ssa_rollback_restricted.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/artifact_exec_ir.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_profile.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c \
  -o /mnt/d/tmp/zr_vm/rollback-ndebug-red/rollback_ndebug_canary
```

The canary was then removed and the original test body was converted to
explicit calls plus `TEST_CHECK` assertions.

## Verification

All compiler and build outputs stayed on D. The registered Debug target was
built and run from `/mnt/d/tmp/zr_vm/close-proxy-core-red`:

```text
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_rollback_restricted_test -j 4
```

Build exit code: 0. Direct Debug binary exit code: 0. Registered CTest
`ssa_rollback_restricted`: 1/1 passed.

A temporary D-only CMake harness compiled the same six source files as separate
Debug and NDEBUG executables (NDEBUG was defined only for the latter). Both
direct binaries exited 0 and its CTest run passed 2/2. The final NDEBUG binary
was also compiled directly with:

```text
gcc -std=c11 -DNDEBUG \
  -I /mnt/e/Git/zr_vm/zr_vm_core/include \
  -I /mnt/e/Git/zr_vm/zr_vm_common/include \
  /mnt/e/Git/zr_vm/tests/library/test_ssa_rollback_restricted.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/artifact_exec_ir.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_profile.c \
  /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c \
  -o /mnt/d/tmp/zr_vm/rollback-ndebug-check/rollback_ndebug_direct
```

The resulting direct binary exited 0. The temporary canary and check directories
were removed after verification from their exact D paths; no persistent cache
was deleted. `tests/library/test_ssa_rollback_restricted.c` contains no
`assert` calls after the fix.
