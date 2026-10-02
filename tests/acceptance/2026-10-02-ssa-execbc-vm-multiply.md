---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/ssa_execbc_vm_multiply_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/ssa_execbc_vm_multiply_cases.inc
  - tests/core/test_execution_checked_multiply.c
doc_type: acceptance-record
status: scoped-accepted
---

# SSA ExecBC VM signed multiplication

## Scope and prerequisite

This independent support slice admits typed i64 MUL in the shared ExecBC VM
materializer, using the existing physical-slot mapping and Core `MUL_SIGNED`
instruction. Its six-file write set includes two parser production files,
the existing formal test main, a multiplication case include, a dedicated
module guide, and this record. It adds no build target or VM bypass.

The default checked-runtime prerequisite was committed first as `8759ccc2`.
Root reported `execution_checked_multiply` passing on native MSVC and a
native-Clang mathematical oracle check. Those runtime results are documented
in `2026-10-02-ssa-checked-multiply.md`; they do not establish this parser
consumer's GREEN result. Linux/computed-goto/runtime-sanitizer validation
remains unverified by that prerequisite.

## Baseline and observed RED

The source MUL investigation reached Oracle return values 18/21 and a valid
projection, then failed in the materializer at MUL instruction 12 with
`UNSUPPORTED` (code 28, actual opcode 9). The root cause was the lower
consumer's ADD/SUB-only validation whitelist and matching ADD/SUB-only native
instruction emission. Both owning parser files were clean; the foreign
dirty Core state materialization file was not involved or edited.

Root built the formal lower target successfully, then its first CTest
invocation timed out at 41.73 seconds without Unity output. That timeout
was not counted as a functional RED:

```text
control/execbc-mul-red-build.log
control/execbc-mul-red-ctest.log
```

A diagnostic one-TU wrapper included the unchanged real
`test_ssa_execbc_vm.c` with renamed `main`, disabled stdout/stderr buffering,
and called the original main. Only that wrapper object was compiled. Its
executable reused the formal target's current four harness objects and
identical static libraries. Compile and link both exited 0; the complete
suite ran to exit 2: 26 tests, two failures, zero ignored. All 22 old cases
passed. The new signed-product case failed at `materialized`; the missing
operand case expected `INVALID_VALUE` (13) but received `UNSUPPORTED` (28),
matching the opcode whitelist's default rejection. The type and Oracle-only
overflow cases passed. No fixture hang reproduced.

The initial diagnostic launch failed to locate `cl.exe`; the driver then
resolved compiler/linker absolute paths from the cached MSVC environment.
The successful diagnostic command was:

```text
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/source-multiply/lower/run_diagnostic.py
```

Exact RED output and the summary are under:

```text
D:/tmp/zr_vm/ssa-20261002-01a0fc3b/source-multiply/lower/diagnostic-unbuffered.log
D:/tmp/zr_vm/ssa-20261002-01a0fc3b/source-multiply/lower/diagnostic-summary.json
```

## Test inventory

The five new Unity cases use a verified, published-identity typed i64
hand-built ExecIR function and the normal Oracle/projection/materializer
entry points:

| Case | Required behavior |
| --- | --- |
| signed products | `9*2=18`, `-7*3=-21`, `0*INT64_MAX=0`, `INT64_MAX*1=INT64_MAX`; emitted native MUL_SIGNED, immutable projection, actual VM/Oracle agreement |
| wrong operation type | bool-tagged MUL rejects with UNSUPPORTED at function 0x5105/block 1/instruction 3/source 903, empty output, unchanged projection |
| missing input | one-input MUL rejects with INVALID_VALUE at the same exact location, empty output, unchanged projection |
| Oracle overflow | INT64_MAX*2 and INT64_MIN*-1 reject with ARITHMETIC_ERROR at instruction 3/source 903 |
| materialized VM overflow and recovery | independently materialized operations raise RUNTIME_ERROR/current exception at the native PC mapped to instruction 3/source 903; after reset, the same state/function returns -91 and clears exception/status |

The VM overflow case continues into materialization after the expected
Oracle arithmetic error specifically to obtain independent runtime evidence.
It uses the committed checked runtime, not a test arithmetic implementation.
Existing 22 cases cover canonical type adapters, ADD/SUB, phi scheduling,
branch paths, sparse slots, and unsupported metadata.

## Required GREEN verification

Root owns the shared Ninja tree. Its focused formal build/test commands are:

```text
cmake --build D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc --target zr_vm_ssa_exec_ir_execbc_vm_test -j 4
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^ssa_exec_ir_execbc_vm$ --output-on-failure --no-tests=error
```

The completed current-source GREEN result and adjacent support results must
be recorded below before acceptance. Compiler temporaries, diagnostics,
objects, and logs stay on D under the task output root; diagnostic artifacts
use its `source-multiply/lower` subtree. No repo build directory or cross-drive
artifact relocation is used.

## Independent root verification

Root rebuilt the current repository target in the existing MSVC Debug Ninja
configuration. Build exit was 0. The registered `ssa_exec_ir_execbc_vm` CTest
passed in 0.17 seconds, reporting **27 Tests, 0 Failures, 0 Ignored**. Its
output includes both representable products and the independently materialized
VM overflow/fault-PC/recovery case. The accompanying 33-case SCCP CTest also
passed; the two-test command exited 0.

The exact commands and output are retained in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/sccp-representation-multiply-build.log`
and `sccp-representation-multiply-ctest.log` in the same directory. The formal
Unity output is also retained in the build's `Testing/Temporary/LastTest.log`.
Root and an independent reviewer checked the current six-file production,
test and documentation scope. No introduced code finding remained; the
review and exact source hashes are retained under `review/multiply-v2/`.

## Acceptance decision

The lower signed-MUL materializer slice is scoped-accepted on MSVC Debug,
following the actual lower RED, committed checked-runtime prerequisite and
current 27-case formal GREEN. No Linux, computed-goto, runtime-sanitizer,
source-branch consumer, or complete 01.05 milestone acceptance is claimed.
