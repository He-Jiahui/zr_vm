---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - docs/instruction-generation/execir-model.md
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/exec_ir_clone_test_allocator.c
  - tests/parser/exec_ir_clone_test_allocator.h
  - tests/parser/exec_ir_clone_test_allocator_cases.inc
doc_type: acceptance
---

# ExecIR clone allocation rollback

## Scope

This slice covers the allocation-failure boundary in SSA 01.01. `CloneFunction`
and `CloneModule` build temporary deep copies and publish them only after every
owned side array succeeds. A failed copy must preserve the source and the
already populated destination, release every temporary allocation, and report
the correct distinction between malformed input, byte-size overflow, and
out-of-memory failure.

## Focused harness

The core-model test remains production-hook free. The CMake
`zr_vm_ssa_core_model_test` target and direct validation builds force-include
`exec_ir_clone_test_allocator.h` and link its support source; the remapping is
confined to that executable. The fixture populates all
function arrays plus GC-map, state-map, frame-layout, binding-row, module-array,
and two-function ownership paths. Snapshots retain object metadata, pointers,
counts, capacities, and copied pool bytes. Each ordinal is reset before
assertions and cleanup, and the live-allocation ledger must return to its
pre-call count.

The function fixture reaches 28 owned allocations. The module fixture reaches
60 allocations across module arrays and two complete functions. Every ordinal
is rejected, the pre-existing destination remains unchanged, and invalid input
is rejected before the first allocation. Native 32-bit builds also exercise
the byte-size overflow guard without fabricating an x64 overflow.

## RED and GREEN evidence

The pre-fix MSVC x86 run failed because an allocation failure was reported as
`INVALID_ARGUMENT`, a module function-array failure was reported as
`CAPACITY_OVERFLOW`, and a null constants pool was reported as out of memory.
The fixed runs pass all focused cases:

- WSL GCC 11.4.0 with AddressSanitizer and UndefinedBehaviorSanitizer: 28/28
  function faults, 60/60 module faults, invalid-input checks, and the core
  model pass. The byte-overflow cases are guarded because this is an x64 build.
- WSL Clang 14.0.0 with AddressSanitizer and UndefinedBehaviorSanitizer: the
  same 28/28 and 60/60 suites, invalid-input checks, and core model pass.
- MSVC x86 (Visual Studio environment 17.14.40): the same suites plus both
  native 32-bit byte-overflow checks pass.

`clone-oom-*-green-*-run.log` under
`D:/tmp/zr_vm/ssa-execir-clone-oom` records the focused runs. The sanitizer
runs use leak detection and report no sanitizer finding. The separate RED
logs retain the expected diagnostic and rollback failures. Superseded ExecIR
RED binaries were removed after verification; the RED logs remain under this
D-only evidence directory for the failure-proof record.

The CMake target enables interception for every source in the executable, using
MSVC `/FI` or separate GCC/Clang `-include` arguments. The MSVC target run
completed the same 28/28 and 60/60 sweeps. The shared WSL build directories still
have older generated rules: attempts to regenerate them on 2026-10-02 did not
complete and were terminated. Their older `ssa core model PASS` output does not
prove allocator coverage. The GCC/Clang evidence above comes from the focused
direct builds.

## Limitations

The allocator remapping is confined to the core-model test target and is absent
from production targets. This evidence proves clone lifecycle and diagnostic behavior, not AOT
emission, physical frame restoration, or execution after a resumed checkpoint.
The full SSA plan remains open beyond this leaf.
