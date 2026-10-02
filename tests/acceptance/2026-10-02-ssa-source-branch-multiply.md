---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - tests/parser/test_ssa_source_execbc_vm.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/test_ssa_source_straight_line_cfg.c
  - tests/parser/test_exec_ir_scalar_scratch_eligibility.c
  - tests/acceptance/ssa-source-execbc-vm.md
doc_type: testing-guide
status: scoped-accepted-msvc
---

# SSA single-level integer-literal branch multiplication

## Scope

The source conditional-arm return preflight now accepts `*` when both AST
operands are direct integer literals. The existing canonical same-type
SemanticIR MUL producer supplies the operation; this change adds no opcode,
type conversion, name lookup, or runtime arithmetic implementation.

The return-specific predicate remains separate from the linear-expression
predicate shared by loop/finally lowering. Nested binary expressions,
nonliteral arithmetic operands, division, calls, and suspension expressions
remain outside the newly admitted shape. This is a bounded 01.02/01.05
extension; it does not complete either milestone or source loop-phi coverage.

## Baseline and RED

Root built the formal `zr_vm_ssa_source_execbc_vm_test` native target in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc`; the fresh build completed
889 steps with exit code 0. The test-only change then ran:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^ssa_source_execbc_vm$ --output-on-failure --no-tests=error
```

The observed CTest exit code was 8: seven Unity cases ran, with six failures
and one pass. Both new multiplication cases failed at line 699 with
`source CFG validation failed`, before ExecIR or VM publication, confirming
the source preflight boundary.

The four pre-existing positive cases also failed independently at line 695:
Builder diagnostic code 24, function token 20741, block/instruction/source 0.
The division rejection case passed. This is historical RED evidence, rather
than the current acceptance result. The attached StateMap Builder failure was
subsequently fixed in `df3d1213`; this syntax change does not own that fix.

After the source preflight change, both MUL cases reached the Oracle (18 and
21) and ExecBC projection, but materialization rejected opcode MUL with
diagnostic code 28. The lower prerequisites are now separately verified and
committed: `8759ccc2` supplies strict checked legacy multiplication forms,
and `d6d5882e` supplies canonical signed i64 ExecBC VM materialization. The
latter's 27-case formal suite includes actual VM overflow, fault PC, and
recovery in the same state. Those are separate prerequisite results; the
full source-suite GREEN for this change is recorded below.

RED evidence:
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/source-multiply-red-ctest.log`.

## Test inventory

The two new cases parse and compile the complete source program:

```zr
if (true) { return 9 * 2; } else { return 7 * 3; }
```

The second case changes the condition to `false`. The expected returned
values are respectively 18 and 21. Each case follows the existing source
SemIR CFG -> published/verified SSA ExecIR -> Oracle -> ExecBC projection ->
Core VM materialization/dispatcher chain. Assertions require canonical
signed i64 MUL in ExecIR and its projection, equal Oracle/VM returns,
valid VM PC/source mappings, declared CFG edges, and the Oracle terminal block.
They also require exactly two MUL operations in ExecIR, their preservation in
the projection, and canonical signed i64 type for every MUL in each stage.

The same suite retains plain branch, ADD/SUB, and division-rejection cases.
The adjacent `ssa_source_straight_line_cfg` suite checks existing canonical
binary producers and unsupported source forms. The
`exec_ir_scalar_scratch_eligibility` suite guards the memory/alias/ownership
boundary required by the source VM fixture.

The multiplication literals have small representable products. Existing
Oracle and ExecBC arithmetic consumers check integer multiplication overflow;
this preflight change does not create an overflow proof or relax those checks.

## Current MSVC GREEN

On 2026-10-03, Root rebuilt the frozen current production and fixture files
in `D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc`; build exit code was 0.
The formal CTest command was:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R "^(ssa_source_execbc_vm|ssa_source_straight_line_cfg|exec_ir_scalar_scratch_eligibility)$" --output-on-failure --no-tests=error -j 1
```

The actual result was three of three passed, CTest exit code 0:

| Suite | Actual result | Time |
| --- | --- | --- |
| `ssa_source_execbc_vm` | 7 Unity cases, 0 failures, 0 ignored | 0.43 s |
| `ssa_source_straight_line_cfg` | 35 Unity cases, 0 failures, 0 ignored | 0.73 s |
| `exec_ir_scalar_scratch_eligibility` | scalar eligibility PASS | 0.28 s |

The seven source cases comprise six positive source -> canonical facts ->
SemIR CFG -> verified ExecIR -> Oracle -> ExecBC -> actual Core VM execution
cases and one division nonpublication case. The six returns are 9, 8, 10,
8, 18, and 21. The MUL cases preserve both operations and require canonical
signed i64 types, Oracle/VM agreement, and valid observed PC/source/CFG paths.
The tests invoke the materialized source projection, without a baseline
return stub or replacement graph.

Evidence is preserved under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/`:
`source-branch-multiply-build.log` and `source-branch-multiply-ctest.log`.
The full Unity output is in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc/Testing/Temporary/LastTest.log`.

## Tooling and remaining verification

Current-source GCC/Clang and sanitizer runs for the new MUL extension have
not been performed. This MSVC result does not establish their acceptance.
All new build directories, compiler temporary files, and logs must
stay under `D:/tmp/zr_vm/ssa-20261002-01a0fc3b`; the focused Linux paths use
its `/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/source-multiply` subtree.

The focused target set is:

```text
zr_vm_ssa_source_execbc_vm_test
zr_vm_ssa_source_straight_line_cfg_test
zr_vm_exec_ir_scalar_scratch_eligibility_test
```

CTest selection:

```text
^(ssa_source_execbc_vm|ssa_source_straight_line_cfg|exec_ir_scalar_scratch_eligibility)$
```

## Acceptance decision

Accepted for the recorded MSVC scope: all seven source cases and both
adjacent suites passed against the current source. Historical RED remains
above to establish the admission gap and separately diagnosed prerequisites.
Checked runtime/materializer prerequisites are independently committed;
their lower overflow/fault-PC/recovery claims do not come from these source
cases. GCC/Clang/sanitizer acceptance, source loop-phi coverage, and the full
01.02/01.05 milestone gates remain open.
