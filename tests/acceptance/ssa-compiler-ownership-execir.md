---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
status: partial
---

# SSA 01.02: ownership facts through ExecIR

## Scope

Compiler-produced ownership facts now carry canonical inferred TypeIds for
both their source and result values. Result types derive the explicit unique,
shared, weak, wake, borrowed, loaned, or GC-box qualifier transition. A fresh
resource construction can replace a stale or moved semantic slot binding left
by nested callable compilation, while ordinary ownership operations still
require a valid typed source and cannot resurrect a consumed value.

Pending receiver aliases inherit the canonical source ValueId before receiver
borrow publication. Borrow and contiguous-view instructions likewise name
their source value, so the ExecIR builder receives explicit dataflow rather
than reconstructing it from a Place or from ExecBC.

The builder maps unique construction, GC-box transfer, and artifact-only
return-to-GC to `MOVE`; sharing, degrading, waking, borrow, reserve-borrow,
reborrow, and dereference to `COPY`; and deterministic release to `DROP`.
Loan activation and end markers become source-mapped `NOP` instructions.
Semantic values without an instruction definition become explicit
external-entry values. Instruction result references are checked as the
compatibility fallback for older hand-built fixtures whose cached definition
field remains zero.

## Focused fixture

`test_source_optional_call_skips_argument_semantic_effects` compiles resource
construction, share, degrade, a nullable wake, receiver aliasing and borrowing,
and a present-only optional call. Its member body also allocates a local so the
top-level resource construction deliberately reuses a nested-callable stack
slot. The fixture requires every semantic Value and Place to have a canonical
TypeId, builds the complete function through `ZrParser_ExecIr_Build`, and
checks that the result contains the expected `MOVE`, `COPY`, and `NOP`
families.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 debug under `D:/zr-ssa-verify-871bc234` passed all 25
  focused pre-SemanticIR cases and the receiver-guard performance case.
- WSL GCC 11.4.0 and Clang 14.0.0 debug builds under the matching `wsl-gcc`
  and `wsl-clang` roots each passed the same 25 focused cases and performance
  case.
- On MSVC, GCC, and Clang, the adjacent `ssa_builder_cfg`,
  `ssa_builder_dominance`, `ssa_builder_control_edges`,
  `ssa_builder_fact_identity`, `ssa_place_eligibility`,
  `ssa_place_promotion`, and `ssa_value_validation` gate passed 7/7.
- The broader ownership-intrinsic executable retained its established four
  failures: weak suffix-throw release, live-weak missing-member diagnostics,
  weak optional intrinsic-name dispatch, and direct-wake intrinsic-name
  dispatch. The focused change introduced no additional failure there.
- The WSL GCC ASan+UBSan build under `wsl-gcc-asan` passed all 25 focused
  cases with leak detection and both sanitizers configured to halt on the
  first error.

## Boundary

The supported nullable `void` member-call path now owns a typed `CALL_*` fact
and lowers it to a split `INVOKE`, as recorded in
[SSA 01.02: nullable optional-call SemanticIR CFG](ssa-compiler-source-optional-call-cfg.md).
General calls, nullable value-producing optional chains, Weak-wake CFGs,
exception payloads/handlers, and cleanup exits remain outside the
compiler-owned graph. ExecIR ownership and nullability fields also remain
`UNKNOWN`; this checkpoint preserves canonical type tokens and executable
opcode expansion but does not implement the later ownership metadata
projection. These items keep the overall 01.02 exit gate partial.
