---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_arithmetic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/test_ssa_source_straight_line_cfg.c
  - tests/parser/test_exec_ir_scalar_scratch_eligibility.c
  - tests/acceptance/2026-10-02-ssa-source-branch-multiply.md
doc_type: testing-guide
status: scoped-accepted-msvc
---

# SSA source to ExecBC Core VM integration

## Coverage

`test_ssa_source_execbc_vm.c` parses source with `ZrParser_Parse`, compiles it
to source-owned SemIR, validates the SemIR CFG, builds and verifies a published
ExecIR module function with `ZrParser_ExecIr_BuildModule`, runs the ExecIR
Oracle, lowers constants into an ExecBC projection, materializes a Core
`SZrFunction`, and invokes it through the Core runtime dispatcher.

The positive cases execute both outcomes of a real source conditional:

- `test_true_source_branch_runs_through_core_dispatcher`: returns 9.
- `test_false_source_branch_runs_through_core_dispatcher`: returns 8.
- `test_source_branch_arithmetic_reaches_core_dispatcher`: executes ADD and returns 10.
- `test_source_branch_subtraction_reaches_core_dispatcher`: executes SUB and returns 8.
- `test_true_source_branch_multiplication_reaches_core_dispatcher`: executes MUL in the then arm and returns 18.
- `test_false_source_branch_multiplication_reaches_core_dispatcher`: executes MUL in the else arm and returns 21.

`test_source_branch_division_does_not_publish_execir` checks that an unsupported
binary arm stays outside executable source CFG, ExecIR projection, and VM
publication. Conditional-arm arithmetic is limited to ADD/SUB/MUL with two integer
literals. The test requires the corresponding ExecIR opcode before comparing
Oracle and Core dispatcher returns. The MUL cases also require canonical
signed i64 type in both ExecIR and the ExecBC projection. Nested arithmetic
and nonliteral operands remain outside this conditional-arm subset.

The Oracle fixture resolves Builder-emitted `PLACE_BASE` values by following
their source IDs back to SemIR and returning the SemIR `placeId` as a stable
signed token. It seeds only external ExecIR values that are used as
`PLACE_BASE` provenance operands. Other undefined ExecIR inputs fail fixture
setup instead of being silently modeled.

Each case compares the signed Oracle return with the Core VM return. It
validates every observed VM PC against the materializer PC map and the
corresponding ExecIR instruction source ID. The collapsed VM block path must
start at the ExecIR entry, follow declared CFG successor edges, and end at the
Oracle's reported final block. The Oracle exposes the final block but not a
complete block history, so this does not claim full path-by-path Oracle trace
equality.

These branch bodies return independently and do not merge a value. The test
records `execIr.phiCount` for diagnosis but does not claim source value-phi or
loop-phi coverage. A real-source loop-carried phi remains an open follow-up.

The source fixture covers bool and signed i64 constants, conditional
branches, literal ADD/SUB/MUL, and signed i64 returns. Other source operations remain outside
this focused materializer slice.

The producer change supports only direct bool/i64 constant initialization of a
root `TEMPORARY` Place with a compiler-authored proof. The Place must be unique
by root identity, disjoint from every other Place, projection/loan/escape/local
free, and have exactly one `PLACE_BASE` plus one `INITIALIZE` with no other
place operation. This does not promote arbitrary temporary Places or relax
the materializer's memory/effect metadata checks. The compiler classifier
checks the live constant pool; Builder can only recheck the SemIR `CONSTANT`,
`hasConstantPoolIndex`, and type/proof consistency because it receives no
constant pool. The separate Core `VerifyModule` owned-pool validation is
documented in `ssa-module-constant-pool-verifier.md`; this source module uses
the compiler pool explicitly when constructing its projection and Oracle
input.

Builder value/instruction types are canonical IDs. The fixture copies the
actual compiler constants into a projection pool using IDs interned in the
same semantic context, then calls
`ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes`. The
adapter resolves those IDs to runtime bool/i64 tokens in copied arrays; the
original graph's types, source metadata, CFG and effect guards remain intact.

## Focused validation

The CMake registration is `ssa_source_execbc_vm`, target
`zr_vm_ssa_source_execbc_vm_test`, included through
`tests/cmake/ssa-source-execbc-vm.cmake`. Root uses the native wrapper with
compiler temporary storage and all build/generated outputs under
`D:/tmp/zr_vm`:

```powershell
python D:/tmp/zr_vm/ssa-control/run_native.py source-build build zr_vm_ssa_source_execbc_vm_test
python D:/tmp/zr_vm/ssa-control/run_native.py source-ctest ctest -R '^ssa_source_execbc_vm$' --no-tests=error
```

