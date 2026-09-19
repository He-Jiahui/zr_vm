---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_infinite_for_cfg.inc
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
status: partial
---

# SSA 01.02: conditionless `for` cycle CFG

## Scope

A conditionless statement-form `for` with a supported falling-through body or
a direct terminal, unvalued `continue` now publishes its exact reachable
cycle: entry-to-header, header-to-body, body-to-step, and step-to-header. It
has no condition value, false edge, reachable join, or reachable function
exit.

The current SemanticIR function schema requires a valid `exitBlockId`, so the
graph owns a separate zero-instruction EXIT block with no predecessors. It is
a schema anchor, not a fabricated path. Closing the cycle latches semantic
termination and invalidates the current block; later unreachable source still
compiles for the established ExecBC path in disposable per-statement
SemanticIR isolation. Legacy instructions, locals, and type bindings survive;
isolated instructions, Values, Places, loans, operand rows, slots, and CFG are
discarded, so the completed graph cannot be polluted or restarted.

## Focused fixtures

`test_source_infinite_for_continue_closes_cycle_cfg` requires the exact
five-block graph, including two header predecessors, the three normal cycle
edges, and a zero-predecessor EXIT anchor. It lowers all five blocks through
ExecIR and verifies that the forward legacy `continue` target is the negative
backedge instruction.

`test_source_infinite_for_fallthrough_closes_cycle_cfg` exercises an
initializer, linear body assignment, and linear step without an explicit
exit. It requires the same cycle and proves that a resolved call after the
loop emits no SemanticIR. Two stepwise suffix fixtures freeze the completed
function's semantic counts before compiling unreachable source. A scalar
initializer must retain its legacy instructions, local-table entry, and type
binding without changing semantic locals, Places, Values, slots, or
instructions. A resource `share` expression must additionally retain exactly
one legacy `OWN_SHARE` while leaving loans and operand rows unchanged. The
active-prefix nonterminal-continue fixture stays conservative: it abandons the
partial graph, retains only the prefix call's straight-line facts, and blocks
body/suffix calls.

To keep focused test units below the repository's modularization threshold,
conditionless-`for` fixtures moved out of the 891-line general loop-exit include.
The original file is now 710 lines and the focused infinite-`for` include is
543 lines. The statement dispatcher change extends its existing SemanticIR
isolation branch instead of adding another lowering responsibility to that
already-large module.

## TDD evidence

- The initial MSVC run passed all prior 63 cases plus the new nonterminal
  fallback boundary. Only the converted terminal-continue and new natural
  fallthrough positives failed at the conservative startup barrier.
- Allowing preflighted conditionless cycles, binding the detached EXIT anchor,
  and latching semantic termination made the complete focused suite pass
  64/64.
- Independent review found that an unreachable variable declaration still
  registered a semantic local, Place, Value, and slot before guarded emission.
  The stepwise scalar suffix fixture reproduced the local-count change as the
  only failure (65 tests, 1 failure); the terminated local-registration guard
  restored 65/65 while retaining legacy/type-environment state.
- Re-review found the same pre-emit mutation class in ownership lowering,
  where the previous loop backedge could be mistaken for a just-emitted
  ownership instruction. The resource suffix fixture reproduced the internal
  compiler error as the only failure (66 tests, 1 failure). Disposable
  per-statement SemanticIR isolation restored 66/66 and covers the audited
  receiver-loan, property-reference, contiguous-view/bounds,
  value-construction, optional-merge, and ownership mutation paths as one
  boundary.
- Final review found that the declaration-as-statement diagnostic returned
  before the isolation epilogue. The invalid unreachable-declaration fixture
  reproduced the lost preserved-function pointer as the only failure (67
  tests, 1 failure); routing the error through common cleanup restored the
  original SemanticIR and `currentAst`, with 67/67 passing.

## Validation evidence (2026-09-18)

- Final-source producer runs pass 67/67 on MSVC 19.44, fresh WSL GCC
  11.4, and fresh WSL Clang 14 builds.
- The seven adjacent SSA CFG/dominance/control-edge/fact-identity/Place
  eligibility/Place promotion/Value validation executables pass on all three
  toolchains. The receiver-guard performance smoke passes 1/1 on all three.
- A fresh static WSL GCC ASan+UBSan build passes the producer 67/67 with
  `detect_leaks=1` and both sanitizers configured to halt on error.
- The MSVC finally/abrupt regression passes 7/7. The broader resource suite
  retains its established unrelated 19/20 baseline; only
  `test_resource_unique_moves_through_value_parameter_and_return` fails.
- Wiki validation passes 116 Markdown files, 115 manifest pages, and 644 local
  links. Final independent review reports no Critical, Important, or Minor
  findings.

## Boundary

Valued or nonterminal exits, nonlinear body/header components,
cleanup/finally transfers, nested unsupported control, and `foreach` remain
conservative. This checkpoint does not claim the complete SSA 01.02 loop gate.
