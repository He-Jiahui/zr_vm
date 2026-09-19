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
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
status: partial
---

# SSA 01.02: explicit source throw CFG

## Scope

An unhandled source `throw expression` now consumes the expression's canonical
ValueId and emits one SemanticIR `THROW` operand. The current source block is
closed with `ZR_PARSER_CFG_TERMINATOR_THROW`, has no successors, and becomes
the function CFG exit. A throw can establish the first source CFG block or
terminate the normal continuation of an existing typed-call `INVOKE` graph.

The compiler retains that completed graph after the throw. Later unreachable
source continues through the legacy ExecBC compiler, but cannot append
SemanticIR instructions, abandon the completed graph, or restart a detached
CFG from a call or control-flow construct.

## Focused fixtures

`test_source_throw_terminates_cfg_and_blocks_unreachable_restart` starts with a
linear local, throws its loaded value, then compiles an unreachable resolved
call and `if` containing another call. It requires one payload-bearing
SemanticIR `THROW`, zero typed-call facts, a single entry/exit throw block with
no successors, and one one-operand ExecIR `THROW`.

`test_source_throw_terminates_existing_invoke_cfg` first emits a resolved typed
call and its ordered normal/exception `INVOKE` successors. The following throw
must terminate the normal block, become the CFG exit, and lower alongside
exactly one ExecIR `INVOKE`. A trailing unreachable call must not add a second
semantic call.

`test_conditional_source_throw_blocks_later_cfg_startup` keeps a throw inside
an unsupported conditional shape on legacy lowering. Seeing that unmodeled
termination promotes scoped startup suppression to a function-level barrier,
so the trailing reachable call cannot create a detached graph that omits the
conditional exceptional exit. The validated result remains the legacy
two-block graph with no semantic call or throw fact.

`test_unmodeled_throw_payload_blocks_later_cfg_startup` uses an arithmetic
payload that has no canonical producer ValueId in this slice. The compiler
keeps the throw on legacy lowering, sets the same function-level startup
barrier, and prevents a trailing resolved call from publishing an incomplete
graph. The modeled post-throw fixture also compiles unreachable store,
`while`, and logical operations to protect their legacy-only path.

The later `test_throw_try_finally_preserves_precleanup_value` milestone covers
one terminal linear throw inside a preflighted no-catch `try/finally`. Its
payload is loaded before cleanup, cleanup reaches a dedicated THROW block, and
the final terminator consumes that original ValueId even when `finally`
overwrites the source local. The subsequent conditional fixture covers one
branch-local throw with a normal sibling: a private selector and payload route
both paths through shared cleanup, then cleanup dispatch selects the THROW
block or normal join. Nonlinear throw payloads remain fail-closed.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 rebuilt and passed the focused pre-execution SemanticIR
  suite 33/33.
- WSL GCC 11.4.0 and Clang 14.0.0 each rebuilt and passed the same focused
  suite 33/33.
- GCC 11.4.0 with AddressSanitizer and UndefinedBehaviorSanitizer passed the
  focused suite 33/33 with leak detection enabled and halt-on-error behavior.
- The adjacent SSA builder, dominance, control-edge, fact-identity,
  Place-eligibility, Place-promotion, and ValueId-validation gate passed 7/7
  under MSVC, WSL GCC, and WSL Clang.
- The receiver-guard performance contract passed 1/1 under all three
  toolchains.
- Wiki validation passed for 116 Markdown files, 115 manifest pages, and 644
  local links.

## Boundary

Later cleanup milestones model exceptional entry for one zero-argument direct
call, but this checkpoint still does not combine it with multiple completion
kinds. Except for the bounded terminal, normal-versus-throw, and direct-call
cleanup paths, a throw inside an unmodeled `try`/`catch`/`finally` scope or a
control-flow shape whose source CFG preflight has already fallen back remains
on legacy lowering. Those handler-aware paths must be introduced as one
coherent exception-region contract rather than connected to this unhandled
zero-successor sink.
