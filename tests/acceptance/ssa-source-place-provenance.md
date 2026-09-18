# SSA source Place provenance acceptance

## Scope

This slice connects compiler-owned source SemanticIR Places to the ExecIR
builder without consulting ExecBC. Semantic value IDs remain stable. Each
canonical Place receives an appended address result and an explicit entry
provenance value; initialization lowers to the same `[place, data]` memory form
as `STORE`. Void source functions lower to a zero-operand `RETURN`.

The source-backed fixture compiles a typed boolean local and conditional,
validates its pre-SemanticIR, builds ExecIR, and checks that every operand is
either instruction-defined or explicitly marked as an external entry. It also
checks nonempty `PLACE_BASE`, `LOAD`, and `STORE` coverage and their exact
result/operand counts.

## Evidence

- RED: MSVC producer test failed at `ZrParser_ExecIr_Build`; the first source
  `PLACE_BASE` had zero operands/results against the one/one ExecIR contract.
- GREEN: MSVC `zr_vm_pre_semantic_ir_test` passed 18/18 after place lowering.
- Adjacent MSVC builder tests `ssa_builder_cfg`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, and `ssa_builder_fact_identity`, plus
  `ssa_core_model`, passed 5/5.
- Final-source WSL GCC and WSL Clang runs each passed the same producer test
  18/18 and the same five clean SSA core/builder suites 5/5.
- The dirty user-owned `ssa_construction` target still fails its pre-existing
  diamond-dominator fixture and is not counted as evidence for this slice.

This acceptance does not claim place promotion, phi insertion/rename,
branch-local temporary merging, exceptional availability, or the 01.02 exit
gate.
