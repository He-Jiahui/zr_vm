---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_exception_fallback.inc
status: partial
---

# SSA 01.02: conservative source exception scope

## Scope

Source `try`/`catch`/`finally` does not yet publish canonical handler payload,
catch-selection, or finally-cleanup edges. Compiling such a scope now abandons
any earlier partial source CFG and suppresses every inactive CFG starter while
the protected block, catches, and finally block compile. Existing ExecBC
exception machinery remains unchanged. Startup stays suppressed through the
remainder of the current SemanticIR function so a later call cannot absorb the
earlier exception scope into a false linear prefix.

This prevents a resolved call or nested supported control-flow construct from
creating a detached graph that incorrectly omits the enclosing exception and
cleanup transfers.

## Focused fixture

`test_unmodeled_try_scope_does_not_start_detached_call_cfg` compiles four
resolved-call cases. The first begins with no source CFG. The second creates a
supported source `if` before entering `try`. Both require zero `CALL_TYPED`
facts and only the validator's two-block legacy graph. A third case places the
resolved call after `try`; the fourth nests `try` inside an outer unsupported
`if` before that trailing call. All require the same fallback. Together they
prove that exception scope entry blocks fresh startup, abandons an earlier
partial graph, prevents trailing code from restarting one, and survives scoped
suppression restoration by an enclosing construct.

The fixture failed before the change with one detached `CALL_TYPED` in the
inactive case; after the first scoped suppression fix, the trailing-call case
also failed with one detached call. Splitting scoped suppression from the
function-level block exposed and fixed the same failure for the nested case.
The active-prefix case protects the abandonment path.

The later `test_finally_return_preserves_precleanup_value` milestone narrows
this historical boundary: one preflighted terminal linear return now publishes
an explicit protected-to-cleanup-to-return path while retaining its pre-cleanup
operand. Nonlinear returns, throw, catch-plus-finally, and multiple completion
kinds still use this persistent fallback and cannot restart a detached graph.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 rebuilt and passed the focused pre-execution SemanticIR
  suite 40/40.
- WSL GCC 11.4.0 and Clang 14.0.0 each rebuilt and passed the same focused
  suite 40/40.
- GCC 11.4.0 with AddressSanitizer and UndefinedBehaviorSanitizer passed the
  focused suite 40/40 with leak detection enabled and halt-on-error behavior.
- The adjacent SSA builder, dominance, control-edge, fact-identity,
  Place-eligibility, Place-promotion, and ValueId-validation gate passed 7/7
  under MSVC, WSL GCC, and WSL Clang.
- The receiver-guard performance contract passed 1/1 under all three
  toolchains.
- Wiki validation passed for 116 Markdown files, 115 manifest pages, and 644
  local links.

## Boundary

This checkpoint is deliberately conservative. It does not claim source-owned
exception payloads, catch dispatch, finally cleanup edges, interrupted
assignment state, or handled non-call throwable operations. Straight-line
unhandled source `throw` is covered by
[the explicit throw CFG checkpoint](ssa-compiler-source-throw-cfg.md). The
remaining handler-aware work is still required for the full 01.02 exit gate;
this slice ensures it cannot be bypassed by a partially modeled nested graph
in the meantime.
