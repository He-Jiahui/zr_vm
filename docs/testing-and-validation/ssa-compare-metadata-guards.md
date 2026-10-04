---
related_code:
  - tests/parser/test_ssa_compare_metadata_guards.c
  - tests/harness/runtime_support.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_compare_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_compare_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
implementation_files:
  - tests/parser/test_ssa_compare_metadata_guards.c
plan_sources:
  - .codex/plans/20261004-source-comparison-producer-lowering-design.md
  - .codex/plans/20261004-compare-metadata-fixture-design.md
tests:
  - tests/parser/test_ssa_compare_metadata_guards.c
doc_type: testing-guide
status: fixture-written-native-pending
---

# Comparison metadata guards

This independent executable checks malformed comparison metadata through real production constructors and consumers. It owns no production changes. Its baseline creates a real VM State and canonical SemanticContext, reserves 32 type IDs, interns INT64/BOOL/DOUBLE/INT8, then calls public SemIR value/instruction and CFG producers. Two integer constants feed a comparison; its result controls a branch whose two arms return different integer values. Public BuildModule, Core VerifyAll and BuildProjectionWithConstants connect this SemIR to the actual canonical VM boundary.

The reserved IDs make canonical INT64 and BOOL identities differ from their runtime enum numbers. A runtime enum used as a canonical ID would therefore fail this fixture rather than accidentally select the intended primitive node. Function and signature constants are fixture identity only. This is not ordinary source compilation, real source callable ABI, automatic compiler publication, AOT execution or full47 acceptance coverage.

## Finite matrix and rejecting layers

The executable contains 63 cases: twenty emission faults, the same twenty stored SemIR faults, two canonical semantic primitive faults, and twenty-one canonical VM cases. A `PRECONDITION` failure is counted separately from an expected semantic rejection. Each case prints its name and final status; malformed builder/VM cases print their actual diagnostics. The program exits nonzero for either kind of unexpected failure.

| Group | Contract |
| --- | --- |
| Emit, 20 cases | Reject source-unsupported EQ and unknown selector; missing/wrong operand metadata; wrong result metadata; zero/out-of-range result; zero/out-of-range/self operand; arity 0/1/3; already-defined result; unrelated comparison metadata; nonzero shared match; wrong operand/result value type; invalid result definition. No SemIR bytes, pool lengths or source-map rows change. |
| Stored SemIR, 20 cases | Public Validate rejects the mutated instruction/value/pool. BuildModule rejects with INVALID_VALUE and the relevant instruction/source identity; the module remains byte-identical and contains no published function. The SemIR input bytes remain unchanged. |
| Canonical semantic, 2 cases | Consistent DOUBLE operands with BOOL result and consistent INT64 operands with DOUBLE result retain opaque relational validity. The actual compiler canonical comparison validator rejects primitive meaning using the real context. |
| Canonical VM success, 5 cases | MATCH0 LT/GT each execute true and false arms. Explicit canonical INT64 incoming match also succeeds. Returns prove a signed integer comparison actually controls the bool branch. |
| Canonical VM failure, 16 cases | Reject BOOL/INT8/unknown match, BOOL/DOUBLE operand primitive, INT64 result primitive, INT64 branch operand, unknown selector, zero operand/result ID, bad arities, pool starts outside range, out-of-range physical slot and wrong slot value ID. Empty emission and immutable input are mandatory. |

SemIR's pointer-free comparison checker knows relations among IDs, not the primitive kind of every opaque ID. Consistent wrong primitive IDs intentionally pass structural Validate in the two canonical semantic cases. The fixture then calls `compiler_semantic_compare_validate` on a borrowed SemIR view in a zeroed CompilerState with the actual context; this validator only reads that view. No compiler lowering routine runs on the borrowed state. This exercises the real primitive boundary rather than inventing a test-only predicate about numeric type IDs.

The typed VM tests independently pass actual canonical primitives through `ZrParser_ExecBcProjection_MaterializeVmFunctionWithCanonicalTypes`. Canonical comparison annotation requires INT64 operands and BOOL result. Its prepared owned copy resolves canonical matched INT64 to runtime INT64; low-level VM validation requires that runtime match, both runtime INT64 operand tags, and runtime BOOL result. Successful VM execution uses a negative integer and a positive integer, and the bool result selects the return arm. An INT64 branch operand reaches the typed branch guard and fails with UNSUPPORTED, block 1, instruction/source 4, expected BOOL, actual INT64. Other malformed VM inputs fail the canonical comparison guard with INVALID_VALUE, block 0, instruction/source 3. The fixture asserts those distinctions.

The public canonical VM boundary accepts incoming match zero or the exact context-proven operand canonical INT64 ID. BOOL, INT8 and unknown IDs fail. Shared source Core/AOT comparison metadata must still have MATCH0: every new baseline verifies Core at all levels before projection, and the fixture checks that the source function and original projected compare retain zero. Public explicit canonical match compatibility is a VM boundary test and does not relax shared MATCH0.

## Mutation and publication evidence

Emission snapshots include the semantic function structure, instructions, values, operand pool and source-map rows after the deliberate fault and before calling Emit. Stored faults snapshot those same inputs and the empty output module before Validate/BuildModule. These prove malformed-input rejection is non-mutating; they do not prove allocator-failure rollback.

Canonical VM cases snapshot the projection structure and every side pool: instruction/opcode, frame, values/slots, operands/results, memory, CFG, phi/copy/move, source-map, constant/layout, GC-root and deopt pools. Canonical type array metadata and node bytes are also checked. The copy checks occur immediately after materialization, before positive VM execution. A rejected result must have null function, null pcMap and zero pcMapCount. A negative case never executes a VM function. Positive functions are rooted using the real GC API, executed through runtime_support, then unrooted and freed.

## Actual build closure and pending validation

Compile the actual new test TU with existing `tests/harness/runtime_support.c`. Reuse the direct fullsource route's parser, core, common, library and xxhash/utf8proc/cjson/miniz static archives; use its native system libraries, assertion/UBSan flags and 8 MiB stack setting. The standalone main does not call Unity. All sources must come directly from the current checkout, including the new compiler comparison, builder comparison and canonical comparison annotation TUs. Native configuration, archive provenance, extracted link objects and executable results are owned by Root. Outputs remain beneath `E:/cargo-targets/zr_vm/*/ssa-20261004-01a0fe2b`.

At this document's static freeze, no compilation or test execution has verified this new fixture. One accidentally attempted `clang -fsyntax-only` command found no clang executable and did not start a compiler; it produced no object or test result. Root must run the real native route and record its immutable receipt before describing these 63 cases as passing. No full47 OPEN state changes here.
