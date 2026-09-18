---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_optional_value.inc
status: partial
---

# SSA 01.02: nullable optional-call value merge

## Scope

A known value-producing `receiver?.method(arguments)` call now keeps its
canonical nullable result in the source-owned CFG. The receiver branch sends
the true edge through the present and invoke blocks and the false edge to a
dedicated absent block. The invoke keeps ordered normal and exception edges.
Only the normal continuation reaches the value merge; the exceptional
continuation remains the explicit zero-instruction propagation sink.

The compiler creates one temporary Place with the chain's canonical nullable
TypeId before branching. On the normal path it converts the call's non-null
return value to that nullable type and stores it. On the absent path it creates
a null constant carrying the same nullable TypeId and stores it into the same
Place. Both paths then enter the join, which loads one fresh merged ValueId.
These semantic-only merge instructions do not duplicate or reorder the
existing ExecBC merge-slot operations.

Direct member calls use their bound receiver as the semantic typed-callee
operand. Runtime `argCount` also includes that hidden receiver, so the bridge
subtracts it before reading the remaining explicit argument slots. This keeps a
zero-argument member call from probing an unrelated stack slot and preserves
the order of real source arguments.

## Focused fixture

`test_source_optional_value_merges_present_and_absent_paths` compiles
`receiver?.read()` after an explicit nullable `wake(weak)`. It checks the exact
eight-block topology, including present, absent, join, invoke, normal,
exception, and exit blocks. It requires one call at the invoke tail, a nullable
`CONVERT` plus `STORE` on the normal path, a typed null `CONSTANT` plus `STORE`
on the absent path, one `LOAD` at the two-predecessor join, and the same Place
and TypeId for every merge operation. The built ExecIR must contain exactly one
`INVOKE` and one exception block.

`test_unmodeled_optional_value_abandons_partial_source_cfg` keeps an optional
member-only chain outside this call-shaped slice. After first starting a source
`if` graph, it requires lowering to discard synthetic branch instructions and
retain the conservative legacy two-block graph.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228, WSL GCC 11.4.0, and WSL Clang 14.0.0 each rebuilt and
  passed the focused pre-execution Semantic IR suite 26/26.
- The adjacent `ssa_builder_cfg`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, `ssa_builder_fact_identity`,
  `ssa_place_eligibility`, `ssa_place_promotion`, and
  `ssa_value_validation` gate passed 7/7 on all three toolchains.
- The receiver-guard performance executable passed its one test on all three
  toolchains.
- WSL GCC ASan+UBSan passed the focused suite 26/26 with leak detection and
  both sanitizers configured to halt on the first error.
- The broader MSVC ownership-intrinsic executable retained the same four
  pre-existing failures. Its nullable optional argument-skipping regression
  passed, so those failures are not attributed to this checkpoint.

## Boundary

This checkpoint covers nullable guards around known member calls with complete
canonical receiver, symbol, result-type, and explicit-argument facts. Missing
facts still abandon an active partial graph. Weak-wake guards, cleanup edges,
edge-defined exception payloads and enclosing handlers, and non-call
exceptional operations remain open for the full 01.02 exit gate. Resolved
general-call CFG startup is covered by
[the follow-up checkpoint](ssa-compiler-source-general-call-cfg.md).
