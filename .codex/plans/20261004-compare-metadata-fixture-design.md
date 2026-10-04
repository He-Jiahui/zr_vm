---
related_code:
  - tests/parser/test_ssa_compare_metadata_guards.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.c
implementation_files:
  - tests/parser/test_ssa_compare_metadata_guards.c
plan_sources:
  - .codex/plans/20261004-source-comparison-producer-lowering-design.md
tests:
  - tests/parser/test_ssa_compare_metadata_guards.c
doc_type: implementation-design
status: approved-fixture-only-native-pending
---

# Finite comparison metadata guard fixture

Root authorized this independent fixture against the existing malformed gate design. Ownership is limited to this plan, the new test TU, and `docs/testing-and-validation/ssa-compare-metadata-guards.md`. Root owns registration, compilation, execution and the finite subtask commit. Production, existing fixtures, shared headers and CMake are outside this task.

## Real construction and finite contracts

Use a standalone C main with per-case precondition and semantic failure reporting. Allocate a real VM State and SemanticContext; reserve 32 type IDs before interning actual INT64, BOOL, DOUBLE and INT8 primitives so a runtime enum cannot accidentally act as a canonical ID. Use SemanticIrFunction_Init/AddValue/Emit and Cfg_AppendBlock/Connect/BindBlockRange to produce two constants, LT/GT compare, conditional branch and two integer returns. Build through the existing public ExecIR builder and projection APIs. No replacement producer, private field-only primitive assertion, runtime provider or foreign function call is introduced.

Emission preflight rejects unknown/unsupported predicate, zero/wrong operand metadata, wrong result metadata, missing/out-of-range/self operand IDs, missing/out-of-range/already-defined result IDs, wrong arity, nonzero match and comparison metadata on another opcode. Check instruction/operand/source-map lengths and existing result definitions remain unchanged after rejection. This is a malformed-input atomicity claim, not OOM rollback coverage.

Stored malformed SemIR goes through public Validate and BuildModule. Test predicate, metadata mismatch, operand/result IDs, arity, result definition and unrelated metadata independently. The empty module must not gain a function. Builder diagnostics must be non-success and report actual instruction/source where the production gate supplies them; exact expectations are derived from the current gate rather than fabricated.

Opaque type identity alone cannot prove primitive meaning. Build a consistent DOUBLE-operand/BOOL-result SemIR and an INT64-operand/DOUBLE-result SemIR with actual canonical IDs. Require public relational Validate to accept them, then require the actual compiler canonical comparison validator to reject them. The compiler validator is called on a borrowed SemIR view in a zeroed CompilerState, using its real SemanticContext; no lowering or other compiler routine is called on that view. Typed VM cases independently reject BOOL/DOUBLE operands and INT64 result via the real canonical boundary.

Positive canonical VM cases preserve source shared MATCH0. Execute LT and GT with both truth outcomes (four cases) and inspect the returned integer selected by the comparison-dependent branch. Add a public compatibility positive case with incoming match equal to the context-proven canonical INT64 operand type. Both zero and explicit canonical INT64 match are legal public canonical VM inputs; shared Core/AOT remains MATCH0. Reject incoming BOOL, INT8 and unknown canonical IDs, never treating the runtime INT64 enum as a canonical ID.

VM negative cases include unsupported predicate, wrong operand/result primitive, compare operand/result ID or arity/range, malformed value-slot mapping, and branch using INT64. Each begins from a validated real projection. Snapshot all touched input arrays and projection fields after mutation, invoke WithCanonicalTypes, require a precise rejecting layer and empty emission (function/pcMap/count), and prove input bytes unchanged. Do not invoke VM after rejection.

## Actual TU and link closure

The executable consists of the new actual test TU and existing `tests/harness/runtime_support.c`. Reuse the direct fullsource route's `ssa_source_zr_vm_parser`, `ssa_source_zr_vm_core`, `ssa_source_zr_vm_common`, `ssa_source_zr_vm_library`, `ssa_source_xxhash`, `ssa_source_utf8proc`, `ssa_source_cjson`, `ssa_source_miniz` and Windows system library closure. Unity is not directly required by the new main; runtime support has no Unity call. Parser/Core archives must be freshly configured from repository sources, including compiler_semantic_compare.c, exec_ir_build_compare.c and exec_ir_execbc_compare_types.c. Existing `/STACK:8388608` and assertion/UBSan settings apply.

Root must verify exact link extraction and compile provenance instead of claiming a stale TU count. Compiled output and evidence stay under `E:/cargo-targets/zr_vm/*/ssa-20261004-01a0fe2b`. Source stays in the checkout; no source snapshot/copy is used. This fixture does not change full47 OPEN or claim ordinary source compilation/AOT/publication coverage.