## Current validation status

The new single-level literal MUL cases, historical source RED, and current
verification status are recorded in
[`2026-10-02-ssa-source-branch-multiply.md`](2026-10-02-ssa-source-branch-multiply.md).
Its acceptance decision supersedes historical passing results for the new MUL scope.
The earlier attached StateMap Builder failure has a separate fix (`df3d1213`).
On 2026-10-02 UTC (2026-10-03 in Asia/Shanghai), the current-source MSVC build in
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc` exited 0. The focused CTest
selection passed all three suites with exit 0: `ssa_source_execbc_vm` (0.43 s),
`ssa_source_straight_line_cfg` (0.73 s), and
`exec_ir_scalar_scratch_eligibility` (0.28 s). The source suite recorded
`7 Tests 0 Failures 0 Ignored`: six real source-to-VM positive cases and the
division nonpublication case. Both multiplication arms returned the expected
18/21 through the Oracle and actual Core dispatcher, with canonical types,
both projected MULs, and PC/source/CFG-path assertions active.

Build and CTest logs are
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/source-branch-multiply-build.log`
and `D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/source-branch-multiply-ctest.log`;
the case output is in that build's `Testing/Temporary/LastTest.log`.
Checked legacy MUL (`8759ccc2`) and canonical signed i64 ExecBC VM MUL
(`d6d5882e`) are independently committed and verified prerequisites. Their
overflow/fault-PC/recovery coverage belongs to the lower suites, rather than
the small-product source cases above.

This source slice is accepted for the recorded MSVC scope. The current MUL
extension has not been rerun under GCC, Clang, or sanitizers; historical
Linux passes below do not validate the new extension. Full 01.02/01.05
acceptance and source loop-phi coverage remain open.

## Historical evidence

The 2026-10-02 current-source MSVC rebuild used a fresh
`D:/tmp/zr_vm/ssa-source-native-current` directory and compiled 888 build steps.
Its initial execution exposed the attached empty StateMap being rejected by
the materializer and the arithmetic arm being refused by producer preflight.
The fixes preserve attached metadata, accept only an identity-matched empty
map with no side-pool values, and preflight only integer-literal ADD/SUB.
See `2026-10-02-ssa-scalar-boundaries.md` for that command/log evidence.

The 2026-09-30 root MSVC build linked the source target and standalone builder
consumers. `current-scalar-shape-fixtures-ctest.log` passed the two source
branches (returns 9 and 8), `ssa_source_execbc_vm` in 0.54 seconds, and
`exec_ir_scalar_scratch_eligibility` in 0.78 seconds. This same selection
passed the 18-case scalar/canonical VM target and 13-case dead-place target.
It was 7/8 overall because an independent conditional-cleanup fixture failed.
The 2026-10-01 GCC 11.4 and Clang 14 WSL reruns each passed all 19 selected
CTest suites, including both source branches, scalar eligibility, the 18-case
VM suite and the 13-case dead-place suite. The standalone PlaceGraph target
also passed all eight cases with each compiler. Logs are preserved at
`D:/tmp/zr_vm/ssa-control/{gcc,clang}-current-scalar-final-{build,ctest,last-test}.log`
and `{gcc,clang}-place-queries-final-direct.log`. This is a focused scalar
matrix; the complete semantic matrix remains open.

The previously recorded source run reached Oracle and projection for both
cases, then failed materialization with diagnostic `UNSUPPORTED` (code 28),
function 20741, block 0, instruction 0, source 0, expected version 0, and
actual version 1. The structured source dump showed three temporary scalar
`PLACE_BASE`/`INITIALIZE` pairs and three memory tokens. The new proof side
table/classifier/eligibility slice makes only the directly
proven bool/i64 literal pairs promotable; it does not relax the materializer's
memory/effect guard or delete arbitrary stores. The old RED is retained as
historical evidence alongside the passing focused run above.

The independent negative target is `exec_ir_scalar_scratch_eligibility`,
executable `zr_vm_exec_ir_scalar_scratch_eligibility_test`, registered by
`tests/cmake/exec-ir-scalar-scratch-eligibility.cmake`. The focused command is:

```powershell
python D:/tmp/zr_vm/ssa-control/run_native.py scratch-build build zr_vm_exec_ir_scalar_scratch_eligibility_test
python D:/tmp/zr_vm/ssa-control/run_native.py scratch-ctest ctest -R '^exec_ir_scalar_scratch_eligibility$' --no-tests=error
```

The source loop-phi claim remains open. This source branch has no value merge;
the acceptance does not count its observed branch as loop-phi coverage.
