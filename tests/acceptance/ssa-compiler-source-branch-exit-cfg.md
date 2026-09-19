---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_branch_exit_cfg.inc
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
status: partial
---

# SSA 01.02: conditional branch-local abrupt exits

## Scope

The compiler-owned source CFG now accepts a supported statement-form `if`
whose exactly one direct arm ends in `return` or unhandled `throw` and whose
other arm falls through. The abrupt arm closes with its existing typed,
zero-successor terminator but does not set the function-wide termination latch
or function exit ID. Its join jump is omitted, so only the fall-through arm is
a predecessor of the join and later source remains in the same CFG.

The state that selects branch-local termination is initialized, reset, and
saved across disposable SemanticIR isolation. Statement-form `if` nodes in a
declared child callable now use that isolation as well, preventing the child's
temporary branch graph or fallback barrier from mutating the entry-body
sidecar.

## Focused fixtures

`test_source_if_then_return_preserves_fallthrough_cfg` requires one
payload-bearing branch return, one separate synthetic final return, one later
typed call, an eight-block graph, a zero-successor return block, and exactly
one predecessor at the conditional join. It also builds the result through
ExecIR.

`test_source_if_else_throw_excludes_interrupted_join` places a direct throw in
the else arm. It requires the throw block to have no successors, the join to
have only the then-arm predecessor, the later typed call to remain present,
and ExecIR construction to succeed.

`test_source_if_two_abrupt_arms_remains_conservative` fixes the current
boundary: when both arms terminate, no reachable join exists in this slice.
The compiler retains the legacy two-block graph, records no partial abrupt or
call facts, and keeps the persistent startup barrier set.

`test_declared_child_if_exit_does_not_pollute_entry_cfg` compiles a conditional
return inside a declared child function, checks that entry instructions,
values, Places, slots, and CFG blocks remain unchanged, then requires a later
entry-body call to publish its normal five-block invoke graph.

The older conditional return/throw fallback fixtures now use nested abrupt
control. They preserve evidence that unsupported nesting abandons the partial
graph and blocks a later call from starting a detached CFG.

## TDD evidence

- The initial 53-case MSVC run failed the direct return, direct throw, and
  child-isolation fixtures while the two-abrupt-arm conservative case passed.
- After adding arm-flow preflight and branch-local termination, the first
  direct return run exposed the intended distinction between its
  payload-bearing return and the final synthetic zero-operand return. The
  fixture now asserts both independently.
- The focused MSVC suite then passed all 53 cases, including exact join
  predecessor counts and successful ExecIR construction.

## Validation evidence (2026-09-18)

- MSVC 19.44, WSL GCC 11.4, and WSL Clang 14 each passed the focused
  pre-execution SemanticIR suite 53/53.
- WSL GCC with AddressSanitizer and UndefinedBehaviorSanitizer passed the same
  53/53 cases with leak detection and halt-on-error behavior enabled.
- The adjacent SSA builder, dominance, control-edge, fact-identity,
  Place-eligibility, Place-promotion, and ValueId-validation gate passed 7/7
  under all three toolchains.
- The receiver-guard performance smoke passed 1/1 under all three toolchains.
- MSVC `zr_vm_cfg_finally_abrupt_test` passed 7/7. The directly relevant
  resource cleanup case for return/break/continue also passed; the broader
  resource binary remains 19/20 because of its existing unrelated Unique
  parameter/return fixture.
- Wiki validation passed 116 Markdown files, 115 manifest pages, and 644 local
  links.
- Independent review reported no Critical, Important, or Minor findings.

## Boundary

This checkpoint accepts only one direct final `return` or `throw` per
conditional, optionally after already-modeled linear statements, with a
linear payload and a fall-through sibling arm. Two abrupt arms, nested abrupt
control, reachable syntax after the transfer, non-linear payloads, handled
throws, and transfers crossing `finally` or ownership cleanup remain on
conservative legacy lowering. It does not yet claim the complete SSA 01.02
exit gate or independently published SemanticIR functions for declared child
callables.
