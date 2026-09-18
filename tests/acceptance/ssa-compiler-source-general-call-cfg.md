---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_general_call.inc
status: partial
---

# SSA 01.02: resolved source-call CFG startup

## Scope

A resolved, non-spread function call can now create the first source-owned CFG
boundary in a caller. The preceding straight-line SemanticIR instructions are
bound to an entry block, which jumps to a dedicated invoke block. The call fact
contains the canonical callable ValueId, ordered explicit argument ValueIds,
resolved function symbol, result TypeId, and result ValueId.

The invoke block has ordered normal and exception successors and lowers to one
ExecIR `INVOKE`. The normal continuation owns the call result. The exception
continuation is an explicit zero-instruction propagation sink with a `THROW`
terminator; it does not fabricate an exception operand before the IR has an
edge-defined payload contract.

An unresolved/dynamic target, spread call, missing symbol, missing result type,
or missing callable/argument ValueId does not start a false-precision graph. If
no graph is active, lowering retains the legacy path. If an earlier source
construct already started a graph, lowering abandons that partial graph and
removes its synthetic branches.

When an enclosing `if`, `while`, or short-circuit expression has already
failed its syntactic CFG preflight, its nested calls cannot restart a detached
graph. The enclosing legacy compilation scope suppresses inactive call-driven
startup while still permitting calls inside an already active supported graph.

## Focused fixture

`test_source_resolved_function_call_emits_typed_invoke_cfg` uses the compiler's
normal function-predeclaration stage, then compiles the caller statements while
keeping the callee body outside the caller SemanticIR fixture. It calls a
resolved one-argument `identity` function and requires exactly one
`CALL_TYPED`. The instruction must carry the function's canonical symbol, two
operands (callable then argument), a result TypeId, and a result ValueId.

The fixture locates the call's owning block and requires ordered normal and
exception edges, an instruction-free exception sink with a `THROW` terminator,
and successful SemanticIR-to-ExecIR construction with exactly one two-successor
`INVOKE`.

`test_nested_calls_do_not_restart_abandoned_source_cfg` covers unsupported
call-bearing `if`, `while`, and `&&` constructs, plus an outer fallback scope
whose nested supported `if` would otherwise restart CFG before the call. Each
case must emit no `CALL_TYPED` and retain only the validator's two-block legacy
graph.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 rebuilt and passed the focused pre-execution SemanticIR
  suite 28/28.
- WSL GCC 11.4.0 and Clang 14.0.0 each rebuilt and passed the same focused
  suite 28/28.
- GCC 11.4.0 with AddressSanitizer and UndefinedBehaviorSanitizer passed the
  focused suite 28/28 with leak detection enabled and halt-on-error behavior.
- The adjacent SSA builder, dominance, control-edge, fact-identity,
  Place-eligibility, Place-promotion, and ValueId-validation gate passed 7/7
  under MSVC, WSL GCC, and WSL Clang.
- The receiver-guard performance contract passed 1/1 under all three
  toolchains.
- Wiki validation passed for 116 Markdown files, 115 manifest pages, and 644
  local links.

## Boundary

This checkpoint covers resolved direct/callable function targets that have a
canonical symbol and fixed explicit argument range. Calls nested in source
constructs whose syntactic CFG preflight still excludes call expressions
stay on the conservative legacy path without restarting a detached graph;
precise composition for those nested calls remains open. Spread and unresolved
dynamic calls, edge-defined exception payloads and enclosing handlers,
Weak/cleanup exits, and non-call exceptional operations also remain open for
the full 01.02 exit gate.
