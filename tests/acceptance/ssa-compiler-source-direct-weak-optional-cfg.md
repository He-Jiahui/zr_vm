---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_optional_value.inc
status: partial
---

# SSA 01.02: direct weak optional source CFG

## Scope

Direct `weak?.method(...)` retains a source-owned present/absent CFG when the
weak receiver has a canonical ValueId and the known member call has supported
argument facts. The entry defines one semantic `OWN_CONSTRUCT(WAKE)` and
branches on its nullable result. The present path alone evaluates arguments
and invokes the member. The normal continuation drops the temporary wake owner
before the join. The exception edge reaches a landing block that reads the
payload, drops the owner, and explicitly throws that payload; the absent branch
never drops a nonexistent owner. Existing ExecBC behavior remains unchanged.
The skipped-argument rule follows `docs/zr_language_specification.md`'s
optional-receiver contract; `lua/QuickJS-master/tests/test_language.js` also
covers short-circuited optional-call arguments as reference-language evidence.

## Test Inventory

`test_direct_weak_optional_call_keeps_source_cfg` requires a weak guard fact,
WAKE-defined branch operand, guarded argument assignment in the present arm,
one normal and one exceptional DROP, no join DROP, explicit exceptional THROW,
and a successfully built ExecIR graph with two DROP instructions, one INVOKE,
and one THROW. `test_direct_weak_optional_value_merges_after_guard`
checks the two-predecessor nullable result merge and balanced path-local drops.
`test_unmodeled_weak_optional_value_abandons_source_cfg` suppresses source CFG
startup and requires no synthetic WAKE or partial graph. This is a producer
fallback injection; allocation-failure and cancel injection are not exercised.

## Baseline And Tooling Evidence

The preceding focused pre-SemanticIR baseline was 102/102 cases. The initial
direct-weak tests failed because the source CFG was abandoned; a targeted GDB
backtrace traced the call rejection to an exact-argument check receiving a
function signature instead of the resolved member signature. Subsequent RED
tests exposed an exception cleanup block without an explicit terminator.

The WSL GCC 11.4.0 debug build at
`/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu` rebuilt the affected parser
objects, shared library, and focused test binary. Direct execution passed
105/105 Unity cases, including all three new tests. The producer's
`ValidatePreSemanticIr` and `ZrParser_ExecIr_Build` both succeed for the two
supported shapes.
The WSL Clang 14.0.0 debug static build at
`/home/hejiahui/zrvm-ssa-nested-clang.kBIWlA` passed the same 105/105 after
rebuilding the stale `semantic_ir.c` object; before that rebuild, the older
object made the pre-existing single-definition validation test fail.
The GCC adjacent `ssa_construction`, `ssa_builder_cfg`,
`ssa_builder_iterator_invokes`, `ssa_source_value_facts`,
`ssa_source_straight_line_cfg`, and `ssa_source_cleanup_cfg` CTest selection
passed 6/6. Its missing source-value-facts executable and four harness objects
were built before the run; no missing test was counted as passing. The final
ExecIR projection assertions were rerun on both toolchains (105/105 each).

## Acceptance Decision

Partial 01.02 source-CFG checkpoint only. Runtime differential behavior is not
established: the current ExecIR builder projects semantic WAKE to COPY. The
full plan's exception, loop, and cross-backend gates remain open. The compiler
state is freshly initialized per fixture; cancellation, OOM, and repeated-build
failure injection are outside this producer-only change.
