---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/acceptance/ssa-pass-manager-token-phi-hash.md
  - tests/acceptance/ssa-pass-failure-snapshot.md
  - tests/acceptance/ssa-pass-verifier-timing.md
doc_type: implementation
status: active
---

# ExecIR scalar pass pipeline

The parser owns optimization policy; the core owns the ExecIR data model and
verifier.  The initial registry is deliberately small and deterministic:

1. `sccp` computes executable CFG edges and the value lattice, then performs
   dominance-safe copy propagation and folds only proven, representable
   integer results.
2. `dce` performs a bounded fixed-point liveness walk and turns only unused,
   side-effect-free definitions into `NOP` tombstones.

The public `SZrExecIrPassInfo` contract records required, preserved, and
invalidated analyses.  SCCP requires dominators when a function has blocks;
its rewrite invalidates its own lattice.  DCE invalidates all current
analyses.  A cache carries an IR/input hash as well as a revision, so callers
that reuse a cache cannot accidentally consume facts computed for an older
operand or constant pool.  The function hash includes each block's effect
phi result and incoming range, and every memory region's phi result and
incoming range.  Changing only a token phi therefore invalidates cached
analyses and is visible to pass-change accounting.  The hash excludes the
derived immediate dominator, which is an analysis result rather than IR.

SCCP uses `unknown`, `constant`, `overdefined`, and `must-throw` states.  Add,
subtract, multiply, negate, and divide use checked signed-64-bit helpers;
overflow, divide-by-zero, and the `INT64_MIN / -1` case become `must-throw`
and remain executable instructions.  A module constant pool is read through
the pass context.  Because `layoutId` is a pool index in that mode, arithmetic
rewrites are retained until a module-aware constant materializer can allocate
an unambiguous pool entry; copy propagation is still allowed.

Copy propagation never substitutes a definition across a non-dominating
edge, across a same-instruction use, or through a `MOVE` whose ownership
semantics are not proven.  Phi incoming values are left edge-sensitive.  This
conservative rule is preferable to turning a malformed or exceptional CFG
into a different program.

DCE treats opcode schema memory/effect flags and instruction boundary flags
(`MAY_THROW`, `MAY_ALLOCATE`, `MAY_GC`, `MAY_SUSPEND`, debug polls, guard exits,
drop, calls, loads, stores, barriers, and returns) as observable even when no
result is used.  GC roots, deoptimization values, and state-map live/root
ranges seed liveness; phi inputs are marked only when their result is live.
When a pure tombstone is emitted, source-map and non-semantic state-map/GC
site entries for that instruction are compacted so metadata never points at a
deleted definition.

`ZrParser_ExecIr_RunPassPipeline` verifies the complete structure/SSA/effect
contract before and after every pass and is transactional: a failed verifier,
pass, malformed range, or remark allocation restores the original function,
analysis cache, diagnostics context, and remark count.  A work budget emits a
`BLOCKED` remark and keeps the last valid IR; budget exhaustion is not reported
as a compiler error.  Remark sinks are append-only and owned by the caller.
Callers may provide an initialized `SZrExecIrPassFailure` in the pass context
or module optimization options.  On a named pass failure, the manager transfers
that function's failed IR, a copy of the pass name, and its diagnostic into
the record before restoring the original function/module.  This is the
smallest self-contained replayable unit; the caller owns it and frees it with
`ZrParser_ExecIr_PassFailureFree`.  The next pipeline call clears an older
record.  Invalid input rejected before a pass starts has a diagnostic but no
pass-failure record.  Allocation failure while copying the name cannot
prevent rollback and leaves the failure record empty.
Each successful pass remark separately records `verifierChecks=2` and their
total `verifierTicks` (CPU ticks from `clock()`).  The pipeline preflight check
is not charged to an individual pass.  `elapsedTicks` still measures only the
pass execution; verifier timing is diagnostic, may round down to zero, and
does not affect work budgets or optimization decisions.

`ZrParser_ExecIr_Optimize` applies the same registry to every module function,
shares the compile budget across functions, supplies the module constant view,
and reports an aggregate module hash.  Individual passes can be disabled for
binary diagnosis without changing legality checks.

Focused validation (from the repository root):

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
cmake --build build/ssa-clang-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir build/ssa-clang-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
```

The focused fixture covers fixed-point idempotence, checked division and
overflow, preservation of an unused call, source-map cleanup, malformed input
diagnostics, failed-pass rollback, bounded execution, and hash sensitivity to
effect and per-region memory phi results and range coordinates.  The failed
pass fixture also checks that the captured function reproduces the verifier
diagnostic after rollback and that its pass name survives caller mutation.
Every successful scalar pass remark is checked for two verifier boundaries.
