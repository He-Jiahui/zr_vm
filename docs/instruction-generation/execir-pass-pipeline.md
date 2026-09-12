---
doc_type: implementation
status: active
related_plan: docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
test: ssa_pass_manager_scalar
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
operand or constant pool.

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
diagnostics, failed-pass rollback, and bounded execution.
