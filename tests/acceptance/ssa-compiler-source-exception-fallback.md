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

This original checkpoint established the fallback used whenever a source
`try`/`catch`/`finally` shape does not belong to a later bounded handler or
cleanup producer. Compiling such a scope abandons any earlier partial source
CFG and suppresses every inactive CFG starter while the protected block,
catches, and finally block compile. Existing ExecBC exception machinery remains
unchanged. Startup stays suppressed through the remainder of the current
SemanticIR function so a later call cannot absorb the earlier exception scope
into a false linear prefix.

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

The later source-cleanup milestones narrow this historical boundary: one or
more same-kind preflighted linear return or throw sites now publish either a
direct protected-to-cleanup-to-abrupt path or, when a sibling path falls
through, a private pending selector and payload followed by cleanup dispatch to
abrupt or normal continuation. Nonlinear abrupt payloads, catch-plus-finally,
mixed return/throw kinds, and most
exceptional cleanup entry still use this
persistent fallback and cannot restart a detached graph. One later bounded
shape admits a resolved direct call with no argument or up to three exact integer,
`bool`, or `float` identifiers or literals passed by value: integer arguments
must match the resolved parameter type exactly; each exceptional edge reaches
one shared landing
that defines and stores `EXCEPTION_PAYLOAD`, enters shared
cleanup, and rethrows only after cleanup dispatch. Dynamic or unresolved calls
and converting, non-value, or more-than-three arguments remain here. One or more same-kind,
operand-free `break`
transfers from a supported `while`, linear statement-form `for`, or statically
typed `foreach` are also claimed by the cleanup producer: terminal and
conditional forms reach the existing loop join only after `finally`. One or
more same-kind, operand-free `continue` transfers reach the existing `while`
condition block, `for` step block, or foreach move-next block by the same route.
A further bounded cleanup shape permits one or more explicit, exact non-null
`object` throws to share the pending `THROW` payload with resolved direct-call
exceptional entry; integer or otherwise incompatible throws remain here. Mixed
`break`/`continue` sites and mixed
loop-transfer/completion kinds retain this fallback boundary; repeated
same-kind transfers use the same cleanup completion target.

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

This checkpoint is deliberately conservative. Later bounded milestones now
claim source-owned catch payload/dispatch (including an exact `int`
integer-literal argument), no-catch finally cleanup edges, the
interrupted-assignment guard, repeated direct-call exceptional cleanup paths, and one
pending `break` or `continue` destination from a supported `while`, `for`, or
`foreach`.
Other handler/finally combinations and handled non-call throwable operations
remain outside that subset. Straight-line unhandled source `throw` is covered by
[the explicit throw CFG checkpoint](ssa-compiler-source-throw-cfg.md). The
remaining handler-aware work is still required for the full 01.02 exit gate;
this slice ensures it cannot be bypassed by a partially modeled nested graph
in the meantime.
